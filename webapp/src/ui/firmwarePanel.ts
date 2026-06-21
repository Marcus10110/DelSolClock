// Firmware update panel: browse GitHub releases and flash one over BLE.
// Mirrors the MAUI UpdatePage. Owns its own DOM; the StatusPage mounts it and
// hands it the active IConnection (real or demo) when connected.

import type { FirmwareUpdateProgress, IConnection } from '../ble/iconnection';
import type { FirmwareInfo } from '../ble/parsers';
import {
  downloadFilesystem,
  downloadFirmware,
  fetchReleases,
  type Release,
  type ReleaseAsset,
} from '../firmware/firmwareBrowser';

export class FirmwarePanel {
  readonly el: HTMLElement;
  private conn: IConnection | null = null;
  private releases: Release[] = [];
  private busy = false;
  /** Device firmware info (proto + current SPIFFS hash), once connected. */
  private deviceInfo: FirmwareInfo | null = null;
  /** Optional sink for progress/diagnostic lines (wired to the on-screen log). */
  private logger: ((level: string, message: string) => void) | null = null;

  constructor() {
    this.el = document.createElement('div');
    this.el.className = 'card';
    this.renderIdle();
  }

  /** Route the panel's diagnostics to the on-screen log (Bluefy has no console). */
  setLogger(logger: (level: string, message: string) => void): void {
    this.logger = logger;
  }

  private log(level: string, message: string): void {
    this.logger?.(level, message);
  }

  /** Called by StatusPage whenever the connection changes (or clears). */
  setConnection(conn: IConnection | null): void {
    this.conn = conn;
    if (!conn) {
      this.releases = [];
      this.busy = false;
      this.deviceInfo = null;
      this.renderIdle();
    }
  }

  /** Called by StatusPage when the device reports its firmware/SPIFFS info. */
  setDeviceInfo(info: FirmwareInfo): void {
    this.deviceInfo = info;
    // Re-render the list (if shown) so FS match badges reflect the device hash.
    if (this.releases.length > 0 && !this.busy) this.renderReleases();
  }

  private renderIdle(): void {
    this.el.innerHTML = `
      <h2>Firmware</h2>
      <p class="muted-text">Check GitHub for the latest Del Sol firmware release.</p>
      <button id="fw-check" class="secondary">Check for updates</button>
      <div id="fw-list"></div>
    `;
    (this.el.querySelector('#fw-check') as HTMLButtonElement).addEventListener(
      'click',
      () => void this.checkForUpdates(),
    );
  }

  private async checkForUpdates(): Promise<void> {
    const btn = this.el.querySelector('#fw-check') as HTMLButtonElement;
    const list = this.el.querySelector('#fw-list') as HTMLElement;
    btn.disabled = true;
    btn.innerHTML = `<span class="spinner"></span> Checking…`;
    list.innerHTML = '';
    try {
      this.releases = await fetchReleases();
      this.renderReleases();
    } catch (err) {
      const msg = err instanceof Error ? err.message : String(err);
      list.innerHTML = `<p class="fw-error">Couldn’t fetch releases: ${escapeHtml(msg)}</p>`;
    } finally {
      btn.disabled = false;
      btn.textContent = 'Check for updates';
    }
  }

  private renderReleases(): void {
    const list = this.el.querySelector('#fw-list') as HTMLElement;
    if (this.releases.length === 0) {
      list.innerHTML = `<p class="muted-text">No releases with a firmware (.bin) asset were found.</p>`;
      return;
    }

    const deviceFsHash = this.deviceInfo?.fsHash ?? null;
    const deviceSupportsFs = (this.deviceInfo?.proto ?? 1) >= 2;

    // Header: show the device's current filesystem hash (full, monospace) so it's
    // visible at a glance and can be compared against any release.
    const fsHeader =
      deviceSupportsFs && deviceFsHash
        ? `<div class="fw-fs-current muted-text">Device filesystem: <code title="${escapeHtml(deviceFsHash)}">${escapeHtml(deviceFsHash)}</code></div>`
        : '';

    list.innerHTML = fsHeader + this.releases
      .map((r, i) => {
        const size = r.firmwareAsset ? formatBytes(r.firmwareAsset.size) : '';
        const date = formatDate(r.publishedAt);
        const notes = renderNotes(r.body);

        // Filesystem status: compare the release's published hash to the device's.
        // Only meaningful when both the device (proto>=2) and the release expose one.
        let fsRow = '';
        if (deviceSupportsFs && r.spiffsAsset && r.meta?.fsSha256) {
          const matches =
            deviceFsHash !== null &&
            deviceFsHash.toLowerCase() === r.meta.fsSha256.toLowerCase();
          const relHash = r.meta.fsSha256.toLowerCase();
          fsRow = matches
            ? `<div class="fw-fs-row"><span class="fw-fs-badge fw-fs-ok">✓ Filesystem up to date</span></div>`
            : `<div class="fw-fs-row">
                 <span class="fw-fs-badge fw-fs-diff">Filesystem differs</span>
                 <button class="fw-update-fs" data-index="${i}">Update filesystem</button>
               </div>
               <div class="fw-fs-hash muted-text">This release: <code title="${escapeHtml(relHash)}">${escapeHtml(relHash)}</code></div>`;
        }

        return `
          <div class="fw-release" data-index="${i}">
            <div class="fw-release-head">
              <div>
                <div class="fw-release-name">${escapeHtml(r.name)}</div>
                <div class="muted-text fw-release-meta">${escapeHtml(r.tagName)} · ${date} · ${size}</div>
              </div>
              <button class="fw-update" data-index="${i}">Update</button>
            </div>
            ${fsRow}
            ${notes}
            ${githubLink(r.htmlUrl)}
          </div>`;
      })
      .join('');

    for (const btn of list.querySelectorAll<HTMLButtonElement>('.fw-update')) {
      btn.addEventListener('click', () => {
        const idx = Number(btn.dataset.index);
        void this.runUpdate(this.releases[idx]);
      });
    }
    for (const btn of list.querySelectorAll<HTMLButtonElement>('.fw-update-fs')) {
      btn.addEventListener('click', () => {
        const idx = Number(btn.dataset.index);
        void this.runFilesystemUpdate(this.releases[idx]);
      });
    }
  }

  private runUpdate(release: Release): Promise<void> {
    if (!release.firmwareAsset) return Promise.resolve();
    return this.runFlash({
      kind: 'firmware',
      asset: release.firmwareAsset,
      releaseName: release.name,
      confirmExtra: '',
      download: downloadFirmware,
      flash: (data, onProgress) => this.conn!.updateFirmware(data, onProgress),
    });
  }

  private runFilesystemUpdate(release: Release): Promise<void> {
    if (!release.spiffsAsset) return Promise.resolve();
    if ((this.deviceInfo?.proto ?? 1) < 2) {
      alert('This device’s firmware does not support filesystem updates.');
      return Promise.resolve();
    }
    return this.runFlash({
      kind: 'filesystem',
      asset: release.spiffsAsset,
      releaseName: release.name,
      // Filesystem writes have no rollback (single partition) — warn explicitly.
      confirmExtra:
        '\n\nThis replaces the device filesystem and cannot be rolled back. ' +
        'If interrupted, the filesystem may be left invalid until re-flashed.',
      download: downloadFilesystem,
      flash: (data, onProgress) => this.conn!.updateFilesystem(data, onProgress),
    });
  }

  /** Shared download-then-flash flow used by both firmware and filesystem updates. */
  private async runFlash(opts: {
    kind: 'firmware' | 'filesystem';
    asset: ReleaseAsset;
    releaseName: string;
    confirmExtra: string;
    download: (asset: ReleaseAsset) => Promise<Uint8Array>;
    flash: (
      data: Uint8Array,
      onProgress: (p: FirmwareUpdateProgress) => void,
    ) => Promise<boolean>;
  }): Promise<void> {
    if (this.busy) return;
    if (!this.conn || !this.conn.isConnected) {
      alert(`Connect to the device before updating the ${opts.kind}.`);
      return;
    }

    const ok = confirm(
      `Flash the ${opts.kind} from "${opts.releaseName}" (${formatBytes(opts.asset.size)}) to the device?\n\n` +
        `Do not close this page or move out of range during the update.` +
        opts.confirmExtra,
    );
    if (!ok) return;

    this.busy = true;
    const list = this.el.querySelector('#fw-list') as HTMLElement;
    list.innerHTML = `
      <div class="fw-progress-wrap">
        <div class="fw-progress-msg" id="fw-msg">Starting…</div>
        <div class="fw-bar"><div class="fw-bar-fill" id="fw-fill" style="width:0%"></div></div>
        <div class="fw-pct" id="fw-pct">0%</div>
        <div class="fw-stats muted-text" id="fw-stats"></div>
      </div>`;

    const msgEl = this.el.querySelector('#fw-msg') as HTMLElement;
    const fillEl = this.el.querySelector('#fw-fill') as HTMLElement;
    const pctEl = this.el.querySelector('#fw-pct') as HTMLElement;
    const statsEl = this.el.querySelector('#fw-stats') as HTMLElement;

    const rate = new RateEstimator();

    try {
      msgEl.textContent = `Downloading ${opts.kind}…`;
      this.log('info', `Downloading ${opts.kind} from ${opts.asset.browserDownloadUrl}`);
      const data = await opts.download(opts.asset);
      this.log('ok', `Downloaded ${opts.kind}: ${data.length} bytes.`);

      const success = await opts.flash(data, (p) => {
        fillEl.style.width = `${p.percent}%`;
        pctEl.textContent = `${p.percent}%`;
        msgEl.textContent = p.message;

        // Speed + ETA from the smoothed byte rate (BLE chunk times are jittery).
        if (p.bytesSent !== undefined && p.totalBytes) {
          rate.update(p.bytesSent);
          const bps = rate.bytesPerSecond();
          if (bps > 0) {
            const remaining = Math.max(0, p.totalBytes - p.bytesSent);
            const etaSec = remaining / bps;
            statsEl.textContent = `${formatRate(bps)} · ${formatEta(etaSec)} remaining`;
          }
        }
      });

      statsEl.textContent = '';
      if (success) {
        const elapsed = rate.elapsedSeconds();
        this.log('ok', `${opts.kind} update complete in ${formatEta(elapsed)}.`);
        msgEl.innerHTML = `<span class="fw-ok">✅ Update complete. The device will reboot.</span>`;
      } else {
        this.log('error', `${opts.kind} update failed (see prior log lines).`);
        msgEl.innerHTML = `<span class="fw-error">❌ Update failed. See log for details.</span>`;
      }
    } catch (err) {
      statsEl.textContent = '';
      const m = err instanceof Error ? err.message : String(err);
      this.log('error', `${opts.kind} update error: ${m}`);
      msgEl.innerHTML = `<span class="fw-error">❌ ${escapeHtml(m)}</span>`;
    } finally {
      this.busy = false;
      const again = document.createElement('button');
      again.className = 'secondary';
      again.textContent = 'Back to releases';
      again.addEventListener('click', () => this.renderReleases());
      list.appendChild(again);
    }
  }
}

/**
 * Render release notes, stripping the machine-readable delsol-meta block so it
 * doesn't show up in the human-facing notes.
 */
function renderNotes(body: string): string {
  const cleaned = body.replace(/<!--\s*delsol-meta[\s\S]*?-->/g, '').trim();
  if (!cleaned) return '';
  return `<details class="fw-notes"><summary>Release notes</summary><pre>${escapeHtml(cleaned)}</pre></details>`;
}

/**
 * Smoothed transfer-rate estimator for ETA. BLE chunk acks arrive unevenly, so
 * an instantaneous rate jumps around; this blends each sample into an
 * exponential moving average for a steady speed/ETA readout.
 */
class RateEstimator {
  private startMs = Date.now();
  private lastMs = this.startMs;
  private lastBytes = 0;
  private emaBps = 0;
  private started = false;

  /** Feed the cumulative bytes-sent so far. */
  update(bytesSent: number): void {
    const now = Date.now();
    if (!this.started) {
      // First sample establishes the baseline (don't count pre-stream time).
      this.started = true;
      this.startMs = now;
      this.lastMs = now;
      this.lastBytes = bytesSent;
      return;
    }
    const dtSec = (now - this.lastMs) / 1000;
    const dBytes = bytesSent - this.lastBytes;
    if (dtSec > 0 && dBytes >= 0) {
      const sampleBps = dBytes / dtSec;
      // EMA: weight new samples ~30%. Seed with the first real sample.
      this.emaBps = this.emaBps === 0 ? sampleBps : 0.7 * this.emaBps + 0.3 * sampleBps;
      this.lastMs = now;
      this.lastBytes = bytesSent;
    }
  }

  bytesPerSecond(): number {
    return this.emaBps;
  }

  elapsedSeconds(): number {
    return (Date.now() - this.startMs) / 1000;
  }
}

function formatRate(bps: number): string {
  if (bps >= 1024 * 1024) return `${(bps / (1024 * 1024)).toFixed(1)} MB/s`;
  if (bps >= 1024) return `${(bps / 1024).toFixed(1)} KB/s`;
  return `${Math.round(bps)} B/s`;
}

/** Human ETA: "<1s", "12s", "1m 05s", "3m". */
function formatEta(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds < 0) return '—';
  const s = Math.round(seconds);
  if (s < 1) return '<1s';
  if (s < 60) return `${s}s`;
  const m = Math.floor(s / 60);
  const rem = s % 60;
  return rem === 0 ? `${m}m` : `${m}m ${String(rem).padStart(2, '0')}s`;
}

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

function formatDate(iso: string): string {
  const d = new Date(iso);
  return Number.isNaN(d.getTime()) ? iso : d.toLocaleDateString();
}

/** Render a GitHub link only if the URL is a real https://github.com URL. */
function githubLink(url: string): string {
  try {
    const u = new URL(url);
    if (u.protocol === 'https:' && u.hostname === 'github.com') {
      return `<a class="fw-link" href="${escapeHtml(u.href)}" target="_blank" rel="noopener noreferrer">View on GitHub ↗</a>`;
    }
  } catch {
    // fall through
  }
  return '';
}

function escapeHtml(s: string): string {
  return s
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}
