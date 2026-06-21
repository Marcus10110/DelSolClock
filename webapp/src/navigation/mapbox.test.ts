import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { directionsUrl } from './mapbox';
import { buildRouteSummary } from './routePrep';
import type { MapboxDirectionsResponse } from './mapbox';

describe('directionsUrl', () => {
  const url = new URL(
    directionsUrl({ lat: 37.7, lng: -122.4 }, { lat: 37.8, lng: -122.3 }, 'pk.test'),
  );

  it('uses driving-traffic with coords in lng,lat order', () => {
    expect(url.pathname).toContain('/directions/v5/mapbox/driving-traffic/');
    expect(url.pathname).toContain('-122.4,37.7;-122.3,37.8');
  });

  it('requests alternatives, steps, polyline6, full overview', () => {
    expect(url.searchParams.get('alternatives')).toBe('true');
    expect(url.searchParams.get('steps')).toBe('true');
    expect(url.searchParams.get('geometries')).toBe('polyline6');
    expect(url.searchParams.get('overview')).toBe('full');
  });

  it('can disable alternatives', () => {
    const u = new URL(
      directionsUrl({ lat: 1, lng: 2 }, { lat: 3, lng: 4 }, 'pk.test', false),
    );
    expect(u.searchParams.get('alternatives')).toBe('false');
  });
});

describe('buildRouteSummary routeIndex (alternatives)', () => {
  // Reuse a real single-route fixture, duplicated into two routes so we can
  // assert routeIndex selects the right one (give #2 a distinct distance).
  it('selects the requested alternative', () => {
    const path = join(__dirname, '..', '..', 'test', 'fixtures', 'simple_turn.json');
    const raw = JSON.parse(readFileSync(path, 'utf8')) as MapboxDirectionsResponse;
    const second = JSON.parse(JSON.stringify(raw.routes[0]));
    second.distance = raw.routes[0].distance + 12345;
    const multi: MapboxDirectionsResponse = { code: 'Ok', routes: [raw.routes[0], second] };

    const r0 = buildRouteSummary(multi, { routeIndex: 0 });
    const r1 = buildRouteSummary(multi, { routeIndex: 1 });
    expect(r0.totalDistanceMeters).toBe(raw.routes[0].distance);
    expect(r1.totalDistanceMeters).toBe(raw.routes[0].distance + 12345);
  });

  it('throws for an out-of-range routeIndex', () => {
    const path = join(__dirname, '..', '..', 'test', 'fixtures', 'simple_turn.json');
    const raw = JSON.parse(readFileSync(path, 'utf8')) as MapboxDirectionsResponse;
    expect(() => buildRouteSummary(raw, { routeIndex: 9 })).toThrow(/no routes/);
  });
});
