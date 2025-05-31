#!/usr/bin/env bash
set -e

# we should have esp32-arduino-lib-builder checked out to the correct version to /opt/esp/lib-builder
# the workdir is set to the same location, /opt/esp/lib-builder
# our custom config file, defconfig.rtc_ext_cry, should already be installed to the correct location.

# Note, the version of esp_littlefs is not pinned, and the latest version depends on IDF 5, breaking the build.
# We need to patch tools/update-components.py to pin esp_littlefs to v1.16.4
patch -N -p1 < fix-esp-littlefs-version.patch

export EXTRA_CONFIGS="rtc_ext_cry qio 80m"
export ARDUINO_BRANCH="release/v2.x"
export ESP_IDF_BRANCH="release/v4.4"
export OUTPUT_DIRECTORY="/arduino-esp32"
export TARGET="esp32"

./build.sh -A $ARDUINO_BRANCH -I $ESP_IDF_BRANCH -t $TARGET -b build $EXTRA_CONFIGS
# I don't know what build actually produces, but there is no out folder. subsequent calls delete the build directory, but I ccache ensures future builds are fast.
./build.sh -A $ARDUINO_BRANCH -I $ESP_IDF_BRANCH -t $TARGET -b idf_libs $EXTRA_CONFIGS
# idf_libs produces an out folder, containing platform.txt and tools/sdk/esp32
./build.sh -A $ARDUINO_BRANCH -I $ESP_IDF_BRANCH -t $TARGET -b copy_bootloader $EXTRA_CONFIGS
# this  adds the tools/sdk/esp32/bin directory.

mkdir -p $OUTPUT_DIRECTORY/tools/
mkdir -p $OUTPUT_DIRECTORY/bootloader/
cp -a out/* $OUTPUT_DIRECTORY/
# this is not a standard location, but we need the *.bin version of the bootloader.
cp -a build/bootloader/bootloader.bin $OUTPUT_DIRECTORY/bootloader/