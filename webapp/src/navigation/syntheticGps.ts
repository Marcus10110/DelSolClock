// Generate a synthetic GPS track along a route, for testing the matcher without
// hardware. Walks the route polyline at a fixed spacing and optionally adds
// seeded (deterministic) perpendicular noise.

import type { LatLng, RouteSummary } from './types';

export interface SampleOptions {
  /** Spacing between samples along the route, in meters. */
  spacingMeters: number;
  /** Max perpendicular noise radius in meters (0 = clean). */
  noiseMeters?: number;
  /** Seed for deterministic noise. */
  seed?: number;
}

const EARTH_RADIUS_M = 6371008.8;
const DEG = Math.PI / 180;

function metersBetween(a: LatLng, b: LatLng): number {
  const latRad = a.lat * DEG;
  const dx = (b.lng - a.lng) * DEG * Math.cos(latRad) * EARTH_RADIUS_M;
  const dy = (b.lat - a.lat) * DEG * EARTH_RADIUS_M;
  return Math.hypot(dx, dy);
}

// Offset a point by (east, north) meters.
function offsetMeters(p: LatLng, east: number, north: number): LatLng {
  const latRad = p.lat * DEG;
  return {
    lat: p.lat + north / EARTH_RADIUS_M / DEG,
    lng: p.lng + east / (EARTH_RADIUS_M * Math.cos(latRad)) / DEG,
  };
}

// Tiny deterministic PRNG (mulberry32) so noise is reproducible from a seed.
function mulberry32(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/**
 * Walk the route polyline at `spacingMeters`, producing on-route GPS fixes.
 * With `noiseMeters > 0`, each fix is offset by a random vector within that
 * radius (deterministic for a given seed).
 */
export function sampleAlongRoute(
  summary: RouteSummary,
  opts: SampleOptions,
): LatLng[] {
  const { spacingMeters, noiseMeters = 0, seed = 1 } = opts;
  const poly = summary.polyline;
  if (poly.length < 2) return [...poly];

  const rand = mulberry32(seed);
  const out: LatLng[] = [];

  const addPoint = (p: LatLng) => {
    if (noiseMeters > 0) {
      // random vector within the noise radius
      const angle = rand() * 2 * Math.PI;
      const r = Math.sqrt(rand()) * noiseMeters;
      out.push(offsetMeters(p, Math.cos(angle) * r, Math.sin(angle) * r));
    } else {
      out.push({ ...p });
    }
  };

  let carry = 0; // distance carried into the current segment
  addPoint(poly[0]);
  for (let i = 0; i + 1 < poly.length; i++) {
    const a = poly[i];
    const b = poly[i + 1];
    const segLen = metersBetween(a, b);
    if (segLen === 0) continue;
    let d = spacingMeters - carry;
    while (d <= segLen) {
      const t = d / segLen;
      addPoint({ lat: a.lat + (b.lat - a.lat) * t, lng: a.lng + (b.lng - a.lng) * t });
      d += spacingMeters;
    }
    carry = segLen - (d - spacingMeters);
  }
  // ensure the final destination is included
  addPoint(poly[poly.length - 1]);
  return out;
}
