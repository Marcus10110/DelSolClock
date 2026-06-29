// Decoder for the on-device GPS recorder's binary trace (the "DSGPS" format
// produced by firmware/src/gps_recorder.cpp). Turns the downloaded bytes into a
// typed list of per-cycle samples, plus CSV and GeoJSON exporters for off-device
// analysis (drift, signal outages, parse failures).
//
// Wire format (little-endian) — keep in sync with gps_recorder.{h,cpp}:
//   Download header (20 bytes):
//     magic "DSGPS\0" (6) | u8 formatVersion | u8 reserved
//     u32 recordCount | u32 totalRecordBytes | u32 droppedRecords
//   Each record — common header (12 bytes):
//     u8 flags (bit0 hasFix, bit1 dateValid) | u8 satsUsed | u8 satsInView
//     u8 fixQuality | u32 millis | u16 hdop(*100, 0xFFFF=invalid)
//     u16 csumFailsDelta
//   Fix extension (+20 bytes, only when hasFix):
//     i32 lat_e7 | i32 lng_e7 | i32 alt_cm | u16 speed_cms | u16 course_cd
//     u32 gpsTimeOfDay (hhmmsscc UTC)

export const GPS_TRACE_MAGIC = 'DSGPS\0';
export const GPS_TRACE_FORMAT_VERSION = 1;

const FLAG_HAS_FIX = 0x01;
const FLAG_DATE_VALID = 0x02;
const HEADER_BYTES = 12;
const FIX_BYTES = 20;
const DOWNLOAD_HEADER_BYTES = 20;
const HDOP_INVALID = 0xffff;

export interface GpsSample {
  /** Device uptime ms at this cycle (monotonic). */
  millis: number;
  hasFix: boolean;
  satsUsed: number;
  satsInView: number;
  /** 0 = none, 1 = GPS, 2 = DGPS. */
  fixQuality: number;
  /** HDOP, or null when the module didn't report it. */
  hdop: number | null;
  /** Checksum-failed NMEA sentences since the previous record. */
  csumFailsDelta: number;
  // Fix-only fields (null on no-fix cycles):
  lat: number | null;
  lng: number | null;
  /** Altitude in meters. */
  altM: number | null;
  /** Speed in m/s. */
  speedMps: number | null;
  /** Course over ground, degrees. */
  courseDeg: number | null;
  /** Raw TinyGPS time.value(), hhmmsscc UTC, or null. */
  gpsTimeOfDay: number | null;
  dateValid: boolean;
}

export interface GpsTrace {
  formatVersion: number;
  recordCount: number;
  droppedRecords: number;
  samples: GpsSample[];
}

function readMagic(view: DataView, offset: number): string {
  let s = '';
  for (let i = 0; i < 6; i++) s += String.fromCharCode(view.getUint8(offset + i));
  return s;
}

/** Decode a downloaded DSGPS blob into a typed trace. Throws on a bad header. */
export function decodeGpsTrace(bytes: Uint8Array): GpsTrace {
  if (bytes.length < DOWNLOAD_HEADER_BYTES) {
    throw new Error('GPS trace too short for header.');
  }
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

  const magic = readMagic(view, 0);
  if (magic !== GPS_TRACE_MAGIC) {
    throw new Error(`Bad GPS trace magic: ${JSON.stringify(magic)}`);
  }
  const formatVersion = view.getUint8(6);
  const recordCount = view.getUint32(8, true);
  // totalRecordBytes at offset 12 is advisory; we parse to end-of-buffer.
  const droppedRecords = view.getUint32(16, true);

  const samples: GpsSample[] = [];
  let off = DOWNLOAD_HEADER_BYTES;
  while (off + HEADER_BYTES <= bytes.length) {
    const flags = view.getUint8(off);
    const hasFix = (flags & FLAG_HAS_FIX) !== 0;
    const dateValid = (flags & FLAG_DATE_VALID) !== 0;
    const satsUsed = view.getUint8(off + 1);
    const satsInView = view.getUint8(off + 2);
    const fixQuality = view.getUint8(off + 3);
    const millis = view.getUint32(off + 4, true);
    const hdopRaw = view.getUint16(off + 8, true);
    const csumFailsDelta = view.getUint16(off + 10, true);
    off += HEADER_BYTES;

    const sample: GpsSample = {
      millis,
      hasFix,
      satsUsed,
      satsInView,
      fixQuality,
      hdop: hdopRaw === HDOP_INVALID ? null : hdopRaw / 100,
      csumFailsDelta,
      lat: null,
      lng: null,
      altM: null,
      speedMps: null,
      courseDeg: null,
      gpsTimeOfDay: null,
      dateValid,
    };

    if (hasFix) {
      if (off + FIX_BYTES > bytes.length) {
        throw new Error('GPS trace truncated mid-fix-record.');
      }
      sample.lat = view.getInt32(off, true) / 1e7;
      sample.lng = view.getInt32(off + 4, true) / 1e7;
      sample.altM = view.getInt32(off + 8, true) / 100;
      sample.speedMps = view.getUint16(off + 12, true) / 100;
      sample.courseDeg = view.getUint16(off + 14, true) / 100;
      sample.gpsTimeOfDay = view.getUint32(off + 16, true);
      off += FIX_BYTES;
    }

    samples.push(sample);
  }

  return { formatVersion, recordCount, droppedRecords, samples };
}

/** Format hhmmsscc (UTC time-of-day) as "HH:MM:SS.cc", or "" if null. */
function formatGpsTime(v: number | null): string {
  if (v === null) return '';
  const cc = v % 100;
  const ss = Math.floor(v / 100) % 100;
  const mm = Math.floor(v / 10000) % 100;
  const hh = Math.floor(v / 1000000) % 100;
  const p2 = (n: number) => n.toString().padStart(2, '0');
  return `${p2(hh)}:${p2(mm)}:${p2(ss)}.${p2(cc)}`;
}

function num(v: number | null, digits: number): string {
  return v === null ? '' : v.toFixed(digits);
}

/** Render a trace as CSV (one row per cycle; blanks for missing fields). */
export function gpsTraceToCsv(trace: GpsTrace): string {
  const header = [
    'millis',
    'hasFix',
    'utc',
    'lat',
    'lng',
    'altM',
    'speedMps',
    'courseDeg',
    'satsUsed',
    'satsInView',
    'fixQuality',
    'hdop',
    'csumFailsDelta',
  ].join(',');

  const rows = trace.samples.map((s) =>
    [
      s.millis,
      s.hasFix ? 1 : 0,
      formatGpsTime(s.gpsTimeOfDay),
      num(s.lat, 7),
      num(s.lng, 7),
      num(s.altM, 2),
      num(s.speedMps, 2),
      num(s.courseDeg, 1),
      s.satsUsed,
      s.satsInView,
      s.fixQuality,
      s.hdop === null ? '' : s.hdop.toFixed(2),
      s.csumFailsDelta,
    ].join(','),
  );

  return [header, ...rows].join('\n');
}

/**
 * Render the trace as GeoJSON: a LineString of the valid fixes (for instant
 * track plotting on geojson.io / Leaflet) plus one Point feature per fix
 * carrying its per-sample properties for inspection. No-fix cycles are omitted
 * from geometry (they show up as gaps); the CSV retains them.
 */
export function gpsTraceToGeoJSON(trace: GpsTrace): string {
  const fixes = trace.samples.filter((s) => s.hasFix && s.lat !== null && s.lng !== null);

  const lineFeature = {
    type: 'Feature' as const,
    properties: {
      kind: 'track',
      fixCount: fixes.length,
      totalCycles: trace.samples.length,
      droppedRecords: trace.droppedRecords,
    },
    geometry: {
      type: 'LineString' as const,
      coordinates: fixes.map((s) => [s.lng as number, s.lat as number]),
    },
  };

  const pointFeatures = fixes.map((s) => ({
    type: 'Feature' as const,
    properties: {
      millis: s.millis,
      utc: formatGpsTime(s.gpsTimeOfDay),
      speedMps: s.speedMps,
      courseDeg: s.courseDeg,
      altM: s.altM,
      satsUsed: s.satsUsed,
      satsInView: s.satsInView,
      fixQuality: s.fixQuality,
      hdop: s.hdop,
      csumFailsDelta: s.csumFailsDelta,
    },
    geometry: {
      type: 'Point' as const,
      coordinates: [s.lng as number, s.lat as number],
    },
  }));

  return JSON.stringify(
    { type: 'FeatureCollection', features: [lineFeature, ...pointFeatures] },
    null,
    2,
  );
}

// --- Encoder (used by tests + the demo connection to synthesize a trace) -----

/** Build a DSGPS blob from samples. Round-trips with decodeGpsTrace. */
export function encodeGpsTrace(
  samples: GpsSample[],
  droppedRecords = 0,
): Uint8Array {
  const bytes: number[] = [];
  const u8 = (v: number) => bytes.push(v & 0xff);
  const u32 = (v: number) => {
    bytes.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
  };

  // Records first, so we know the byte count for the header.
  const recordBytes: number[] = [];
  const r8 = (v: number) => recordBytes.push(v & 0xff);
  const r16 = (v: number) => {
    recordBytes.push(v & 0xff, (v >> 8) & 0xff);
  };
  const r32 = (v: number) => {
    recordBytes.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >> 24) & 0xff);
  };
  const ri32 = (v: number) => r32(v >>> 0);

  for (const s of samples) {
    let flags = 0;
    if (s.hasFix) flags |= FLAG_HAS_FIX;
    if (s.dateValid) flags |= FLAG_DATE_VALID;
    r8(flags);
    r8(s.satsUsed);
    r8(s.satsInView);
    r8(s.fixQuality);
    r32(s.millis);
    r16(s.hdop === null ? HDOP_INVALID : Math.round(s.hdop * 100));
    r16(s.csumFailsDelta);
    if (s.hasFix) {
      ri32(Math.round((s.lat ?? 0) * 1e7));
      ri32(Math.round((s.lng ?? 0) * 1e7));
      ri32(Math.round((s.altM ?? 0) * 100));
      r16(Math.round((s.speedMps ?? 0) * 100));
      r16(Math.round((s.courseDeg ?? 0) * 100));
      r32(s.gpsTimeOfDay ?? 0);
    }
  }

  // Download header.
  for (const c of GPS_TRACE_MAGIC) u8(c.charCodeAt(0));
  u8(GPS_TRACE_FORMAT_VERSION);
  u8(0); // reserved
  u32(samples.length);
  u32(recordBytes.length);
  u32(droppedRecords);

  return new Uint8Array([...bytes, ...recordBytes]);
}
