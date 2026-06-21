// Saved destinations (Home, Work, …) persisted in localStorage, surfaced as
// one-tap chips in the navigation panel. No external deps.

import type { LatLng } from './types';

export interface Favorite {
  id: string;
  label: string;     // user label, e.g. "Home"
  name: string;      // resolved place name / address
  coords: LatLng;
}

const STORAGE_KEY = 'delsol.favorites.v1';

function read(): Favorite[] {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    return Array.isArray(parsed) ? (parsed as Favorite[]) : [];
  } catch {
    return [];
  }
}

function write(list: Favorite[]): void {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(list));
}

export function listFavorites(): Favorite[] {
  return read();
}

/**
 * Add (or replace, by matching label case-insensitively) a favorite. Returns the
 * updated list. New favorites get a generated id.
 */
export function addFavorite(fav: Omit<Favorite, 'id'> & { id?: string }): Favorite[] {
  const list = read();
  const id = fav.id ?? crypto.randomUUID();
  const entry: Favorite = { id, label: fav.label, name: fav.name, coords: fav.coords };
  const existing = list.findIndex(
    (f) => f.label.toLowerCase() === entry.label.toLowerCase(),
  );
  if (existing >= 0) {
    list[existing] = entry;
  } else {
    list.push(entry);
  }
  write(list);
  return list;
}

export function removeFavorite(id: string): Favorite[] {
  const list = read().filter((f) => f.id !== id);
  write(list);
  return list;
}
