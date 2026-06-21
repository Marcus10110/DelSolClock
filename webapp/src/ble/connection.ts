// DelSolConnection — the Web Bluetooth port of DelSolClockAppMaui's BLE services.
//
// Phase 1 scope: device selection, connect/disconnect, vehicle status + battery
// notifications, firmware version read. Firmware OTA and Debug services are
// declared in optionalServices so later phases can use them without re-pairing.

import {
  ADVERTISED_NAME,
  ALL_SERVICES,
  CHR_BATTERY,
  CHR_DEBUG_CONTROL,
  CHR_DEBUG_DATA,
  CHR_DEBUG_STATUS,
  CHR_FW_VERSION,
  CHR_FW_WRITE,
  CHR_STATUS,
  FIRMWARE_CHUNK_SIZE,
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
    const version = parseFirmwareVersion(value);
    this.log('ok', `Firmware version: ${version}`);
    this.emit('firmwareVersion', version);
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

  /**
   * Flash a firmware image over BLE. Ported from DelSolDevice.UpdateFirmwareAsync.
   * Protocol (firmware ble_ota.cpp):
   *  - write the image in 512-byte chunks to the FW write characteristic
   *  - after each chunk, the device notifies "continue" | "success" | "error"
   *  - a final chunk shorter than 512 bytes signals end-of-stream; if the image
   *    is an exact multiple of 512, send a zero-length final write.
   */
  async updateFirmware(
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean> {
    if (!this.server) throw new Error('Not connected.');

    onProgress({ percent: 5, message: 'Preparing firmware update…' });
    const svc = await this.server.getPrimaryService(SVC_FIRMWARE);
    const writeChar = await svc.getCharacteristic(CHR_FW_WRITE);

    // Set up a single-shot notification waiter reused for each chunk's ack.
    let pending: ((response: string) => void) | null = null;
    const onResponse = (e: Event) => {
      const char = e.target as BluetoothRemoteGATTCharacteristic;
      const text = char.value ? new TextDecoder().decode(char.value).trim() : '';
      pending?.(text);
    };
    writeChar.addEventListener('characteristicvaluechanged', onResponse);
    await writeChar.startNotifications();

    const waitForAck = (): Promise<string> =>
      new Promise((resolve, reject) => {
        const timer = setTimeout(() => {
          pending = null;
          reject(new Error('Timed out waiting for device response.'));
        }, OTA_NOTIFY_TIMEOUT_MS);
        pending = (response) => {
          clearTimeout(timer);
          pending = null;
          resolve(response);
        };
      });

    try {
      const total = data.length;
      const totalChunks = Math.ceil(total / FIRMWARE_CHUNK_SIZE);
      const needsZeroLengthChunk = total % FIRMWARE_CHUNK_SIZE === 0;
      this.log('info', `Uploading firmware: ${total} bytes, ${totalChunks} chunks.`);
      onProgress({ percent: 10, message: `Uploading firmware (${total} bytes)…` });

      for (let i = 0; i < totalChunks; i++) {
        const offset = i * FIRMWARE_CHUNK_SIZE;
        // Copy into a fresh ArrayBuffer-backed view (satisfies BufferSource).
        const chunk = data.slice(offset, offset + FIRMWARE_CHUNK_SIZE);

        const ack = waitForAck();
        await writeChar.writeValueWithoutResponse(chunk);
        const response = await ack;

        if (response === 'error') {
          this.log('error', `Firmware update failed at chunk ${i + 1}.`);
          onProgress({ percent: 0, message: `Update failed at chunk ${i + 1}.` });
          return false;
        }
        if (response === 'success') {
          this.log('ok', 'Firmware update completed successfully.');
          onProgress({ percent: 100, message: 'Update completed successfully.' });
          return true;
        }
        if (response !== 'continue') {
          this.log('error', `Unexpected response: "${response}"`);
        }

        const percent = 10 + Math.round(((i + 1) * 85) / totalChunks);
        onProgress({ percent, message: `Uploading… ${i + 1}/${totalChunks} chunks` });
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
