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
    // Node's fetch follows the GitHub redirect chain to the CDN and reads the
    // bytes server-side (no CORS restriction applies here).
    const upstream = await fetch(assetUrl, {
      headers: {
        Accept: 'application/octet-stream',
        'User-Agent': 'delsol-api',
      },
    });
    if (!upstream.ok) {
      res.status(upstream.status).json({
        error: `Upstream ${upstream.status}: ${upstream.statusText}`,
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
  console.log(`delsol-api listening on :${port}`);
});
