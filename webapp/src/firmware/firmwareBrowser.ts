// GitHub release browser, ported from DelSolClockAppMaui/Services/FirmwareBrowser.cs.
// Fetches releases from the public GitHub API (CORS-enabled) and the .bin asset
// download URLs. No auth needed for public repos (subject to GitHub's rate limit).

export const REPO_OWNER = 'Marcus10110';
export const REPO_NAME = 'DelSolClock';

// Base URL of the Del Sol backend API (see api/ + render.yaml). It currently
// proxies asset downloads: GitHub's CDN doesn't send CORS on the final asset
// response, so the static frontend can't read asset bytes directly; the API
// fetches them server-side and re-serves with CORS. Overridable via
// VITE_API_BASE for local/dev or a different host.
export const API_BASE =
  import.meta.env.VITE_API_BASE || 'https://delsolapi.markgarrison.io';

/** Fire-and-forget warmup so the free-tier API is awake before a download. */
export function warmupApi(): void {
  // Don't await; failures are harmless (the service may simply be cold-starting).
  fetch(`${API_BASE}/healthz`, { method: 'GET' }).catch(() => {});
}

export interface ReleaseAsset {
  name: string;
  size: number;
  /** Human-facing github.com download URL (302-redirects; NOT CORS-safe in iOS WebKit). */
  browserDownloadUrl: string;
  /** API asset endpoint (api.github.com/.../assets/<id>); CORS-safe with Accept: octet-stream. */
  apiUrl: string;
}

/** Machine-readable metadata embedded in the release body (delsol-meta block). */
export interface ReleaseMeta {
  proto: number;
  app?: string;
  /** SHA-256 of the released spiffs.bin (hex) — compare to the device's fs hash. */
  fsSha256: string;
  fsSize?: number;
}

export interface Release {
  tagName: string;
  name: string;
  body: string;
  htmlUrl: string;
  publishedAt: string;
  assets: ReleaseAsset[];
  /** The firmware app image asset (firmware.bin), if any. */
  firmwareAsset: ReleaseAsset | null;
  /** The SPIFFS image asset (spiffs.bin), if any. */
  spiffsAsset: ReleaseAsset | null;
  /** Parsed delsol-meta block from the body, or null (legacy / proto 1 release). */
  meta: ReleaseMeta | null;
}

/**
 * Extract the delsol-meta JSON from a release body. The block is an HTML comment
 * so it's invisible on the rendered GitHub page:
 *   <!-- delsol-meta
 *   {"proto":2,"fsSha256":"...","fsSize":655360}
 *   -->
 * Returns null if absent or unparseable (treated as a legacy/proto-1 release).
 */
export function parseReleaseMeta(body: string): ReleaseMeta | null {
  const match = body.match(/<!--\s*delsol-meta\s*([\s\S]*?)-->/);
  if (!match) return null;
  try {
    const obj = JSON.parse(match[1].trim()) as Partial<ReleaseMeta>;
    if (typeof obj.fsSha256 !== 'string' || typeof obj.proto !== 'number') {
      return null;
    }
    return {
      proto: obj.proto,
      app: obj.app,
      fsSha256: obj.fsSha256.toLowerCase(),
      fsSize: obj.fsSize,
    };
  } catch {
    return null;
  }
}

interface GhAsset {
  name: string;
  size: number;
  browser_download_url: string;
  url: string;
}
interface GhRelease {
  tag_name: string;
  name: string | null;
  body: string | null;
  html_url: string;
  published_at: string;
  assets: GhAsset[];
}

/**
 * Fetch releases for the Del Sol repo. Returns only releases that ship a .bin
 * asset (the firmware image), newest first.
 */
export async function fetchReleases(
  owner = REPO_OWNER,
  repo = REPO_NAME,
): Promise<Release[]> {
  const url = `https://api.github.com/repos/${owner}/${repo}/releases`;
  const res = await fetch(url, {
    headers: { Accept: 'application/vnd.github+json' },
  });
  if (!res.ok) {
    throw new Error(`GitHub API ${res.status}: ${res.statusText}`);
  }
  const raw = (await res.json()) as GhRelease[];

  return raw
    .map((r): Release => {
      const assets = r.assets.map((a) => ({
        name: a.name,
        size: a.size,
        browserDownloadUrl: a.browser_download_url,
        apiUrl: a.url,
      }));
      const byName = (name: string) =>
        assets.find((a) => a.name.toLowerCase() === name) ?? null;
      // firmware.bin specifically; fall back to the first non-spiffs .bin for
      // older releases that named the app image differently.
      const firmwareAsset =
        byName('firmware.bin') ??
        assets.find(
          (a) =>
            a.name.toLowerCase().endsWith('.bin') &&
            a.name.toLowerCase() !== 'spiffs.bin',
        ) ??
        null;
      const body = r.body || '';
      return {
        tagName: r.tag_name,
        name: r.name || r.tag_name,
        body,
        htmlUrl: r.html_url,
        publishedAt: r.published_at,
        assets,
        firmwareAsset,
        spiffsAsset: byName('spiffs.bin'),
        meta: parseReleaseMeta(body),
      };
    })
    .filter((r) => r.firmwareAsset !== null);
}

/**
 * Download an asset as raw bytes, via the backend API's download proxy.
 *
 * GitHub release-asset bytes are served from a CDN with no CORS header on the
 * final response, so the browser can't read them directly (this is the "Load
 * failed" seen in iOS WebKit / Bluefy). We pass the GitHub API asset URL to our
 * API, which fetches it server-side and re-serves it with CORS.
 */
async function downloadAsset(asset: ReleaseAsset): Promise<Uint8Array> {
  const url = `${API_BASE}/dl?url=${encodeURIComponent(asset.apiUrl)}`;
  const res = await fetch(url, {
    headers: { Accept: 'application/octet-stream' },
  });
  if (!res.ok) {
    // Surface the API's JSON error message when present.
    let detail = res.statusText;
    try {
      const body = (await res.json()) as { error?: string };
      if (body?.error) detail = body.error;
    } catch {
      // non-JSON body; keep statusText
    }
    throw new Error(`Download failed (${res.status}): ${detail}`);
  }
  const buf = await res.arrayBuffer();
  return new Uint8Array(buf);
}

/** Download a firmware (app) image asset as raw bytes. */
export function downloadFirmware(asset: ReleaseAsset): Promise<Uint8Array> {
  return downloadAsset(asset);
}

/** Download a SPIFFS image asset as raw bytes. */
export function downloadFilesystem(asset: ReleaseAsset): Promise<Uint8Array> {
  return downloadAsset(asset);
}
