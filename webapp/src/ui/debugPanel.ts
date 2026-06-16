// Debug / crash-dump panel, mirroring the MAUI DebugPage.
//  - Check for crash dumps (status read)
//  - Download a crash dump (chunked reads) → saved as a browser file download
//  - Clear the crash dump
//  - Control commands: Reboot, Print, Crash Now (ASSERT), Crash Later (ASSERT_LATER)
//
// The native app stored dumps in app storage with a list/share/delete UI; in the
// browser sandbox the web-native equivalent is to trigger a file download, so we
// do that instead of maintaining an in-app file list.

import type { DebugCommand, IConnection } from '../ble/iconnection';

export class DebugPanel {
  readonly el: HTMLElement;
  private conn: IConnection | null = null;
  private firmwareVersion = 'unknown';
  private busy = false;

  constructor() {
    this.el = document.createElement('div');
    this.el.className = 'card';
    this.render();
  }

  setConnection(conn: IConnection | null): void {
    this.conn = conn;
    if (!conn) {
      this.busy = false;
      this.render();
    }
  }

  /** StatusPage passes the device's firmware version for the dump filename. */
  setFirmwareVersion(version: string): void {
    this.firmwareVersion = version || 'unknown';
  }

  private render(): void {
    this.el.innerHTML = `
      <h2>Debug</h2>
      <p class="muted-text">Crash dumps and device control.</p>

      <button id="dbg-check" class="secondary">Check for crash dumps</button>
      <div id="dbg-status"></div>

      <div class="dbg-controls">
        <div class="dbg-controls-title">Device control</div>
        <div class="dbg-btn-grid">
          <button class="secondary" data-cmd="REBOOT">Reboot Device</button>
          <button class="secondary" data-cmd="PRINT">Print Test Log</button>
          <button class="dbg-danger" data-cmd="ASSERT">Crash Now</button>
          <button class="dbg-danger" data-cmd="ASSERT_LATER">Crash Later</button>
        </div>
      </div>
    `;

    (this.el.querySelector('#dbg-check') as HTMLButtonElement).addEventListener(
      'click',
      () => void this.checkStatus(),
    );
    for (const btn of this.el.querySelectorAll<HTMLButtonElement>('[data-cmd]')) {
      btn.addEventListener('click', () =>
        void this.runCommand(btn.dataset.cmd as DebugCommand),
      );
    }
  }

  private async checkStatus(): Promise<void> {
    if (!this.conn) return;
    const btn = this.el.querySelector('#dbg-check') as HTMLButtonElement;
    const out = this.el.querySelector('#dbg-status') as HTMLElement;
    btn.disabled = true;
    btn.innerHTML = `<span class="spinner"></span> Checking…`;
    out.innerHTML = '';
    try {
      const status = await this.conn.checkCrashDump();
      if (!status.hasCrash) {
        out.innerHTML = `<p class="dbg-nocrash">✅ No crash dump on the device.</p>`;
        return;
      }
      out.innerHTML = `
        <div class="dbg-crash">
          <div class="dbg-crash-msg">⚠️ Crash dump present — ${formatBytes(status.sizeBytes)} in ${status.chunkCount} chunks.</div>
          <div class="dbg-crash-actions">
            <button id="dbg-download">Download dump</button>
            <button id="dbg-clear" class="secondary">Clear dump</button>
          </div>
          <div id="dbg-progress"></div>
        </div>`;
      (out.querySelector('#dbg-download') as HTMLButtonElement).addEventListener(
        'click',
        () => void this.downloadDump(status.chunkCount),
      );
      (out.querySelector('#dbg-clear') as HTMLButtonElement).addEventListener(
        'click',
        () => void this.clearDump(),
      );
    } catch (err) {
      out.innerHTML = `<p class="fw-error">${escapeHtml(errMsg(err))}</p>`;
    } finally {
      btn.disabled = false;
      btn.textContent = 'Check for crash dumps';
    }
  }

  private async downloadDump(chunkCount: number): Promise<void> {
    if (!this.conn || this.busy) return;
    this.busy = true;
    const prog = this.el.querySelector('#dbg-progress') as HTMLElement;
    prog.innerHTML = `
      <div class="fw-progress-wrap">
        <div class="fw-bar"><div class="fw-bar-fill" id="dbg-fill" style="width:0%"></div></div>
        <div class="fw-pct" id="dbg-pct">0%</div>
      </div>`;
    const fill = prog.querySelector('#dbg-fill') as HTMLElement;
    const pct = prog.querySelector('#dbg-pct') as HTMLElement;
    try {
      const data = await this.conn.downloadCrashDump(chunkCount, (p) => {
        fill.style.width = `${p}%`;
        pct.textContent = `${p}%`;
      });
      saveFile(data, dumpFileName(this.firmwareVersion));
      prog.innerHTML = `<p class="dbg-nocrash">✅ Downloaded ${formatBytes(data.length)}. Check your downloads.</p>`;
    } catch (err) {
      prog.innerHTML = `<p class="fw-error">${escapeHtml(errMsg(err))}</p>`;
    } finally {
      this.busy = false;
    }
  }

  private async clearDump(): Promise<void> {
    if (!this.conn) return;
    if (!confirm('Clear the crash dump from the device?')) return;
    try {
      await this.conn.sendDebugCommand('CLEAR');
      await this.checkStatus();
    } catch (err) {
      alert(errMsg(err));
    }
  }

  private async runCommand(cmd: DebugCommand): Promise<void> {
    if (!this.conn) return;

    const confirms: Partial<Record<DebugCommand, string>> = {
      REBOOT: 'Reboot the device? This will drop the BLE connection.',
      ASSERT: 'Force the device to crash now? It will reset.',
      ASSERT_LATER: 'Arm a delayed crash (ASSERT_LATER) on the device?',
    };
    const prompt = confirms[cmd];
    if (prompt && !confirm(prompt)) return;

    try {
      await this.conn.sendDebugCommand(cmd);
    } catch (err) {
      alert(errMsg(err));
    }
  }
}

function dumpFileName(fw: string): string {
  // Mirror the MAUI convention: crash_v<fw>_<YYYYMMDD_HHMMSS>.dump
  const now = new Date();
  const p = (n: number) => String(n).padStart(2, '0');
  const ts =
    `${now.getFullYear()}${p(now.getMonth() + 1)}${p(now.getDate())}` +
    `_${p(now.getHours())}${p(now.getMinutes())}${p(now.getSeconds())}`;
  const safeFw = fw.replace(/[^\w.-]/g, '');
  return `crash_v${safeFw}_${ts}.dump`;
}

function saveFile(data: Uint8Array, filename: string): void {
  // Copy into a fresh ArrayBuffer-backed view so it satisfies BlobPart.
  const bytes = new Uint8Array(data);
  const blob = new Blob([bytes], { type: 'application/octet-stream' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

function errMsg(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}

function escapeHtml(s: string): string {
  return s
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}
