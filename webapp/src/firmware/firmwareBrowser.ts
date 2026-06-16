// GitHub release browser, ported from DelSolClockAppMaui/Services/FirmwareBrowser.cs.
// Fetches releases from the public GitHub API (CORS-enabled) and the .bin asset
// download URLs. No auth needed for public repos (subject to GitHub's rate limit).

export const REPO_OWNER = 'Marcus10110';
export const REPO_NAME = 'DelSolClock';

export interface ReleaseAsset {
  name: string;
  size: number;
  browserDownloadUrl: string;
}

export interface Release {
  tagName: string;
  name: string;
  body: string;
  htmlUrl: string;
  publishedAt: string;
  assets: ReleaseAsset[];
  /** The first .bin asset, if any — what we flash. */
  binaryAsset: ReleaseAsset | null;
}

interface GhAsset {
  name: string;
  size: number;
  browser_download_url: string;
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
      }));
      const binaryAsset =
        assets.find((a) => a.name.toLowerCase().endsWith('.bin')) ?? null;
      return {
        tagName: r.tag_name,
        name: r.name || r.tag_name,
        body: r.body || '',
        htmlUrl: r.html_url,
        publishedAt: r.published_at,
        assets,
        binaryAsset,
      };
    })
    .filter((r) => r.binaryAsset !== null);
}

/** Download a firmware asset as raw bytes. */
export async function downloadFirmware(asset: ReleaseAsset): Promise<Uint8Array> {
  const res = await fetch(asset.browserDownloadUrl);
  if (!res.ok) {
    throw new Error(`Download ${res.status}: ${res.statusText}`);
  }
  const buf = await res.arrayBuffer();
  return new Uint8Array(buf);
}
