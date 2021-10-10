#!/bin/bash

set -e

export PATH=$PATH:deps/bin
rm -rf output 
rm -rf build
mkdir -p output

arduino-cli compile --fqbn esp32:esp32:featheresp32 DelSolClock -e

cp build/*/*.ino.bin output/