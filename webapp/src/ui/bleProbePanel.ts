// BLE Probe panel — a hands-on diagnostic for the Web Bluetooth reconnect issue
// on iOS/Bluefy. There's no JS console on the phone, so every probe writes its
// result to an on-screen log. Each button exercises ONE piece of the Web
// Bluetooth API directly (independent of the app's connection state machine) so
// we can see exactly what works and what doesn't after the browser is reopened
// while the device is still OS-connected.
//
// Typical investigation: connect with Bluefy, close it, reopen, then run these
// probes in order to find a working reconnect path.

import { ADVERTISED_NAME, ALL_SERVICES, SVC_VEHICLE } from '../ble/constants';

// navigator.bluetooth typings vary by browser; treat optional bits loosely.
type AnyBt = {
  getDevices?: () => Promise<BluetoothDevice[]>;
  getAvailability?: () => Promise<boolean>;
  requestDevice: (opts: unknown) => Promise<BluetoothDevice>;
};

export class BleProbePanel {
  readonly el: HTMLElement;

  // A device handle retained across probes within this page session so we can
  // try gatt.connect() on it directly (the most promising reconnect path).
  private retained: BluetoothDevice | null = null;
  private logEl!: HTMLElement;

  constructor() {
    this.el = document.createElement('div');
    this.el.className = 'card';
    this.render();
  }

  private render(): void {
    this.el.innerHTML = `
      <h2>BLE Probe</h2>
      <p class="muted-text">
        Reconnect diagnostics. Run these from the phone — results print below.
        Order to try after reopening the browser: Capabilities → getAvailability →
        getDevices → (retained) GATT connect.
      </p>
      <div class="dbg-btn-grid">
        <button class="secondary" data-probe="caps">Capabilities</button>
        <button class="secondary" data-probe="avail">getAvailability()</button>
        <button class="secondary" data-probe="getDevices">getDevices()</button>
        <button class="secondary" data-probe="request">requestDevice (picker)</button>
        <button class="secondary" data-probe="gattState">Retained GATT state</button>
        <button class="secondary" data-probe="gattConnect">Retained GATT connect</button>
        <button class="secondary" data-probe="gattDisconnect">Retained GATT disconnect</button>
        <button class="secondary" data-probe="watchAdv">watchAdvertisements (5s)</button>
        <button class="secondary" data-probe="readVehicle">Read vehicle svc</button>
      </div>
      <button id="probe-clear" class="secondary">Clear log</button>
      <div id="probe-log" class="probe-log"></div>
    `;
    this.logEl = this.el.querySelector('#probe-log') as HTMLElement;
    for (const btn of this.el.querySelectorAll<HTMLButtonElement>('[data-probe]')) {
      btn.addEventListener('click', () => void this.run(btn.dataset.probe as string, btn));
    }
    (this.el.querySelector('#probe-clear') as HTMLButtonElement).addEventListener(
      'click',
      () => {
        this.logEl.innerHTML = '';
      },
    );
  }

  private bt(): AnyBt | null {
    if (typeof navigator === 'undefined' || !('bluetooth' in navigator)) return null;
    return navigator.bluetooth as unknown as AnyBt;
  }

  private async run(probe: string, btn: HTMLButtonElement): Promise<void> {
    btn.disabled = true;
    try {
      switch (probe) {
        case 'caps': this.caps(); break;
        case 'avail': await this.avail(); break;
        case 'getDevices': await this.getDevices(); break;
        case 'request': await this.request(); break;
        case 'gattState': this.gattState(); break;
        case 'gattConnect': await this.gattConnect(); break;
        case 'gattDisconnect': await this.gattDisconnect(); break;
        case 'watchAdv': await this.watchAdv(); break;
        case 'readVehicle': await this.readVehicle(); break;
      }
    } catch (err) {
      this.log('error', errMsg(err));
    } finally {
      btn.disabled = false;
    }
  }

  // ---- Probes -------------------------------------------------------------

  private caps(): void {
    const bt = this.bt();
    this.log('info', '--- Capabilities ---');
    this.log('info', `navigator.bluetooth: ${bt ? 'present' : 'MISSING'}`);
    if (!bt) return;
    this.log(bt.getDevices ? 'ok' : 'warn', `getDevices(): ${bt.getDevices ? 'supported' : 'NOT supported'}`);
    this.log(bt.getAvailability ? 'ok' : 'warn', `getAvailability(): ${bt.getAvailability ? 'supported' : 'NOT supported'}`);
    // watchAdvertisements lives on the device instance; report it from the
    // retained handle if we have one (otherwise unknown until one is obtained).
    if (this.retained) {
      const w = typeof (this.retained as { watchAdvertisements?: unknown }).watchAdvertisements === 'function';
      this.log(w ? 'ok' : 'warn', `watchAdvertisements(): ${w ? 'supported' : 'NOT supported'}`);
    } else {
      this.log('info', 'watchAdvertisements(): unknown (get a device first)');
    }
    this.log('info', `userAgent: ${navigator.userAgent}`);
  }

  private async avail(): Promise<void> {
    const bt = this.bt();
    if (!bt?.getAvailability) {
      this.log('warn', 'getAvailability() not supported here.');
      return;
    }
    const ok = await bt.getAvailability();
    this.log(ok ? 'ok' : 'warn', `getAvailability() => ${ok}`);
  }

  private async getDevices(): Promise<void> {
    const bt = this.bt();
    if (!bt?.getDevices) {
      this.log('warn', 'getDevices() not supported (expected on Bluefy/iOS).');
      return;
    }
    const devices = await bt.getDevices();
    this.log('info', `getDevices() => ${devices.length} device(s):`);
    for (const d of devices) {
      this.log('info', `  • name="${d.name ?? '(none)'}" id=${d.id} gatt.connected=${d.gatt?.connected ?? '?'}`);
    }
    // Retain the first Del Sol match so the GATT probes can use it.
    const match = devices.find((d) => d.name === ADVERTISED_NAME) ?? devices[0];
    if (match) {
      this.retained = match;
      this.log('ok', `Retained handle: ${match.name ?? match.id}`);
    }
  }

  private async request(): Promise<void> {
    const bt = this.bt();
    if (!bt) {
      this.log('error', 'navigator.bluetooth missing.');
      return;
    }
    this.log('info', 'Opening picker (filter name="Del Sol")…');
    const device = await bt.requestDevice({
      filters: [{ name: ADVERTISED_NAME }],
      optionalServices: ALL_SERVICES,
    });
    this.retained = device;
    this.log('ok', `Picked + retained: name="${device.name ?? '(none)'}" id=${device.id} gatt.connected=${device.gatt?.connected ?? '?'}`);
  }

  private gattState(): void {
    if (!this.retained) {
      this.log('warn', 'No retained device. Run getDevices() or requestDevice first.');
      return;
    }
    const d = this.retained;
    this.log('info', `Retained: name="${d.name ?? '(none)'}" id=${d.id}`);
    this.log('info', `  gatt present: ${d.gatt ? 'yes' : 'no'}`);
    this.log('info', `  gatt.connected: ${d.gatt?.connected ?? '?'}`);
  }

  private async gattConnect(): Promise<void> {
    if (!this.retained?.gatt) {
      this.log('warn', 'No retained device with a GATT server. Run getDevices()/requestDevice first.');
      return;
    }
    this.log('info', `gatt.connect() on "${this.retained.name ?? this.retained.id}"…`);
    const t0 = performance.now();
    const server = await this.retained.gatt.connect();
    const ms = Math.round(performance.now() - t0);
    this.log('ok', `gatt.connect() OK in ${ms}ms. connected=${server.connected}`);
  }

  private async gattDisconnect(): Promise<void> {
    if (!this.retained?.gatt) {
      this.log('warn', 'No retained device.');
      return;
    }
    this.retained.gatt.disconnect();
    this.log('ok', `gatt.disconnect() called. connected=${this.retained.gatt.connected}`);
  }

  private async watchAdv(): Promise<void> {
    if (!this.retained) {
      this.log('warn', 'No retained device. Run getDevices()/requestDevice first.');
      return;
    }
    const dev = this.retained as BluetoothDevice & {
      watchAdvertisements?: (opts: { signal: AbortSignal }) => Promise<void>;
    };
    if (typeof dev.watchAdvertisements !== 'function') {
      this.log('warn', 'watchAdvertisements() not supported here.');
      return;
    }
    this.log('info', 'Watching for advertisements for 5s…');
    const controller = new AbortController();
    let seen = 0;
    const onAdv = () => {
      seen++;
      this.log('ok', `advertisementreceived (#${seen})`);
    };
    dev.addEventListener('advertisementreceived', onAdv);
    try {
      await dev.watchAdvertisements({ signal: controller.signal });
      await new Promise((r) => setTimeout(r, 5000));
    } finally {
      controller.abort();
      dev.removeEventListener('advertisementreceived', onAdv);
      this.log(seen ? 'ok' : 'warn', `Done. ${seen} advertisement(s) seen (0 = not advertising / not in range).`);
    }
  }

  private async readVehicle(): Promise<void> {
    if (!this.retained?.gatt) {
      this.log('warn', 'No retained device. Connect first.');
      return;
    }
    if (!this.retained.gatt.connected) {
      this.log('warn', 'GATT not connected. Run "Retained GATT connect" first.');
      return;
    }
    this.log('info', 'Reading vehicle service…');
    const svc = await this.retained.gatt.getPrimaryService(SVC_VEHICLE);
    this.log('ok', `Got vehicle service ${svc.uuid}`);
  }

  // ---- Logging ------------------------------------------------------------

  private log(level: 'info' | 'ok' | 'warn' | 'error', msg: string): void {
    const time = new Date().toLocaleTimeString();
    const row = document.createElement('div');
    row.className = `probe-line probe-${level}`;
    row.textContent = `${time}  ${msg}`;
    this.logEl.appendChild(row);
    this.logEl.scrollTop = this.logEl.scrollHeight;
  }
}

function errMsg(err: unknown): string {
  if (err instanceof Error) return `${err.name}: ${err.message}`;
  return String(err);
}
