// Send a route to the Del Sol device over BLE from the desktop, reusing the same
// TS code the web app will use. Uses the `webbluetooth` package as the Web
// Bluetooth implementation (no browser / HTTPS needed).
//
// Run:
//   npx tsx tools/send-route.ts [fixtureName]
//   (or: npm run send-route -- [fixtureName])
// Default fixture: daily_commute. Requires the device powered + advertising.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { bluetooth } from 'webbluetooth';

import { buildRouteSummary } from '../src/navigation/routePrep';
import { uploadRoute } from '../src/navigation/routeUpload';
import type { MapboxDirectionsResponse } from '../src/navigation/mapbox';

const here = dirname(fileURLToPath(import.meta.url));
const fixture = process.argv[2] ?? 'daily_commute';

async function main() {
  const path = join(here, '..', 'test', 'fixtures', `${fixture}.json`);
  const raw = JSON.parse(readFileSync(path, 'utf8')) as MapboxDirectionsResponse;
  const route = buildRouteSummary(raw);
  console.log(
    `Route "${fixture}": ${route.polyline.length} pts, ` +
      `${route.maneuvers.length} maneuvers, ` +
      `${route.totalDistanceMeters.toFixed(0)} m`,
  );

  await uploadRoute(bluetooth, route, {
    onLog: (m) => console.log(`  ${m}`),
    onProgress: (p) =>
      console.log(`  sent ${p.bytesSent}/${p.totalBytes} bytes`),
  });
  console.log('Route uploaded successfully.');
}

main().catch((err) => {
  console.error('upload failed:', err.message);
  process.exit(1);
});
