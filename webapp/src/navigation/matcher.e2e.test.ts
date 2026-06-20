// End-to-end matcher test: drives the REAL C++ matcher (nav_cli) over synthetic
// GPS tracks generated from the captured Mapbox fixtures. Requires nav_cli to be
// built (the bridge hard-fails with build instructions otherwise).

import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { buildRouteSummary } from './routePrep';
import { sampleAlongRoute } from './syntheticGps';
import { runMatcher } from './matcherBridge';
import type { MapboxDirectionsResponse } from './mapbox';
import type { LatLng, RouteSummary } from './types';

function loadRoute(name: string): RouteSummary {
  const path = join(__dirname, '..', '..', 'test', 'fixtures', `${name}.json`);
  const raw = JSON.parse(readFileSync(path, 'utf8')) as MapboxDirectionsResponse;
  return buildRouteSummary(raw);
}

const DEG = Math.PI / 180;
function offsetMeters(p: LatLng, east: number, north: number): LatLng {
  const R = 6371008.8;
  return {
    lat: p.lat + north / R / DEG,
    lng: p.lng + east / (R * Math.cos(p.lat * DEG)) / DEG,
  };
}

const FIXTURES = ['simple_turn', 'long_route', 'self_crossing', 'daily_commute'];

describe.each(FIXTURES)('matcher e2e: %s', (name) => {
  const route = loadRoute(name);

  it('clean track: on-route, monotonic progress, arrives', () => {
    const track = sampleAlongRoute(route, { spacingMeters: 20 });
    const results = runMatcher(route, track);
    expect(results.length).toBe(track.length);

    // every clean fix is on-route
    for (const r of results) {
      expect(r.isOffRoute).toBe(false);
      expect(r.offRouteDistanceMeters).toBeLessThan(5);
    }
    // distance-to-destination is (weakly) decreasing
    for (let i = 1; i < results.length; i++) {
      expect(results[i].distanceToDestinationMeters).toBeLessThanOrEqual(
        results[i - 1].distanceToDestinationMeters + 1, // 1m slack
      );
    }
    // next-maneuver index is non-decreasing
    for (let i = 1; i < results.length; i++) {
      expect(results[i].nextManeuverIndex).toBeGreaterThanOrEqual(
        results[i - 1].nextManeuverIndex,
      );
    }
    // final fix has essentially arrived
    expect(results[results.length - 1].distanceToDestinationMeters).toBeLessThan(5);
  });

  it('noisy track stays on-route', () => {
    const track = sampleAlongRoute(route, {
      spacingMeters: 20,
      noiseMeters: 8,
      seed: 123,
    });
    const results = runMatcher(route, track);
    for (const r of results) {
      expect(r.offRouteDistanceMeters).toBeLessThan(50);
      expect(r.isOffRoute).toBe(false);
    }
  });

  it('a deliberately offset fix reports off-route', () => {
    // take a mid-route point and shove it ~120m perpendicular (north)
    const mid = route.polyline[Math.floor(route.polyline.length / 2)];
    const off = offsetMeters(mid, 0, 120);
    const [r] = runMatcher(route, [off]);
    expect(r.isOffRoute).toBe(true);
    expect(r.offRouteDistanceMeters).toBeGreaterThan(50);
    expect(r.offRouteDistanceMeters).toBeLessThan(200); // sane, not wild
  });
});
