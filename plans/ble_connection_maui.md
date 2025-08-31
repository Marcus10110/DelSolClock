# BLE Connection Architecture for MAUI App (Revised)

## Overview
Design a clean, simple BLE connection architecture for the Del Sol Clock MAUI application. The old Xamarin implementation provides insights into the device interface. This revised architecture prioritizes simplicity and explicit user control over automatic behavior.

## Key Observations from Legacy Implementation
- Uses Plugin.BLE library (same as new project: v3.2.0-beta.1)
- Device uses multiple services (Vehicle, Firmware, Location, Navigation)  
- Status characteristic provides comma-separated boolean values: "0,0,0,1,1"
- Battery characteristic provides 4-byte float value
- Firmware update uses 512-byte chunks with response validation
- BLE auto-reconnection is unreliable across platforms - needs explicit management
- Uses characteristic notifications for real-time updates

## Architecture Design Principles

### 1. **Explicit Over Automatic**
- UI controls device discovery and connection
- No surprise auto-connections
- Clear user feedback for all operations

### 2. **Simple Over Complex** 
- Concrete classes instead of interfaces where possible
- Automatic notification management (start on connect, stop on disconnect)
- Minimal API surface

### 3. **User-Controlled Discovery**
- Separate discovery from connection
- Show available devices before connection attempt
- Support manual refresh of device list

## Core Classes

#### 1. `DelSolDevice` - Main Class (Simplified)
```csharp
public class DelSolDevice : IDisposable
{
    private BleConnectionManager _connectionManager;
    private VehicleService _vehicleService;
    private FirmwareService _firmwareService;
    private readonly ILogger<DelSolDevice> _logger;
    
    // Public API
    public bool IsConnected => _connectionManager?.IsConnected ?? false;
    public string ConnectedDeviceName => _connectionManager?.ConnectedDeviceName;
    public string FirmwareVersion { get; private set; }
    
    // Events
    public event EventHandler<ConnectionStatusChangedEventArgs> ConnectionStatusChanged;
    public event EventHandler<VehicleStatusChangedEventArgs> VehicleStatusChanged;
    public event EventHandler<BatteryStatusChangedEventArgs> BatteryStatusChanged;
    public event EventHandler<FirmwareUpdateProgressEventArgs> FirmwareUpdateProgress;
    
    // Methods
    public async Task<bool> InitializeAsync();
    public async Task<List<DiscoveredDevice>> ScanForDevicesAsync(TimeSpan timeout = default);
    public async Task<List<DiscoveredDevice>> GetKnownDevicesAsync();
    public async Task<bool> ConnectAsync(DiscoveredDevice device);
    public async Task DisconnectAsync();
    public async Task<VehicleStatus> GetVehicleStatusAsync();
    public async Task<float> GetBatteryVoltageAsync();
    public async Task<bool> UpdateFirmwareAsync(Stream firmwareStream, CancellationToken cancellationToken = default);
    
    void Dispose();
}
```

#### 2. `BleConnectionManager` - Connection Management
```csharp
public class BleConnectionManager : IDisposable
{
    private IAdapter _adapter;
    private IDevice _connectedDevice;
    private readonly ILogger<BleConnectionManager> _logger;
    
    public bool IsConnected => _connectedDevice?.State == DeviceState.Connected;
    public string ConnectedDeviceName => _connectedDevice?.Name;
    public IDevice ConnectedDevice => _connectedDevice;
    
    public event EventHandler<ConnectionStatusChangedEventArgs> ConnectionStatusChanged;
    
    public async Task<bool> InitializeAsync()
    {
        // Check BLE permissions for current platform
        // Initialize Plugin.BLE adapter
        // Subscribe to BLE state changes
        // Set up device connection/disconnection event handlers
    }
    
    public async Task<List<DiscoveredDevice>> ScanForDevicesAsync(Guid serviceId, TimeSpan timeout)
    {
        // Scan for devices advertising the specified service
        // Return list of discovered devices with metadata
        // Don't automatically connect - let UI decide
    }
    
    public async Task<List<DiscoveredDevice>> GetKnownDevicesAsync(Guid serviceId)
    {
        // Get system connected/paired devices with specific service
        // Check which ones are currently in range
        // Return list with connection status
    }
    
    public async Task<bool> ConnectAsync(DiscoveredDevice device)
    {
        // Connect to specific device
        // Validate connection and required services
        // Set up connection monitoring
    }
    
    public async Task DisconnectAsync()
    {
        // Cleanly disconnect current device
        // Clean up resources
    }
    
    public async Task<IService> GetServiceAsync(Guid serviceId)
    {
        // Get service from connected device
        // Throw if not connected or service not found
    }
}
```

#### 3. `DiscoveredDevice` - Device Information
```csharp
public class DiscoveredDevice
{
    public string Name { get; set; }
    public string Id { get; set; }
    public Guid DeviceId { get; set; }
    public bool IsKnownDevice { get; set; }  // Previously paired
    public int SignalStrength { get; set; }
    public DateTime LastSeen { get; set; }
    
    // Internal reference to Plugin.BLE device
    internal IDevice BleDevice { get; set; }
    
    public override string ToString() => $"{Name} ({(IsKnownDevice ? "Known" : "New")})";
}
```

#### 4. `VehicleService` - Vehicle Data Management
```csharp
public class VehicleService : IDisposable
{
    private ICharacteristic _statusCharacteristic;
    private ICharacteristic _batteryCharacteristic;
    private readonly ILogger<VehicleService> _logger;
    
    public event EventHandler<VehicleStatusChangedEventArgs> StatusChanged;
    public event EventHandler<BatteryStatusChangedEventArgs> BatteryChanged;
    
    public async Task<bool> InitializeAsync(IDevice device)
    {
        // Get vehicle service from device
        // Discover status and battery characteristics
        // Automatically start notifications (simplified)
        await _statusCharacteristic.StartUpdatesAsync();
        // Set up event handlers
    }
    
    public async Task<VehicleStatus> GetStatusAsync()
    {
        if (_statusCharacteristic == null) return null;
        
        // Read current value from characteristic
        var value = await _statusCharacteristic.ReadAsync();
        return ParseStatusBytes(value);
    }
    
    public async Task<float> GetBatteryVoltageAsync()
    {
        if (_batteryCharacteristic == null) return 0f;
        
        // Read 4-byte float from characteristic
        var bytes = await _batteryCharacteristic.ReadAsync();
        return BitConverter.ToSingle(bytes, 0);
    }
    
    private VehicleStatus ParseStatusBytes(byte[] value)
    {
        // Parse "0,0,0,1,1" format into VehicleStatus
        var statusString = Encoding.UTF8.GetString(value);
        var parts = statusString.Split(',');
        
        if (parts.Length != 5) return null;
        
        return new VehicleStatus
        {
            RearWindowDown = parts[0] == "1",
            TrunkOpen = parts[1] == "1", 
            ConvertibleRoofDown = parts[2] == "1",
            IgnitionOn = parts[3] == "1",
            HeadlightsOn = parts[4] == "1",
            LastUpdated = DateTime.Now
        };
    }
    
    private void OnStatusUpdated(object sender, CharacteristicUpdatedEventArgs e)
    {
        var status = ParseStatusBytes(e.Characteristic.Value);
        StatusChanged?.Invoke(this, new VehicleStatusChangedEventArgs { Status = status });
    }
    
    public void Dispose()
    {
        // Automatically stop notifications and clean up
        if (_statusCharacteristic != null)
        {
            _statusCharacteristic.ValueUpdated -= OnStatusUpdated;
            _statusCharacteristic.StopUpdatesAsync().Wait();
        }
    }
}
```

#### 5. `FirmwareService` - Firmware Management  
```csharp
public class FirmwareService : IDisposable
{
    private ICharacteristic _writeCharacteristic;
    private ICharacteristic _versionCharacteristic;
    private TaskCompletionSource<string> _writeResponseTask;
    private readonly ILogger<FirmwareService> _logger;
    
    public event EventHandler<FirmwareUpdateProgressEventArgs> UpdateProgressChanged;
    
    public async Task<bool> InitializeAsync(IDevice device)
    {
        // Get firmware service
        // Discover characteristics
        // Set up write response notifications
        await _writeCharacteristic.StartUpdatesAsync();
    }
    
    public async Task<string> GetVersionAsync()
    {
        if (_versionCharacteristic == null) return "Unknown";
        
        var bytes = await _versionCharacteristic.ReadAsync();
        return Encoding.UTF8.GetString(bytes);
    }
    
    public async Task<bool> UpdateFirmwareAsync(Stream firmwareStream, CancellationToken cancellationToken = default)
    {
        const int chunkSize = 512;
        var buffer = new byte[chunkSize];
        long totalBytes = firmwareStream.Length;
        long bytesWritten = 0;
        var startTime = DateTime.Now;
        
        while (!cancellationToken.IsCancellationRequested)
        {
            int bytesRead = await firmwareStream.ReadAsync(buffer, 0, chunkSize, cancellationToken);
            if (bytesRead == 0) break;
            
            // Prepare chunk to write
            var chunk = new byte[bytesRead];
            Array.Copy(buffer, chunk, bytesRead);
            
            // Write and wait for response
            var response = await WriteChunkAndWaitForResponseAsync(chunk, TimeSpan.FromSeconds(10));
            bytesWritten += bytesRead;
            
            // Calculate and report progress
            var progress = new FirmwareUpdateProgressEventArgs
            {
                PercentComplete = (double)bytesWritten / totalBytes * 100,
                SpeedBytesPerSecond = bytesWritten / (DateTime.Now - startTime).TotalSeconds,
                EstimatedTimeRemaining = TimeSpan.FromSeconds((totalBytes - bytesWritten) / 
                    (bytesWritten / (DateTime.Now - startTime).TotalSeconds))
            };
            UpdateProgressChanged?.Invoke(this, progress);
            
            // Handle response
            if (response == "success") return true;
            if (response == "error") return false;
            if (response != "continue") 
                throw new Exception($"Unexpected response: {response}");
        }
        
        // Handle edge case: exact 512-byte multiple needs zero-length write
        if (totalBytes % chunkSize == 0)
        {
            var finalResponse = await WriteChunkAndWaitForResponseAsync(new byte[0], TimeSpan.FromSeconds(10));
            return finalResponse == "success";
        }
        
        return false;
    }
    
    private async Task<string> WriteChunkAndWaitForResponseAsync(byte[] chunk, TimeSpan timeout)
    {
        _writeResponseTask = new TaskCompletionSource<string>();
        
        var success = await _writeCharacteristic.WriteAsync(chunk);
        if (!success) throw new Exception("Write failed");
        
        using (var cts = new CancellationTokenSource(timeout))
        {
            cts.Token.Register(() => _writeResponseTask.TrySetException(new TimeoutException()));
            return await _writeResponseTask.Task;
        }
    }
    
    private void OnWriteResponseReceived(object sender, CharacteristicUpdatedEventArgs e)
    {
        var response = Encoding.UTF8.GetString(e.Characteristic.Value);
        _writeResponseTask?.TrySetResult(response);
    }
    
    public void Dispose()
    {
        if (_writeCharacteristic != null)
        {
            _writeCharacteristic.ValueUpdated -= OnWriteResponseReceived;
            _writeCharacteristic.StopUpdatesAsync().Wait();
        }
    }
}
```

### Data Models

#### 6. `VehicleStatus` - Vehicle State Data
```csharp
public class VehicleStatus
{
    public bool RearWindowDown { get; set; }
    public bool TrunkOpen { get; set; }
    public bool ConvertibleRoofDown { get; set; }
    public bool IgnitionOn { get; set; }
    public bool HeadlightsOn { get; set; }
    
    public DateTime LastUpdated { get; set; }
    
    public override string ToString()
    {
        var items = new List<string>();
        items.Add($"Window: {(RearWindowDown ? "DOWN" : "UP")}");
        items.Add($"Trunk: {(TrunkOpen ? "OPEN" : "CLOSED")}");
        items.Add($"Roof: {(ConvertibleRoofDown ? "DOWN" : "UP")}");
        items.Add($"Ignition: {(IgnitionOn ? "ON" : "OFF")}");
        items.Add($"Lights: {(HeadlightsOn ? "ON" : "OFF")}");
        return string.Join(", ", items);
    }
}
```

#### 7. Event Args Classes
```csharp
public class ConnectionStatusChangedEventArgs : EventArgs
{
    public bool IsConnected { get; set; }
    public string DeviceName { get; set; }
    public string ErrorMessage { get; set; }
}

public class VehicleStatusChangedEventArgs : EventArgs
{
    public VehicleStatus Status { get; set; }
}

public class BatteryStatusChangedEventArgs : EventArgs
{
    public float Voltage { get; set; }
    public DateTime Timestamp { get; set; }
}

public class FirmwareUpdateProgressEventArgs : EventArgs
{
    public double PercentComplete { get; set; }
    public double SpeedBytesPerSecond { get; set; }
    public TimeSpan EstimatedTimeRemaining { get; set; }
    public bool IsCompleted { get; set; }
    public string ErrorMessage { get; set; }
}
```

## Implementation Strategy

### Phase 1.1: BLE Service Implementation (3-4 days)

**Day 1: Core Infrastructure**
1. Implement `BleConnectionManager` with basic Plugin.BLE integration
2. Add permission handling for Windows/Android/iOS
3. Implement explicit device discovery (scan and known devices)
4. Add proper disposal and cleanup

**Day 2: Connection Management**  
1. Implement user-controlled connection to specific devices
2. Add connection state management and error handling
3. Implement service discovery and validation
4. Add comprehensive logging

**Day 3: Service Layer Foundation**
1. Implement `VehicleService` basic structure
2. Add characteristic discovery and initialization
3. Implement basic read operations for status and battery
4. Add automatic notification subscription logic

**Day 4: Integration and Testing**
1. Integrate all components into `DelSolDevice`
2. Add comprehensive error handling
3. Test connection/disconnection scenarios
4. Validate service discovery and characteristic access

### Phase 1.2: Characteristic Reading Infrastructure (2-3 days)

**Day 1: Status Characteristic**
1. Implement status string parsing ("0,0,0,1,1" format)
2. Add notification handling for real-time updates
3. Implement `VehicleStatus` data model
4. Add status change event propagation

**Day 2: Battery and Firmware Characteristics**
1. Implement battery voltage reading (4-byte float)
2. Add firmware version reading
3. Implement proper data conversion and validation
4. Add battery change notifications (if available)

**Day 3: Event System and Polish**
1. Implement comprehensive event system
2. Add proper error handling for all characteristic operations
3. Implement data validation and error recovery
4. Test with actual device if available

## Key Design Decisions (Revised)

### 1. **Explicit User Control**
- `BleConnectionManager` separates discovery from connection
- UI decides when to scan, what devices to show, and when to connect
- No automatic or surprise connections

### 2. **Simplified Architecture**  
- Concrete classes instead of interfaces where appropriate
- Automatic notification management (start on initialize, stop on dispose)
- Minimal API surface reduces complexity

### 3. **Event-Driven Updates**
- Real-time updates via events rather than polling
- Allows UI to react immediately to device changes
- Simple event model with clear event args

### 4. **Resource Management**
- Proper IDisposable implementation throughout
- Automatic cleanup of BLE resources and event subscriptions
- Memory-efficient stream handling for firmware updates

### 5. **User-Centered Discovery**
- Separate methods for scanning vs. known devices
- Rich device information (name, signal strength, known status)
- Manual refresh model supports UI-controlled periodic updates

## Constants and Configuration

```csharp
public static class DelSolConstants
{
    // Service UUIDs
    public static readonly Guid VehicleServiceGuid = new("8fb88487-73cf-4cce-b495-505a4b54b802");
    public static readonly Guid FirmwareServiceGuid = new("69da0f2b-43a4-4c2a-b01d-0f11564c732b");
    public static readonly Guid LocationServiceGuid = new("61d33c70-e3cd-4b31-90d8-a6e14162fffd");
    public static readonly Guid NavigationServiceGuid = new("77f5d2b5-efa1-4d55-b14a-cc92b72708a0");
    
    // Vehicle Service Characteristics
    public static readonly Guid StatusCharacteristicGuid = new("40d527f5-3204-44a2-a4ee-d8d3c16f970e");
    public static readonly Guid BatteryCharacteristicGuid = new("5c258bb8-91fc-43bb-8944-b83d0edc9b43");
    
    // Firmware Service Characteristics  
    public static readonly Guid FirmwareWriteCharacteristicGuid = new("7efc013a-37b7-44da-8e1c-06e28256d83b");
    public static readonly Guid FirmwareVersionCharacteristicGuid = new("a5c0d67a-9576-47ea-85c6-0347f8473cf3");
    
    // Configuration
    public const int FirmwareChunkSize = 512;
    public const int DefaultTimeoutSeconds = 10;
    public static readonly TimeSpan DefaultScanTimeout = TimeSpan.FromSeconds(10);
}
```

## Simplified Dependency Injection Setup

```csharp
// In MauiProgram.cs - Simplified without interfaces
public static MauiApp CreateMauiApp()
{
    var builder = MauiApp.CreateBuilder();
    builder
        .UseMauiApp<App>()
        .ConfigureFonts(fonts => fonts.AddFont("OpenSans-Regular.ttf", "OpenSansRegular"));

    // Register BLE services (concrete classes)
    builder.Services.AddSingleton<DelSolDevice>();
    
    // Register pages
    builder.Services.AddTransient<StatusPage>();
    builder.Services.AddTransient<UpdatePage>();
    builder.Services.AddTransient<LogPage>();
    
    return builder.Build();
}
```

## Example UI Usage Pattern

```csharp
public partial class StatusPage : ContentPage
{
    private readonly DelSolDevice _delSolDevice;
    
    public StatusPage(DelSolDevice delSolDevice)
    {
        InitializeComponent();
        _delSolDevice = delSolDevice;
        _delSolDevice.VehicleStatusChanged += OnVehicleStatusChanged;
        _delSolDevice.ConnectionStatusChanged += OnConnectionStatusChanged;
    }
    
    private async void OnScanButtonClicked(object sender, EventArgs e)
    {
        try 
        {
            ScanButton.IsEnabled = false;
            StatusLabel.Text = "Scanning...";
            
            // Show known devices first
            var knownDevices = await _delSolDevice.GetKnownDevicesAsync();
            if (knownDevices.Any())
            {
                DeviceList.ItemsSource = knownDevices;
                return;
            }
            
            // If no known devices, scan for new ones
            var discoveredDevices = await _delSolDevice.ScanForDevicesAsync(TimeSpan.FromSeconds(10));
            DeviceList.ItemsSource = discoveredDevices;
        }
        finally
        {
            ScanButton.IsEnabled = true;
            StatusLabel.Text = "Ready";
        }
    }
    
    private async void OnConnectButtonClicked(object sender, EventArgs e)
    {
        if (sender is Button button && button.CommandParameter is DiscoveredDevice device)
        {
            var success = await _delSolDevice.ConnectAsync(device);
            if (!success)
            {
                await DisplayAlert("Error", "Failed to connect to device", "OK");
            }
        }
    }
}
```

This revised architecture prioritizes simplicity and user control while maintaining clean separation of concerns and extensibility for future features.

## Recommended File/Folder Structure

```
DelSolClockAppMaui/
├── Services/
│   ├── DelSolDevice.cs                    // Main orchestration class
│   ├── BleConnectionManager.cs            // BLE connection management
│   ├── VehicleService.cs                  // Vehicle status & battery
│   ├── FirmwareService.cs                 // Firmware updates
│   └── DelSolConstants.cs                 // UUIDs and configuration
├── Models/
│   ├── VehicleStatus.cs                   // Vehicle state data
│   ├── DiscoveredDevice.cs                // Device discovery info
│   └── EventArgs/
│       ├── ConnectionStatusChangedEventArgs.cs
│       ├── VehicleStatusChangedEventArgs.cs
│       ├── BatteryStatusChangedEventArgs.cs
│       └── FirmwareUpdateProgressEventArgs.cs
├── Pages/
│   ├── StatusPage.xaml                    // Status display UI
│   ├── StatusPage.xaml.cs                 // Status page logic
│   ├── UpdatePage.xaml                    // Firmware update UI
│   ├── UpdatePage.xaml.cs                 // Firmware update logic
│   ├── LogPage.xaml                       // Logs display UI
│   └── LogPage.xaml.cs                    // Logs page logic
├── Components/                            // Reusable UI components
│   ├── StatusItem.xaml                    // Individual status display
│   ├── StatusItem.xaml.cs
│   ├── DeviceListItem.xaml                // Device list entry (new)
│   ├── DeviceListItem.xaml.cs
│   ├── UpdateItem.xaml                    // Firmware release item
│   └── UpdateItem.xaml.cs
├── ViewModels/                            // If using MVVM pattern
│   ├── StatusItemData.cs                  // Existing status item data
│   └── UpdateItemData.cs                  // Existing update item data
├── Resources/
│   └── Images/
│       └── icons/                         // BLE and status icons
│           ├── bluetooth.svg
│           ├── bluetooth_connected.svg    // New - connected state
│           ├── bluetooth_disconnected.svg // New - disconnected state
│           ├── battery.svg                // New - battery icon
│           ├── headlights_on.svg          // Existing
│           └── ...                        // Other status icons
├── Platforms/                             // Platform-specific code
│   ├── Android/
│   │   └── BluetoothPermissions.cs        // Android BLE permissions
│   ├── iOS/
│   │   └── BluetoothPermissions.cs        // iOS BLE permissions
│   └── Windows/
│       └── BluetoothPermissions.cs        // Windows BLE permissions
├── AppShell.xaml                          // Navigation shell
├── AppShell.xaml.cs
├── MauiProgram.cs                         // DI and logging setup
└── DelSolClockAppMaui.csproj             // Project file with Plugin.BLE
```

## Implementation Order by File

### Phase 1.1 Files (Days 1-4):

**Day 1: Core Infrastructure**
1. `Services/DelSolConstants.cs` - UUIDs and configuration constants
2. `Models/DiscoveredDevice.cs` - Device discovery data model
3. `Models/EventArgs/ConnectionStatusChangedEventArgs.cs` - Connection events
4. `Services/BleConnectionManager.cs` - Basic BLE connection (discovery/connect methods)

**Day 2: Connection Management**
1. Complete `Services/BleConnectionManager.cs` - Add error handling, logging
2. `Platforms/*/BluetoothPermissions.cs` - Platform-specific permissions (if needed)
3. Test basic device discovery and connection

**Day 3: Service Layer**
1. `Models/VehicleStatus.cs` - Vehicle status data model
2. `Models/EventArgs/VehicleStatusChangedEventArgs.cs` - Vehicle events
3. `Models/EventArgs/BatteryStatusChangedEventArgs.cs` - Battery events
4. `Services/VehicleService.cs` - Vehicle characteristic handling

**Day 4: Integration**
1. `Services/DelSolDevice.cs` - Main orchestration class
2. Update `MauiProgram.cs` - Add DI registration and logging
3. Integration testing

### Phase 1.2 Files (Days 5-7):

**Day 5: Status Implementation**
1. Complete parsing logic in `Services/VehicleService.cs`
2. Test status characteristic reading and notifications
3. Update `Pages/StatusPage.xaml.cs` - Connect to new BLE service

**Day 6: Firmware Characteristics**
1. `Models/EventArgs/FirmwareUpdateProgressEventArgs.cs` - Firmware events
2. `Services/FirmwareService.cs` - Basic firmware version reading
3. Update `Pages/UpdatePage.xaml.cs` - Connect to new BLE service

**Day 7: Polish**
1. Add comprehensive error handling across all services
2. Test event propagation and UI updates
3. Add any missing logging and validation

## File Size Estimates

- **Small files** (< 100 lines): EventArgs classes, Models, Constants
- **Medium files** (100-300 lines): BleConnectionManager, VehicleService, Page code-behind
- **Large files** (300+ lines): DelSolDevice (orchestration), FirmwareService (complex update logic)

## Notes

- **Existing files to modify**: `MauiProgram.cs`, `StatusPage.xaml.cs`, `UpdatePage.xaml.cs`
- **Platform-specific files**: Only create if Plugin.BLE doesn't handle permissions automatically
- **ViewModels folder**: Keep existing files, may add more if moving to full MVVM pattern
- **Components folder**: Existing UI components, may add DeviceListItem for device discovery UI