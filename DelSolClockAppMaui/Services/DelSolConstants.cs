using System;

namespace DelSolClockAppMaui.Services;

public static class DelSolConstants
{
    // Service UUIDs
    public static readonly Guid VehicleServiceGuid = new("8fb88487-73cf-4cce-b495-505a4b54b802");
    public static readonly Guid FirmwareServiceGuid = new("69da0f2b-43a4-4c2a-b01d-0f11564c732b");
    public static readonly Guid LocationServiceGuid = new("61d33c70-e3cd-4b31-90d8-a6e14162fffd");
    public static readonly Guid NavigationServiceGuid = new("77f5d2b5-efa1-4d55-b14a-cc92b72708a0");
    public static readonly Guid DebugServiceGuid = new("2b0caa53-e543-4bb0-8f7f-50d1c64aa0dd");
    
    // Vehicle Service Characteristics
    public static readonly Guid StatusCharacteristicGuid = new("40d527f5-3204-44a2-a4ee-d8d3c16f970e");
    public static readonly Guid BatteryCharacteristicGuid = new("5c258bb8-91fc-43bb-8944-b83d0edc9b43");
    
    // Firmware Service Characteristics  
    public static readonly Guid FirmwareWriteCharacteristicGuid = new("7efc013a-37b7-44da-8e1c-06e28256d83b");
    public static readonly Guid FirmwareVersionCharacteristicGuid = new("a5c0d67a-9576-47ea-85c6-0347f8473cf3");
    
    // Debug Service Characteristics
    public static readonly Guid DebugStatusCharacteristicGuid = new("32a18dc5-fdda-4601-b5b7-dc2920ac3f37");
    public static readonly Guid DebugDataCharacteristicGuid = new("2969eccf-48f1-4069-a662-1ae77fe69118");
    public static readonly Guid DebugControlCharacteristicGuid = new("65376e10-7797-435b-ac52-14ac0fab362c");
    
    // Configuration
    public const int FirmwareChunkSize = 512;
    public const int DefaultTimeoutSeconds = 10;
    public static readonly TimeSpan DefaultScanTimeout = TimeSpan.FromSeconds(10);
    public static readonly TimeSpan DefaultConnectionTimeout = TimeSpan.FromSeconds(30);
}