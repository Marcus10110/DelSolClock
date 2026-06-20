// Shared maneuver type/modifier <-> small-int mappings for the binary route wire
// format. MUST stay in sync with firmware/lib/navigation/include/maneuver_codes.h
// (same order = same codes). Unknown strings map to 0.

// Mapbox maneuver types (https://docs.mapbox.com/api/navigation/directions/).
export const MANEUVER_TYPES = [
  'unknown', // 0 — fallback
  'turn',
  'new name',
  'depart',
  'arrive',
  'merge',
  'on ramp',
  'off ramp',
  'fork',
  'end of road',
  'continue',
  'roundabout',
  'rotary',
  'roundabout turn',
  'notification',
  'exit roundabout',
  'exit rotary',
] as const;

export const MANEUVER_MODIFIERS = [
  'none', // 0 — no modifier
  'uturn',
  'sharp right',
  'right',
  'slight right',
  'straight',
  'slight left',
  'left',
  'sharp left',
] as const;

export function maneuverTypeCode(type: string): number {
  const i = MANEUVER_TYPES.indexOf(type as (typeof MANEUVER_TYPES)[number]);
  return i < 0 ? 0 : i;
}

export function maneuverModifierCode(modifier: string | undefined): number {
  if (!modifier) return 0;
  const i = MANEUVER_MODIFIERS.indexOf(
    modifier as (typeof MANEUVER_MODIFIERS)[number],
  );
  return i < 0 ? 0 : i;
}
