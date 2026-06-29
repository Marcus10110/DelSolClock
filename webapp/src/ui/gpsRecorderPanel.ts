// GPS recorder panel — start/stop an on-device GPS recording and download it for
// off-device analysis (drift, signal outages, parse failures).
//
//  - Start / Stop recording (one record per NMEA cycle, fix or not).
//  - Live status (recording?, record count, ~minutes, dropped).
//  - Download → decodes the DSGPS blob and offers .csv + .geojson (+ raw .bin)
//    via browser file downloads.
//
// Older firmware without the recorder characteristics is detected and the
// controls are disabled with a clear "update firmware" message (like bezelPanel).

import type { GpsRecStatus, IConnection } from '../ble/iconnection';
import {
  decodeGpsTrace,
  gpsTraceToCsv,
  gpsTraceToGeoJSON,
} from '../navigation/gpsTrace';

export class GpsRecorderPanel {
  readonly el: HTMLElement;
  private conn: IConnection | null = null;
  private firmwareVersion = 'unknown';
  private busy = false;
  private unsupported = false;
  private statusTimer: ReturnType<typeof setInterval> | null = null;

  constructor() {
    this.el = document.createElement('div');
    this.el.className = 'card';
    this.render();
  }

  setConnection(conn: IConnection | null): void {
    this.conn = conn;
    this.unsupported = false;
    this.stopPolling();
    if (conn?.isConnected) {
      void this.refreshStatus();
    } else {
      this.setEnabled(false);
      this.setStatus('Connect a device to record GPS.');
    }
  }

  setFirmwareVersion(version: string): void {
    this.firmwareVersion = version || 'unknown';
  }

  private render(): void {
    this.el.innerHTML = `
      <h2>GPS Recorder</h2>
      <p class="muted-text">
        Record a drive on the device (one sample per second, fix or not), then
        download it as CSV + GeoJSON to analyze drift and signal outages.
      </p>
      <div class="gpsrec-actions">
        <button id="gpsrec-start">Start</button>
        <button id="gpsrec-stop" class="secondary">Stop</button>
        <button id="gpsrec-download" class="secondary">Download</button>
      </div>
      <div id="gpsrec-progress"></div>
      <div id="gpsrec-status" class="muted-text"></div>
    `;

    (this.el.querySelector('#gpsrec-start') as HTMLButtonElement).addEventListener(
      'click',
      () => void this.start(),
    );
    (this.el.querySelector('#gpsrec-stop') as HTMLButtonElement).addEventListener(
      'click',
      () => void this.stop(),
    );
    (this.el.querySelector('#gpsrec-download') as HTMLButtonElement).addEventListener(
      'click',
      () => void this.download(),
    );
    this.setEnabled(false);
  }

  /** Enable/disable the action buttons. Download stays usable to pull a buffer. */
  private setEnabled(enabled: boolean): void {
    for (const btn of this.el.querySelectorAll<HTMLButtonElement>(
      '#gpsrec-start, #gpsrec-stop, #gpsrec-download',
    )) {
      btn.disabled = !enabled;
    }
  }

  private async refreshStatus(): Promise<void> {
    if (!this.conn?.isConnected) return;
    try {
      const s = await this.conn.gpsRecGetStatus();
      this.unsupported = false;
      this.setEnabled(true);
      this.showStatus(s);
      // While recording, poll so the live counts climb on screen.
      if (s.recording && !this.statusTimer) this.startPolling();
      if (!s.recording) this.stopPolling();
    } catch (err) {
      if (isCharacteristicMissing(err)) {
        this.unsupported = true;
        this.setEnabled(false);
        this.stopPolling();
        this.setStatus(
          'Not available on this firmware. Update the device (Firmware tab) to enable GPS recording.',
          true,
        );
      } else {
        console.error('GPS recorder status read failed:', err);
        this.setStatus(`Status read failed: ${errMsg(err)}`, true);
      }
    }
  }

  private showStatus(s: GpsRecStatus): void {
    const mins = (s.recordCount / 60).toFixed(1);
    const dropped = s.dropped > 0 ? ` · ${s.dropped} dropped (buffer full)` : '';
    const state = s.recording ? '🔴 Recording' : '⏹ Stopped';
    this.setStatus(
      `${state} · ${s.recordCount} samples (~${mins} min) · ${formatBytes(s.byteCount)}${dropped}`,
    );
  }

  private startPolling(): void {
    this.statusTimer = setInterval(() => void this.refreshStatus(), 2000);
  }

  private stopPolling(): void {
    if (this.statusTimer !== null) {
      clearInterval(this.statusTimer);
      this.statusTimer = null;
    }
  }

  private async start(): Promise<void> {
    if (!this.conn?.isConnected || this.unsupported) return;
    try {
      await this.conn.gpsRecStart();
      await this.refreshStatus();
      this.startPolling();
    } catch (err) {
      this.setStatus(`Start failed: ${errMsg(err)}`, true);
    }
  }

  private async stop(): Promise<void> {
    if (!this.conn?.isConnected || this.unsupported) return;
    try {
      await this.conn.gpsRecStop();
      this.stopPolling();
      await this.refreshStatus();
    } catch (err) {
      this.setStatus(`Stop failed: ${errMsg(err)}`, true);
    }
  }

  private async download(): Promise<void> {
    if (!this.conn?.isConnected || this.unsupported || this.busy) return;
    this.busy = true;
    this.stopPolling();
    const prog = this.el.querySelector('#gpsrec-progress') as HTMLElement;
    prog.innerHTML = `
      <div class="fw-progress-wrap">
        <div class="fw-bar"><div class="fw-bar-fill" id="gpsrec-fill" style="width:0%"></div></div>
        <div class="fw-pct" id="gpsrec-pct">0%</div>
      </div>`;
    const fill = prog.querySelector('#gpsrec-fill') as HTMLElement;
    const pct = prog.querySelector('#gpsrec-pct') as HTMLElement;
    try {
      // Arming the download stops recording on the device.
      const raw = await this.conn.downloadGpsRecording((p) => {
        fill.style.width = `${p}%`;
        pct.textContent = `${p}%`;
      });
      if (raw.length === 0) {
        prog.innerHTML = `<p class="fw-error">No GPS data recorded yet.</p>`;
        return;
      }
      const trace = decodeGpsTrace(raw);
      const base = fileBaseName(this.firmwareVersion);
      saveFile(raw, `${base}.bin`, 'application/octet-stream');
      saveFile(gpsTraceToCsv(trace), `${base}.csv`, 'text/csv');
      saveFile(gpsTraceToGeoJSON(trace), `${base}.geojson`, 'application/geo+json');

      const fixes = trace.samples.filter((s) => s.hasFix).length;
      const noFix = trace.samples.length - fixes;
      prog.innerHTML = `<p class="dbg-nocrash">✅ Downloaded ${trace.samples.length} samples (${fixes} with fix, ${noFix} no-fix). Saved .csv, .geojson, .bin.</p>`;
    } catch (err) {
      prog.innerHTML = `<p class="fw-error">${escapeHtml(errMsg(err))}</p>`;
    } finally {
      this.busy = false;
      await this.refreshStatus();
    }
  }

  private setStatus(msg: string, isError = false): void {
    const el = this.el.querySelector('#gpsrec-status') as HTMLElement;
    if (!el) return;
    el.textContent = msg;
    el.classList.toggle('fw-error', isError);
  }
}

function fileBaseName(fw: string): string {
  const now = new Date();
  const p = (n: number) => String(n).padStart(2, '0');
  const ts =
    `${now.getFullYear()}${p(now.getMonth() + 1)}${p(now.getDate())}` +
    `_${p(now.getHours())}${p(now.getMinutes())}${p(now.getSeconds())}`;
  const safeFw = fw.replace(/[^\w.-]/g, '');
  return `gps_v${safeFw}_${ts}`;
}

function saveFile(data: Uint8Array | string, filename: string, mime: string): void {
  const part: BlobPart = typeof data === 'string' ? data : new Uint8Array(data);
  const blob = new Blob([part], { type: mime });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}

// A missing GATT characteristic (older firmware) throws NotFoundError. Match
// broadly so we degrade gracefully across browsers/Bluefy.
function isCharacteristicMissing(err: unknown): boolean {
  if (err instanceof Error) {
    if (err.name === 'NotFoundError') return true;
    return /characteristic|not found|no characteristic/i.test(err.message);
  }
  return false;
}

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

function errMsg(err: unknown): string {
  // Surface as much as we can — Web Bluetooth GATT failures are often a
  // DOMException whose `.message` is empty/terse, with the useful bit in
  // `.name`. A bare value (e.g. a numeric ATT error code) also needs help.
  if (err instanceof Error) {
    const name = err.name && err.name !== 'Error' ? `${err.name}: ` : '';
    return `${name}${err.message || '(no message)'}`;
  }
  if (err === null || err === undefined) return String(err);
  if (typeof err === 'object') {
    try {
      return JSON.stringify(err);
    } catch {
      return Object.prototype.toString.call(err);
    }
  }
  return String(err);
}

function escapeHtml(s: string): string {
  return s
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}
