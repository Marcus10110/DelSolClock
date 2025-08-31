I am working on a .NET maui application that I want to target windows desktop, iOS and Android.

This is a companion app for a bluetooth LE device I am working on, which is a small display installed in a car (specifically a Honda Del Sol).

The app should be simple, and have the following features.

- Bluetooth LE discovery and pairing. The app should have a feature that allows you to scan for bluetooth LE devices and connect. The device advertises a specific service ID, and we can filter for devices that offer this service.

- Current status Screen.
  This should show if our device is currently paired or not. If it is paired, it should show values from a few characteristics that the device exposes.
  The values it should show are:

  - Battery voltage
  - Rear Window up or down
  - Trunk open or closed
  - Convertible roof up or down
  - Car ignition on or off
  - Headlights on or off

  This screen may be extended later to include values like current screen, longitude and latitude, current acceleration data (XYZ) and more.

- Firmware screen.
  This screen is used for firmware updates. Firmware updates are published as releases to the a public github repository. This page uses the github API to list all releases.
  When the user selects a release, they can install it (if the device is connected over BLE) or they can click a button to open the release page in their default browser. (outside of the app)
  For each release, the release title, description, and creation date are shown.
  If the device is currently paired, the current FW version should be shown at the top. Also, if the FW version matches any entries in the list of available firmware, that item should be tagged as "installed"

When a firmware update is initiated, the associated asset file is downloaded into memory, and written to a specific BLE characteristic 512 bytes at a time. The progress is reported with a progress bar. A cancel button allows the operation to be aborted.
If the FW happens to be an exact multiple of 512 bytes, then the app needs to write a zero-length value at the end to trigger the FW update. otherwise the update triggers on the final, non-512 byte write.
The characteristic updates itself after each write, and will contain the string "success", "continue", or "error" after each buffer is received by the device. The app continues to upload if it sees "continue", and reports success or failure once one of the other values appears. Also, if the entire firmware image is uploaded but the final response is still "continue", an error is reported to the user. Also if an upload times out, the operation is canceled and an error is reported. If the user cancels, no more messages are sent to the device. (likely to change later)

- Logs screen.

The device generates logs. Ideally, if the app is connected to the device, the app should be able to show the logs.
The firmware does not currently support this, but we could easily add either (A) a circular buffer on the device that the app can read from, or (B) a characteristic that notifies when new logs are added, or something else. Recomendations welcome.
This screen should show logs, which look like this:

```
[7786] [T] [src/main.cpp:106] setup, bluetooth service...
[7786] [D] [src/bluetooth.cpp:365] connected!
```

This screen should have some way to save these logs to view later. it could be as simple as just having a save button that allows the user to pick a file name (default should be human readable date & time stamp) and save it. Doesn't really matter where as long as the file can be found later when say attaching it to an email.

I have a new .NET MAUI application in the DelSolClockAppMaui directory, and an older Xamarin app in the DelSolClockApp directory. The firmware is in the firmware directory.

The new application is bare bones and has little to no functionality implemented yet.

The old app is considerably more complete, but was never finished.

The scope of this project is to implement the new .NET maui application, and test it from windows desktop, on a machine that has a BLE adapter. Once working, I'll setup a mac for iOS deployment.

UUIDs, services, and characteristics.

The main service UUID for the device, included in advertisement.
#define DELSOL_VEHICLE_SERVICE_UUID "8fb88487-73cf-4cce-b495-505a4b54b802"

Read/write/notify characteristic for general device status, w/ descriptor BLE2902.
provides an array of 5 bools, encoded in a string like so: "0,0,0,1,1"
these represent 5 booleans:
rear window down, trunk open, convertible roof down, car ignition on, headlights on.
#define DELSOL_STATUS_CHARACTERISTIC_UUID "40d527f5-3204-44a2-a4ee-d8d3c16f970e"

This characteristic publishes the car's battery voltage, as a 4 byte float.
#define DELSOL_BATTERY_CHARACTERISTIC_UUID "5c258bb8-91fc-43bb-8944-b83d0edc9b43"

// this is the service for updating the device firmware.
#define FIRMWARE_UPDATE_SERVICE_UUID "69da0f2b-43a4-4c2a-b01d-0f11564c732b"

notify/write_NR characteristic.
the app needs to write firmware here up to 512 bytes at a time during a FW update.
it will set and notify the value "success", "continue", or "error" after each buffer is received by the device.
#define FIRMWARE_UPDATE_WRITE_CHARACTERISTIC_UUID "7efc013a-37b7-44da-8e1c-06e28256d83b"

read only descriptor. publishes a fixed string, the version number of the currently installed FW.
#define FIRMWARE_UPDATE_VERSION_CHARACTERISTIC_UUID "a5c0d67a-9576-47ea-85c6-0347f8473cf3"
