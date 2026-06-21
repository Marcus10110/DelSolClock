import { describe, it, expect } from 'vitest';
import { staticRouteImageUrl } from './staticMap';
import type { RouteSummary } from './types';

function route(): RouteSummary {
  return {
    base: { lat: 37.0, lng: -122.0 },
    polyline: [
      { lat: 37.0, lng: -122.0 },
      { lat: 37.001, lng: -122.001 },
      { lat: 37.002, lng: -122.0 },
    ],
    maneuvers: [],
    totalDistanceMeters: 300,
    totalDurationSeconds: 60,
    decorationSegments: [],
  };
}

describe('staticRouteImageUrl', () => {
  const url = staticRouteImageUrl(route(), 'pk.test', { width: 600, height: 360 });

  it('targets the Mapbox Static Images endpoint with a path overlay + auto bbox', () => {
    expect(url.startsWith('https://api.mapbox.com/styles/v1/mapbox/')).toBe(true);
    expect(url).toContain('/static/path-');
    expect(url).toContain('/auto/600x360');
  });

  it('includes the access token', () => {
    expect(url).toContain('access_token=pk.test');
  });

  it('clamps width to Mapbox max 1280', () => {
    const wide = staticRouteImageUrl(route(), 'pk.test', { width: 5000 });
    expect(wide).toContain('/auto/1280x');
  });

  it('encodes the polyline overlay (URL-encoded path data)', () => {
    // path-<w>+<color>(<encoded>) — the encoded polyline is URL-encoded, so the
    // raw backslash/special chars are percent-escaped, not literal.
    expect(url).toMatch(/\/static\/path-\d\+[0-9a-fA-F]{6}\(/);
  });
});
