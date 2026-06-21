// Upload a RouteSummary to the device over BLE, using the navigation service's
// chunked-write protocol (mirrors the OTA upload in src/ble/connection.ts). The
// device side is firmware/src/navigation_service.cpp.
//
// Two entry points:
//   - uploadRouteOverServer(server, ...) — chunk-writes over an ALREADY-connected
//     GATT server (used by the app's shared connection; no device picker).
//   - uploadRoute(bluetooth, ...) — requestDevice + connect, then delegates to the
//     core. Used by the Node sender tool (tools/send-route.ts).

import { encodeRoute } from './routeCodec';
import type { RouteSummary } from './types';
import { SVC_NAVIGATION, CHR_NAV_ROUTE, ALL_SERVICES } from '../ble/constants';

const NAV_SERVICE_UUID = SVC_NAVIGATION;
const NAV_ROUTE_CHAR_UUID = CHR_NAV_ROUTE;
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

// Minimal structural types so this compiles in Node builds too (no DOM lib).
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
export interface BluetoothServerLike {
  getPrimaryService(uuid: string): Promise<BluetoothServiceLike>;
}
interface BluetoothServiceLike {
  getCharacteristic(uuid: string): Promise<BluetoothCharLike>;
}
interface BluetoothCharLike {
  writeValueWithoutResponse(value: BufferSource): Promise<void>;
  startNotifications(): Promise<void>;
  addEventListener(
    type: string,
    listener: (e: { target: { value?: DataView } }) => void,
  ): void;
  removeEventListener(
    type: string,
    listener: (e: { target: { value?: DataView } }) => void,
  ): void;
}

/**
 * Chunk-write the encoded route to an already-connected GATT server. Resolves
 * when the device acks "success"; throws on error/timeout. Does NOT
 * connect/disconnect — the caller owns the connection.
 */
export async function uploadRouteOverServer(
  server: BluetoothServerLike,
  summary: RouteSummary,
  opts: UploadOptions = {},
): Promise<void> {
  const log = (m: string) => opts.onLog?.(m);
  const data = encodeRoute(summary);
  log(`Encoded route: ${data.length} bytes`);

  const svc = await server.getPrimaryService(NAV_SERVICE_UUID);
  const ch = await svc.getCharacteristic(NAV_ROUTE_CHAR_UUID);

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
      opts.onProgress?.({
        bytesSent: Math.min((i + 1) * CHUNK_SIZE, data.length),
        totalBytes: data.length,
      });

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
  }
}

/**
 * Connect to the Del Sol device via a Web Bluetooth instance, upload the route,
 * then disconnect. Used by the Node sender tool.
 */
export async function uploadRoute(
  bluetooth: BluetoothLike,
  summary: RouteSummary,
  opts: UploadOptions = {},
): Promise<void> {
  const log = (m: string) => opts.onLog?.(m);
  const device = await bluetooth.requestDevice({
    filters: [{ name: 'Del Sol' }],
    optionalServices: ALL_SERVICES,
  });
  log(`Selected ${device.name ?? 'device'}; connecting…`);
  const gatt = device.gatt;
  if (!gatt) throw new Error('device has no GATT server');
  const server = await gatt.connect();
  try {
    await uploadRouteOverServer(server, summary, opts);
  } finally {
    if (gatt.connected) gatt.disconnect();
  }
}
