// DemoConnection — a fake IConnection for iterating on UI/UX without hardware.
// Drives realistic, changing data so we can see live updates: it walks through
// a short scripted sequence of vehicle states and drifts the battery voltage.

import { Emitter } from './emitter';
import type {
  BezelOffsets,
  ConnectionEvents,
  CrashDumpStatus,
  DebugCommand,
  FirmwareUpdateProgress,
  IConnection,
} from './iconnection';
import type { ConnectionState, VehicleStatus } from './types';
import type { UploadProgress } from '../navigation/routeUpload';
import type { RouteSummary } from '../navigation/types';

const DEMO_FW_VERSION = 'demo-1.0.0';
// A plausible-looking SPIFFS hash so the firmware panel's match UI has something
// to compare against in demo mode.
const DEMO_FS_HASH =
  '23533c94e92f95de06fc347caedebf11148a924125d6ecc47eee081e5d3793cf';

// A scripted "drive": a few states it cycles through every tick so we can watch
// each flag and timestamp update on screen.
const SCRIPT: Omit<VehicleStatus, 'lastUpdated'>[] = [
  { rearWindowDown: false, trunkOpen: false, convertibleRoofDown: false, ignitionOn: false, headlightsOn: false },
  { rearWindowDown: false, trunkOpen: false, convertibleRoofDown: false, ignitionOn: true, headlightsOn: false },
  { rearWindowDown: false, trunkOpen: false, convertibleRoofDown: false, ignitionOn: true, headlightsOn: true },
  { rearWindowDown: true, trunkOpen: false, convertibleRoofDown: true, ignitionOn: true, headlightsOn: true },
  { rearWindowDown: true, trunkOpen: true, convertibleRoofDown: true, ignitionOn: true, headlightsOn: true },
  { rearWindowDown: false, trunkOpen: false, convertibleRoofDown: false, ignitionOn: true, headlightsOn: false },
];

export class DemoConnection extends Emitter<ConnectionEvents> implements IConnection {
  private _state: ConnectionState = 'disconnected';
  private tickHandle: ReturnType<typeof setInterval> | null = null;
  private step = 0;
  private voltage = 12.6;
  private demoBezel: BezelOffsets = { top: 0, bottom: 0, left: 0, right: 0 };

  get state(): ConnectionState {
    return this._state;
  }
  get isConnected(): boolean {
    return this._state === 'connected';
  }
  get deviceName(): string | null {
    return this._state === 'connected' ? 'Del Sol (Demo)' : null;
  }

  async connect(): Promise<void> {
    this.setState('connecting');
    this.log('info', 'Demo mode — simulating connection…');
    await delay(400);

    this.emit('firmwareVersion', DEMO_FW_VERSION);
    this.emit('firmwareInfo', {
      proto: 2,
      appVersion: DEMO_FW_VERSION,
      fsHash: DEMO_FS_HASH,
    });
    this.log('ok', `Firmware version: ${DEMO_FW_VERSION}`);

    this.setState('connected');
    this.log('ok', 'Connected (demo).');

    // Emit an initial frame immediately, then tick.
    this.emitFrame();
    this.tickHandle = setInterval(() => this.emitFrame(), 2000);
  }

  disconnect(): void {
    this.stopTicking();
    this.log('info', 'Demo disconnected.');
    this.setState('disconnected');
  }

  /** Simulate a firmware flash: step the progress bar through to 100%. */
  updateFirmware(
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean> {
    return this.simulateFlash('firmware', data, onProgress);
  }

  /** Simulate a filesystem flash. */
  updateFilesystem(
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean> {
    return this.simulateFlash('filesystem', data, onProgress);
  }

  private async simulateFlash(
    label: string,
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean> {
    const totalChunks = Math.max(1, Math.ceil(data.length / 512));
    this.log('info', `Demo ${label} flash: ${data.length} bytes, ${totalChunks} chunks.`);
    onProgress({ percent: 10, message: `Uploading ${label} (${data.length} bytes)…` });
    for (let i = 0; i < totalChunks; i++) {
      await delay(40);
      const bytesSent = Math.min((i + 1) * 512, data.length);
      const percent = 10 + Math.round(((i + 1) * 85) / totalChunks);
      onProgress({
        percent,
        message: `Uploading… ${i + 1}/${totalChunks} chunks`,
        bytesSent,
        totalBytes: data.length,
      });
    }
    onProgress({ percent: 100, message: 'Update completed successfully.' });
    this.log('ok', `Demo ${label} flash complete.`);
    return true;
  }

  // --- Debug / crash-dump simulation ---
  private demoHasCrash = true;

  async checkCrashDump(): Promise<CrashDumpStatus> {
    await delay(250);
    if (!this.demoHasCrash) {
      this.log('rx', 'Debug status: "NOCRASH"');
      return { hasCrash: false, sizeBytes: 0, chunkCount: 0 };
    }
    const sizeBytes = 4096;
    const chunkCount = Math.ceil(sizeBytes / 510);
    this.log('rx', `Debug status: "CRASH:${sizeBytes}:${chunkCount}"`);
    return { hasCrash: true, sizeBytes, chunkCount };
  }

  async downloadCrashDump(
    chunkCount: number,
    onProgress: (percent: number) => void,
  ): Promise<Uint8Array> {
    const data = new Uint8Array(chunkCount * 510);
    for (let i = 0; i < chunkCount; i++) {
      await delay(30);
      // fill with recognizable pattern so the saved file isn't empty
      for (let j = 0; j < 510; j++) data[i * 510 + j] = (i + j) & 0xff;
      onProgress(Math.round(((i + 1) * 100) / chunkCount));
    }
    this.log('ok', `Crash dump downloaded: ${data.length} bytes.`);
    return data;
  }

  async sendDebugCommand(command: DebugCommand): Promise<void> {
    await delay(150);
    if (command === 'CLEAR') this.demoHasCrash = false;
    if (command === 'ASSERT' || command === 'ASSERT_LATER') this.demoHasCrash = true;
    this.log('ok', `Sent debug command: ${command} (demo)`);
  }

  async readBezelOffsets(): Promise<BezelOffsets> {
    await delay(80);
    return { ...this.demoBezel };
  }

  async writeBezelOffsets(o: BezelOffsets): Promise<void> {
    await delay(80);
    this.demoBezel = { ...o };
    this.log('ok', `Set bezel offsets (demo): ${JSON.stringify(o)}`);
  }

  async uploadRoute(
    summary: RouteSummary,
    onProgress?: (p: UploadProgress) => void,
  ): Promise<void> {
    const total = Math.max(1, summary.polyline.length);
    this.log('info', `Demo route upload: ${summary.polyline.length} points.`);
    for (let i = 0; i < total; i += Math.ceil(total / 5)) {
      await delay(60);
      onProgress?.({ bytesSent: i, totalBytes: total });
    }
    onProgress?.({ bytesSent: total, totalBytes: total });
    this.log('ok', 'Route uploaded (demo).');
  }

  private emitFrame(): void {
    const base = SCRIPT[this.step % SCRIPT.length];
    this.step += 1;

    const status: VehicleStatus = { ...base, lastUpdated: new Date() };
    this.emit('status', status);

    // Battery: drift slightly, dipping when ignition is on (cranking/load).
    const drift = (Math.sin(this.step / 3) * 0.05);
    const load = base.ignitionOn ? -0.15 : 0;
    this.voltage = clamp(12.6 + drift + load, 11.8, 13.2);
    this.emit('battery', { voltage: this.voltage, timestamp: new Date() });
  }

  private stopTicking(): void {
    if (this.tickHandle !== null) {
      clearInterval(this.tickHandle);
      this.tickHandle = null;
    }
  }

  private setState(state: ConnectionState): void {
    this._state = state;
    this.emit('state', state);
  }

  private log(level: ConnectionEvents['log']['level'], message: string): void {
    this.emit('log', { level, message });
  }
}

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function clamp(v: number, lo: number, hi: number): number {
  return Math.min(hi, Math.max(lo, v));
}
