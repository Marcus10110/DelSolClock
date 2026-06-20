// Capture a real Mapbox Directions response into test/fixtures/ for offline tests.
//
// Usage (run from the webapp dir):
//   node tools/capture-route.mjs <name> <startLng,startLat> <endLng,endLat>
// Example:
//   node tools/capture-route.mjs simple_turn -122.4194,37.7749 -122.4089,37.7849
//
// Reads MAPBOX_TOKEN from webapp/.env (gitignored). The token only needs the
// default public scopes; leave URL restrictions OFF (this runs in Node).
import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const webappRoot = join(here, '..');

function readToken() {
  if (process.env.MAPBOX_TOKEN) return process.env.MAPBOX_TOKEN;
  const envPath = join(webappRoot, '.env');
  if (!existsSync(envPath)) {
    throw new Error('No MAPBOX_TOKEN env var and no webapp/.env file found.');
  }
  const line = readFileSync(envPath, 'utf8')
    .split(/\r?\n/)
    .find((l) => l.startsWith('MAPBOX_TOKEN='));
  const token = line?.slice('MAPBOX_TOKEN='.length).trim();
  if (!token) throw new Error('MAPBOX_TOKEN not set in webapp/.env');
  return token;
}

async function main() {
  const [name, start, end] = process.argv.slice(2);
  if (!name || !start || !end) {
    console.error(
      'Usage: node tools/capture-route.mjs <name> <startLng,startLat> <endLng,endLat>',
    );
    process.exit(1);
  }
  const token = readToken();
  const params = new URLSearchParams({
    steps: 'true',
    geometries: 'polyline6',
    overview: 'full',
    banner_instructions: 'true',
    access_token: token,
  });
  const url =
    `https://api.mapbox.com/directions/v5/mapbox/driving-traffic/${start};${end}?${params}`;

  const res = await fetch(url);
  if (!res.ok) {
    console.error(`Mapbox error ${res.status}: ${res.statusText}`);
    console.error(await res.text());
    process.exit(1);
  }
  const json = await res.json();
  if (json.code !== 'Ok') {
    console.error(`Mapbox returned code=${json.code}`);
    process.exit(1);
  }

  const outDir = join(webappRoot, 'test', 'fixtures');
  mkdirSync(outDir, { recursive: true });
  const outPath = join(outDir, `${name}.json`);
  writeFileSync(outPath, JSON.stringify(json, null, 2));

  const route = json.routes[0];
  const steps = route.legs.reduce((n, l) => n + l.steps.length, 0);
  console.log(
    `Wrote ${outPath}\n  ${route.distance.toFixed(0)} m, ${steps} steps, ` +
      `geometry ${route.geometry.length} chars`,
  );
}

main().catch((err) => {
  console.error(err.message);
  process.exit(1);
});
