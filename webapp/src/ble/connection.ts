// DelSolConnection — the Web Bluetooth port of DelSolClockAppMaui's BLE services.
//
// Phase 1 scope: device selection, connect/disconnect, vehicle status + battery
// notifications, firmware version read. Firmware OTA and Debug services are
// declared in optionalServices so later phases can use them without re-pairing.

import {
  ADVERTISED_NAME,
  ALL_SERVICES,
  CHR_BATTERY,
  CHR_DEBUG_BEZEL,
  CHR_DEBUG_CONTROL,
  CHR_DEBUG_DATA,
  CHR_DEBUG_STATUS,
  CHR_FW_VERSION,
  CHR_FW_WRITE,
  CHR_STATUS,
  FIRMWARE_CHUNK_SIZE,
  FW_CMD_WRITE_APP,
  FW_CMD_WRITE_SPIFFS,
  FW_HEADER_SIZE,
  OTA_NOTIFY_TIMEOUT_MS,
  SVC_DEBUG,
  SVC_FIRMWARE,
  SVC_VEHICLE,
} from './constants';
import { Emitter } from './emitter';
import {
  parseBatteryVoltage,
  parseFirmwareVersion,
  parseVehicleStatus,
} from './parsers';
import type { ConnectionState } from './types';
import type {
  BezelOffsets,
  ConnectionEvents,
  CrashDumpStatus,
  DebugCommand,
  FirmwareUpdateProgress,
  IConnection,
} from './iconnection';
import { uploadRouteOverServer } from '../navigation/routeUpload';
import type { UploadProgress, BluetoothServerLike } from '../navigation/routeUpload';
import type { RouteSummary } from '../navigation/types';

export class DelSolConnection extends Emitter<ConnectionEvents> implements IConnection {
  private device: BluetoothDevice | null = null;
  private server: BluetoothRemoteGATTServer | null = null;
  private statusChar: BluetoothRemoteGATTCharacteristic | null = null;
  private batteryChar: BluetoothRemoteGATTCharacteristic | null = null;
  private _state: ConnectionState = 'disconnected';
  /** Protocol version of the connected device (1 = legacy, app-only). */
  private proto = 1;

  private readonly onStatusValue = (e: Event) => {
    const char = e.target as BluetoothRemoteGATTCharacteristic;
    if (!char.value) return;
    const status = parseVehicleStatus(char.value);
    if (status) this.emit('status', status);
  };

  private readonly onBatteryValue = (e: Event) => {
    const char = e.target as BluetoothRemoteGATTCharacteristic;
    if (!char.value) return;
    const voltage = parseBatteryVoltage(char.value);
    if (voltage !== null) {
      this.emit('battery', { voltage, timestamp: new Date() });
    }
  };

  private readonly onDisconnected = () => {
    this.log('info', 'Device disconnected.');
    this.cleanup();
    this.setState('disconnected');
  };

  get state(): ConnectionState {
    return this._state;
  }

  get isConnected(): boolean {
    return this._state === 'connected';
  }

  get deviceName(): string | null {
    return this.device?.name ?? null;
  }

  static isSupported(): boolean {
    return typeof navigator !== 'undefined' && 'bluetooth' in navigator;
  }

  /**
   * Prompt the user to select the Del Sol device and connect to it.
   * On iOS/Bluefy the OS pairing prompt (passkey 123456) appears the first
   * time an encrypted characteristic is accessed.
   */
  async connect(): Promise<void> {
    if (!DelSolConnection.isSupported()) {
      throw new Error('Web Bluetooth is not available in this browser.');
    }

    try {
      this.setState('requesting');
      this.log('info', `Requesting device "${ADVERTISED_NAME}"…`);
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ name: ADVERTISED_NAME }],
        optionalServices: ALL_SERVICES,
      });
      this.log('ok', `Selected: ${device.name ?? device.id}`);
      await this.establishConnection(device);
    } catch (err) {
      this.fail(err);
      throw err instanceof Error ? err : new Error(String(err));
    }
  }

  /**
   * Devices the user has previously granted access to (via getDevices()), so we
   * can reconnect without the picker. Returns [] only if the browser lacks
   * getDevices(). (Bluefy on iOS DOES support it and persists handles across
   * page loads — confirmed on-device — so reconnect-without-picker works there.)
   */
  static async getKnownDevices(): Promise<BluetoothDevice[]> {
    if (!DelSolConnection.isSupported() || !navigator.bluetooth.getDevices) {
      return [];
    }
    try {
      const devices = await navigator.bluetooth.getDevices();
      // Only our clock (the picker may have granted others in other sessions).
      return devices.filter((d) => d.name === ADVERTISED_NAME);
    } catch {
      return [];
    }
  }

  /**
   * Reconnect to a previously-granted device without showing the picker.
   *
   * Calls device.gatt.connect() DIRECTLY — no advertisement wait. A bonded /
   * previously-granted device (from getDevices()) can be reconnected directly
   * even when it isn't advertising and doesn't show up in the picker. This is
   * the behavior confirmed on iOS/Bluefy: the device stays connectable at the OS
   * level after the page closes, so a direct gatt.connect() succeeds where a
   * scan/picker finds nothing. (Waiting for an advertisement here was the cause
   * of the old "times out after 10s" reconnect bug — the device never
   * advertises while it's OS-connected.)
   */
  async reconnect(device: BluetoothDevice): Promise<void> {
    if (!DelSolConnection.isSupported()) {
      throw new Error('Web Bluetooth is not available in this browser.');
    }
    try {
      this.setState('connecting');
      this.log('info', `Reconnecting to "${device.name ?? device.id}"…`);
      await this.establishConnection(device);
    } catch (err) {
      this.fail(err);
      throw err instanceof Error ? err : new Error(String(err));
    }
  }

  /**
   * Reconnect by trying every previously-granted device in turn, stopping at the
   * first that connects. The picker can grant several handles over time (e.g.
   * pairing with more than one clock, or stale handles after "forget device");
   * only one is live, and dead handles fail fast (a GATT error), so we just try
   * them in order. Throws if none connect.
   */
  async reconnectAny(): Promise<void> {
    if (!DelSolConnection.isSupported()) {
      throw new Error('Web Bluetooth is not available in this browser.');
    }
    const devices = await DelSolConnection.getKnownDevices();
    if (devices.length === 0) {
      throw new Error('No previously-paired device to reconnect to.');
    }
    let lastErr: unknown = null;
    for (let i = 0; i < devices.length; i++) {
      const device = devices[i];
      try {
        this.setState('connecting');
        this.log('info',
          `Reconnecting to "${device.name ?? device.id}" (${i + 1}/${devices.length})…`);
        await this.establishConnection(device);
        return; // connected
      } catch (err) {
        lastErr = err;
        this.log('info', `Handle ${i + 1} failed: ${err instanceof Error ? err.message : String(err)}`);
        this.cleanup(); // drop the half-open attempt before trying the next
      }
    }
    this.fail(lastErr ?? new Error('Reconnect failed.'));
    throw lastErr instanceof Error ? lastErr : new Error('Reconnect failed.');
  }

  /** Shared connect path used by both connect() (picker) and reconnect(). */
  private async establishConnection(device: BluetoothDevice): Promise<void> {
    this.device = device;
    this.device.addEventListener('gattserverdisconnected', this.onDisconnected);

    this.setState('connecting');
    this.log('info', 'Connecting to GATT server…');
    const gatt = this.device.gatt;
    if (!gatt) throw new Error('Device has no GATT server.');
    // Bound the connect so a dead/stale handle fails fast (lets reconnectAny()
    // move on to the next handle instead of hanging).
    this.server = await withTimeout(
      gatt.connect(),
      15_000,
      'GATT connect timed out.',
    );
    this.log('ok', 'GATT connected.');

    await this.readFirmwareVersion();
    await this.subscribeVehicle();

    this.setState('connected');
    this.log('ok', 'Connected.');
  }

  private fail(err: unknown): void {
    const error = err instanceof Error ? err : new Error(String(err));
    this.log('error', error.message);
    this.emit('error', error);
    this.cleanup();
    this.setState('disconnected');
  }

  disconnect(): void {
    const gatt = this.device?.gatt;
    if (gatt?.connected) {
      this.log('info', 'Disconnecting…');
      gatt.disconnect(); // fires gattserverdisconnected → onDisconnected → cleanup
    } else {
      this.cleanup();
      this.setState('disconnected');
    }
  }

  /** Read the (encrypted) firmware version characteristic. */
  private async readFirmwareVersion(): Promise<void> {
    if (!this.server) return;
    const svc = await this.server.getPrimaryService(SVC_FIRMWARE);
    const chr = await svc.getCharacteristic(CHR_FW_VERSION);
    const value = await chr.readValue();
    const info = parseFirmwareVersion(value);
    this.proto = info.proto;
    this.log(
      'ok',
      `Firmware: ${info.appVersion} (proto ${info.proto}` +
        (info.fsHash ? `, fs ${info.fsHash.slice(0, 12)}…` : '') +
        ')',
    );
    this.emit('firmwareVersion', info.appVersion);
    this.emit('firmwareInfo', info);
  }

  /** Subscribe to vehicle status + battery notifications and prime current values. */
  private async subscribeVehicle(): Promise<void> {
    if (!this.server) return;
    const svc = await this.server.getPrimaryService(SVC_VEHICLE);

    this.statusChar = await svc.getCharacteristic(CHR_STATUS);
    this.statusChar.addEventListener('characteristicvaluechanged', this.onStatusValue);
    await this.statusChar.startNotifications();
    const initialStatus = parseVehicleStatus(await this.statusChar.readValue());
    if (initialStatus) this.emit('status', initialStatus);
    this.log('ok', 'Subscribed to status notifications.');

    this.batteryChar = await svc.getCharacteristic(CHR_BATTERY);
    this.batteryChar.addEventListener('characteristicvaluechanged', this.onBatteryValue);
    await this.batteryChar.startNotifications();
    const initialVoltage = parseBatteryVoltage(await this.batteryChar.readValue());
    if (initialVoltage !== null) {
      this.emit('battery', { voltage: initialVoltage, timestamp: new Date() });
    }
    this.log('ok', 'Subscribed to battery notifications.');
  }

  /** Flash the app firmware image over BLE. */
  updateFirmware(
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean> {
    return this.streamImage('app', data, onProgress);
  }

  /** Flash the SPIFFS filesystem image over BLE. Requires a proto >= 2 device. */
  updateFilesystem(
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean> {
    if (this.proto < 2) {
      return Promise.reject(
        new Error('This device’s firmware does not support filesystem updates.'),
      );
    }
    return this.streamImage('fs', data, onProgress);
  }

  /**
   * Stream an image (app or filesystem) to the device over BLE.
   * Protocol (firmware ble_ota.cpp):
   *  - proto >= 2: first send a 5-byte header [command:1][totalSize:4 LE] and
   *    wait for "continue", then stream the image.
   *  - proto 1: app only, NO header — start streaming immediately (legacy path).
   *  - write the image in 512-byte chunks; the device acks each chunk with
   *    "continue" | "success" | "error".
   *  - a final chunk shorter than 512 bytes signals end-of-stream; if the image
   *    is an exact multiple of 512, send a zero-length final write.
   */
  private async streamImage(
    target: 'app' | 'fs',
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean> {
    if (!this.server) throw new Error('Not connected.');
    if (target === 'fs' && this.proto < 2) {
      throw new Error('Filesystem updates require proto >= 2 firmware.');
    }

    const label = target === 'fs' ? 'filesystem' : 'firmware';
    onProgress({ percent: 5, message: `Preparing ${label} update…` });
    const svc = await this.server.getPrimaryService(SVC_FIRMWARE);
    const writeChar = await svc.getCharacteristic(CHR_FW_WRITE);

    // Ack waiter. The device acks each write with a notification, but a
    // notification can arrive in the gap between one `await ack` resolving and the
    // next `waitForAck()` arming its handler. A single-shot handler would DROP
    // that ack and the next wait would hang until timeout (the intermittent,
    // position-varying "Timed out waiting for device response" seen on proto-2).
    // Fix: queue acks that arrive with no waiter armed, so the next waitForAck()
    // consumes the buffered one immediately instead of waiting.
    let pending: ((response: string) => void) | null = null;
    const queued: string[] = [];
    const onResponse = (e: Event) => {
      const char = e.target as BluetoothRemoteGATTCharacteristic;
      const text = char.value ? new TextDecoder().decode(char.value).trim() : '';
      if (pending) {
        const resolve = pending;
        pending = null;
        resolve(text);
      } else {
        queued.push(text);
      }
    };
    writeChar.addEventListener('characteristicvaluechanged', onResponse);
    await writeChar.startNotifications();

    const waitForAck = (): Promise<string> =>
      new Promise((resolve, reject) => {
        // If an ack already arrived before we armed, consume it now.
        const buffered = queued.shift();
        if (buffered !== undefined) {
          resolve(buffered);
          return;
        }
        const timer = setTimeout(() => {
          pending = null;
          reject(new Error('Timed out waiting for device response.'));
        }, OTA_NOTIFY_TIMEOUT_MS);
        pending = (response) => {
          clearTimeout(timer);
          resolve(response);
        };
      });

    try {
      const total = data.length;
      const totalChunks = Math.ceil(total / FIRMWARE_CHUNK_SIZE);
      const needsZeroLengthChunk = total % FIRMWARE_CHUNK_SIZE === 0;

      // proto >= 2: send the 5-byte header [command:1][totalSize:4 LE] first and
      // wait for the device to ack before streaming. proto 1 devices have no
      // header and only accept an app image (legacy behavior).
      if (this.proto >= 2) {
        const command = target === 'fs' ? FW_CMD_WRITE_SPIFFS : FW_CMD_WRITE_APP;
        const header = new Uint8Array(FW_HEADER_SIZE);
        header[0] = command;
        new DataView(header.buffer).setUint32(1, total, /* littleEndian */ true);
        const ack = waitForAck();
        await writeChar.writeValueWithoutResponse(header);
        const response = await ack;
        if (response === 'error') {
          this.log('error', `Device rejected the ${label} update header.`);
          onProgress({ percent: 0, message: 'Device rejected the update.' });
          return false;
        }
      }

      this.log('info', `Uploading ${label}: ${total} bytes, ${totalChunks} chunks.`);
      onProgress({ percent: 10, message: `Uploading ${label} (${total} bytes)…` });

      for (let i = 0; i < totalChunks; i++) {
        const offset = i * FIRMWARE_CHUNK_SIZE;
        // Copy into a fresh ArrayBuffer-backed view (satisfies BufferSource).
        const chunk = data.slice(offset, offset + FIRMWARE_CHUNK_SIZE);

        const ack = waitForAck();
        await writeChar.writeValueWithoutResponse(chunk);
        const response = await ack;

        if (response === 'error') {
          this.log('error', `${cap(label)} update failed at chunk ${i + 1}.`);
          onProgress({ percent: 0, message: `Update failed at chunk ${i + 1}.` });
          return false;
        }
        if (response === 'success') {
          this.log('ok', `${cap(label)} update completed successfully.`);
          onProgress({ percent: 100, message: 'Update completed successfully.' });
          return true;
        }
        if (response !== 'continue') {
          this.log('error', `Unexpected response: "${response}"`);
        }

        const bytesSent = Math.min((i + 1) * FIRMWARE_CHUNK_SIZE, total);
        const percent = 10 + Math.round(((i + 1) * 85) / totalChunks);
        onProgress({
          percent,
          message: `Uploading… ${i + 1}/${totalChunks} chunks`,
          bytesSent,
          totalBytes: total,
        });
      }

      if (needsZeroLengthChunk) {
        this.log('info', 'Sending zero-length final chunk.');
        const ack = waitForAck();
        await writeChar.writeValueWithoutResponse(new Uint8Array(0));
        const response = await ack;
        if (response === 'success') {
          onProgress({ percent: 100, message: 'Update completed successfully.' });
          return true;
        }
        if (response === 'error') {
          onProgress({ percent: 0, message: 'Update failed at final verification.' });
          return false;
        }
      }

      onProgress({ percent: 95, message: 'Update completed, verifying…' });
      return true;
    } finally {
      writeChar.removeEventListener('characteristicvaluechanged', onResponse);
      try {
        await writeChar.stopNotifications();
      } catch {
        // device may already be rebooting after a successful flash
      }
    }
  }

  /**
   * Read the crash-dump status. Ported from DebugService.CheckCrashDumpStatusAsync.
   * Value is ASCII: "NOCRASH" or "CRASH:<sizeBytes>:<chunkCount>".
   */
  async checkCrashDump(): Promise<CrashDumpStatus> {
    if (!this.server) throw new Error('Not connected.');
    const svc = await this.server.getPrimaryService(SVC_DEBUG);
    const chr = await svc.getCharacteristic(CHR_DEBUG_STATUS);
    const text = new TextDecoder().decode(await chr.readValue()).trim();
    this.log('rx', `Debug status: "${text}"`);

    if (/^CRASH:/i.test(text)) {
      const parts = text.slice(6).split(':');
      const sizeBytes = Number.parseInt(parts[0] ?? '', 10) || 0;
      const chunkCount = Number.parseInt(parts[1] ?? '', 10) || 0;
      return { hasCrash: true, sizeBytes, chunkCount };
    }
    return { hasCrash: false, sizeBytes: 0, chunkCount: 0 };
  }

  /**
   * Download the crash dump. Ported from DebugService.DownloadCrashDumpAsync.
   * Each read of the Debug Data characteristic returns the next slice:
   * [u16 LE chunk index][payload]. The firmware advances its pointer per read.
   */
  async downloadCrashDump(
    chunkCount: number,
    onProgress: (percent: number) => void,
  ): Promise<Uint8Array> {
    if (!this.server) throw new Error('Not connected.');
    if (chunkCount <= 0) throw new Error('No crash dump to download.');

    const svc = await this.server.getPrimaryService(SVC_DEBUG);
    const chr = await svc.getCharacteristic(CHR_DEBUG_DATA);

    const chunks: Uint8Array[] = [];
    let totalBytes = 0;

    for (let expected = 0; expected < chunkCount; expected++) {
      const view = await chr.readValue();
      if (view.byteLength < 2) {
        throw new Error(`Invalid chunk at index ${expected} (too short).`);
      }
      const index = view.getUint16(0, /* littleEndian */ true);
      if (index !== expected) {
        throw new Error(`Chunk index mismatch: expected ${expected}, got ${index}.`);
      }
      const payload = new Uint8Array(view.buffer, view.byteOffset + 2, view.byteLength - 2);
      // Copy out of the characteristic's backing buffer (reused on next read).
      chunks.push(payload.slice());
      totalBytes += payload.length;
      onProgress(Math.round(((expected + 1) * 100) / chunkCount));
    }

    const result = new Uint8Array(totalBytes);
    let offset = 0;
    for (const c of chunks) {
      result.set(c, offset);
      offset += c.length;
    }
    this.log('ok', `Crash dump downloaded: ${totalBytes} bytes.`);
    return result;
  }

  /** Send a debug control command. Ported from DebugService.SendDebugControlCommandAsync. */
  async sendDebugCommand(command: DebugCommand): Promise<void> {
    if (!this.server) throw new Error('Not connected.');
    const svc = await this.server.getPrimaryService(SVC_DEBUG);
    const chr = await svc.getCharacteristic(CHR_DEBUG_CONTROL);
    await chr.writeValue(new TextEncoder().encode(command));
    this.log('ok', `Sent debug command: ${command}`);
  }

  /** Read the device's display bezel offsets (4 signed bytes). */
  async readBezelOffsets(): Promise<BezelOffsets> {
    if (!this.server) throw new Error('Not connected.');
    const svc = await this.server.getPrimaryService(SVC_DEBUG);
    const chr = await svc.getCharacteristic(CHR_DEBUG_BEZEL);
    const v = await chr.readValue();
    if (v.byteLength < 4) throw new Error('Bezel read too short.');
    return {
      top: v.getInt8(0),
      bottom: v.getInt8(1),
      left: v.getInt8(2),
      right: v.getInt8(3),
    };
  }

  /** Write display bezel offsets (applied live + persisted on the device). */
  async writeBezelOffsets(o: BezelOffsets): Promise<void> {
    if (!this.server) throw new Error('Not connected.');
    const svc = await this.server.getPrimaryService(SVC_DEBUG);
    const chr = await svc.getCharacteristic(CHR_DEBUG_BEZEL);
    const buf = new Int8Array([o.top, o.bottom, o.left, o.right]);
    await chr.writeValue(buf);
  }

  async uploadRoute(
    summary: RouteSummary,
    onProgress?: (p: UploadProgress) => void,
  ): Promise<void> {
    if (!this.server) throw new Error('Not connected.');
    // The real BluetoothRemoteGATTServer is structurally compatible with the
    // loader's minimal BluetoothServerLike (which is intentionally DOM-free for
    // Node); the cast bridges TS's stricter DOM event-listener typing.
    await uploadRouteOverServer(
      this.server as unknown as BluetoothServerLike,
      summary,
      { onProgress, onLog: (m) => this.log('info', m) },
    );
    this.log('ok', 'Route uploaded.');
  }

  private cleanup(): void {
    this.statusChar?.removeEventListener('characteristicvaluechanged', this.onStatusValue);
    this.batteryChar?.removeEventListener('characteristicvaluechanged', this.onBatteryValue);
    this.device?.removeEventListener('gattserverdisconnected', this.onDisconnected);
    this.statusChar = null;
    this.batteryChar = null;
    this.server = null;
    this.device = null;
  }

  private setState(state: ConnectionState): void {
    this._state = state;
    this.emit('state', state);
  }

  private log(level: ConnectionEvents['log']['level'], message: string): void {
    this.emit('log', { level, message });
  }
}

/** Capitalize the first letter (for log messages). */
function cap(s: string): string {
  return s.length ? s[0].toUpperCase() + s.slice(1) : s;
}

/** Reject if `p` doesn't settle within `ms`. */
function withTimeout<T>(p: Promise<T>, ms: number, message: string): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const t = setTimeout(() => reject(new Error(message)), ms);
    p.then(
      (v) => {
        clearTimeout(t);
        resolve(v);
      },
      (e) => {
        clearTimeout(t);
        reject(e);
      },
    );
  });
}
