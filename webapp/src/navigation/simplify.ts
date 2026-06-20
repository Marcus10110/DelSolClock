// Douglas–Peucker polyline simplification in meters.
//
// Reduces a dense route polyline (Mapbox returns thousands of points) to far
// fewer while keeping the shape within `toleranceMeters`. Crucially it also
// returns an index map (old index -> new index, or -1 if dropped) so maneuver
// anchors can be remapped onto the simplified polyline without losing their
// position.

import type { LatLng } from './types';

export interface SimplifyResult {
  points: LatLng[];
  /**
   * keptIndexMap[i] = the index in `points` of original point i, or -1 if that
   * original point was removed. Kept points are strictly increasing.
   */
  keptIndexMap: number[];
}

const EARTH_RADIUS_M = 6371008.8;

/**
 * Local equirectangular projection of (p) relative to (origin), in meters.
 * Accurate enough for the small distances involved in road geometry.
 */
function toLocalMeters(p: LatLng, origin: LatLng): { x: number; y: number } {
  const latRad = (origin.lat * Math.PI) / 180;
  const x = ((p.lng - origin.lng) * Math.PI) / 180 * Math.cos(latRad) * EARTH_RADIUS_M;
  const y = ((p.lat - origin.lat) * Math.PI) / 180 * EARTH_RADIUS_M;
  return { x, y };
}

/** Perpendicular distance (meters) from point p to the segment a–b. */
function perpDistanceMeters(p: LatLng, a: LatLng, b: LatLng): number {
  const pa = toLocalMeters(p, a);
  const ba = toLocalMeters(b, a);
  const segLenSq = ba.x * ba.x + ba.y * ba.y;
  if (segLenSq === 0) {
    return Math.hypot(pa.x, pa.y); // a == b
  }
  // projection factor of pa onto ba, clamped to the segment
  let t = (pa.x * ba.x + pa.y * ba.y) / segLenSq;
  t = Math.max(0, Math.min(1, t));
  const projX = ba.x * t;
  const projY = ba.y * t;
  return Math.hypot(pa.x - projX, pa.y - projY);
}

/**
 * Simplify `points` to within `toleranceMeters`. Endpoints are always kept, as
 * are any indices in `forceKeep` (e.g. maneuver vertices, which must survive so
 * their anchor isn't lost). A tolerance <= 0 returns the input unchanged.
 */
export function simplifyPath(
  points: LatLng[],
  toleranceMeters: number,
  forceKeep: Iterable<number> = [],
): SimplifyResult {
  const n = points.length;
  if (n <= 2 || toleranceMeters <= 0) {
    return { points: [...points], keptIndexMap: points.map((_, i) => i) };
  }

  const keep = new Array<boolean>(n).fill(false);
  keep[0] = true;
  keep[n - 1] = true;
  for (const idx of forceKeep) {
    if (idx >= 0 && idx < n) keep[idx] = true;
  }

  // Run Douglas–Peucker independently between each pair of consecutive
  // already-kept anchors (endpoints + forceKeep), so forced points act as hard
  // segment boundaries and are never simplified away.
  const anchors: number[] = [];
  for (let i = 0; i < n; i++) {
    if (keep[i]) anchors.push(i);
  }
  const stack: Array<[number, number]> = [];
  for (let a = 0; a < anchors.length - 1; a++) {
    stack.push([anchors[a], anchors[a + 1]]);
  }
  while (stack.length > 0) {
    const [start, end] = stack.pop()!;
    let maxDist = 0;
    let maxIndex = -1;
    for (let i = start + 1; i < end; i++) {
      const d = perpDistanceMeters(points[i], points[start], points[end]);
      if (d > maxDist) {
        maxDist = d;
        maxIndex = i;
      }
    }
    if (maxDist > toleranceMeters && maxIndex !== -1) {
      keep[maxIndex] = true;
      stack.push([start, maxIndex]);
      stack.push([maxIndex, end]);
    }
  }

  const outPoints: LatLng[] = [];
  const keptIndexMap = new Array<number>(n).fill(-1);
  for (let i = 0; i < n; i++) {
    if (keep[i]) {
      keptIndexMap[i] = outPoints.length;
      outPoints.push(points[i]);
    }
  }
  return { points: outPoints, keptIndexMap };
}

/**
 * Remap an original-polyline index to the simplified polyline. If the original
 * point was dropped, snaps to the nearest kept point at or before it (so a
 * maneuver still lands on a real vertex of the simplified path).
 */
export function remapIndex(keptIndexMap: number[], originalIndex: number): number {
  for (let i = originalIndex; i >= 0; i--) {
    if (keptIndexMap[i] !== -1) return keptIndexMap[i];
  }
  return 0;
}
