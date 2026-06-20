// Domain model for the navigation route summary — the typed, hardware-agnostic
// representation produced by the route-prep pipeline (Phase 1). The compact
// binary/fixed-point encoding for BLE download is a later phase; this is the
// in-memory form everything else builds on.

export interface LatLng {
  lat: number;
  lng: number;
}

/**
 * A single turn-by-turn maneuver. `polylineIndex` is the index into
 * RouteSummary.polyline where this maneuver occurs — the linkage the on-device
 * matcher will use to decide "which step am I on / how far to the next turn".
 * Mapbox does not provide this index, so the pipeline computes it.
 */
export interface RouteManeuver {
  /** Mapbox maneuver type, e.g. "turn", "merge", "fork", "arrive". */
  type: string;
  /** Mapbox modifier, e.g. "left", "slight right". Optional (e.g. depart). */
  modifier?: string;
  /** Human-readable instruction text. */
  instruction: string;
  /** Road name for the step, "" if unknown. */
  roadName: string;
  /** Index into RouteSummary.polyline where this maneuver occurs. */
  polylineIndex: number;
  /** Distance of the step that begins at this maneuver, in meters. */
  distanceMeters: number;
}

export interface RouteSummary {
  /**
   * Anchor point (first polyline coordinate). Reserved as the base for future
   * fixed-point delta encoding when this is packed for BLE.
   */
  base: LatLng;
  /** Simplified route geometry, in order, start -> destination. */
  polyline: LatLng[];
  /** Maneuvers in order; each references an index into `polyline`. */
  maneuvers: RouteManeuver[];
  totalDistanceMeters: number;
  totalDurationSeconds: number;
  /**
   * Reserved for cross-street "decoration" segments (rendered grey for visual
   * cues). Empty in v1; populated in a later (Valhalla/OSM) phase.
   */
  decorationSegments: LatLng[][];
}
