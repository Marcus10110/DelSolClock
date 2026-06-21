// Build a Mapbox Static Images API URL that renders a route's polyline as a flat
// PNG, framed automatically to the route. Used as an <img src> for the preview
// (no interactivity). https://docs.mapbox.com/api/maps/static-images/

import { encodePolyline } from './polyline';
import type { RouteSummary } from './types';

const BASE = 'https://api.mapbox.com/styles/v1/mapbox/navigation-day-v1/static';

export interface StaticMapOptions {
  width?: number;   // px (Mapbox max 1280)
  height?: number;  // px
  strokeColor?: string;  // hex without '#'
  strokeWidth?: number;
}

/**
 * URL for a static map image of the route. The polyline overlay uses Mapbox's
 * `path` syntax with a precision-5 encoded polyline (the Static Images default).
 * `auto` framing fits the whole route in the image.
 */
export function staticRouteImageUrl(
  summary: RouteSummary,
  token: string,
  opts: StaticMapOptions = {},
): string {
  const width = Math.min(opts.width ?? 600, 1280);
  const height = opts.height ?? 360;
  const color = opts.strokeColor ?? '2b6cff'; // matches --accent
  const strokeWidth = opts.strokeWidth ?? 4;

  // Static Images `path` overlay expects a precision-5 polyline, URL-encoded.
  const encoded = encodeURIComponent(encodePolyline(summary.polyline, 5));
  const overlay = `path-${strokeWidth}+${color}(${encoded})`;

  const params = new URLSearchParams({ access_token: token, padding: '24' });
  return `${BASE}/${overlay}/auto/${width}x${height}?${params.toString()}`;
}
