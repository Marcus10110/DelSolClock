cmake_minimum_required(VERSION 3.21)
project(UIDesignerTool LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer static runtime on MSVC for simpler distribution (optional)
if(MSVC)
  string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  string(REPLACE "/MDd" "/MTd" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()

# ---------------------------------------------------------------------------
# Shared display library, built on the REAL (unmodified) Adafruit GFX.
#
# The library lives in the firmware tree at firmware/lib/display so a single
# source of truth compiles into both firmware (PlatformIO) and here. On desktop
# we add the vendored GFX + the compat shim that PlatformIO excludes, and define
# DEMO_ENABLED so the shared demo-screen registry data is included.
# ---------------------------------------------------------------------------
set(DISPLAY_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../firmware/lib/display")

add_library(display-shared
  ${DISPLAY_LIB}/vendor/Adafruit_GFX/Adafruit_GFX.cpp
  ${DISPLAY_LIB}/src/draw_helpers.cpp
  ${DISPLAY_LIB}/src/clock_screen.cpp
  ${DISPLAY_LIB}/src/simple_screens.cpp
  ${DISPLAY_LIB}/src/status_screen.cpp
  ${DISPLAY_LIB}/src/notification_screens.cpp
  ${DISPLAY_LIB}/src/gmeter_screen.cpp
  ${DISPLAY_LIB}/src/quarter_mile_screens.cpp
  ${DISPLAY_LIB}/src/demo_screens.cpp
  # image_loader_spiffs.cpp is firmware-only; the desktop file-backed loader is
  # compiled into the executables below instead.
)

target_include_directories(display-shared PUBLIC
  ${DISPLAY_LIB}/include              # shared public headers
  ${DISPLAY_LIB}/fonts                # font GFXfont headers
  ${DISPLAY_LIB}/vendor/Adafruit_GFX  # real Adafruit GFX
  ${DISPLAY_LIB}/compat               # desktop Arduino.h/Print.h shim
)

# ARDUINO>=100 routes Adafruit_GFX.h to our shim; DEMO_ENABLED includes the
# shared demo-screen data.
target_compile_definitions(display-shared PUBLIC ARDUINO=10819 DEMO_ENABLED)
target_compile_features(display-shared PUBLIC cxx_std_20)

# Executable: render every shared screen to a BMP + a composite grid.
add_executable(render_screens
  runner/main.cpp
  runner/image_loader_file.cpp
)
target_link_libraries(render_screens PRIVATE display-shared)
target_compile_features(render_screens PRIVATE cxx_std_20)

# ---------------------------------------------------------------------------
# Navigation matcher (shared with firmware) + the nav_cli test driver.
# Pure geometry/std — no GFX. Same single-source-of-truth pattern as the
# display library: the matcher source lives in firmware/lib/navigation.
# ---------------------------------------------------------------------------
set(NAV_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../firmware/lib/navigation")

add_library(navigation-shared
  ${NAV_LIB}/src/geo.cpp
  ${NAV_LIB}/src/matcher.cpp
  ${NAV_LIB}/src/route_io.cpp
  ${NAV_LIB}/src/route_codec.cpp
)
target_include_directories(navigation-shared PUBLIC ${NAV_LIB}/include)
target_compile_features(navigation-shared PUBLIC cxx_std_20)

add_executable(nav_cli ${NAV_LIB}/cli/nav_cli.cpp)
target_link_libraries(nav_cli PRIVATE navigation-shared)
target_compile_features(nav_cli PRIVATE cxx_std_20)

set(DEFAULT_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/out")
message(STATUS "Default output directory: ${DEFAULT_OUTPUT_DIR}")
