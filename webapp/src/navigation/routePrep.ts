// Route-prep orchestrator: Mapbox Directions response -> typed RouteSummary.
//
// Mapbox gives per-step geometry + one maneuver per step, but no index linking
// maneuvers to the overall polyline (unlike Valhalla). So we concatenate the
// step geometries into one polyline and record where each maneuver lands in it,
// then simplify and remap those indices. That maneuver->polyline index linkage
// is what the on-device matcher will rely on later.

import type { MapboxDirectionsResponse, MapboxStep } from './mapbox';
import { decodePolyline } from './polyline';
import { simplifyPath, remapIndex } from './simplify';
import type { LatLng, RouteManeuver, RouteSummary } from './types';

export interface BuildOptions {
  /** Douglas–Peucker tolerance in meters. Default 5m (~GPS accuracy). */
  toleranceMeters?: number;
}

const DEFAULT_TOLERANCE_M = 5;

function samePoint(a: LatLng, b: LatLng): boolean {
  return Math.abs(a.lat - b.lat) < 1e-7 && Math.abs(a.lng - b.lng) < 1e-7;
}

// Squared equirectangular distance (relative metric; fine for nearest-point).
function distSq(a: LatLng, b: LatLng): number {
  const latRad = (a.lat * Math.PI) / 180;
  const dx = (b.lng - a.lng) * Math.cos(latRad);
  const dy = b.lat - a.lat;
  return dx * dx + dy * dy;
}

// Index of the polyline point globally nearest `target`. Maneuver locations sit
// essentially on the route geometry (≤ a few meters), so the global nearest is
// unambiguous and accurate. A forward-only search can miss it when consecutive
// Mapbox step geometries don't share exact endpoints, so we search the whole
// polyline; monotonicity of the resulting indices is verified by tests.
function nearestIndex(polyline: LatLng[], target: LatLng): number {
  let best = 0;
  let bestD = Infinity;
  for (let i = 0; i < polyline.length; i++) {
    const d = distSq(polyline[i], target);
    if (d < bestD) {
      bestD = d;
      best = i;
    }
  }
  return best;
}

/** Build a RouteSummary from a Mapbox Directions response (route 0). */
export function buildRouteSummary(
  raw: MapboxDirectionsResponse,
  opts: BuildOptions = {},
): RouteSummary {
  if (raw.code !== 'Ok') {
    throw new Error(`Mapbox Directions error: ${raw.code}`);
  }
  const route = raw.routes?.[0];
  if (!route) {
    throw new Error('Mapbox response contained no routes');
  }

  const tolerance = opts.toleranceMeters ?? DEFAULT_TOLERANCE_M;

  // 1. Concatenate step geometries into one polyline, deduping the shared point
  //    between consecutive steps.
  const fullPolyline: LatLng[] = [];
  const rawManeuvers: MapboxStep[] = [];

  for (const leg of route.legs) {
    for (const step of leg.steps) {
      const stepPoints = decodePolyline(step.geometry);
      if (stepPoints.length === 0) continue;

      const startAt =
        fullPolyline.length > 0 &&
        samePoint(fullPolyline[fullPolyline.length - 1], stepPoints[0])
          ? 1
          : 0;
      for (let i = startAt; i < stepPoints.length; i++) {
        fullPolyline.push(stepPoints[i]);
      }
      rawManeuvers.push(step);
    }
  }

  if (fullPolyline.length === 0) {
    throw new Error('Route had no geometry');
  }

  // Anchor each maneuver to the polyline point nearest its reported location.
  // Search forward from the PREVIOUS maneuver's index (route maneuvers are
  // ordered, so this keeps anchors monotonic while still finding the true
  // nearest point — the step's own geometry start can drift on forks/ramps when
  // consecutive step geometries don't share an exact endpoint).
  // `arrive` is special: its location is the destination waypoint (off-road), so
  // it always anchors to the final route vertex.
  const lastIndex = fullPolyline.length - 1;
  const rawIndexed = rawManeuvers.map((step) => {
    if (step.maneuver.type === 'arrive') {
      return { step, index: lastIndex };
    }
    const [lng, lat] = step.maneuver.location;
    return { step, index: nearestIndex(fullPolyline, { lat, lng }) };
  });

  // 2. Simplify the combined polyline; force-keep maneuver vertices so their
  //    anchors survive, then remap maneuver indices onto the simplified path.
  const forceKeep = rawIndexed.map((r) => r.index);
  const { points: simplified, keptIndexMap } = simplifyPath(
    fullPolyline,
    tolerance,
    forceKeep,
  );

  const maneuvers: RouteManeuver[] = rawIndexed.map(({ step, index }) => ({
    type: step.maneuver.type,
    modifier: step.maneuver.modifier,
    instruction: step.maneuver.instruction,
    roadName: step.name ?? '',
    polylineIndex: remapIndex(keptIndexMap, index),
    distanceMeters: step.distance,
  }));

  return {
    base: simplified[0],
    polyline: simplified,
    maneuvers,
    totalDistanceMeters: route.distance,
    totalDurationSeconds: route.duration,
    decorationSegments: [],
  };
}
