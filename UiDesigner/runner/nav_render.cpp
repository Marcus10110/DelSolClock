// nav_render — perspective view dev/verification harness.
//
// Loads a route (route_io text format), samples N positions evenly along it,
// computes the car-local centerline at each, and renders the perspective screen
// to individual BMPs + one labeled grid (like all_screens). Run from the
// UiDesigner dir (data/ + out/ are CWD-relative):
//   nav_render <route.txt> [numSamples]
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <Adafruit_GFX.h>

#include "perspective_screen.h"
#include "nav_overlay.h"
#include "draw_helpers.h"
#include "route_io.h"
#include "geo.h"
#include "centerline.h"

namespace fs = std::filesystem;

static bool SaveBMP24(const fs::path& path, const uint16_t* fb, int w, int h) {
  const int rowStride = ((w * 3 + 3) / 4) * 4;
  const int imageSize = rowStride * h;
  const int fileSize = 14 + 40 + imageSize;
  FILE* f = nullptr;
#ifdef _WIN32
  fopen_s(&f, path.string().c_str(), "wb");
#else
  f = fopen(path.string().c_str(), "wb");
#endif
  if (!f) return false;
  unsigned char header[14] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
  header[2] = (unsigned char)(fileSize);
  header[3] = (unsigned char)(fileSize >> 8);
  header[4] = (unsigned char)(fileSize >> 16);
  header[5] = (unsigned char)(fileSize >> 24);
  unsigned char dib[40] = {0};
  dib[0] = 40;
  dib[4] = (unsigned char)(w);
  dib[5] = (unsigned char)(w >> 8);
  dib[8] = (unsigned char)(h);
  dib[9] = (unsigned char)(h >> 8);
  dib[12] = 1;
  dib[14] = 24;
  dib[20] = (unsigned char)(imageSize);
  dib[21] = (unsigned char)(imageSize >> 8);
  dib[22] = (unsigned char)(imageSize >> 16);
  fwrite(header, 1, sizeof(header), f);
  fwrite(dib, 1, sizeof(dib), f);
  std::vector<unsigned char> row(rowStride, 0);
  for (int y = h - 1; y >= 0; --y) {
    unsigned char* p = row.data();
    for (int x = 0; x < w; ++x) {
      uint16_t px = fb[y * w + x];
      *p++ = (unsigned char)((px & 0x1F) * 255 / 31);
      *p++ = (unsigned char)(((px >> 5) & 0x3F) * 255 / 63);
      *p++ = (unsigned char)(((px >> 11) & 0x1F) * 255 / 31);
    }
    fwrite(row.data(), 1, rowStride, f);
  }
  fclose(f);
  return true;
}

// Position at a given distance (meters) along the route polyline.
static nav::LatLng posAtDistance(const nav::RouteSummary& route,
                                 const std::vector<double>& cum, double target) {
  if (target <= 0) return route.polyline.front();
  for (size_t i = 1; i < cum.size(); ++i) {
    if (cum[i] >= target) {
      double segLen = cum[i] - cum[i - 1];
      double t = segLen > 0 ? (target - cum[i - 1]) / segLen : 0;
      const auto& a = route.polyline[i - 1];
      const auto& b = route.polyline[i];
      return {a.lat + (b.lat - a.lat) * t, a.lng + (b.lng - a.lng) * t};
    }
  }
  return route.polyline.back();
}

struct Sample {
  double distanceM;   // distance along the route
  std::string label;  // short descriptor for the grid / filename
};

// ---- Navigation overlay data (computed from the route for the harness) ------
// On the firmware this comes from the matcher; here we derive it directly from
// the route + current distance-along so we can preview the overlay. The actual
// formatting (TurnDir mapping, imperial distance, ETA) lives in the display lib
// (display::TurnDirFromManeuver / FormatDistanceImperial / FormatEta) so the
// firmware and this harness produce identical strings.
static display::NavOverlayProps buildOverlay(const nav::RouteSummary& route,
                                             const std::vector<double>& cum,
                                             double distanceM) {
  display::NavOverlayProps ov;

  // Next maneuver at or ahead of distanceM.
  const nav::RouteManeuver* next = nullptr;
  double nextDist = 0;
  for (const auto& m : route.maneuvers) {
    if (m.polylineIndex < 0 ||
        m.polylineIndex >= static_cast<int>(cum.size())) continue;
    double md = cum[m.polylineIndex];
    if (md + 1.0 >= distanceM) {  // small epsilon so we don't skip the one we're at
      // skip the depart maneuver at the very start
      if (m.type == "depart") continue;
      next = &m;
      nextDist = md - distanceM;
      break;
    }
  }
  if (next) {
    ov.dir = display::TurnDirFromManeuver(next->type, next->modifier);
    ov.distance = display::FormatDistanceImperial(nextDist > 0 ? nextDist : 0);
    ov.street = next->roadName;
  } else {
    ov.dir = display::TurnDir::None;
  }

  // Remaining distance to destination.
  double remaining = cum.back() - distanceM;
  if (remaining < 0) remaining = 0;
  char rb[24];
  std::snprintf(rb, sizeof(rb), "%.1f mi", remaining / 1609.344);
  ov.remaining = rb;

  // ETA: placeholder wall clock for the harness; speed: placeholder constant.
  ov.eta = display::FormatEta(17, 5, remaining / 1609.344 / 30.0 * 60.0);
  ov.speed = "34";
  return ov;
}

// Render the perspective view + nav overlay at a given distance along the route
// into `canvas`. Returns the number of centerline points used (0 => degenerate).
static size_t RenderAt(GFXcanvas16& canvas, const nav::RouteSummary& route,
                       const std::vector<double>& cum, double distanceM,
                       float phaseM = 0.0f, bool daytime = false) {
  nav::LatLng pos = posAtDistance(route, cum, distanceM);
  nav::CenterlineParams cp;
  cp.aheadMeters = 200.0;  // look farther so 100m-out turns enter view
  auto local = nav::routeCenterline(route, pos, cp);

  // Heading: bearing of the route from here to ~20m ahead (compass degrees,
  // 0 = N, 90 = E). Drives the background skyline pan.
  nav::LatLng ahead = posAtDistance(route, cum, distanceM + 20.0);
  nav::MetersEN d = nav::localOffsetMeters(ahead, pos);
  float heading = static_cast<float>(std::atan2(d.east, d.north) * 180.0 / 3.14159265358979);
  if (heading < 0) heading += 360.0f;

  display::PerspectiveProps p;
  p.maxDrawDistanceM = 200.0;  // match the centerline look-ahead
  p.headingDegrees = heading;
  p.centerlinePhaseM = phaseM;
  p.carSpritePath = "/del_sol_sprite_24b.bmp";
  p.daytime = daytime;
  for (const auto& lp : local) {
    p.centerline.push_back(
        {static_cast<float>(lp.forward), static_cast<float>(lp.right)});
  }
  canvas.fillScreen(0x0000);
  display::DrawPerspective(&canvas, p);

  display::NavOverlayProps ov = buildOverlay(route, cum, distanceM);
  ov.daytime = daytime;
  display::DrawNavOverlay(&canvas, ov);
  return local.size();
}

// Append a frame as raw RGB24 (top-to-bottom, no padding) to an open file —
// the pixel format ffmpeg reads with `-pix_fmt rgb24`.
static void WriteRGB24(FILE* f, const uint16_t* fb, int w, int h) {
  std::vector<unsigned char> row(static_cast<size_t>(w) * 3);
  for (int y = 0; y < h; ++y) {
    unsigned char* o = row.data();
    for (int x = 0; x < w; ++x) {
      uint16_t px = fb[y * w + x];
      *o++ = static_cast<unsigned char>(((px >> 11) & 0x1F) * 255 / 31);  // R
      *o++ = static_cast<unsigned char>(((px >> 5) & 0x3F) * 255 / 63);   // G
      *o++ = static_cast<unsigned char>((px & 0x1F) * 255 / 31);          // B
    }
    fwrite(row.data(), 1, row.size(), f);
  }
}

// Build a frame->distance schedule that eases through maneuvers: the virtual
// car cruises fast on straights and slows to a near-stop near each maneuver
// (lingering ~dwellSeconds there with smooth decel/accel), while still covering
// the whole route in exactly totalFrames. Done by integrating a per-distance
// "time cost" (1/speed) so slow regions naturally get more frames.
static std::vector<double> buildSpeedSchedule(const nav::RouteSummary& route,
                                              const std::vector<double>& cum,
                                              int totalFrames,
                                              double dwellSeconds, int fps) {
  const double total = cum.back();
  // Maneuver distances along the route (skip depart/arrive endpoints).
  std::vector<double> mDist;
  for (const auto& m : route.maneuvers) {
    if (m.polylineIndex < 0 || m.polylineIndex >= (int)cum.size()) continue;
    if (m.type == "depart" || m.type == "arrive") continue;
    mDist.push_back(cum[m.polylineIndex]);
  }

  // Speed multiplier in [slow,1] at a given distance: 1 on straights, dropping
  // to `slow` within ±window m of any maneuver, with cosine ramps.
  constexpr double kWindow = 45.0;  // meters of slow-down around a maneuver
  constexpr double kSlow = 0.18;    // fraction of cruise speed at a maneuver
  auto speedAt = [&](double d) -> double {
    double s = 1.0;
    for (double md : mDist) {
      double dist = std::abs(d - md);
      if (dist < kWindow) {
        // 0 at the maneuver -> 1 at the window edge (cosine ease).
        double t = dist / kWindow;
        double ease = 0.5 - 0.5 * std::cos(t * 3.14159265358979);  // 0..1
        double local = kSlow + (1.0 - kSlow) * ease;
        s = std::min(s, local);
      }
    }
    return s;
  };

  // Integrate cost = 1/speed over the route (fine steps) to get cumulative time.
  const int kSteps = 4000;
  std::vector<double> dAt(kSteps + 1), timeAt(kSteps + 1);
  double t = 0.0;
  for (int i = 0; i <= kSteps; ++i) {
    double d = total * i / kSteps;
    dAt[i] = d;
    timeAt[i] = t;
    if (i < kSteps) {
      double dd = total / kSteps;
      double mid = total * (i + 0.5) / kSteps;
      t += dd / speedAt(mid);
    }
  }
  const double totalTime = timeAt[kSteps];

  // Map each frame's uniform time to a distance (invert the time curve).
  std::vector<double> frameDist(totalFrames);
  int idx = 0;
  for (int fr = 0; fr < totalFrames; ++fr) {
    double targetTime = totalTime * fr / std::max(1, totalFrames - 1);
    while (idx < kSteps && timeAt[idx + 1] < targetTime) ++idx;
    double seg = timeAt[idx + 1] - timeAt[idx];
    double frac = seg > 1e-9 ? (targetTime - timeAt[idx]) / seg : 0.0;
    frameDist[fr] = dAt[idx] + (dAt[idx + 1] - dAt[idx]) * frac;
  }
  (void)dwellSeconds;
  (void)fps;
  return frameDist;
}

// Render a flythrough of the whole route to a raw RGB24 stream, then invoke
// ffmpeg to encode out/nav_route.mp4. Frames ease through maneuvers (slow near
// turns, fast on straights) while totaling fps*seconds frames. Returns false on
// I/O / encode failure.
static bool RenderVideo(const nav::RouteSummary& route,
                        const std::vector<double>& cum, int fps, int seconds) {
  const int W = display::kWidth, H = display::kHeight;
  const int totalFrames = fps * seconds;
  const double total = cum.back();
  const std::vector<double> frameDist =
      buildSpeedSchedule(route, cum, totalFrames, /*dwellSeconds=*/2.0, fps);
  // Speed-profile sanity check: report slowest/fastest per-frame advance.
  {
    double minStep = 1e9, maxStep = 0;
    for (int i = 1; i < totalFrames; ++i) {
      double s = frameDist[i] - frameDist[i - 1];
      if (s < minStep) minStep = s;
      if (s > maxStep) maxStep = s;
    }
    std::printf("schedule: slowest %.2f m/frame, fastest %.2f m/frame (%.0fx)\n",
                minStep, maxStep, maxStep / (minStep > 1e-6 ? minStep : 1));
  }
  const fs::path raw = fs::path("out") / "nav_video.rgb";

  FILE* f = nullptr;
#ifdef _WIN32
  fopen_s(&f, raw.string().c_str(), "wb");
#else
  f = fopen(raw.string().c_str(), "wb");
#endif
  if (!f) {
    std::fprintf(stderr, "nav_render: cannot open %s\n", raw.string().c_str());
    return false;
  }

  GFXcanvas16 canvas(W, H);
  for (int i = 0; i < totalFrames; ++i) {
    double d = frameDist[i];
    // Phase tracks distance traveled so the centerline dashes flow with the road
    // toward the camera (and slow/stop as the schedule slows near maneuvers).
    float phase = static_cast<float>(d);
    RenderAt(canvas, route, cum, d, phase);
    WriteRGB24(f, canvas.getBuffer(), W, H);
    if (i % fps == 0) {
      std::printf("\rrendering video %d/%d frames", i, totalFrames);
      std::fflush(stdout);
    }
  }
  fclose(f);
  std::printf("\rrendered %d frames                 \n", totalFrames);

  const fs::path mp4 = fs::path("out") / "nav_route.mp4";
  // Build the ffmpeg command. -y overwrite; read raw rgb24 at WxH; encode H.264.
  char cmd[1024];
  std::snprintf(cmd, sizeof(cmd),
                "ffmpeg -y -loglevel error -f rawvideo -pixel_format rgb24 "
                "-video_size %dx%d -framerate %d -i \"%s\" "
                "-c:v libx264 -pix_fmt yuv420p -vf \"scale=%d:%d:flags=neighbor\" "
                "\"%s\"",
                W, H, fps, raw.string().c_str(), W * 4, H * 4,
                mp4.string().c_str());
  std::printf("encoding: %s\n", mp4.string().c_str());
  int rc = std::system(cmd);
  if (rc != 0) {
    std::fprintf(stderr, "nav_render: ffmpeg failed (rc=%d)\n", rc);
    return false;
  }
  std::remove(raw.string().c_str());  // drop the large intermediate
  std::printf("Wrote %s (%dx%d, %d fps, %ds)\n", mp4.string().c_str(), W * 4,
              H * 4, fps, seconds);
  return true;
}

// Render the full 360° skyline panorama to out/skyline_360.bmp for review. The
// canvas is the panorama width; horizonY leaves a little ground below the city.
static void RenderSkyline360() {
  const int W = display::PanoramaWidth();
  const int H = 110;
  const int16_t horizonY = 92;  // buildings rest here, ~92px of sky band
  GFXcanvas16 pano(W, H);
  display::DrawSkylinePanorama(&pano, horizonY);
  // 0/90/180/270° tick marks along the bottom so bearings are easy to read.
  for (int deg = 0; deg < 360; deg += 90) {
    int x = static_cast<int>(deg / 360.0 * W);
    pano.drawFastVLine(x, H - 6, 6, 0xFFFF);
  }
  SaveBMP24(fs::path("out") / "skyline_360.bmp", pano.getBuffer(), W, H);
  std::printf("Wrote out/skyline_360.bmp (%dx%d, full 360 panorama)\n", W, H);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage: nav_render <route.txt> [numSamples]\n"
                 "       nav_render <route.txt> --video [fps] [seconds]\n");
    return 2;
  }
  const bool videoMode = argc >= 3 && std::string(argv[2]) == "--video";
  const int samples = (!videoMode && argc >= 3) ? std::atoi(argv[2]) : 12;
  const int fps = (videoMode && argc >= 4) ? std::atoi(argv[3]) : 30;
  const int seconds = (videoMode && argc >= 5) ? std::atoi(argv[4]) : 90;

  std::ifstream in(argv[1]);
  if (!in) {
    std::fprintf(stderr, "nav_render: cannot open %s\n", argv[1]);
    return 2;
  }
  nav::RouteSummary route;
  if (!nav::readRoute(in, route)) {
    std::fprintf(stderr, "nav_render: failed to parse route\n");
    return 2;
  }
  const std::vector<double> cum = nav::cumulativeDistances(route.polyline);

  const double total = cum.back();

  fs::create_directories("out");
  RenderSkyline360();  // always export the full panorama for review
  if (videoMode) {
    return RenderVideo(route, cum, fps, seconds) ? 0 : 1;
  }

  // Build the sample list: evenly-spaced points, plus approach points before
  // each maneuver (100m / 25m / 5m out) so the "action" is easy to preview.
  std::vector<Sample> list;
  for (int s = 0; s < samples; ++s) {
    double frac = samples > 1 ? static_cast<double>(s) / (samples - 1) : 0.0;
    list.push_back({frac * total, "even"});
  }
  const double approaches[] = {100.0, 50.0, 25.0, 5.0};
  for (size_t m = 0; m < route.maneuvers.size(); ++m) {
    int idx = route.maneuvers[m].polylineIndex;
    if (idx < 0 || idx >= static_cast<int>(cum.size())) continue;
    double mDist = cum[idx];
    const std::string& type = route.maneuvers[m].type;
    // skip depart (start) and arrive (handled by the last even sample)
    if (type == "depart" || type == "arrive") continue;
    for (double a : approaches) {
      double d = mDist - a;
      if (d < 0) continue;
      char lbl[48];
      std::snprintf(lbl, sizeof(lbl), "m%zu-%dm-%s", m, static_cast<int>(a),
                    type.c_str());
      list.push_back({d, lbl});
    }
  }

  // Sort by distance-along-route, then drop near-duplicate distances (<2m).
  std::sort(list.begin(), list.end(),
            [](const Sample& a, const Sample& b) { return a.distanceM < b.distanceM; });
  std::vector<Sample> samplesList;
  for (const auto& s : list) {
    if (!samplesList.empty() && s.distanceM - samplesList.back().distanceM < 2.0) {
      continue;
    }
    samplesList.push_back(s);
  }

  const int W = display::kWidth, H = display::kHeight;
  GFXcanvas16 canvas(W, H);

  std::vector<std::vector<uint16_t>> frames;
  std::vector<std::string> names;

  for (size_t s = 0; s < samplesList.size(); ++s) {
    const Sample& smp = samplesList[s];
    size_t pts = RenderAt(canvas, route, cum, smp.distanceM);

    char name[64];
    std::snprintf(name, sizeof(name), "nav_%02zu_%s", s, smp.label.c_str());
    SaveBMP24(fs::path("out") / (std::string(name) + ".bmp"), canvas.getBuffer(), W, H);
    names.push_back(name);
    frames.emplace_back(canvas.getBuffer(), canvas.getBuffer() + W * H);
    std::printf("Wrote out/%s.bmp (%.0fm, %zu centerline pts)\n", name,
                smp.distanceM, pts);
  }

  const int n = static_cast<int>(samplesList.size());
  // Labeled grid (3 columns).
  const int cols = 3;
  const int rows = (n + cols - 1) / cols;
  const int labelH = 12, pad = 6;
  const int cellW = W + pad * 2, cellH = H + labelH + pad * 2;
  GFXcanvas16 grid(cellW * cols, cellH * rows);
  grid.fillScreen(0x18E3);
  for (int i = 0; i < n; ++i) {
    int cx = (i % cols) * cellW, cy = (i / cols) * cellH;
    grid.drawRGBBitmap(cx + pad, cy + pad + labelH, frames[i].data(), W, H);
    grid.drawRect(cx + pad, cy + pad + labelH, W, H, 0xFFFF);
    grid.setFont(nullptr);
    grid.setTextSize(1);
    grid.setTextColor(0xFFFF);
    grid.setCursor(cx + pad, cy + pad);
    grid.print(names[i].c_str());
  }
  SaveBMP24(fs::path("out") / "nav_all.bmp", grid.getBuffer(), grid.width(),
            grid.height());
  std::printf("Wrote out/nav_all.bmp (%d samples)\n", n);

  // Day-mode review: render a couple of representative samples in the daytime
  // palette so the day/night looks can be compared.
  if (!samplesList.empty()) {
    size_t mid = samplesList.size() / 2;
    for (size_t s : {size_t(0), mid}) {
      RenderAt(canvas, route, cum, samplesList[s].distanceM, 0.0f,
               /*daytime=*/true);
      char nm[64];
      std::snprintf(nm, sizeof(nm), "day_%02zu.bmp", s);
      SaveBMP24(fs::path("out") / nm, canvas.getBuffer(), W, H);
      std::printf("Wrote out/%s (daytime)\n", nm);
    }
  }

  // Bezel-inset preview: render one frame with a chunky inset to confirm the HUD
  // reflows inside the visible area (the dark border is the hidden bezel region).
  if (!samplesList.empty()) {
    display::SetBezelInsets(/*top=*/12, /*bottom=*/12, /*left=*/16, /*right=*/16);
    RenderAt(canvas, route, cum, samplesList.front().distanceM);
    SaveBMP24(fs::path("out") / "bezel_demo.bmp", canvas.getBuffer(), W, H);
    display::SetBezelInsets(0, 0, 0, 0);  // reset
    std::printf("Wrote out/bezel_demo.bmp (inset preview)\n");
  }
  return 0;
}
