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
             bool delete_after_draw, uint16_t monochrome_color) {
  if (!gfx) return;
  if (!PreloadImage(path)) return;

  auto& loaded_image = g_loaded_images.at(path);
  void* raw_canvas = loaded_image.getCanvas();
  if (raw_canvas == nullptr) return;

  if (loaded_image.getFormat() == IMAGE_16) {
    auto* image_canvas = static_cast<GFXcanvas16*>(raw_canvas);
    gfx->drawRGBBitmap(x, y, image_canvas->getBuffer(), image_canvas->width(),
                       image_canvas->height());
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
