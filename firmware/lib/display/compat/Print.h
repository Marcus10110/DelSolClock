// Minimal Arduino Print base class for desktop builds of Adafruit GFX.
// Adafruit_GFX derives from Print and implements write(uint8_t); the print/
// println helpers below are built on top of that single virtual.
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>

class Print {
public:
  virtual ~Print() = default;

  // The one method subclasses must implement (Adafruit_GFX does).
  virtual size_t write(uint8_t) = 0;

  virtual size_t write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    while (size--) {
      if (!write(*buffer++)) break;
      n++;
    }
    return n;
  }

  size_t write(const char* str) {
    if (!str) return 0;
    return write(reinterpret_cast<const uint8_t*>(str), std::strlen(str));
  }

  size_t print(const char* s) { return write(s); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(const std::string& s) {
    return write(reinterpret_cast<const uint8_t*>(s.data()), s.size());
  }
  size_t print(int n) { return print(std::to_string(n)); }
  size_t print(unsigned int n) { return print(std::to_string(n)); }
  size_t print(long n) { return print(std::to_string(n)); }
  size_t print(unsigned long n) { return print(std::to_string(n)); }
  size_t print(double n) { return print(std::to_string(n)); }

  size_t println() { return write("\r\n"); }
  size_t println(const char* s) { return print(s) + println(); }
  size_t println(const std::string& s) { return print(s) + println(); }
  template <typename T>
  size_t println(T v) { return print(v) + println(); }
};
