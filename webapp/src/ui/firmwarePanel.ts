// Firmware update panel: browse GitHub releases and flash one over BLE.
// Mirrors the MAUI UpdatePage. Owns its own DOM; the StatusPage mounts it and
// hands it the active IConnection (real or demo) when connected.

import type { IConnection } from '../ble/iconnection';
import {
  downloadFirmware,
  fetchReleases,
  type Release,
} from '../firmware/firmwareBrowser';

export class FirmwarePanel {
  readonly el: HTMLElement;
  private conn: IConnection | null = null;
  private releases: Release[] = [];
  private busy = false;

  constructor() {
    this.el = document.createElement('div');
    this.el.className = 'card';
    this.renderIdle();
  }

  /** Called by StatusPage whenever the connection changes (or clears). */
  setConnection(conn: IConnection | null): void {
    this.conn = conn;
    if (!conn) {
      this.releases = [];
      this.busy = false;
      this.renderIdle();
    }
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

    list.innerHTML = this.releases
      .map((r, i) => {
        const size = r.binaryAsset ? formatBytes(r.binaryAsset.size) : '';
        const date = formatDate(r.publishedAt);
        const notes = r.body.trim()
          ? `<details class="fw-notes"><summary>Release notes</summary><pre>${escapeHtml(r.body.trim())}</pre></details>`
          : '';
        return `
          <div class="fw-release" data-index="${i}">
            <div class="fw-release-head">
              <div>
                <div class="fw-release-name">${escapeHtml(r.name)}</div>
                <div class="muted-text fw-release-meta">${escapeHtml(r.tagName)} · ${date} · ${size}</div>
              </div>
              <button class="fw-update" data-index="${i}">Update</button>
            </div>
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
  }

  private async runUpdate(release: Release): Promise<void> {
    if (this.busy) return;
    if (!this.conn || !this.conn.isConnected) {
      alert('Connect to the device before updating firmware.');
      return;
    }
    if (!release.binaryAsset) return;

    const ok = confirm(
      `Flash "${release.name}" (${formatBytes(release.binaryAsset.size)}) to the device?\n\n` +
        `Do not close this page or move out of range during the update.`,
    );
    if (!ok) return;

    this.busy = true;
    const list = this.el.querySelector('#fw-list') as HTMLElement;
    list.innerHTML = `
      <div class="fw-progress-wrap">
        <div class="fw-progress-msg" id="fw-msg">Starting…</div>
        <div class="fw-bar"><div class="fw-bar-fill" id="fw-fill" style="width:0%"></div></div>
        <div class="fw-pct" id="fw-pct">0%</div>
      </div>`;

    const msgEl = this.el.querySelector('#fw-msg') as HTMLElement;
    const fillEl = this.el.querySelector('#fw-fill') as HTMLElement;
    const pctEl = this.el.querySelector('#fw-pct') as HTMLElement;

    try {
      msgEl.textContent = 'Downloading firmware…';
      const data = await downloadFirmware(release.binaryAsset);

      const success = await this.conn.updateFirmware(data, (p) => {
        fillEl.style.width = `${p.percent}%`;
        pctEl.textContent = `${p.percent}%`;
        msgEl.textContent = p.message;
      });

      if (success) {
        msgEl.innerHTML = `<span class="fw-ok">✅ Update complete. The device will reboot.</span>`;
      } else {
        msgEl.innerHTML = `<span class="fw-error">❌ Update failed. See log for details.</span>`;
      }
    } catch (err) {
      const m = err instanceof Error ? err.message : String(err);
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
