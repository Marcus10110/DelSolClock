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

/** Firmware version characteristic: plain ASCII string. */
export function parseFirmwareVersion(view: DataView): string {
  return decoder.decode(view).trim();
}
