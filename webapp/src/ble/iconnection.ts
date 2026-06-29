// Shared interface implemented by both the real DelSolConnection (Web Bluetooth)
// and the DemoConnection (fake data). The UI depends only on this, so it works
// identically whether or not a real device is present.

import type { Emitter } from './emitter';
import type { FirmwareInfo } from './parsers';
import type { BatteryStatus, ConnectionState, VehicleStatus } from './types';
import type { RouteSummary } from '../navigation/types';
import type { UploadProgress } from '../navigation/routeUpload';

export interface ConnectionEvents {
  state: ConnectionState;
  status: VehicleStatus;
  battery: BatteryStatus;
  /** Legacy: the app version string only. Kept for existing listeners. */
  firmwareVersion: string;
  /** Full parsed firmware info: protocol, app version, SPIFFS hash. */
  firmwareInfo: FirmwareInfo;
  /** Human-readable log lines for the on-screen log / debugging. */
  log: { level: 'info' | 'ok' | 'error' | 'rx'; message: string };
  error: Error;
}

export interface FirmwareUpdateProgress {
  /** 0–100. */
  percent: number;
  message: string;
  /** Image bytes sent so far (for speed/ETA). Omitted before streaming starts. */
  bytesSent?: number;
  /** Total image bytes to send (for speed/ETA). */
  totalBytes?: number;
}

export interface CrashDumpStatus {
  hasCrash: boolean;
  sizeBytes: number;
  chunkCount: number;
}

/** Control commands accepted by the Debug Control characteristic. */
export type DebugCommand =
  | 'REBOOT'
  | 'CLEAR'
  | 'PRINT'
  | 'ASSERT'
  | 'ASSERT_LATER';

/** Display bezel insets (px) per side; positive pushes content off that edge. */
export interface BezelOffsets {
  top: number;
  bottom: number;
  left: number;
  right: number;
}

export interface IConnection extends Emitter<ConnectionEvents> {
  readonly state: ConnectionState;
  readonly isConnected: boolean;
  readonly deviceName: string | null;
  connect(): Promise<void>;
  disconnect(): void;
  /**
   * Flash the app firmware image over BLE. Resolves true on success.
   * Reports progress via the callback. Must be connected.
   */
  updateFirmware(
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean>;

  /**
   * Flash the SPIFFS filesystem image over BLE. Resolves true on success; the
   * device reboots afterward. Requires a proto >= 2 device. Reports progress.
   */
  updateFilesystem(
    data: Uint8Array,
    onProgress: (p: FirmwareUpdateProgress) => void,
  ): Promise<boolean>;

  /** Read the device's crash-dump status. */
  checkCrashDump(): Promise<CrashDumpStatus>;

  /**
   * Download the crash dump as raw bytes via sequential chunk reads.
   * Reports 0–100 progress. chunkCount comes from checkCrashDump().
   */
  downloadCrashDump(
    chunkCount: number,
    onProgress: (percent: number) => void,
  ): Promise<Uint8Array>;

  /** Send a debug control command (reboot, clear, print, assert…). */
  sendDebugCommand(command: DebugCommand): Promise<void>;

  /** Read the device's current display bezel offsets. */
  readBezelOffsets(): Promise<BezelOffsets>;

  /** Write display bezel offsets (applied live + persisted on the device). */
  writeBezelOffsets(offsets: BezelOffsets): Promise<void>;

  /**
   * Upload a navigation route to the device over the existing connection.
   * Resolves on the device's "success" ack; throws on error/timeout.
   */
  uploadRoute(
    summary: RouteSummary,
    onProgress?: (p: UploadProgress) => void,
  ): Promise<void>;
}
