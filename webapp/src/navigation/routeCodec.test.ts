// Verifies the TS binary encoder agrees with the real C++ decoder: encode a
// route to the wire format, run it through nav_cli --binary, and confirm the
// matcher output matches the text-route path (which uses route_io). This proves
// encodeRoute (TS) ↔ decodeRoute (C++) round-trip over the actual bytes.

import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { buildRouteSummary } from './routePrep';
import { sampleAlongRoute } from './syntheticGps';
import { runMatcher } from './matcherBridge';
import { encodeRoute, ROUTE_MAGIC, ROUTE_VERSION } from './routeCodec';
import type { MapboxDirectionsResponse } from './mapbox';
import type { RouteSummary } from './types';

function loadRoute(name: string): RouteSummary {
  const path = join(__dirname, '..', '..', 'test', 'fixtures', `${name}.json`);
  const raw = JSON.parse(readFileSync(path, 'utf8')) as MapboxDirectionsResponse;
  return buildRouteSummary(raw);
}

const FIXTURES = ['simple_turn', 'long_route', 'self_crossing', 'daily_commute'];

describe('routeCodec header', () => {
  it('writes magic + version at the start', () => {
    const bytes = encodeRoute(loadRoute('simple_turn'));
    const dv = new DataView(bytes.buffer);
    expect(dv.getUint16(0, true)).toBe(ROUTE_MAGIC);
    expect(dv.getUint8(2)).toBe(ROUTE_VERSION);
  });

  it('size is compact for a real route (single-digit KB)', () => {
    const bytes = encodeRoute(loadRoute('daily_commute'));
    expect(bytes.length).toBeLessThan(4096);
  });
});

describe.each(FIXTURES)('binary codec round-trips via C++ (%s)', (name) => {
  const route = loadRoute(name);

  it('binary-decoded matcher output matches text-route output', () => {
    const track = sampleAlongRoute(route, { spacingMeters: 25 });
    const viaText = runMatcher(route, track, 'text');
    const viaBinary = runMatcher(route, track, 'binary');

    expect(viaBinary.length).toBe(viaText.length);
    for (let i = 0; i < viaText.length; i++) {
      const t = viaText[i];
      const b = viaBinary[i];
      // Numeric fields must match within int32 coord quantization (~0.1m).
      expect(b.distanceAlongRouteMeters).toBeCloseTo(t.distanceAlongRouteMeters, 0);
      expect(b.offRouteDistanceMeters).toBeCloseTo(t.offRouteDistanceMeters, 0);
      expect(b.isOffRoute).toBe(t.isOffRoute);
      expect(b.nextManeuverIndex).toBe(t.nextManeuverIndex);
      expect(b.distanceToDestinationMeters).toBeCloseTo(
        t.distanceToDestinationMeters,
        0,
      );
      // v2 carries instruction + roadName text; it must survive the binary round-trip.
      expect(b.instruction).toBe(t.instruction);
      expect(b.roadName).toBe(t.roadName);
    }
  });
});
