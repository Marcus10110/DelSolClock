// Minimal Arduino compatibility shim for compiling the real Adafruit GFX
// library on desktop (MSVC/clang). This file is ONLY on the desktop build's
// include path; the firmware build uses the real Arduino framework instead.
//
// Adafruit_GFX.cpp only needs: PROGMEM, pgm_read_* macros, the Print base
// class, and standard integer types. It never touches real hardware.
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <string>

#include "Print.h"

// PROGMEM is a no-op on desktop (no Harvard architecture / flash addressing).
#ifndef PROGMEM
#define PROGMEM
#endif

// Flash-read macros become plain dereferences on desktop.
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char*)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short*)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long*)(addr))
#endif
// Note: pgm_read_pointer is intentionally NOT defined here. Adafruit_GFX.cpp
// defines it itself (guarded), and defining it here first causes a C4005
// redefinition warning on MSVC.

// Arduino convenience typedefs occasionally referenced by GFX-adjacent code.
typedef uint8_t byte;
typedef bool boolean;

// PROGMEM string helper. Adafruit_GFX uses const char* directly; F() just
// passes through on desktop.
class __FlashStringHelper;
#ifndef F
#define F(string_literal) (string_literal)
#endif

// MSVC lacks the GCC/Clang byte-swap builtin used by GFXcanvas16::byteSwap().
#if defined(_MSC_VER)
#include <cstdlib>
static inline unsigned short __builtin_bswap16(unsigned short v) {
  return _byteswap_ushort(v);
}
#endif

// Minimal Arduino String. Adafruit_GFX only needs it to exist for the
// getTextBounds(const String&, ...) overload signature; we don't call it.
class String {
public:
  String() = default;
  String(const char* s) : s_(s ? s : "") {}
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
private:
  std::string s_;
};
