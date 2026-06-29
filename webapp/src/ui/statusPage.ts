// Phase 1 Status page. Applies the misc_todo.md refinements:
//  - vehicle values default to "--" (not misleading real-looking defaults)
//  - labels "Rear Window" and "Head Lights"
//  - a proper "connecting" state: details hidden until truly connected
//  - immediate UI updates on every event (no stale values / stale spinner)

import { DelSolConnection } from '../ble/connection';
import { DemoConnection } from '../ble/demoConnection';
import type { IConnection } from '../ble/iconnection';
import type {
  BatteryStatus,
  ConnectionState,
  VehicleStatus,
} from '../ble/types';
import { FirmwarePanel } from './firmwarePanel';
import { warmupApi } from '../firmware/firmwareBrowser';
import { DebugPanel } from './debugPanel';
import { NavigationPanel } from './navigationPanel';
import { BleProbePanel } from './bleProbePanel';
import { BezelPanel } from './bezelPanel';
import { GpsRecorderPanel } from './gpsRecorderPanel';

const DASH = '--';

export class StatusPage {
  readonly el: HTMLElement;
  private conn: IConnection | null = null;
  private unsubscribers: Array<() => void> = [];
  private readonly firmwarePanel = new FirmwarePanel();
  private readonly debugPanel = new DebugPanel();
  private readonly navigationPanel = new NavigationPanel();
  private readonly bleProbePanel = new BleProbePanel();
  private readonly bezelPanel = new BezelPanel();
  private readonly gpsRecorderPanel = new GpsRecorderPanel();

  // A previously-granted device we can reconnect to without the picker.
  private knownDevice: BluetoothDevice | null = null;

  // cached element refs
  private reconnectBtn!: HTMLButtonElement;
  private connectBtn!: HTMLButtonElement;
  private demoBtn!: HTMLButtonElement;
  private busyBtn!: HTMLButtonElement;
  private disconnectBtn!: HTMLButtonElement;
  private stateDot!: HTMLElement;
  private stateText!: HTMLElement;
  private deviceRow!: HTMLElement;
  private deviceVal!: HTMLElement;
  private fwRow!: HTMLElement;
  private fwVal!: HTMLElement;
  private statusCard!: HTMLElement;
  private logEl!: HTMLElement;
  private flagEls: Record<string, HTMLElement> = {};
  private battVal!: HTMLElement;
  private timeVal!: HTMLElement;
  private navMount!: HTMLElement;
  private fwMount!: HTMLElement;
  private dbgMount!: HTMLElement;
  private probeMount!: HTMLElement;
  private bezelMount!: HTMLElement;
  private gpsRecMount!: HTMLElement;
  private connBar!: HTMLButtonElement;
  private connPanel!: HTMLElement;
  private connChevron!: HTMLElement;
  private connBarDevice!: HTMLElement;
  private statusDisconnected!: HTMLElement;
  private debugBadge!: HTMLElement;
  private toastEl!: HTMLElement;
  private activeTab = 'nav';
  private toastTimer: ReturnType<typeof setTimeout> | null = null;

  constructor() {
    this.el = document.createElement('div');
    this.el.innerHTML = this.template();
    this.cacheRefs();
    this.navMount.appendChild(this.navigationPanel.el);
    this.fwMount.appendChild(this.firmwarePanel.el);
    this.dbgMount.appendChild(this.debugPanel.el);
    this.probeMount.appendChild(this.bleProbePanel.el);
    this.bezelMount.appendChild(this.bezelPanel.el);
    this.gpsRecMount.appendChild(this.gpsRecorderPanel.el);
    this.wireEvents();
    this.checkSupport();
    this.render('disconnected');
    void this.loadKnownDevice();
  }

  /** On load, surface a "Reconnect" button if a device was previously granted. */
  private async loadKnownDevice(): Promise<void> {
    const devices = await DelSolConnection.getKnownDevices();
    if (devices.length === 0) return;
    this.knownDevice = devices[0];
    const name = this.knownDevice.name ?? 'Del Sol';
    this.reconnectBtn.textContent = `Reconnect to ${name}`;
    // Only show while idle (render() manages busy/connected visibility).
    if (this.conn === null || !this.conn.isConnected) {
      this.reconnectBtn.classList.remove('hidden');
    }
  }

  private template(): string {
    return `
      <!-- Persistent connection bar: tap to expand controls + details. -->
      <button class="conn-bar" id="conn-bar" aria-expanded="false">
        <span class="conn-bar-main">
          <span class="dot" id="state-dot"></span>
          <span class="conn-bar-text" id="state-text">Disconnected</span>
          <span class="conn-bar-device" id="conn-bar-device"></span>
        </span>
        <span class="conn-bar-chevron" id="conn-chevron">▾</span>
      </button>

      <div class="conn-panel hidden" id="conn-panel">
        <div id="support-banner" class="banner hidden"></div>
        <button id="reconnect" class="hidden">Reconnect</button>
        <button id="connect">Connect to “Del Sol”</button>
        <button id="demo" class="secondary">Demo mode (no device)</button>
        <button id="busy" class="hidden" disabled></button>
        <button id="disconnect" class="secondary hidden">Disconnect</button>
        <div class="row hidden" id="device-row">
          <span class="k">Device</span><span class="v" id="device-val">${DASH}</span>
        </div>
        <div class="row hidden" id="fw-row">
          <span class="k">Firmware</span><span class="v" id="fw-val">${DASH}</span>
        </div>
      </div>

      <!-- Tab content panes. Exactly one is visible at a time. -->
      <main class="tab-content">
        <section class="tab-pane" data-pane="nav">
          <div id="nav-mount"></div>
        </section>

        <section class="tab-pane hidden" data-pane="status">
          <div class="card" id="status-card">
            <h2>Vehicle Status</h2>
            <div class="row"><span class="k">Rear Window</span><span class="v muted" data-flag="rearWindowDown">${DASH}</span></div>
            <div class="row"><span class="k">Trunk</span><span class="v muted" data-flag="trunkOpen">${DASH}</span></div>
            <div class="row"><span class="k">Roof</span><span class="v muted" data-flag="convertibleRoofDown">${DASH}</span></div>
            <div class="row"><span class="k">Ignition</span><span class="v muted" data-flag="ignitionOn">${DASH}</span></div>
            <div class="row"><span class="k">Head Lights</span><span class="v muted" data-flag="headlightsOn">${DASH}</span></div>
            <div class="row"><span class="k">Battery</span><span class="v muted" id="battery-val">${DASH}</span></div>
            <div class="row"><span class="k">Last update</span><span class="v muted" id="time-val">${DASH}</span></div>
            <p class="muted-text" id="status-disconnected">Connect to a device to see vehicle status.</p>
          </div>
        </section>

        <section class="tab-pane hidden" data-pane="firmware">
          <div id="fw-mount"></div>
        </section>

        <section class="tab-pane hidden" data-pane="debug">
          <div id="dbg-mount"></div>
          <div id="gpsrec-mount"></div>
          <div id="bezel-mount"></div>
          <div id="probe-mount"></div>
          <div class="card">
            <h2>Log</h2>
            <div id="log"></div>
          </div>
        </section>
      </main>

      <!-- Bottom tab bar (thumb-reachable on phones). -->
      <nav class="tab-bar" id="tab-bar">
        <button class="tab-btn is-active" data-tab="nav">
          <span class="tab-icon">🧭</span><span class="tab-label">Navigate</span>
        </button>
        <button class="tab-btn" data-tab="status">
          <span class="tab-icon">🚗</span><span class="tab-label">Status</span>
        </button>
        <button class="tab-btn" data-tab="firmware">
          <span class="tab-icon">⬆️</span><span class="tab-label">Firmware</span>
        </button>
        <button class="tab-btn" data-tab="debug">
          <span class="tab-icon">🛠️</span><span class="tab-label">Debug</span>
          <span class="tab-badge hidden" id="debug-badge"></span>
        </button>
      </nav>

      <!-- Transient error toast (errors surface even when not on the Debug tab). -->
      <div class="toast hidden" id="toast"></div>
    `;
  }

  private cacheRefs(): void {
    const q = <T extends HTMLElement>(sel: string) => this.el.querySelector(sel) as T;
    this.reconnectBtn = q('#reconnect');
    this.connectBtn = q('#connect');
    this.demoBtn = q('#demo');
    this.busyBtn = q('#busy');
    this.disconnectBtn = q('#disconnect');
    this.stateDot = q('#state-dot');
    this.stateText = q('#state-text');
    this.deviceRow = q('#device-row');
    this.deviceVal = q('#device-val');
    this.fwRow = q('#fw-row');
    this.fwVal = q('#fw-val');
    this.statusCard = q('#status-card');
    this.battVal = q('#battery-val');
    this.timeVal = q('#time-val');
    this.navMount = q('#nav-mount');
    this.fwMount = q('#fw-mount');
    this.dbgMount = q('#dbg-mount');
    this.probeMount = q('#probe-mount');
    this.bezelMount = q('#bezel-mount');
    this.gpsRecMount = q('#gpsrec-mount');
    this.connBar = q('#conn-bar');
    this.connPanel = q('#conn-panel');
    this.connChevron = q('#conn-chevron');
    this.connBarDevice = q('#conn-bar-device');
    this.statusDisconnected = q('#status-disconnected');
    this.debugBadge = q('#debug-badge');
    this.toastEl = q('#toast');
    this.logEl = q('#log');
    for (const node of this.el.querySelectorAll<HTMLElement>('[data-flag]')) {
      this.flagEls[node.dataset.flag as string] = node;
    }
    // Route the firmware panel's diagnostics to the on-screen log (Bluefy has no
    // dev console), so update download/flash failures are visible on-device.
    this.firmwarePanel.setLogger((level, message) => this.appendLog(level, message));
  }

  private wireEvents(): void {
    this.reconnectBtn.addEventListener('click', () => void this.handleReconnect());
    this.connectBtn.addEventListener('click', () => void this.handleConnect());
    this.demoBtn.addEventListener('click', () => void this.handleDemo());
    this.disconnectBtn.addEventListener('click', () => this.conn?.disconnect());

    // Bottom tab bar: switch the visible content pane.
    for (const btn of this.el.querySelectorAll<HTMLButtonElement>('.tab-btn')) {
      btn.addEventListener('click', () => this.switchTab(btn.dataset.tab as string));
    }

    // Connection bar: tap to expand/collapse the controls + details panel.
    this.connBar.addEventListener('click', () => this.toggleConnPanel());
  }

  /** Show one tab pane, hide the rest, and update the active tab-bar button. */
  private switchTab(tab: string): void {
    this.activeTab = tab;
    for (const pane of this.el.querySelectorAll<HTMLElement>('.tab-pane')) {
      pane.classList.toggle('hidden', pane.dataset.pane !== tab);
    }
    for (const btn of this.el.querySelectorAll<HTMLButtonElement>('.tab-btn')) {
      btn.classList.toggle('is-active', btn.dataset.tab === tab);
    }
    // Clear the Debug error badge once the user visits the Debug tab.
    if (tab === 'debug') this.debugBadge.classList.add('hidden');
  }

  private toggleConnPanel(force?: boolean): void {
    const open = force ?? this.connPanel.classList.contains('hidden');
    this.connPanel.classList.toggle('hidden', !open);
    this.connBar.setAttribute('aria-expanded', String(open));
    this.connChevron.textContent = open ? '▴' : '▾';
  }

  /** Briefly show an error toast (errors are visible even off the Debug tab). */
  private showToast(message: string): void {
    this.toastEl.textContent = message;
    this.toastEl.classList.remove('hidden');
    if (this.toastTimer) clearTimeout(this.toastTimer);
    this.toastTimer = setTimeout(() => this.toastEl.classList.add('hidden'), 5000);
    // Flag the Debug tab (where the full log lives) unless already viewing it.
    if (this.activeTab !== 'debug') this.debugBadge.classList.remove('hidden');
  }

  /** Subscribe the UI to a given connection (real or demo), replacing any prior one. */
  private useConnection(conn: IConnection): void {
    this.unsubscribers.forEach((off) => off());
    this.unsubscribers = [
      conn.on('state', (s) => {
        this.render(s);
        // A connection is a strong signal an update may follow — warm the API
        // again so it's ready by the time the user clicks Update.
        if (s === 'connected') warmupApi();
      }),
      conn.on('status', (s) => this.renderStatus(s)),
      conn.on('battery', (b) => this.renderBattery(b)),
      conn.on('firmwareVersion', (v) => {
        this.fwVal.textContent = v || DASH;
        this.debugPanel.setFirmwareVersion(v);
        this.gpsRecorderPanel.setFirmwareVersion(v);
      }),
      conn.on('firmwareInfo', (info) => {
        this.firmwarePanel.setDeviceInfo(info);
      }),
      conn.on('log', ({ level, message }) => this.appendLog(level, message)),
      // errors are already logged via the 'log' event; swallow to avoid unhandled rejection
      conn.on('error', () => {}),
    ];
    this.conn = conn;
    this.firmwarePanel.setConnection(conn);
    this.debugPanel.setConnection(conn);
    this.navigationPanel.setConnection(conn);
    this.bezelPanel.setConnection(conn);
    this.gpsRecorderPanel.setConnection(conn);
  }

  private checkSupport(): void {
    if (DelSolConnection.isSupported()) return;
    const banner = this.el.querySelector('#support-banner') as HTMLElement;
    banner.classList.remove('hidden');
    banner.innerHTML =
      'Web Bluetooth isn’t available in this browser. On iPhone, open this page in ' +
      '<a href="https://apps.apple.com/us/app/bluefy-web-ble-browser/id1492822055" ' +
      'target="_blank" rel="noopener noreferrer"><strong>Bluefy</strong></a>. ' +
      'On desktop/Android use Chrome or Edge.';
    this.connectBtn.disabled = true;
  }

  private async handleConnect(): Promise<void> {
    this.useConnection(new DelSolConnection());
    try {
      await this.conn!.connect();
    } catch {
      // state already reset to 'disconnected' by the connection; nothing to do
    }
  }

  private async handleReconnect(): Promise<void> {
    const conn = new DelSolConnection();
    this.useConnection(conn);
    try {
      // Try every previously-granted handle (stale ones fail fast) via a direct
      // gatt.connect() — no picker, no advertisement wait.
      await conn.reconnectAny();
    } catch {
      // reconnect failed (device off / all handles dead). Fall back to picker.
      this.appendLog('info', 'Reconnect failed — use Connect to pick the device.');
    }
  }

  private async handleDemo(): Promise<void> {
    this.useConnection(new DemoConnection());
    await this.conn!.connect();
  }

  private render(state: ConnectionState): void {
    const labels: Record<ConnectionState, string> = {
      disconnected: 'Disconnected',
      requesting: 'Selecting device…',
      connecting: 'Connecting…',
      connected: 'Connected',
    };
    this.stateText.textContent = labels[state];
    this.stateDot.className = `dot ${state}`;

    const busy = state === 'requesting' || state === 'connecting';
    const connected = state === 'connected';

    // While busy, show a single spinner button in place of connect/demo.
    // The state line (pulsing dot + label) also reflects the connecting state.
    this.connectBtn.classList.toggle('hidden', busy || connected);
    this.connectBtn.disabled = !DelSolConnection.isSupported();
    this.demoBtn.classList.toggle('hidden', busy || connected);
    // Reconnect: only while idle and a previously-granted device is known.
    // When shown, it's the primary action; demote Connect to a secondary style.
    const showReconnect = !busy && !connected && this.knownDevice !== null;
    this.reconnectBtn.classList.toggle('hidden', !showReconnect);
    this.connectBtn.classList.toggle('secondary', showReconnect);
    this.busyBtn.classList.toggle('hidden', !busy);
    this.busyBtn.innerHTML = `<span class="spinner"></span> ${labels[state]}`;

    // Disconnect button: only when connected (no lingering spinner — disconnect is instant in our flow)
    this.disconnectBtn.classList.toggle('hidden', !connected);

    // Top connection bar: device name + dot reflect state; auto-collapse the
    // panel once connected (so the user lands on content), auto-open when idle.
    this.connBarDevice.textContent = connected ? (this.conn?.deviceName ?? '') : '';
    this.deviceRow.classList.toggle('hidden', !connected);
    this.fwRow.classList.toggle('hidden', !connected);

    // Within-pane content: the tabs are always navigable, but the device-
    // dependent content shows a hint when disconnected.
    this.statusCard.classList.toggle('disconnected', !connected);
    this.statusDisconnected.classList.toggle('hidden', connected);
    for (const node of this.el.querySelectorAll<HTMLElement>(
      '#status-card .row',
    )) {
      node.classList.toggle('hidden', !connected);
    }

    // Reflect connection state on the panels on EVERY render. This must not clear
    // the connection only in the disconnected branch: during connect, the
    // intermediate 'requesting'/'connecting' states would clear it and the final
    // 'connected' state would never restore it (the panel would be left null even
    // though we're connected). Set based on `connected` every time instead.
    this.navigationPanel.setConnection(connected ? this.conn : null);
    this.firmwarePanel.setConnection(connected ? this.conn : null);
    this.debugPanel.setConnection(connected ? this.conn : null);
    this.bezelPanel.setConnection(connected ? this.conn : null);
    this.gpsRecorderPanel.setConnection(connected ? this.conn : null);

    if (connected) {
      this.deviceVal.textContent = this.conn?.deviceName ?? DASH;
      this.toggleConnPanel(false); // collapse to reveal content
    } else {
      this.resetDetails();
      if (!busy) this.toggleConnPanel(true); // expand so Connect is visible
    }
  }

  private resetDetails(): void {
    this.deviceVal.textContent = DASH;
    this.fwVal.textContent = DASH;
    this.battVal.textContent = DASH;
    this.battVal.className = 'v muted';
    this.timeVal.textContent = DASH;
    this.timeVal.className = 'v muted';
    for (const node of Object.values(this.flagEls)) {
      node.textContent = DASH;
      node.className = 'v muted';
    }
  }

  private renderStatus(s: VehicleStatus): void {
    const set = (key: keyof VehicleStatus, on: boolean) => {
      const node = this.flagEls[key];
      if (!node) return;
      node.textContent = on ? 'Yes' : 'No';
      node.className = `v ${on ? 'on' : 'off'}`;
    };
    set('rearWindowDown', s.rearWindowDown);
    set('trunkOpen', s.trunkOpen);
    set('convertibleRoofDown', s.convertibleRoofDown);
    set('ignitionOn', s.ignitionOn);
    set('headlightsOn', s.headlightsOn);
    this.touchTime(s.lastUpdated);
  }

  private renderBattery(b: BatteryStatus): void {
    this.battVal.textContent = `${b.voltage.toFixed(2)} V`;
    this.battVal.className = 'v';
    this.touchTime(b.timestamp);
  }

  private touchTime(when: Date): void {
    this.timeVal.textContent = when.toLocaleTimeString();
    this.timeVal.className = 'v';
  }

  private appendLog(level: string, message: string): void {
    // Surface errors as a toast so they're seen even off the Debug tab.
    if (level === 'error') this.showToast(message);
    const line = document.createElement('div');
    line.className = `log-${level}`;
    line.textContent = message;
    this.logEl.appendChild(line);
    this.logEl.scrollTop = this.logEl.scrollHeight;
  }
}
