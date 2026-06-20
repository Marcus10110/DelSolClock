// Mapbox Directions API: wire types + a thin fetch wrapper.
//
// Mirrors the fetch pattern in src/firmware/firmwareBrowser.ts. The fetch is not
// exercised by the test suite (tests use captured fixtures); it exists for the
// later phase that wires up a live in-browser route request. The wire types
// below describe only the fields the route-prep pipeline consumes.

import type { LatLng } from './types';

export interface MapboxManeuver {
  type: string;
  modifier?: string;
  instruction: string;
  location: [number, number]; // [lng, lat]
  bearing_before?: number;
  bearing_after?: number;
}

export interface MapboxStep {
  geometry: string; // encoded polyline (polyline6) for this step
  name: string;
  distance: number; // meters
  duration: number; // seconds
  maneuver: MapboxManeuver;
}

export interface MapboxLeg {
  steps: MapboxStep[];
  distance: number;
  duration: number;
}

export interface MapboxRoute {
  geometry: string; // overview polyline (polyline6)
  legs: MapboxLeg[];
  distance: number;
  duration: number;
}

export interface MapboxDirectionsResponse {
  code: string; // "Ok" on success
  routes: MapboxRoute[];
}

const BASE = 'https://api.mapbox.com/directions/v5/mapbox/driving-traffic';

/**
 * Fetch a traffic-aware route between two points. Coordinates are {lat,lng};
 * Mapbox wants lng,lat order in the URL. Not used by tests.
 */
export async function fetchRoute(
  start: LatLng,
  end: LatLng,
  accessToken: string,
): Promise<MapboxDirectionsResponse> {
  const coords = `${start.lng},${start.lat};${end.lng},${end.lat}`;
  const params = new URLSearchParams({
    steps: 'true',
    geometries: 'polyline6',
    overview: 'full',
    banner_instructions: 'true',
    access_token: accessToken,
  });
  const url = `${BASE}/${coords}?${params.toString()}`;
  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(`Mapbox Directions ${res.status}: ${res.statusText}`);
  }
  return (await res.json()) as MapboxDirectionsResponse;
}
