# Del Sol Clock UI Product Specification

The Del Sol Clock is an accessory for the 90s Honda Del Sol, a 2 seat convertible.

The car comes standard with a in-dash clock capable of displaying 12 hour time.

The Del Sol Clock project is a hardware drop-in replacement for the in-dash clock.

This project replaces the 7-segment display with a 1.1 inch full color display, 240x136 pixels.

The display is driven by a C++ application running on an ESP32.

# Functions

The application has access to a number of inputs, which are used by its features.

## Inputs

- Headlights on/off.
- Ignition on/off
- rear window down
- trunk open
- convertible roof down
- car battery voltage (analog)
- GPS module (speed, etc.)
- 6DoF inertial measurement unit

# Screens (Essentially the same as pages)

The Del Sol Clock project has several screens:

## Splash Screen

When the car turns on, the splash screen is shown for several seconds. It shows a picture of a white Honda Del Sol, and the "OldSols" logo.

## Default Screen (Time Screen)

The default screen. This screen displays the current time in large type, using a font that looks like a 7 segment display.

It also shows AM or PM in small print.

Across the bottom, it shows the name of the current song that's playing, if one is playing.

In the top left, it shows the current speed.

In the top right, there is a head lights on/off indicator.

If bluetooth is lost, a red bluetooth icon appears on the right.

## Lights On Alarm Screen

This screen is only shown when the ignition is off, but the headlights are on.
It features a large headlights icon, and the text "LIGHTS ON"

## Bluetooth Discoverable Screen

If the device hasn't been paired to a phone yet, instead of showing the default screen, it shows the bluetooth pairing screen.
This screen shows the name of the device, "DelSolClock" and informs the user that it's bluetooth discoverable. Once connected, the default screen is displayed. This screen is only shown on boot if no device is connected. If the device disconnects, a red bluetooth icon is added to the default screen, but it does not return to the discoverable screen.

## Navigation Screen

The navigation screen shows the next step in turn-by-turn navigation.

Typical steps are "Turn left onto <street name>", "Keep right, follow signs for <> and merge onto <>", etc.

The appropriate icon is also shown.

## Notification Screen

This screen just shows the latest iPhone notification in large text and the previous 2 notifications in small text across the bottom.

## Firmware Update Screen

This screen is not selectable from the device. however if the paired mobile phone is connected, and initiates a firmware update, this screen pops up and shows a progress bar for the firmware update. All it shows is the label "Firmware Update" and the progress bar.

## Quarter Mile Timer

This screen is used to allow the driver to time their quarter mile.

It's manually initiated and has 4 phases.

All four phases show the title "Quarter Mile"

The phases are:

Phase 1: "Press M to Start". The user presses the minute button on the clock to start the quarter mile timer.
Phase 2: "Waiting for Launch..." This screen shows the forward direction accelerometer data (0g when stationary.) The quarter mile timer will automatically activate when sufficient G forces are detected. It simply shows the current value (0.0g, etc.)
Phase 3: The record phase. This screen shows the time elapsed since the start, the current forward G force, and the distanced traveled (in miles) Below that is a progress bar, displaying the distance out of 0.25 miles traveled.
Phase 4: The result phase. This screen shows the total quarter mile time, the 0-60 mph time (if 60 mph was reached, otherwise "--"), the max speed, and the max acceleration measured.

## Status page

This page shows several key/value pairs in a grid. Latitude, Longitude, Speed, Heading, and Battery Voltage to start with.

# Interface

The user can press the "H" (hour) button to cycle between the screens. If the current screen supports interaction, the "M" (minute) button is used. The clock as a reset button, which acts as a full hardware reset.
