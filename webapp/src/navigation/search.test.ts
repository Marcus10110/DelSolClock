import { describe, it, expect } from 'vitest';
import {
  suggestUrl,
  retrieveUrl,
  forwardUrl,
  parseSuggestions,
  parsePlace,
} from './search';

const TOKEN = 'pk.test';
const SESSION = '11111111-1111-4111-8111-111111111111';

describe('search URL builders', () => {
  it('suggestUrl includes q, token, session, and proximity', () => {
    const u = new URL(
      suggestUrl('coffee', SESSION, TOKEN, { lat: 37.7, lng: -122.4 }),
    );
    expect(u.origin + u.pathname).toBe(
      'https://api.mapbox.com/search/searchbox/v1/suggest',
    );
    expect(u.searchParams.get('q')).toBe('coffee');
    expect(u.searchParams.get('access_token')).toBe(TOKEN);
    expect(u.searchParams.get('session_token')).toBe(SESSION);
    // proximity is lng,lat
    expect(u.searchParams.get('proximity')).toBe('-122.4,37.7');
  });

  it('suggestUrl omits proximity when not given', () => {
    const u = new URL(suggestUrl('coffee', SESSION, TOKEN));
    expect(u.searchParams.has('proximity')).toBe(false);
  });

  it('retrieveUrl embeds the mapbox id in the path with the session token', () => {
    const u = new URL(retrieveUrl('dXJuOm1ieHBvaTphYmM', SESSION, TOKEN));
    expect(u.pathname).toBe(
      '/search/searchbox/v1/retrieve/dXJuOm1ieHBvaTphYmM',
    );
    expect(u.searchParams.get('session_token')).toBe(SESSION);
    expect(u.searchParams.get('access_token')).toBe(TOKEN);
  });

  it('forwardUrl is non-session and limited to 1', () => {
    const u = new URL(forwardUrl('123 Main St, Anytown', TOKEN));
    expect(u.origin + u.pathname).toBe(
      'https://api.mapbox.com/search/searchbox/v1/forward',
    );
    expect(u.searchParams.get('q')).toBe('123 Main St, Anytown');
    expect(u.searchParams.has('session_token')).toBe(false);
    expect(u.searchParams.get('limit')).toBe('1');
  });
});

describe('search parsers', () => {
  it('parseSuggestions maps fields and falls back for address', () => {
    const out = parseSuggestions({
      suggestions: [
        { mapbox_id: 'a', name: 'Blue Bottle', full_address: '1 Main St' },
        { mapbox_id: 'b', name: 'Park', place_formatted: 'Somewhere' },
        { mapbox_id: 'c', name: 'Bare' },
      ],
    });
    expect(out).toHaveLength(3);
    expect(out[0]).toEqual({ mapboxId: 'a', name: 'Blue Bottle', address: '1 Main St' });
    expect(out[1].address).toBe('Somewhere');
    expect(out[2].address).toBe('');
  });

  it('parseSuggestions handles empty/missing', () => {
    expect(parseSuggestions({})).toEqual([]);
    expect(parseSuggestions({ suggestions: [] })).toEqual([]);
  });

  it('parsePlace returns first feature as {name, coords} (lng,lat -> lat,lng)', () => {
    const place = parsePlace({
      features: [
        {
          geometry: { coordinates: [-122.4, 37.7] },
          properties: { full_address: '1 Main St, Anytown' },
        },
      ],
    });
    expect(place).toEqual({
      name: '1 Main St, Anytown',
      coords: { lat: 37.7, lng: -122.4 },
    });
  });

  it('parsePlace returns null for no features', () => {
    expect(parsePlace({ features: [] })).toBeNull();
    expect(parsePlace({})).toBeNull();
  });
});
