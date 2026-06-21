// Mapbox Search Box API: autocomplete (suggest -> retrieve) + one-shot forward
// geocoding for pasted strings. https://docs.mapbox.com/api/search/search-box/
//
// /suggest and /retrieve are session-grouped (one session_token across the
// keystroke suggestions + the final retrieve), which Mapbox uses for billing.
// /forward is non-session (each call billed separately) and is used when the
// user pastes a full address and submits without picking a suggestion.

import type { LatLng } from './types';

const SUGGEST_URL = 'https://api.mapbox.com/search/searchbox/v1/suggest';
const RETRIEVE_URL = 'https://api.mapbox.com/search/searchbox/v1/retrieve';
const FORWARD_URL = 'https://api.mapbox.com/search/searchbox/v1/forward';

export interface Suggestion {
  mapboxId: string;   // pass to retrieve() to get coordinates
  name: string;       // primary text, e.g. "Blue Bottle Coffee"
  address: string;    // secondary text / full address line ("" if none)
}

export interface Place {
  name: string;
  coords: LatLng;
}

/** New session token grouping a suggest/retrieve interaction (UUIDv4). */
export function newSessionToken(): string {
  return crypto.randomUUID();
}

// --- URL builders (pure; unit-tested) ---

export function suggestUrl(
  query: string,
  sessionToken: string,
  token: string,
  proximity?: LatLng,
): string {
  const params = new URLSearchParams({
    q: query,
    access_token: token,
    session_token: sessionToken,
    types: 'address,place,poi,street',
    limit: '6',
  });
  if (proximity) params.set('proximity', `${proximity.lng},${proximity.lat}`);
  return `${SUGGEST_URL}?${params.toString()}`;
}

export function retrieveUrl(
  mapboxId: string,
  sessionToken: string,
  token: string,
): string {
  const params = new URLSearchParams({
    access_token: token,
    session_token: sessionToken,
  });
  return `${RETRIEVE_URL}/${encodeURIComponent(mapboxId)}?${params.toString()}`;
}

export function forwardUrl(
  query: string,
  token: string,
  proximity?: LatLng,
): string {
  const params = new URLSearchParams({
    q: query,
    access_token: token,
    limit: '1',
  });
  if (proximity) params.set('proximity', `${proximity.lng},${proximity.lat}`);
  return `${FORWARD_URL}?${params.toString()}`;
}

// --- response parsers (pure; unit-tested) ---

interface RawSuggestResponse {
  suggestions?: Array<{
    mapbox_id: string;
    name: string;
    full_address?: string;
    place_formatted?: string;
  }>;
}

export function parseSuggestions(raw: unknown): Suggestion[] {
  const r = raw as RawSuggestResponse;
  if (!r.suggestions) return [];
  return r.suggestions.map((s) => ({
    mapboxId: s.mapbox_id,
    name: s.name,
    address: s.full_address ?? s.place_formatted ?? '',
  }));
}

interface RawFeatureResponse {
  features?: Array<{
    geometry: { coordinates: [number, number] }; // [lng, lat]
    properties: { name?: string; full_address?: string };
  }>;
}

/** Parse a /retrieve or /forward response into the first Place, or null. */
export function parsePlace(raw: unknown): Place | null {
  const r = raw as RawFeatureResponse;
  const f = r.features?.[0];
  if (!f) return null;
  const [lng, lat] = f.geometry.coordinates;
  return {
    name: f.properties.full_address ?? f.properties.name ?? '',
    coords: { lat, lng },
  };
}

// --- fetch wrappers (not unit-tested; live API) ---

export async function suggest(
  query: string,
  sessionToken: string,
  token: string,
  proximity?: LatLng,
): Promise<Suggestion[]> {
  const res = await fetch(suggestUrl(query, sessionToken, token, proximity));
  if (!res.ok) throw new Error(`Mapbox suggest ${res.status}: ${res.statusText}`);
  return parseSuggestions(await res.json());
}

export async function retrieve(
  mapboxId: string,
  sessionToken: string,
  token: string,
): Promise<Place | null> {
  const res = await fetch(retrieveUrl(mapboxId, sessionToken, token));
  if (!res.ok) throw new Error(`Mapbox retrieve ${res.status}: ${res.statusText}`);
  return parsePlace(await res.json());
}

export async function forward(
  query: string,
  token: string,
  proximity?: LatLng,
): Promise<Place | null> {
  const res = await fetch(forwardUrl(query, token, proximity));
  if (!res.ok) throw new Error(`Mapbox forward ${res.status}: ${res.statusText}`);
  return parsePlace(await res.json());
}
