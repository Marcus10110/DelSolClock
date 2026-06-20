// Binary RouteSummary encoder for the BLE wire format. MUST stay in sync with
// firmware/lib/navigation/route_codec.{h,cpp} (and route_codec.h's layout doc).
// Coordinates are int32 = round(deg * 1e6), little-endian. v2 carries each
// maneuver's instruction + roadName as u16-length-prefixed UTF-8 strings.

import { maneuverModifierCode, maneuverTypeCode } from './maneuverCodes';
import type { RouteSummary } from './types';

export const ROUTE_MAGIC = 0x4453; // 'DS'
export const ROUTE_VERSION = 2;
const COORD_SCALE = 1e6;

function coordToInt(deg: number): number {
  return Math.round(deg * COORD_SCALE);
}

export function encodeRoute(summary: RouteSummary): Uint8Array {
  const enc = new TextEncoder();

  // Pre-encode maneuver strings so we can size the buffer exactly.
  const mvStrings = summary.maneuvers.map((m) => ({
    instruction: enc.encode(m.instruction ?? ''),
    roadName: enc.encode(m.roadName ?? ''),
  }));

  const ptCount = summary.polyline.length;
  const mvCount = summary.maneuvers.length;

  let mvBytes = 0;
  for (const s of mvStrings) {
    // u16 idx + u32 dist + u8 type + u8 mod + u16 ilen + instr + u16 rlen + road
    mvBytes += 2 + 4 + 1 + 1 + 2 + s.instruction.length + 2 + s.roadName.length;
  }

  const size = 12 + 2 + ptCount * 8 + 2 + mvBytes + 8;
  const buf = new ArrayBuffer(size);
  const dv = new DataView(buf);
  const bytes = new Uint8Array(buf);
  let o = 0;

  dv.setUint16(o, ROUTE_MAGIC, true); o += 2;
  dv.setUint8(o, ROUTE_VERSION); o += 1;
  dv.setUint8(o, 0); o += 1; // flags
  dv.setInt32(o, coordToInt(summary.base.lat), true); o += 4;
  dv.setInt32(o, coordToInt(summary.base.lng), true); o += 4;

  dv.setUint16(o, ptCount, true); o += 2;
  for (const p of summary.polyline) {
    dv.setInt32(o, coordToInt(p.lat), true); o += 4;
    dv.setInt32(o, coordToInt(p.lng), true); o += 4;
  }

  dv.setUint16(o, mvCount, true); o += 2;
  summary.maneuvers.forEach((m, i) => {
    dv.setUint16(o, m.polylineIndex, true); o += 2;
    dv.setUint32(o, Math.max(0, Math.round(m.distanceMeters)), true); o += 4;
    dv.setUint8(o, maneuverTypeCode(m.type)); o += 1;
    dv.setUint8(o, maneuverModifierCode(m.modifier)); o += 1;
    const s = mvStrings[i];
    dv.setUint16(o, s.instruction.length, true); o += 2;
    bytes.set(s.instruction, o); o += s.instruction.length;
    dv.setUint16(o, s.roadName.length, true); o += 2;
    bytes.set(s.roadName, o); o += s.roadName.length;
  });

  dv.setUint32(o, Math.max(0, Math.round(summary.totalDistanceMeters)), true); o += 4;
  dv.setUint32(o, Math.max(0, Math.round(summary.totalDurationSeconds)), true); o += 4;

  return bytes;
}
