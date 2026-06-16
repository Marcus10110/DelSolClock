// Shared interface implemented by both the real DelSolConnection (Web Bluetooth)
// and the DemoConnection (fake data). The UI depends only on this, so it works
// identically whether or not a real device is present.

import type { Emitter } from './emitter';
import type { BatteryStatus, ConnectionState, VehicleStatus } from './types';

export interface ConnectionEvents {
  state: ConnectionState;
  status: VehicleStatus;
  battery: BatteryStatus;
  firmwareVersion: string;
  /** Human-readable log lines for the on-screen log / debugging. */
  log: { level: 'info' | 'ok' | 'error' | 'rx'; message: string };
  error: Error;
}

export interface FirmwareUpdateProgress {
  /** 0–100. */
  percent: number;
  message: string;
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

export interface IConnection extends Emitter<ConnectionEvents> {
  readonly state: ConnectionState;
  readonly isConnected: boolean;
  readonly deviceName: string | null;
  connect(): Promise<void>;
  disconnect(): void;
  /**
   * Flash a firmware image over BLE. Resolves true on success.
   * Reports progress via the callback. Must be connected.
   */
  updateFirmware(
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
}
