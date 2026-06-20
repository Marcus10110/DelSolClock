// Bridge to the C++ nav_cli matcher for tests. Writes the route to a temp file,
// spawns nav_cli with that path, pipes GPS fixes on stdin, parses the result
// lines from stdout. Hard-fails with build instructions if nav_cli is missing.

import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { dirname } from 'node:path';

import { routeSummaryToText } from './exportRoute';
import { encodeRoute } from './routeCodec';
import type { LatLng, RouteSummary } from './types';

export interface MatchResultRow {
  distanceAlongRouteMeters: number;
  offRouteDistanceMeters: number;
  isOffRoute: boolean;
  nextManeuverIndex: number;
  distanceToNextTurnMeters: number;
  distanceToDestinationMeters: number;
  instruction: string;
  roadName: string;
}

// Resolve the built nav_cli binary (Release config of the UiDesigner CMake build).
function navCliPath(): string {
  const here = dirname(fileURLToPath(import.meta.url));
  // webapp/src/navigation -> repo root -> UiDesigner/build/Release/nav_cli.exe
  const repoRoot = join(here, '..', '..', '..');
  const exe = process.platform === 'win32' ? 'nav_cli.exe' : 'nav_cli';
  return join(repoRoot, 'UiDesigner', 'build', 'Release', exe);
}

export function navCliExists(): boolean {
  return existsSync(navCliPath());
}

/**
 * Run the C++ matcher over a route + a list of GPS fixes.
 * `format` selects how the route is handed to nav_cli: the text route_io format
 * (default) or the binary wire format (routeCodec) — the latter exercises the
 * exact bytes that cross BLE, decoded by the real C++ decoder.
 */
export function runMatcher(
  route: RouteSummary,
  fixes: LatLng[],
  format: 'text' | 'binary' = 'text',
): MatchResultRow[] {
  const cli = navCliPath();
  if (!existsSync(cli)) {
    throw new Error(
      `nav_cli not found at ${cli}\n` +
        `Build it first:\n` +
        `  cd UiDesigner && cmake -S . -B build -G "Visual Studio 17 2022" -A x64\n` +
        `  cmake --build build --target nav_cli --config Release`,
    );
  }

  const dir = mkdtempSync(join(tmpdir(), 'navcli-'));
  try {
    const stdin = fixes.map((f) => `${f.lat} ${f.lng}`).join('\n') + '\n';
    let args: string[];
    if (format === 'binary') {
      const routePath = join(dir, 'route.bin');
      writeFileSync(routePath, encodeRoute(route));
      args = ['--binary', routePath];
    } else {
      const routePath = join(dir, 'route.txt');
      writeFileSync(routePath, routeSummaryToText(route));
      args = [routePath];
    }
    const proc = spawnSync(cli, args, { input: stdin, encoding: 'utf8' });
    if (proc.status !== 0) {
      throw new Error(`nav_cli exited ${proc.status}: ${proc.stderr}`);
    }
    return parseOutput(proc.stdout);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

// Each line: <along> <off> <isOff> <nextIdx> <toTurn> <toDest> "<instr>" "<road>"
function parseOutput(stdout: string): MatchResultRow[] {
  const rows: MatchResultRow[] = [];
  for (const line of stdout.split('\n')) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    const m = trimmed.match(
      /^(\S+) (\S+) (\S+) (\S+) (\S+) (\S+) "(.*)" "(.*)"$/,
    );
    if (!m) throw new Error(`unparseable nav_cli line: ${line}`);
    rows.push({
      distanceAlongRouteMeters: Number(m[1]),
      offRouteDistanceMeters: Number(m[2]),
      isOffRoute: m[3] === '1',
      nextManeuverIndex: Number(m[4]),
      distanceToNextTurnMeters: Number(m[5]),
      distanceToDestinationMeters: Number(m[6]),
      instruction: m[7],
      roadName: m[8],
    });
  }
  return rows;
}
