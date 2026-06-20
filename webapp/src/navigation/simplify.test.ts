import { describe, it, expect } from 'vitest';
import { simplifyPath, remapIndex } from './simplify';
import type { LatLng } from './types';

// A nearly-straight east-west run of points with one tiny wiggle. With a
// reasonable tolerance the collinear interior points should collapse.
function straightish(): LatLng[] {
  const pts: LatLng[] = [];
  for (let i = 0; i <= 10; i++) {
    pts.push({ lat: 37.0, lng: -122.0 + i * 0.001 }); // ~88m steps
  }
  return pts;
}

describe('simplifyPath', () => {
  it('keeps endpoints and drops collinear interior points', () => {
    const input = straightish();
    const { points, keptIndexMap } = simplifyPath(input, 5);
    expect(points.length).toBeLessThan(input.length);
    // endpoints preserved
    expect(points[0]).toEqual(input[0]);
    expect(points[points.length - 1]).toEqual(input[input.length - 1]);
    // index map: endpoints map to first/last kept
    expect(keptIndexMap[0]).toBe(0);
    expect(keptIndexMap[input.length - 1]).toBe(points.length - 1);
  });

  it('respects the tolerance: a deviation larger than tolerance is kept', () => {
    const input: LatLng[] = [
      { lat: 37.0, lng: -122.0 },
      { lat: 37.0009, lng: -122.0 }, // ~100m north spike off the line
      { lat: 37.0, lng: -122.002 },
    ];
    // tolerance 10m -> the spike must be kept
    const kept = simplifyPath(input, 10);
    expect(kept.points).toHaveLength(3);
    // tolerance 500m -> the spike collapses
    const dropped = simplifyPath(input, 500);
    expect(dropped.points).toHaveLength(2);
  });

  it('returns input unchanged for tolerance <= 0 or <= 2 points', () => {
    const input = straightish();
    expect(simplifyPath(input, 0).points).toHaveLength(input.length);
    expect(simplifyPath([input[0], input[1]], 5).points).toHaveLength(2);
  });

  it('remapIndex snaps dropped indices to the nearest kept point at/before', () => {
    const input = straightish();
    const { keptIndexMap } = simplifyPath(input, 5);
    // every original index remaps to a valid simplified index
    for (let i = 0; i < input.length; i++) {
      const m = remapIndex(keptIndexMap, i);
      expect(m).toBeGreaterThanOrEqual(0);
    }
    // remap is monotonic non-decreasing
    let prev = -1;
    for (let i = 0; i < input.length; i++) {
      const m = remapIndex(keptIndexMap, i);
      expect(m).toBeGreaterThanOrEqual(prev);
      prev = m;
    }
  });
});
