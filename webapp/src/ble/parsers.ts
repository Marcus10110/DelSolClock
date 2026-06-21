// Byte-level decoders for the Del Sol characteristics.
// Each mirrors the MAUI parsing logic, which itself matches the firmware.

import type { VehicleStatus } from './types';

const decoder = new TextDecoder();

/**
 * Vehicle status characteristic: ASCII CSV of 5 ints "rw,trunk,roof,ign,lights",
 * each "1" (true) or "0" (false). Returns null if the format is unexpected
 * (e.g. the firmware's initial empty value), matching the MAUI behavior.
 * See firmware bluetooth.cpp SetVehicleStatus: snprintf("%i,%i,%i,%i,%i", ...).
 */
export function parseVehicleStatus(view: DataView): VehicleStatus | null {
  if (view.byteLength === 0) return null;

  const text = decoder.decode(view).trim();
  const parts = text.split(',');
  if (parts.length !== 5) {
    console.warn(`status: expected 5 fields, got ${parts.length}: "${text}"`);
    return null;
  }

  const flag = (s: string) => s.trim() === '1';
  return {
    rearWindowDown: flag(parts[0]),
    trunkOpen: flag(parts[1]),
    convertibleRoofDown: flag(parts[2]),
    ignitionOn: flag(parts[3]),
    headlightsOn: flag(parts[4]),
    lastUpdated: new Date(),
  };
}

/**
 * Battery characteristic: 4-byte little-endian IEEE-754 float (volts).
 * See firmware: BatteryCharacteristic->setValue(LastBatteryVoltage) (float).
 * Returns null if length != 4.
 */
export function parseBatteryVoltage(view: DataView): number | null {
  if (view.byteLength !== 4) {
    console.warn(`battery: expected 4 bytes, got ${view.byteLength}`);
    return null;
  }
  return view.getFloat32(0, /* littleEndian */ true);
}

/** Parsed firmware-version characteristic. */
export interface FirmwareInfo {
  /** Update protocol version. 1 = legacy (bare version string, app-only). */
  proto: number;
  /** App firmware version (git tag/branch). */
  appVersion: string;
  /** SHA-256 of the device's SPIFFS partition (hex), or null if proto 1. */
  fsHash: string | null;
}

/**
 * Firmware version characteristic.
 * proto >= 2: "proto=N;app=<version>;fs=<sha256-hex>" (semicolon-separated key=val).
 * proto 1 (legacy firmware): a bare version string with no "proto=" field.
 * See firmware ble_ota.cpp Begin().
 */
export function parseFirmwareVersion(view: DataView): FirmwareInfo {
  const text = decoder.decode(view).trim();

  // Legacy: no key=value structure -> protocol 1, app-only, FS hash unknown.
  if (!text.includes('proto=')) {
    return { proto: 1, appVersion: text, fsHash: null };
  }

  const fields = new Map<string, string>();
  for (const part of text.split(';')) {
    const eq = part.indexOf('=');
    if (eq > 0) fields.set(part.slice(0, eq).trim(), part.slice(eq + 1).trim());
  }
  const proto = Number.parseInt(fields.get('proto') ?? '1', 10) || 1;
  return {
    proto,
    appVersion: fields.get('app') ?? '',
    fsHash: fields.get('fs') || null,
  };
}
