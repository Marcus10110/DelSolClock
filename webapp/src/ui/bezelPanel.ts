// Bezel offset panel — tune the display's per-side insets (the physical bezel
// hides a few pixels around each edge). Four steppers (Top/Bottom/Left/Right)
// with −/value/+ and a Reset. Every change writes to the device immediately,
// which applies it live AND persists it in NVS.

import type { BezelOffsets, IConnection } from '../ble/iconnection';

type Side = keyof BezelOffsets;
const SIDES: Side[] = ['top', 'bottom', 'left', 'right'];
const LABELS: Record<Side, string> = {
  top: 'Top',
  bottom: 'Bottom',
  left: 'Left',
  right: 'Right',
};
const MAX = 40; // matches the firmware clamp

export class BezelPanel {
  readonly el: HTMLElement;
  private conn: IConnection | null = null;
  private offsets: BezelOffsets = { top: 0, bottom: 0, left: 0, right: 0 };
  private busy = false;
  // The bezel characteristic was added in a later firmware. Older builds (the
  // ones you flash FROM) don't have it; detect that and disable the controls
  // with a clear message instead of showing read/write errors.
  private unsupported = false;

  constructor() {
    this.el = document.createElement('div');
    this.el.className = 'card';
    this.render();
  }

  setConnection(conn: IConnection | null): void {
    this.conn = conn;
    this.unsupported = false;
    if (conn?.isConnected) {
      void this.loadFromDevice();
    } else {
      this.offsets = { top: 0, bottom: 0, left: 0, right: 0 };
      this.updateValues();
      this.setEnabled(true);
      this.setStatus('');
    }
  }

  private async loadFromDevice(): Promise<void> {
    if (!this.conn) return;
    try {
      this.offsets = await this.conn.readBezelOffsets();
      this.unsupported = false;
      this.setEnabled(true);
      this.updateValues();
      this.setStatus('Loaded from device.');
    } catch (err) {
      if (isCharacteristicMissing(err)) {
        // Old firmware without the bezel characteristic — the device you flash
        // FROM. Disable the controls and point to the firmware update.
        this.unsupported = true;
        this.setEnabled(false);
        this.setStatus(
          'Not available on this firmware. Update the device (Firmware tab) to enable bezel tuning.',
          true,
        );
      } else {
        this.setStatus(`Read failed: ${errMsg(err)}`, true);
      }
    }
  }

  /** Enable/disable the steppers + reset (greyed out when unsupported). */
  private setEnabled(enabled: boolean): void {
    for (const btn of this.el.querySelectorAll<HTMLButtonElement>(
      '.bezel-step, #bezel-reset',
    )) {
      btn.disabled = !enabled;
    }
  }

  private render(): void {
    this.el.innerHTML = `
      <h2>Display Bezel</h2>
      <p class="muted-text">
        Trim the edges hidden by the bezel. Changes apply live and are saved on
        the device.
      </p>
      <div id="bezel-rows"></div>
      <div class="bezel-actions">
        <button id="bezel-reset" class="secondary">Reset to 0</button>
      </div>
      <div id="bezel-status" class="muted-text"></div>
    `;

    const rows = this.el.querySelector('#bezel-rows') as HTMLElement;
    for (const side of SIDES) {
      const row = document.createElement('div');
      row.className = 'bezel-row';
      row.innerHTML = `
        <span class="bezel-label">${LABELS[side]}</span>
        <button class="secondary bezel-step" data-side="${side}" data-delta="-1">−</button>
        <span class="bezel-value" data-value="${side}">0</span>
        <button class="secondary bezel-step" data-side="${side}" data-delta="1">+</button>
      `;
      rows.appendChild(row);
    }

    for (const btn of this.el.querySelectorAll<HTMLButtonElement>('.bezel-step')) {
      btn.addEventListener('click', () =>
        void this.bump(btn.dataset.side as Side, Number(btn.dataset.delta)),
      );
    }
    (this.el.querySelector('#bezel-reset') as HTMLButtonElement).addEventListener(
      'click',
      () => void this.reset(),
    );
    this.updateValues();
  }

  private updateValues(): void {
    for (const side of SIDES) {
      const span = this.el.querySelector(`[data-value="${side}"]`) as HTMLElement;
      if (span) span.textContent = String(this.offsets[side]);
    }
  }

  private async bump(side: Side, delta: number): Promise<void> {
    const next = clamp(this.offsets[side] + delta);
    if (next === this.offsets[side]) return;
    this.offsets = { ...this.offsets, [side]: next };
    this.updateValues();
    await this.push();
  }

  private async reset(): Promise<void> {
    this.offsets = { top: 0, bottom: 0, left: 0, right: 0 };
    this.updateValues();
    await this.push();
  }

  private async push(): Promise<void> {
    if (this.unsupported) return;  // controls are disabled; nothing to write
    if (!this.conn?.isConnected) {
      this.setStatus('Connect a device to apply.', true);
      return;
    }
    if (this.busy) return;
    this.busy = true;
    try {
      await this.conn.writeBezelOffsets(this.offsets);
      this.setStatus('Saved.');
    } catch (err) {
      if (isCharacteristicMissing(err)) {
        this.unsupported = true;
        this.setEnabled(false);
        this.setStatus(
          'Not available on this firmware. Update the device to enable bezel tuning.',
          true,
        );
      } else {
        this.setStatus(`Write failed: ${errMsg(err)}`, true);
      }
    } finally {
      this.busy = false;
    }
  }

  private setStatus(msg: string, isError = false): void {
    const el = this.el.querySelector('#bezel-status') as HTMLElement;
    if (!el) return;
    el.textContent = msg;
    el.classList.toggle('fw-error', isError);
  }
}

// A missing GATT characteristic (older firmware) throws a NotFoundError
// DOMException; its message also mentions "No Characteristic". Match broadly so
// we degrade gracefully across browsers/Bluefy.
function isCharacteristicMissing(err: unknown): boolean {
  if (err instanceof Error) {
    if (err.name === 'NotFoundError') return true;
    return /characteristic|not found|no characteristic/i.test(err.message);
  }
  return false;
}

function clamp(v: number): number {
  return Math.max(0, Math.min(MAX, v));
}

function errMsg(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}
