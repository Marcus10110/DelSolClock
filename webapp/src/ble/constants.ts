// BLE GATT contract for the Del Sol Clock.
// Ported from DelSolClockAppMaui/Services/DelSolConstants.cs and verified
// against the firmware (firmware/src/bluetooth.cpp, ble_ota.cpp, debug_service.cpp).
//
// Web Bluetooth requires UUIDs lowercase.

export const ADVERTISED_NAME = 'Del Sol';

// Service UUIDs
export const SVC_VEHICLE = '8fb88487-73cf-4cce-b495-505a4b54b802';
export const SVC_FIRMWARE = '69da0f2b-43a4-4c2a-b01d-0f11564c732b';
export const SVC_DEBUG = '2b0caa53-e543-4bb0-8f7f-50d1c64aa0dd';
export const SVC_NAVIGATION = '77f5d2b5-efa1-4d55-b14a-cc92b72708a0';

// Vehicle service characteristics
export const CHR_STATUS = '40d527f5-3204-44a2-a4ee-d8d3c16f970e'; // R/N  ASCII CSV "rw,trunk,roof,ign,lights"
export const CHR_BATTERY = '5c258bb8-91fc-43bb-8944-b83d0edc9b43'; // R/N  float32 little-endian (volts)

// Firmware service characteristics
export const CHR_FW_WRITE = '7efc013a-37b7-44da-8e1c-06e28256d83b'; // W-NR/N  512-byte OTA chunks; notifies continue/success/error
export const CHR_FW_VERSION = 'a5c0d67a-9576-47ea-85c6-0347f8473cf3'; // R  ASCII version string

// Debug service characteristics
export const CHR_DEBUG_STATUS = '32a18dc5-fdda-4601-b5b7-dc2920ac3f37'; // R   "NOCRASH" | "CRASH:<bytes>:<chunks>"
export const CHR_DEBUG_DATA = '2969eccf-48f1-4069-a662-1ae77fe69118'; // R   [u16 LE index][<=510 bytes], pointer auto-advances
export const CHR_DEBUG_CONTROL = '65376e10-7797-435b-ac52-14ac0fab362c'; // W   "REBOOT"|"CLEAR"|"PRINT"|"ASSERT"|"ASSERT_LATER"

// Navigation service characteristic
export const CHR_NAV_ROUTE = 'b9f0a2d1-6c3e-4a8b-9d27-1f5c0e6a4b30'; // W-NR/N  512-byte route chunks; notifies continue/success/error

// Configuration
export const FIRMWARE_CHUNK_SIZE = 512;
export const OTA_NOTIFY_TIMEOUT_MS = 10_000;

// All custom services the app may touch — must be listed in requestDevice
// optionalServices, or getPrimaryService() is blocked at runtime.
export const ALL_SERVICES = [SVC_VEHICLE, SVC_FIRMWARE, SVC_DEBUG, SVC_NAVIGATION];
