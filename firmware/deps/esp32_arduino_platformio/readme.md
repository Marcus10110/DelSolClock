# Building ESP32 Arduino Libs for PlatformIO.

In order to use the 32Khz external crystal with the ESP32, you need to re-build the ESP32 Arduino libraries and bootloader with `CONFIG_RTC_CLK_SRC_EXT_CRYS` enabled.

## Background

Libraries & Tools involved:

- ESP32 IDF. This is the official development framework for the ESP32 chip from espressif.
- arduino-esp32. This is the Arduino core for the ESP32 chip, provided by espressif, which provides the Arduino API and libraries.
- esp32-arduino-lib-builder. This is a utility provided by espressif to build the Arduino libraries for the ESP32 chip, which is used by the ESP32 IDF.
- PlatformIO. This is a cross-platform IDE and build system for embedded systems, which supports the ESP32 and Arduino.

Unfortunately, at the time of writing, PlatformIO does not support the latest versions of arduino-esp32, so older versions are needed to use PlatformIO. If you're not using platformIO, you can likely use the latest versions of everything. Specifically, arduino-esp32 3.x is out now, but PlatformIO only supports 2.x.

## Version Details

[platformio/espressif32](https://registry.platformio.org/platforms/platformio/espressif32)

- This is the official PlatformIO platform for the ESP32, which includes the ESP32 IDF and arduino-esp32.
- Current version: 6.11.0
- uses arduino-esp32 2.0.17 (through framework-arduinoespressif32 v3.2, very confusing versioning here.)

[arduino-esp32](https://github.com/espressif/arduino-esp32)

- This is the official Arduino core for the ESP32 chip, provided by espressif.
- The latest is 3.2, however we're stuck on 2.0.17 for now.
- 2.0.17 depends on IDF 4.4.7

[esp-idf](https://github.com/espressif/esp-idf)

- This is the official development framework for the ESP32 chip from espressif.
- The latest version is 5.4.1, however we're stuck on 4.4.7 for now.

[esp32-arduino-lib-builder](https://github.com/espressif/esp32-arduino-lib-builder)

- This is a utility provided by espressif to build the Arduino libraries for the ESP32 chip, which is used by the ESP32 IDF.
- Releases on this repo appear to be informal. There are tags and branches for the matching IDF version.
- the latest release w/ tag is for IDF 5.5.
- We'll use the branch named `release/v4.4`.

## Build Summary

The basic build process is as follows. Assume a Linux environment, since we'll be building all of this in a Docker container.

Clone esp32-arduino-lib-builder, checkout branch `release/v4.4`.

Add a new defconfig file which has our flags to enable the external crystal.

Run `./build.sh` a few times, building the libraries and bootloader for our targets w/ our custom configuration, specifiying the IDF branch and the arduino branch.

TBD: fix up issues with dependencies that weren't pinned to a specific version and the latest is no longer compatible.

Copy the built libraries and bootloaders back out.

Create a new PlatformIO platform directory by combining the default one and our new builds.

Create a script to use our custom bootloader instead of the default one.

configure platformio.ini to use our custom platform.

## Build Steps

From windows, make sure you have docker installed and running.

Run `./host-build.ps1`

Wait forever. The build output will be placed in `./arduino-esp32`

## Installation

The build output needs to be combined with a stock copy of PlatformIO's espressif32 platform.

I recommend continuing in a subdirectory of your platformIO project, e.g. `deps/esp32_arduino_platformio`.

First, make sure you're project is already building using the stock PlatformIO espressif32 platform.

Then, copy the contents of the `HOME_DIR/.platformio/packages/framework-arduinoespressif32` to a new directory, e.g. `deps/esp32_arduino_platformio/framework-arduinoespressif32`.

Next, delete the existing esp32 directory `framework-arduinoespressif32/tools/sdk/esp32` and replace it with the new `.arduino-esp32/tools/sdk/esp32`

### Update your platformio.ini to use the new platform

In your `platformio.ini`, you should already have the platform set, something like this:

```ini
platform = espressif32@6.10.0
```

Below that, add the following block, with a path to you modified framework-arduinoespressif32 directory:

```ini
platform_packages =
	framework-arduinoespressif32@file:///path/to/deps/esp32_arduino_platformio/framework-arduinoespressif32/
```

At this point, you should be able to build and run your project. Your project should link against the newly build libraries.

However, clock configuration is done in the bootloader, so you still need to deploy the new bootloader for the clock source changes to take effect.

### Add a script to use the custom bootloader

Create a file called `replace_bootloader.py` in the root of your project with the following contents.

Be sure to update the path to the new bootloader.bin file. The new bootloader.bin was copied to `./arduino-esp32/bootloader/bootloader.bin` during the build process. You can leave it there or copy it somewhere inside your project.

```python
Import("env")

def override_bootloader_path(source, target, env):
    custom_bootloader = "esp32_images/bootloader.bin"

    flags = env["UPLOADERFLAGS"]
    for i, flag in enumerate(flags):
        if str(flag).endswith("bootloader.bin") and i > 0:
            print(f"⚙️ Replacing bootloader: {flags[i]} → {custom_bootloader}")
            flags[i] = custom_bootloader
            break


env.AddPreAction("upload", override_bootloader_path)
```

And in your `platformio.ini`, add the following line to the `[env]` section:

```ini
extra_scripts = pre:extra_replace_bootloader.py
```

### Testing & Troubleshooting

Somewhere in your project, run the following code:

```cpp
#include "soc/rtc.h"

auto src = rtc_clk_slow_freq_get();
auto hz = rtc_clk_slow_freq_get_hz();
LOG_TRACE( "RTC_SLOW_CLK frequency: %u Hz, source: %i", hz, src );
```

If the source is 0, that indicates the old 150 kHz internal RC oscillator is being used. If the source is 1, that indicates the 32kHz external crystal is being used. 2 indicates the 8MHz internal RC oscillator is being used.

If you see "1", then it's working!

If you see "0", there are two main possibilities:

1. Either the bootloader wasn't replaced, or it was replaced with one that wasn't configured properly.
2. The crystal failed to start up properly. There are a number of reasons this could happen. With the ESP32-WROOM-32E, a 2M to 5M ohm resistor is needed in parallel with the crystal. Please review the datasheet for your specific module to understand how the crystal should be connected.

## Additional customization and troubleshooting

You can set any ESP32 IDF flags you like by adding them to the `defconfig.rtc_ext_cry` file provided.

You can also launch the docker container interactively by running `./host-build.ps1 -Interactive`. This will re-use the same container over and over, so your changes will persist. Also, if you edit the `Dockerfile`, this command will delete your old container and create a new one with the new changes.

This is very useful to exploring menuconfig.

Because this uses IDF 4.4, some of the configuration options are different than what you may see in the latest online documentation. Take a look at `arduino-esp32/tools/sdk/esp32/sdkconfig` in the output to see what options are available, and if they were set or not by default.
