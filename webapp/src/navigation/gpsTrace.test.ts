import { describe, it, expect } from 'vitest';
import {
  decodeGpsTrace,
  encodeGpsTrace,
  gpsTraceToCsv,
  gpsTraceToGeoJSON,
  GPS_TRACE_FORMAT_VERSION,
  type GpsSample,
} from './gpsTrace';

function fixSample(over: Partial<GpsSample> = {}): GpsSample {
  return {
    millis: 1000,
    hasFix: true,
    satsUsed: 8,
    satsInView: 11,
    fixQuality: 1,
    hdop: 0.9,
    csumFailsDelta: 0,
    lat: 37.7749,
    lng: -122.4194,
    altM: 52.3,
    speedMps: 13.4,
    courseDeg: 45.5,
    gpsTimeOfDay: 17300012, // 17:30:00.12
    dateValid: true,
    ...over,
  };
}

function noFixSample(over: Partial<GpsSample> = {}): GpsSample {
  return {
    millis: 2000,
    hasFix: false,
    satsUsed: 0,
    satsInView: 2,
    fixQuality: 0,
    hdop: null,
    csumFailsDelta: 3,
    lat: null,
    lng: null,
    altM: null,
    speedMps: null,
    courseDeg: null,
    gpsTimeOfDay: null,
    dateValid: true,
    ...over,
  };
}

describe('gpsTrace encode/decode round-trip', () => {
  it('round-trips a fix record within fixed-point precision', () => {
    const blob = encodeGpsTrace([fixSample()]);
    const trace = decodeGpsTrace(blob);
    expect(trace.formatVersion).toBe(GPS_TRACE_FORMAT_VERSION);
    expect(trace.recordCount).toBe(1);
    expect(trace.samples).toHaveLength(1);

    const s = trace.samples[0];
    expect(s.hasFix).toBe(true);
    expect(s.lat).toBeCloseTo(37.7749, 6);
    expect(s.lng).toBeCloseTo(-122.4194, 6);
    expect(s.altM).toBeCloseTo(52.3, 1);
    expect(s.speedMps).toBeCloseTo(13.4, 2);
    expect(s.courseDeg).toBeCloseTo(45.5, 1);
    expect(s.satsUsed).toBe(8);
    expect(s.satsInView).toBe(11);
    expect(s.fixQuality).toBe(1);
    expect(s.hdop).toBeCloseTo(0.9, 2);
    expect(s.gpsTimeOfDay).toBe(17300012);
  });

  it('encodes a no-fix record compactly (no lat/lon space) and decodes nulls', () => {
    // 20-byte download header + 12-byte no-fix record = 32 bytes total.
    const blob = encodeGpsTrace([noFixSample()]);
    expect(blob.length).toBe(20 + 12);

    const trace = decodeGpsTrace(blob);
    const s = trace.samples[0];
    expect(s.hasFix).toBe(false);
    expect(s.lat).toBeNull();
    expect(s.lng).toBeNull();
    expect(s.hdop).toBeNull();
    expect(s.satsInView).toBe(2);
    expect(s.csumFailsDelta).toBe(3);
  });

  it('handles a mixed sequence and preserves order + dropped count', () => {
    const samples = [fixSample({ millis: 1000 }), noFixSample({ millis: 2000 }), fixSample({ millis: 3000 })];
    const blob = encodeGpsTrace(samples, 7);
    // header + 32 (fix) + 12 (no-fix) + 32 (fix)
    expect(blob.length).toBe(20 + 32 + 12 + 32);

    const trace = decodeGpsTrace(blob);
    expect(trace.droppedRecords).toBe(7);
    expect(trace.samples.map((s) => s.millis)).toEqual([1000, 2000, 3000]);
    expect(trace.samples.map((s) => s.hasFix)).toEqual([true, false, true]);
  });

  it('rejects a blob with a bad magic', () => {
    const bad = new Uint8Array(20);
    expect(() => decodeGpsTrace(bad)).toThrow(/magic/i);
  });
});

describe('gpsTrace exporters', () => {
  it('CSV has a header + one row per cycle, blanks for no-fix position', () => {
    const csv = gpsTraceToCsv(decodeGpsTrace(encodeGpsTrace([fixSample(), noFixSample()])));
    const lines = csv.split('\n');
    expect(lines).toHaveLength(3); // header + 2 rows
    expect(lines[0]).toContain('lat');
    expect(lines[0]).toContain('hdop');

    const fixRow = lines[1].split(',');
    const header = lines[0].split(',');
    const latIdx = header.indexOf('lat');
    expect(fixRow[latIdx]).toBe('37.7749000');

    const noFixRow = lines[2].split(',');
    expect(noFixRow[latIdx]).toBe(''); // blank, not 0
    const utcIdx = header.indexOf('utc');
    expect(noFixRow[utcIdx]).toBe('');
  });

  it('CSV formats the UTC time-of-day as HH:MM:SS.cc', () => {
    const csv = gpsTraceToCsv(decodeGpsTrace(encodeGpsTrace([fixSample()])));
    expect(csv).toContain('17:30:00.12');
  });

  it('GeoJSON is a FeatureCollection with a track LineString over the fixes only', () => {
    const trace = decodeGpsTrace(
      encodeGpsTrace([
        fixSample({ lat: 37.0, lng: -122.0 }),
        noFixSample(),
        fixSample({ lat: 37.1, lng: -122.1 }),
      ]),
    );
    const geo = JSON.parse(gpsTraceToGeoJSON(trace));
    expect(geo.type).toBe('FeatureCollection');

    const line = geo.features.find((f: any) => f.geometry.type === 'LineString');
    expect(line).toBeTruthy();
    // Only the 2 fixes contribute coordinates; the no-fix cycle is omitted.
    expect(line.geometry.coordinates).toEqual([
      [-122.0, 37.0],
      [-122.1, 37.1],
    ]);
    expect(line.properties.totalCycles).toBe(3);

    const points = geo.features.filter((f: any) => f.geometry.type === 'Point');
    expect(points).toHaveLength(2);
  });
});
