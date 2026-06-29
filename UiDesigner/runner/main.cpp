// UiDesigner runner: render every shared screen (the real firmware screen code)
// to a BMP, plus a composite grid. Uses the real Adafruit GFX library and the
// shared display library directly — no UiDesigner-specific GFX reimplementation.

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <Adafruit_GFX.h>

#include "draw_helpers.h"
#include "demo_screens.h"

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

int main() {
  const int w = display::kWidth;
  const int h = display::kHeight;
  const auto& screens = demo::GetDemoScreens();

  fs::create_directories("out");

  GFXcanvas16 canvas(w, h);
  for (const auto& s : screens) {
    canvas.fillScreen(0x0000);
    s.draw(&canvas);
    fs::path outPath = fs::path("out") / (s.name + ".bmp");
    SaveBMP24(outPath, canvas.getBuffer(), w, h);
    std::printf("Wrote %s\n", outPath.string().c_str());
  }

  // Composite grid: all screens, labeled, in a grid. Each cell that has a
  // non-zero bezel also gets the hidden border drawn as a translucent red
  // overlay so it's obvious which pixels the physical bezel would clip.
  const int cols = 3;
  const int rows = (static_cast<int>(screens.size()) + cols - 1) / cols;
  const int label_h = 12;
  const int pad = 6;
  const int cell_w = w + pad * 2;
  const int cell_h = h + label_h + pad * 2;

  auto render_grid = [&](const fs::path& out_path, int16_t bz_top,
                         int16_t bz_bottom, int16_t bz_left, int16_t bz_right) {
    display::SetBezelInsets(bz_top, bz_bottom, bz_left, bz_right);
    GFXcanvas16 grid(cell_w * cols, cell_h * rows);
    grid.fillScreen(0x18E3);  // dark gray background

    for (size_t i = 0; i < screens.size(); ++i) {
      int cx = static_cast<int>(i % cols) * cell_w;
      int cy = static_cast<int>(i / cols) * cell_h;
      canvas.fillScreen(0x0000);
      screens[i].draw(&canvas);
      const int gx = cx + pad;
      const int gy = cy + pad + label_h;
      // blit the screen into the grid
      grid.drawRGBBitmap(gx, gy, canvas.getBuffer(), w, h);
      grid.drawRect(gx, gy, w, h, 0xFFFF);
      // Shade the bezel-hidden border red so clipped content is obvious.
      const uint16_t kBezelTint = 0xF800;  // red
      if (bz_top > 0) grid.fillRect(gx, gy, w, bz_top, kBezelTint);
      if (bz_bottom > 0)
        grid.fillRect(gx, gy + h - bz_bottom, w, bz_bottom, kBezelTint);
      if (bz_left > 0) grid.fillRect(gx, gy, bz_left, h, kBezelTint);
      if (bz_right > 0)
        grid.fillRect(gx + w - bz_right, gy, bz_right, h, kBezelTint);
      grid.setFont(nullptr);
      grid.setTextSize(1);
      grid.setTextColor(0xFFFF);
      grid.setCursor(cx + pad, cy + pad);
      grid.print(screens[i].name.c_str());
    }
    SaveBMP24(out_path, grid.getBuffer(), grid.width(), grid.height());
    std::printf("Wrote %s (%zu screens)\n", out_path.string().c_str(),
                screens.size());
  };

  // Baseline: no bezel.
  render_grid(fs::path("out") / "all_screens.bmp", 0, 0, 0, 0);

  // Exaggerated, deliberately asymmetric bezel so it's easy to verify every
  // screen reflows its content inward on all four sides (and to catch any edge
  // that ignores a particular side). The red band marks the hidden region — no
  // readable content should fall inside it.
  render_grid(fs::path("out") / "all_screens_bezel.bmp", 14, 20, 18, 24);

  // Leave the bezel reset so any later use of the lib starts clean.
  display::SetBezelInsets(0, 0, 0, 0);
  return 0;
}
