// Firmware image loader: SPIFFS-backed, using SPIFFS_ImageReader.
// Ported from the old firmware Display.cpp. Compiled ONLY into the firmware
// (PlatformIO srcDir picks it up; UiDesigner's CMake does not list it and links
// its own file-backed loader instead).
#include "image_loader.h"

#include <Arduino.h>
#include <SPIFFS_ImageReader.h>

#include <map>
#include <string>

namespace display {
namespace {
// Cache loaded images — load time is SLOW (≈5 s for the splash).
std::map<std::string, SPIFFS_Image> g_loaded_images;
}  // namespace

bool PreloadImage(const char* path) {
  if (g_loaded_images.count(path) != 0) return true;

  SPIFFS_ImageReader reader;
  SPIFFS_Image& image = g_loaded_images[path];
  // SPIFFS_ImageReader's API takes char*; our public API is const char*.
  auto load_result = reader.loadBMP(const_cast<char*>(path), image);
  if (load_result != IMAGE_SUCCESS) {
    g_loaded_images.erase(path);
    return false;
  }
  auto format = image.getFormat();
  if (format != IMAGE_16 && format != IMAGE_1) {
    g_loaded_images.erase(path);
    return false;
  }
  return true;
}

void DrawBMP(Adafruit_GFX* gfx, const char* path, int16_t x, int16_t y,
             bool delete_after_draw, uint16_t monochrome_color,
             int32_t transparent_color) {
  if (!gfx) return;
  if (!PreloadImage(path)) return;

  auto& loaded_image = g_loaded_images.at(path);
  void* raw_canvas = loaded_image.getCanvas();
  if (raw_canvas == nullptr) return;

  if (loaded_image.getFormat() == IMAGE_16) {
    auto* image_canvas = static_cast<GFXcanvas16*>(raw_canvas);
    if (transparent_color < 0) {
      gfx->drawRGBBitmap(x, y, image_canvas->getBuffer(), image_canvas->width(),
                         image_canvas->height());
    } else {
      // Per-pixel draw, skipping near-magenta pixels (the sprite transparency
      // key). The image is already RGB565 here, so test the channels directly:
      // bright red (R5) + bright blue (B5) + low green (G6). A tolerance test is
      // used because lossy sprite encoders spread the key over several values.
      const uint16_t* buf = image_canvas->getBuffer();
      const int16_t iw = image_canvas->width();
      const int16_t ih = image_canvas->height();
      for (int16_t row = 0; row < ih; ++row) {
        for (int16_t col = 0; col < iw; ++col) {
          uint16_t px = buf[row * iw + col];
          uint8_t r5 = (px >> 11) & 0x1F;
          uint8_t g6 = (px >> 5) & 0x3F;
          uint8_t b5 = px & 0x1F;
          bool isKey = r5 >= 25 && b5 >= 25 && g6 <= 18;  // ~magenta
          if (!isKey) gfx->drawPixel(x + col, y + row, px);
        }
      }
    }
  } else if (loaded_image.getFormat() == IMAGE_1) {
    auto* image_canvas = static_cast<GFXcanvas1*>(raw_canvas);
    gfx->drawBitmap(x, y, image_canvas->getBuffer(), image_canvas->width(),
                    image_canvas->height(), monochrome_color);
  }

  if (delete_after_draw) {
    g_loaded_images.erase(path);
  }
}

}  // namespace display
