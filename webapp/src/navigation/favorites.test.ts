import { describe, it, expect, beforeEach, vi } from 'vitest';
import { listFavorites, addFavorite, removeFavorite } from './favorites';

// Minimal in-memory localStorage for the Node test env.
beforeEach(() => {
  const store = new Map<string, string>();
  vi.stubGlobal('localStorage', {
    getItem: (k: string) => (store.has(k) ? store.get(k)! : null),
    setItem: (k: string, v: string) => void store.set(k, v),
    removeItem: (k: string) => void store.delete(k),
    clear: () => store.clear(),
  });
  vi.stubGlobal('crypto', { randomUUID: () => 'id-' + Math.random().toString(36).slice(2) });
});

const home = { label: 'Home', name: '1 Main St', coords: { lat: 37.7, lng: -122.4 } };
const work = { label: 'Work', name: '99 Market St', coords: { lat: 37.79, lng: -122.39 } };

describe('favorites', () => {
  it('starts empty', () => {
    expect(listFavorites()).toEqual([]);
  });

  it('adds and lists favorites with generated ids', () => {
    addFavorite(home);
    const list = addFavorite(work);
    expect(list).toHaveLength(2);
    expect(list[0].label).toBe('Home');
    expect(list[0].id).toBeTruthy();
    expect(listFavorites()).toHaveLength(2);
  });

  it('replaces by label (case-insensitive) rather than duplicating', () => {
    addFavorite(home);
    const list = addFavorite({ ...home, label: 'home', name: 'New Address' });
    expect(list).toHaveLength(1);
    expect(list[0].name).toBe('New Address');
  });

  it('removes by id', () => {
    addFavorite(home);
    const after = addFavorite(work);
    const homeId = after.find((f) => f.label === 'Home')!.id;
    const list = removeFavorite(homeId);
    expect(list).toHaveLength(1);
    expect(list[0].label).toBe('Work');
  });

  it('survives corrupt storage', () => {
    localStorage.setItem('delsol.favorites.v1', '{not json');
    expect(listFavorites()).toEqual([]);
  });
});
