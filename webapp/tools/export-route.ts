// Export a captured Mapbox fixture to the text route format (route_io) for the
// C++ harness (nav_render / nav_cli). Writes to UiDesigner/out/<name>.route.txt
// by default (gitignored), or a given path.
//
//   npx tsx tools/export-route.ts [fixtureName] [outPath]
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildRouteSummary } from '../src/navigation/routePrep';
import { routeSummaryToText } from '../src/navigation/exportRoute';
import type { MapboxDirectionsResponse } from '../src/navigation/mapbox';

const here = dirname(fileURLToPath(import.meta.url));
const fixture = process.argv[2] ?? 'daily_commute';
const outPath =
  process.argv[3] ??
  join(here, '..', '..', 'UiDesigner', 'out', `${fixture}.route.txt`);

const raw = JSON.parse(
  readFileSync(join(here, '..', 'test', 'fixtures', `${fixture}.json`), 'utf8'),
) as MapboxDirectionsResponse;
const summary = buildRouteSummary(raw);
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, routeSummaryToText(summary));
console.log(
  `Wrote ${outPath}: ${summary.polyline.length} pts, ` +
    `${summary.maneuvers.length} maneuvers, ${summary.totalDistanceMeters.toFixed(0)} m`,
);
