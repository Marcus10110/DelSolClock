// Del Sol Clock backend API.
//
// Currently a single-purpose CORS proxy for release-asset downloads, but named
// generically (delsol-api) so other backend endpoints can live here later — add
// new routes alongside /healthz and /dl below.
//
// Why the proxy exists: the web app is a static site. GitHub release-asset
// binaries (firmware.bin / spiffs.bin) are served from a CDN that does NOT send
// an Access-Control-Allow-Origin header on the final response, so a browser fetch
// cannot read the bytes ("Load failed" in iOS WebKit / Bluefy). This service
// fetches the asset server-side (no CORS in Node) and re-serves it with CORS.
//
// The /dl route is intentionally NOT an open proxy: it only fetches release
// assets from the one repo below (validated against the GitHub API asset URL prefix).

import express from 'express';

const app = express();

const ALLOWED_OWNER = 'Marcus10110';
const ALLOWED_REPO = 'DelSolClock';
// GitHub API asset URLs look like:
//   https://api.github.com/repos/<owner>/<repo>/releases/assets/<id>
const ALLOWED_PREFIX = `https://api.github.com/repos/${ALLOWED_OWNER}/${ALLOWED_REPO}/releases/assets/`;

// Allow the frontend origins to read responses. '*' is fine here: the only thing
// this service exposes is public release assets, and requests carry no cookies.
const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, OPTIONS',
  'Access-Control-Allow-Headers': 'Accept',
};

// Optional GitHub token. Anonymous API requests from cloud/datacenter IPs (like
// Render's) get rate-limited/403'd by GitHub; an authenticated request gets
// 5000/hr and isn't IP-throttled the same way. A read-only token for public
// repos is sufficient (public release assets need no special scope). Set via the
// GITHUB_TOKEN env var on Render. Sent ONLY on the api.github.com call, never on
// the signed CDN URL.
const GITHUB_TOKEN = process.env.GITHUB_TOKEN || '';

app.use((req, res, next) => {
  for (const [k, v] of Object.entries(CORS_HEADERS)) res.setHeader(k, v);
  if (req.method === 'OPTIONS') {
    res.status(204).end();
    return;
  }
  next();
});

// Warmup / liveness. The frontend pings this on load + on BLE connect so the
// free-tier service is awake (it cold-starts after idling) before a download.
app.get('/healthz', (_req, res) => {
  res.status(200).json({ ok: true });
});

// GET /dl?id=<assetId>  — download a release asset's raw bytes.
// Accepts a bare numeric asset id (preferred) or a full GitHub API asset URL;
// either way the resolved URL must start with ALLOWED_PREFIX.
app.get('/dl', async (req, res) => {
  const { id, url } = req.query;

  let assetUrl;
  if (typeof id === 'string' && /^\d+$/.test(id)) {
    assetUrl = `${ALLOWED_PREFIX}${id}`;
  } else if (typeof url === 'string') {
    assetUrl = url;
  } else {
    res.status(400).json({ error: 'Provide ?id=<assetId> or ?url=<github api asset url>.' });
    return;
  }

  if (!assetUrl.startsWith(ALLOWED_PREFIX)) {
    res.status(403).json({ error: 'Only Del Sol Clock release assets may be proxied.' });
    return;
  }

  try {
    // Step 1: hit the GitHub API asset endpoint WITHOUT following the redirect.
    // With Accept: octet-stream it returns a 302 to a signed CDN URL. Authenticate
    // if a token is configured (anonymous requests from Render's IP get 403'd).
    const apiHeaders = {
      Accept: 'application/octet-stream',
      'User-Agent': 'delsol-api',
      'X-GitHub-Api-Version': '2022-11-28',
    };
    if (GITHUB_TOKEN) apiHeaders.Authorization = `Bearer ${GITHUB_TOKEN}`;
    const apiRes = await fetch(assetUrl, {
      redirect: 'manual',
      headers: apiHeaders,
    });

    // Step 2: follow the redirect to the CDN with a CLEAN request (no Accept /
    // User-Agent / auth headers). The signed CDN URL already encodes the content
    // type and disposition; forwarding the API request's headers to the signed
    // blob URL makes release-assets.githubusercontent.com reject it with 403
    // (this is why the auto-following fetch failed on Render). A bare GET — like
    // a browser following a download link — succeeds.
    let upstream = apiRes;
    if (apiRes.status >= 300 && apiRes.status < 400) {
      const location = apiRes.headers.get('location');
      if (!location) {
        res.status(502).json({ error: 'GitHub redirect had no Location header.' });
        return;
      }
      upstream = await fetch(location); // no custom headers
    }

    if (!upstream.ok) {
      const body = await upstream.text().catch(() => '');
      res.status(upstream.status).json({
        error: `Upstream ${upstream.status} ${upstream.statusText}`.trim() +
          (body ? `: ${body.slice(0, 200)}` : ''),
      });
      return;
    }

    res.setHeader('Content-Type', 'application/octet-stream');
    const len = upstream.headers.get('content-length');
    if (len) res.setHeader('Content-Length', len);

    const buf = Buffer.from(await upstream.arrayBuffer());
    res.status(200).end(buf);
  } catch (err) {
    res.status(502).json({ error: `Proxy fetch failed: ${err?.message ?? String(err)}` });
  }
});

const port = process.env.PORT || 3001;
app.listen(port, () => {
  console.log(
    `delsol-api listening on :${port} (github auth: ${GITHUB_TOKEN ? 'on' : 'off'})`,
  );
});
