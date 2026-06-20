import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { buildRouteSummary } from './routePrep';
import type { MapboxDirectionsResponse } from './mapbox';

function loadFixture(name: string): MapboxDirectionsResponse {
  const path = join(__dirname, '..', '..', 'test', 'fixtures', `${name}.json`);
  return JSON.parse(readFileSync(path, 'utf8')) as MapboxDirectionsResponse;
}

// Equirectangular meters between two lat/lngs (good enough for assertions).
function metersBetween(
  a: { lat: number; lng: number },
  b: { lat: number; lng: number },
): number {
  const R = 6371008.8;
  const latRad = (a.lat * Math.PI) / 180;
  const dx = ((b.lng - a.lng) * Math.PI) / 180 * Math.cos(latRad) * R;
  const dy = ((b.lat - a.lat) * Math.PI) / 180 * R;
  return Math.hypot(dx, dy);
}

const FIXTURES = ['simple_turn', 'long_route', 'self_crossing', 'daily_commute'];

describe.each(FIXTURES)('buildRouteSummary(%s)', (name) => {
  const raw = loadFixture(name);
  const summary = buildRouteSummary(raw);
  const route = raw.routes[0];

  it('produces a non-empty polyline and base anchor', () => {
    expect(summary.polyline.length).toBeGreaterThan(1);
    expect(summary.base).toEqual(summary.polyline[0]);
  });

  it('preserves totals from the Mapbox route', () => {
    expect(summary.totalDistanceMeters).toBe(route.distance);
    expect(summary.totalDurationSeconds).toBe(route.duration);
  });

  it('has one maneuver per step', () => {
    const stepCount = route.legs.reduce((n, l) => n + l.steps.length, 0);
    expect(summary.maneuvers).toHaveLength(stepCount);
  });

  it('maneuver indices are in range and non-decreasing', () => {
    let prev = -1;
    for (const m of summary.maneuvers) {
      expect(m.polylineIndex).toBeGreaterThanOrEqual(0);
      expect(m.polylineIndex).toBeLessThan(summary.polyline.length);
      expect(m.polylineIndex).toBeGreaterThanOrEqual(prev);
      prev = m.polylineIndex;
    }
  });

  it('each turn maneuver index lands on its Mapbox location', () => {
    const steps = route.legs.flatMap((l) => l.steps);
    summary.maneuvers.forEach((m, i) => {
      // `arrive` location is the destination waypoint, which legitimately sits
      // off the snapped road geometry; it anchors to the final polyline vertex.
      if (m.type === 'arrive') return;
      const [lng, lat] = steps[i].maneuver.location;
      const at = summary.polyline[m.polylineIndex];
      // Within the 5m simplification tolerance + slack.
      expect(metersBetween(at, { lat, lng })).toBeLessThan(15);
    });
  });

  it('arrive maneuver anchors to the final polyline vertex', () => {
    const last = summary.maneuvers[summary.maneuvers.length - 1];
    expect(last.type).toBe('arrive');
    expect(last.polylineIndex).toBe(summary.polyline.length - 1);
  });

  it('first maneuver is depart at index 0; last is arrive', () => {
    expect(summary.maneuvers[0].polylineIndex).toBe(0);
    expect(summary.maneuvers[0].type).toBe('depart');
    expect(summary.maneuvers[summary.maneuvers.length - 1].type).toBe('arrive');
  });

  it('decorationSegments is reserved-empty in v1', () => {
    expect(summary.decorationSegments).toEqual([]);
  });
});

describe('buildRouteSummary error handling', () => {
  it('throws on non-Ok code', () => {
    expect(() =>
      buildRouteSummary({ code: 'NoRoute', routes: [] }),
    ).toThrow(/NoRoute/);
  });
  it('throws when there are no routes', () => {
    expect(() => buildRouteSummary({ code: 'Ok', routes: [] })).toThrow(/no routes/);
  });

  it('simplification reduces point count on the long route', () => {
    const raw = loadFixture('long_route');
    const full = buildRouteSummary(raw, { toleranceMeters: 0 });
    const simplified = buildRouteSummary(raw, { toleranceMeters: 5 });
    expect(simplified.polyline.length).toBeLessThan(full.polyline.length);
  });
});
