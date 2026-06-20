import { describe, it, expect } from 'vitest';
import { decodePolyline, encodePolyline } from './polyline';
import type { LatLng } from './types';

describe('polyline codec', () => {
  it('decodes the classic precision-5 reference string', () => {
    // Google's documented example (precision 5).
    const pts = decodePolyline('_p~iF~ps|U_ulLnnqC_mqNvxq`@', 5);
    expect(pts).toHaveLength(3);
    expect(pts[0].lat).toBeCloseTo(38.5, 5);
    expect(pts[0].lng).toBeCloseTo(-120.2, 5);
    expect(pts[1].lat).toBeCloseTo(40.7, 5);
    expect(pts[1].lng).toBeCloseTo(-120.95, 5);
    expect(pts[2].lat).toBeCloseTo(43.252, 5);
    expect(pts[2].lng).toBeCloseTo(-126.453, 5);
  });

  it('round-trips precision-6 points to full precision', () => {
    const points: LatLng[] = [
      { lat: 37.774929, lng: -122.419418 },
      { lat: 37.775033, lng: -122.41801 },
      { lat: 37.7766, lng: -122.41706 },
      { lat: 37.78, lng: -122.41 },
    ];
    const round = decodePolyline(encodePolyline(points, 6), 6);
    expect(round).toHaveLength(points.length);
    for (let i = 0; i < points.length; i++) {
      expect(round[i].lat).toBeCloseTo(points[i].lat, 6);
      expect(round[i].lng).toBeCloseTo(points[i].lng, 6);
    }
  });

  it('handles empty input', () => {
    expect(decodePolyline('')).toEqual([]);
    expect(encodePolyline([])).toBe('');
  });

  it('round-trips a single point', () => {
    const round = decodePolyline(encodePolyline([{ lat: 1.234567, lng: -7.654321 }]));
    expect(round[0].lat).toBeCloseTo(1.234567, 6);
    expect(round[0].lng).toBeCloseTo(-7.654321, 6);
  });
});
