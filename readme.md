# Del Sol Clock

This is a project to replace the clock in my car with an ESP32 and a small display.

The replacement clock screen will display time, the currently playing music, and iOS notifications.

Bluetooth low energy is used to communicate with a phone.
Note, some servies should support any BLE phone, for example the current time service. However the Apple Media Service and the notification service only support iOS.

## Features

### Apple Media Service

This displays the current playback information from the iPhone. It doesn't currently support playback controls (e.g. next/previous track).

[Apple Documentation.](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Specification/Specification.html#//apple_ref/doc/uid/TP40014716-CH1-SW7)

Special thanks to the following examples. As far as I can tell, this hasn't been implemented using the [Arduino ESP32 board library](https://github.com/espressif/arduino-esp32) before.

- [James Hudson's Apple Notification Center Service](https://github.com/Smartphone-Companions/ESP32-ANCS-Notifications/blob/master/src/ancs_ble_client.cpp) This example finally showed how to solicit services using the Arduino ESP32 library, which is not built-in.
- [John Park's Adafruit Guide to the Apple Media Service Display](https://learn.adafruit.com/now-playing-bluetooth-apple-media-service-display/apple-music-service) This was the first working example I could find. Written in CircutPython.
- [Moddable's Apple Media Service example](https://github.com/Moddable-OpenSource/moddable/blob/6d7f33f8f318663bd5ba8d6ca6536443d42e68ea/modules/network/ble/ams-client/amsclient.js#L129) This was another good example, written in javascript.

### Current Time Service

This gets the current time.

[Current Time Service Documentation](https://www.bluetooth.com/specifications/specs/current-time-service-1-1/)

Note, the documentation is split over several PDFs. See CurrentTimeService.cpp for more documentation links.
