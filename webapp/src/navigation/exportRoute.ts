// Serialize a RouteSummary to the text route format consumed by the C++ nav_cli
// (see firmware/lib/navigation/route_io.{h,cpp}). Stream-friendly; instruction
// and roadName are double-quoted so the data stays human-reviewable.

import type { RouteSummary } from './types';

// Drop any stray double-quotes (Mapbox text has none; defensive).
function quote(s: string): string {
  return `"${s.replace(/"/g, '')}"`;
}

export function routeSummaryToText(summary: RouteSummary): string {
  const lines: string[] = [];
  lines.push(`base ${summary.base.lat} ${summary.base.lng}`);

  lines.push(`polyline ${summary.polyline.length}`);
  for (const p of summary.polyline) {
    lines.push(`${p.lat} ${p.lng}`);
  }

  lines.push(`maneuvers ${summary.maneuvers.length}`);
  for (const m of summary.maneuvers) {
    const modifier = m.modifier && m.modifier.length > 0 ? m.modifier : '-';
    // type/modifier are single tokens; spaces in a modifier would break parsing.
    const safeModifier = modifier.replace(/\s+/g, '_');
    const safeType = m.type.replace(/\s+/g, '_');
    lines.push(
      `${m.polylineIndex} ${m.distanceMeters} ${safeType} ${safeModifier} ` +
        `${quote(m.instruction)} ${quote(m.roadName)}`,
    );
  }

  lines.push(
    `totals ${summary.totalDistanceMeters} ${summary.totalDurationSeconds}`,
  );
  return lines.join('\n') + '\n';
}
