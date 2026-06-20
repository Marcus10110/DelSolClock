import { describe, it, expect } from 'vitest';
import { sampleAlongRoute } from './syntheticGps';
import type { RouteSummary } from './types';

// A straight ~880m east-west route (10 segments of ~88m).
function straightRoute(): RouteSummary {
  const polyline = [];
  for (let i = 0; i <= 10; i++) {
    polyline.push({ lat: 37.0, lng: -122.0 + i * 0.001 });
  }
  return {
    base: polyline[0],
    polyline,
    maneuvers: [],
    totalDistanceMeters: 0,
    totalDurationSeconds: 0,
    decorationSegments: [],
  };
}

const DEG = Math.PI / 180;
function meters(a: { lat: number; lng: number }, b: { lat: number; lng: number }) {
  const R = 6371008.8;
  const dx = (b.lng - a.lng) * DEG * Math.cos(a.lat * DEG) * R;
  const dy = (b.lat - a.lat) * DEG * R;
  return Math.hypot(dx, dy);
}

describe('sampleAlongRoute', () => {
  it('produces samples spaced ~spacingMeters apart on a clean route', () => {
    const pts = sampleAlongRoute(straightRoute(), { spacingMeters: 50 });
    expect(pts.length).toBeGreaterThan(10);
    // consecutive spacing close to 50m (except possibly the final dest point)
    for (let i = 1; i < pts.length - 1; i++) {
      expect(meters(pts[i - 1], pts[i])).toBeCloseTo(50, 0);
    }
  });

  it('starts at the origin and ends at the destination', () => {
    const route = straightRoute();
    const pts = sampleAlongRoute(route, { spacingMeters: 50 });
    expect(pts[0]).toEqual(route.polyline[0]);
    const last = route.polyline[route.polyline.length - 1];
    expect(meters(pts[pts.length - 1], last)).toBeLessThan(1e-6);
  });

  it('is deterministic for a given seed and varies with noise', () => {
    const route = straightRoute();
    const a = sampleAlongRoute(route, { spacingMeters: 50, noiseMeters: 10, seed: 42 });
    const b = sampleAlongRoute(route, { spacingMeters: 50, noiseMeters: 10, seed: 42 });
    expect(a).toEqual(b);
    const clean = sampleAlongRoute(route, { spacingMeters: 50 });
    // noisy interior points differ from clean ones
    expect(a[2]).not.toEqual(clean[2]);
  });

  it('keeps noise within the requested radius', () => {
    const route = straightRoute();
    const noisy = sampleAlongRoute(route, { spacingMeters: 50, noiseMeters: 8, seed: 7 });
    const clean = sampleAlongRoute(route, { spacingMeters: 50 });
    // compare interior points (same count/order since spacing identical)
    for (let i = 1; i < Math.min(noisy.length, clean.length) - 1; i++) {
      expect(meters(noisy[i], clean[i])).toBeLessThanOrEqual(8 + 1e-6);
    }
  });
});
