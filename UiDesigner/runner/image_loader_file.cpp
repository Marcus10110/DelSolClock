// Desktop file-backed image loader (the UiDesigner counterpart to the firmware's
// SPIFFS loader). Same display::DrawBMP / PreloadImage signatures; whichever is
// linked is used. Resolves "/foo.bmp" against ./data/foo.bmp. Adapted from the
// old UiDesigner Bmp.cpp.
#include "image_loader.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace display {
namespace {

#pragma pack(push, 1)
struct BMPHeader {
  uint16_t signature;
  uint32_t fileSize;
  uint16_t reserved1;
  uint16_t reserved2;
  uint32_t dataOffset;
};
struct BMPInfoHeader {
  uint32_t headerSize;
  int32_t width;
  int32_t height;
  uint16_t planes;
  uint16_t bitsPerPixel;
  uint32_t compression;
  uint32_t imageSize;
  int32_t xPixelsPerMeter;
  int32_t yPixelsPerMeter;
  uint32_t colorsUsed;
  uint32_t colorsImportant;
};
#pragma pack(pop)

uint16_t RGB_to_RGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Magenta transparent-key test: bright red + bright blue + low green. Tolerant
// of the near-magenta spread that lossy sprite encoders produce.
bool IsKeyColor(uint8_t r, uint8_t g, uint8_t b) {
  return r > 200 && b > 200 && g < 80;
}

std::string NormalizePath(const char* path) {
  if (!path) return "";
  std::string n = path;
  std::replace(n.begin(), n.end(), '\\', '/');
  if (!n.empty() && n[0] == '/') {
    n = "data" + n;
  } else {
    n = "data/" + n;
  }
  return n;
}

}  // namespace

bool PreloadImage(const char* /*path*/) {
  // Desktop loads from disk on demand; nothing to pre-cache.
  return true;
}

void DrawBMP(Adafruit_GFX* gfx, const char* path, int16_t x, int16_t y,
             bool /*delete_after_draw*/, uint16_t /*monochrome_color*/,
             int32_t transparent_color) {
  if (!gfx || !path) return;
  std::string p = NormalizePath(path);
  if (!fs::exists(p)) {
    std::cerr << "BMP not found: " << p << std::endl;
    return;
  }
  FILE* file = nullptr;
#ifdef _WIN32
  fopen_s(&file, p.c_str(), "rb");
#else
  file = fopen(p.c_str(), "rb");
#endif
  if (!file) return;

  BMPHeader header;
  if (fread(&header, sizeof(header), 1, file) != 1 ||
      header.signature != 0x4D42) {
    fclose(file);
    return;
  }
  BMPInfoHeader info;
  if (fread(&info, sizeof(info), 1, file) != 1) {
    fclose(file);
    return;
  }
  if (info.planes != 1 || info.compression != 0) {
    fclose(file);
    return;
  }
  if (info.bitsPerPixel != 24 && info.bitsPerPixel != 32 &&
      info.bitsPerPixel != 8) {
    fclose(file);
    return;
  }

  int32_t imgWidth = info.width;
  int32_t imgHeight = std::abs(info.height);
  bool bottomUp = info.height > 0;
  int32_t bytesPerPixel = info.bitsPerPixel / 8;
  int32_t rowStride = ((imgWidth * bytesPerPixel + 3) / 4) * 4;

  std::vector<uint32_t> palette;
  if (info.bitsPerPixel == 8) {
    palette.resize(256);
    fseek(file, sizeof(BMPHeader) + info.headerSize, SEEK_SET);
    for (int i = 0; i < 256; i++) {
      uint8_t bgra[4];
      if (fread(bgra, 4, 1, file) != 1) break;
      palette[i] = (bgra[2] << 16) | (bgra[1] << 8) | bgra[0];
    }
  }
  fseek(file, header.dataOffset, SEEK_SET);

  std::vector<uint8_t> row(rowStride);
  for (int32_t r = 0; r < imgHeight; r++) {
    if (fread(row.data(), rowStride, 1, file) != 1) break;
    int16_t destY = bottomUp ? y + (imgHeight - 1 - r) : y + r;
    for (int32_t c = 0; c < imgWidth; c++) {
      int16_t destX = x + c;
      uint8_t sr = 0, sg = 0, sb = 0;  // source 8-bit RGB
      if (info.bitsPerPixel == 24) {
        sb = row[c * 3]; sg = row[c * 3 + 1]; sr = row[c * 3 + 2];
      } else if (info.bitsPerPixel == 32) {
        sb = row[c * 4]; sg = row[c * 4 + 1]; sr = row[c * 4 + 2];
      } else {  // 8-bit indexed
        uint8_t idx = row[c];
        if (idx < palette.size()) {
          uint32_t rgb = palette[idx];
          sr = (rgb >> 16) & 0xFF; sg = (rgb >> 8) & 0xFF; sb = rgb & 0xFF;
        }
      }
      // Magenta-key transparency: skip pixels close to the key color. Lossy
      // sprite encoders spread the key over several near-magenta values, so a
      // tolerance test is needed (an exact RGB565 match misses many).
      if (transparent_color >= 0 && IsKeyColor(sr, sg, sb)) continue;
      gfx->drawPixel(destX, destY, RGB_to_RGB565(sr, sg, sb));
    }
  }
  fclose(file);
}

}  // namespace display
