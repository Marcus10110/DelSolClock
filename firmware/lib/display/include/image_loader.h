// Link-time-polymorphic image loader.
//
// The shared screen code calls display::DrawBMP(...). Exactly one
// implementation of these functions is linked per build target:
//   - firmware links src_firmware/image_loader_spiffs.cpp (SPIFFS_ImageReader)
//   - desktop  links a file-backed loader (UiDesigner)
// No runtime interface or injection — whichever is linked wins.
#pragma once

#include <Adafruit_GFX.h>
#include <cstdint>

namespace display {

// Draw a BMP at (x,y). For monochrome (1-bit) images, monochrome_color is the
// foreground color. delete_after_draw drops any cache entry after drawing
// (matches the old Display::DrawBMP semantics for one-shot large images).
// transparent_color: an RGB565 color to treat as transparent (skip those
// pixels); -1 (default) draws every pixel opaque. BMPs can't store alpha, so a
// magenta key color is the convention for sprites.
void DrawBMP(Adafruit_GFX* gfx, const char* path, int16_t x, int16_t y,
             bool delete_after_draw = false, uint16_t monochrome_color = 0xFFFF,
             int32_t transparent_color = -1);

// Preload (and cache) an image so a later DrawBMP is fast. Returns false on
// failure. (Firmware uses this for the slow splash image.)
bool PreloadImage(const char* path);

}  // namespace display
