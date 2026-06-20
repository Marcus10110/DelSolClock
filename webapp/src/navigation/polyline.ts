// Mapbox / Google encoded-polyline codec.
//
// Mapbox returns route geometry as an encoded polyline string. With
// `geometries=polyline6` the precision is 6 decimal places (~0.11 m). The
// algorithm is the standard Google Encoded Polyline Algorithm Format; only the
// precision factor differs (1e5 for polyline, 1e6 for polyline6).
//
// Reference: https://developers.google.com/maps/documentation/utilities/polylinealgorithm

import type { LatLng } from './types';

const DEFAULT_PRECISION = 6; // polyline6

/** Decode an encoded polyline string into ordered [lat,lng] points. */
export function decodePolyline(
  encoded: string,
  precision: number = DEFAULT_PRECISION,
): LatLng[] {
  const factor = Math.pow(10, precision);
  const points: LatLng[] = [];
  let index = 0;
  let lat = 0;
  let lng = 0;

  while (index < encoded.length) {
    lat += decodeSignedValue(encoded, index, (next) => (index = next));
    lng += decodeSignedValue(encoded, index, (next) => (index = next));
    points.push({ lat: lat / factor, lng: lng / factor });
  }
  return points;
}

/** Encode ordered points into an encoded polyline string. */
export function encodePolyline(
  points: LatLng[],
  precision: number = DEFAULT_PRECISION,
): string {
  const factor = Math.pow(10, precision);
  let result = '';
  let prevLat = 0;
  let prevLng = 0;

  for (const { lat, lng } of points) {
    const latI = Math.round(lat * factor);
    const lngI = Math.round(lng * factor);
    result += encodeSignedValue(latI - prevLat);
    result += encodeSignedValue(lngI - prevLng);
    prevLat = latI;
    prevLng = lngI;
  }
  return result;
}

// --- internals ---

// Reads one var-length, zig-zag-encoded signed integer starting at `start`,
// reporting the new cursor position via `setIndex`.
function decodeSignedValue(
  encoded: string,
  start: number,
  setIndex: (next: number) => void,
): number {
  let index = start;
  let shift = 0;
  let result = 0;
  let byte: number;
  do {
    byte = encoded.charCodeAt(index++) - 63;
    result |= (byte & 0x1f) << shift;
    shift += 5;
  } while (byte >= 0x20);
  setIndex(index);
  // zig-zag decode
  return result & 1 ? ~(result >> 1) : result >> 1;
}

function encodeSignedValue(value: number): string {
  // zig-zag encode
  let v = value < 0 ? ~(value << 1) : value << 1;
  let out = '';
  while (v >= 0x20) {
    out += String.fromCharCode((0x20 | (v & 0x1f)) + 63);
    v >>= 5;
  }
  out += String.fromCharCode(v + 63);
  return out;
}
