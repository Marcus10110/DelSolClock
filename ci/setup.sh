#!/bin/bash

set -e

mkdir -p $HOME/Arduino
mkdir -p $HOME/Arduino/libraries
mkdir -p deps
pushd deps
export PATH=$PATH:$PWD/bin
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
popd
arduino-cli config init
arduino-cli core update-index --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json
arduino-cli core install esp32:esp32@1.0.6 --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json

arduino-cli lib install TinyGPSPlus
arduino-cli lib install "Adafruit ST7735 and ST7789 Library"
arduino-cli lib install "SPIFFS ImageReader Library"
