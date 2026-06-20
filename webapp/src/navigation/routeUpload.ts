// Upload a RouteSummary to the device over BLE, using the navigation service's
// chunked-write protocol (mirrors the OTA upload in src/ble/connection.ts).
//
// Takes a Web Bluetooth `bluetooth` instance so the same code runs in the browser
// (navigator.bluetooth) and in Node (the `webbluetooth` package). The device side
// is firmware/src/navigation_service.cpp.

import { encodeRoute } from './routeCodec';
import type { RouteSummary } from './types';

const NAV_SERVICE_UUID = '77f5d2b5-efa1-4d55-b14a-cc92b72708a0';
const NAV_ROUTE_CHAR_UUID = 'b9f0a2d1-6c3e-4a8b-9d27-1f5c0e6a4b30';
const VEHICLE_SERVICE_UUID = '8fb88487-73cf-4cce-b495-505a4b54b802';
const CHUNK_SIZE = 512;
const ACK_TIMEOUT_MS = 10_000;

export interface UploadProgress {
  bytesSent: number;
  totalBytes: number;
}

export interface UploadOptions {
  onProgress?: (p: UploadProgress) => void;
  onLog?: (message: string) => void;
}

// Minimal structural type so we don't depend on DOM lib in Node builds.
interface BluetoothLike {
  requestDevice(options: unknown): Promise<BluetoothDeviceLike>;
}
interface BluetoothDeviceLike {
  name?: string | null;
  gatt?: {
    connect(): Promise<BluetoothServerLike>;
    connected: boolean;
    disconnect(): void;
  };
}
interface BluetoothServerLike {
  getPrimaryService(uuid: string): Promise<BluetoothServiceLike>;
}
interface BluetoothServiceLike {
  getCharacteristic(uuid: string): Promise<BluetoothCharLike>;
}
interface BluetoothCharLike {
  writeValueWithoutResponse(value: BufferSource): Promise<void>;
  startNotifications(): Promise<void>;
  addEventListener(type: string, listener: (e: { target: { value?: DataView } }) => void): void;
  removeEventListener(type: string, listener: (e: { target: { value?: DataView } }) => void): void;
}

/**
 * Connect to the Del Sol device, then chunk-write the encoded route. Resolves
 * when the device acks "success". Throws on error/timeout.
 */
export async function uploadRoute(
  bluetooth: BluetoothLike,
  summary: RouteSummary,
  opts: UploadOptions = {},
): Promise<void> {
  const log = (m: string) => opts.onLog?.(m);
  const data = encodeRoute(summary);
  log(`Encoded route: ${data.length} bytes`);

  const device = await bluetooth.requestDevice({
    filters: [{ name: 'Del Sol' }],
    optionalServices: [NAV_SERVICE_UUID, VEHICLE_SERVICE_UUID],
  });
  log(`Selected ${device.name ?? 'device'}; connecting…`);
  const gatt = device.gatt;
  if (!gatt) throw new Error('device has no GATT server');
  const server = await gatt.connect();

  const svc = await server.getPrimaryService(NAV_SERVICE_UUID);
  const ch = await svc.getCharacteristic(NAV_ROUTE_CHAR_UUID);

  // Single-shot ack waiter reused per chunk.
  let pending: ((response: string) => void) | null = null;
  const dec = new TextDecoder();
  const onResponse = (e: { target: { value?: DataView } }) => {
    const v = e.target.value;
    pending?.(v ? dec.decode(v).trim() : '');
  };
  ch.addEventListener('characteristicvaluechanged', onResponse);
  await ch.startNotifications();

  const waitForAck = (): Promise<string> =>
    new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        pending = null;
        reject(new Error('timed out waiting for device ack'));
      }, ACK_TIMEOUT_MS);
      pending = (response) => {
        clearTimeout(timer);
        pending = null;
        resolve(response);
      };
    });

  try {
    const totalChunks = Math.ceil(data.length / CHUNK_SIZE);
    const needsZeroFinal = data.length % CHUNK_SIZE === 0;
    log(`Sending ${data.length} bytes in ${totalChunks} chunk(s)…`);

    for (let i = 0; i < totalChunks; i++) {
      const chunk = data.slice(i * CHUNK_SIZE, i * CHUNK_SIZE + CHUNK_SIZE);
      const ack = waitForAck();
      await ch.writeValueWithoutResponse(chunk);
      const response = await ack;
      opts.onProgress?.({ bytesSent: Math.min((i + 1) * CHUNK_SIZE, data.length), totalBytes: data.length });

      if (response === 'success') {
        log('Device acked: success');
        return;
      }
      if (response === 'error') {
        throw new Error('device reported decode error');
      }
      if (response !== 'continue') {
        log(`unexpected ack: "${response}"`);
      }
    }

    if (needsZeroFinal) {
      const ack = waitForAck();
      await ch.writeValueWithoutResponse(new Uint8Array(0));
      const response = await ack;
      if (response === 'success') {
        log('Device acked: success');
        return;
      }
      throw new Error(`unexpected final ack: "${response}"`);
    }

    throw new Error('transfer ended without a success ack');
  } finally {
    ch.removeEventListener('characteristicvaluechanged', onResponse);
    if (gatt.connected) gatt.disconnect();
  }
}
