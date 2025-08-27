using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using DelSolClockAppMaui.Models;
using DelSolClockAppMaui.Models.EventArgs;

namespace DelSolClockAppMaui.Services;

public class DelSolDevice : IDisposable
{
    private BleConnectionManager? _connectionManager;
    private VehicleService? _vehicleService;
    private readonly ILogger<DelSolDevice> _logger;
    private bool _disposed = false;
    private bool _isInitialized = false;

    // Public API
    public bool IsConnected => _connectionManager?.IsConnected ?? false;
    public string? ConnectedDeviceName => _connectionManager?.ConnectedDeviceName;
    public string FirmwareVersion { get; private set; } = "Unknown";

    // Events
    public event EventHandler<ConnectionStatusChangedEventArgs>? ConnectionStatusChanged;
    public event EventHandler<VehicleStatusChangedEventArgs>? VehicleStatusChanged;
    public event EventHandler<BatteryStatusChangedEventArgs>? BatteryStatusChanged;
    public event EventHandler<FirmwareUpdateProgressEventArgs>? FirmwareUpdateProgress;

    public DelSolDevice(ILogger<DelSolDevice> logger)
    {
        _logger = logger;
    }

    public async Task<bool> InitializeAsync()
    {
        if (_isInitialized)
            return true;

        try
        {
            _logger.LogInformation("Initializing DelSol device manager");

            // Create BLE connection manager
            var bleLogger = LoggerFactory.Create(builder => builder.AddDebug()).CreateLogger<BleConnectionManager>();
            _connectionManager = new BleConnectionManager(bleLogger);

            // Initialize BLE
            var bleInitialized = await _connectionManager.InitializeAsync();
            if (!bleInitialized)
            {
                _logger.LogError("Failed to initialize BLE connection manager");
                return false;
            }

            // Subscribe to connection events
            _connectionManager.ConnectionStatusChanged += OnConnectionStatusChanged;

            // Create vehicle service
            var vehicleLogger = LoggerFactory.Create(builder => builder.AddDebug()).CreateLogger<VehicleService>();
            _vehicleService = new VehicleService(vehicleLogger);

            // Subscribe to vehicle events
            _vehicleService.StatusChanged += OnVehicleStatusChanged;
            _vehicleService.BatteryChanged += OnBatteryStatusChanged;

            _isInitialized = true;
            _logger.LogInformation("DelSol device manager initialized successfully");
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to initialize DelSol device manager");
            return false;
        }
    }

    public async Task<List<DiscoveredDevice>> ScanForDevicesAsync(TimeSpan timeout = default)
    {
        if (_connectionManager == null || !_isInitialized)
        {
            _logger.LogWarning("Device manager not initialized");
            return new List<DiscoveredDevice>();
        }

        if (timeout == default)
            timeout = DelSolConstants.DefaultScanTimeout;

        try
        {
            _logger.LogInformation("Scanning for DelSol devices");
            return await _connectionManager.ScanForDevicesAsync(DelSolConstants.VehicleServiceGuid, timeout);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error during device scan");
            return new List<DiscoveredDevice>();
        }
    }

    public async Task<List<DiscoveredDevice>> GetKnownDevicesAsync()
    {
        if (_connectionManager == null || !_isInitialized)
        {
            _logger.LogWarning("Device manager not initialized");
            return new List<DiscoveredDevice>();
        }

        try
        {
            _logger.LogInformation("Getting known DelSol devices");
            return await _connectionManager.GetKnownDevicesAsync(DelSolConstants.VehicleServiceGuid);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error getting known devices");
            return new List<DiscoveredDevice>();
        }
    }

    public async Task<bool> ConnectAsync(DiscoveredDevice device)
    {
        if (_connectionManager == null || _vehicleService == null || !_isInitialized)
        {
            _logger.LogWarning("Device manager not initialized");
            return false;
        }

        try
        {
            _logger.LogInformation("Connecting to DelSol device: {DeviceName}", device.Name);

            // Connect via BLE
            var connected = await _connectionManager.ConnectAsync(device);
            if (!connected)
            {
                _logger.LogError("Failed to establish BLE connection");
                return false;
            }

            // Initialize vehicle service
            if (_connectionManager.ConnectedDevice != null)
            {
                var serviceInitialized = await _vehicleService.InitializeAsync(_connectionManager.ConnectedDevice);
                if (!serviceInitialized)
                {
                    _logger.LogError("Failed to initialize vehicle service");
                    await _connectionManager.DisconnectAsync();
                    return false;
                }

                // Try to get firmware version
                await ReadFirmwareVersionAsync();
            }

            _logger.LogInformation("Successfully connected to DelSol device");
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error connecting to DelSol device");
            return false;
        }
    }

    public async Task DisconnectAsync()
    {
        if (_connectionManager == null)
            return;

        try
        {
            _logger.LogInformation("Disconnecting from DelSol device");

            // Dispose vehicle service to stop notifications
            _vehicleService?.Dispose();

            // Disconnect BLE
            await _connectionManager.DisconnectAsync();

            FirmwareVersion = "Unknown";
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error during disconnect");
        }
    }

    public async Task<VehicleStatus?> GetVehicleStatusAsync()
    {
        if (_vehicleService == null || !IsConnected)
        {
            _logger.LogWarning("Vehicle service not available or not connected");
            return null;
        }

        try
        {
            return await _vehicleService.GetStatusAsync();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error reading vehicle status");
            return null;
        }
    }

    public async Task<float> GetBatteryVoltageAsync()
    {
        if (_vehicleService == null || !IsConnected)
        {
            _logger.LogWarning("Vehicle service not available or not connected");
            return 0f;
        }

        try
        {
            return await _vehicleService.GetBatteryVoltageAsync();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error reading battery voltage");
            return 0f;
        }
    }

    public async Task<bool> UpdateFirmwareAsync(Stream firmwareStream, CancellationToken cancellationToken = default)
    {
        // This will be implemented later with FirmwareService
        _logger.LogWarning("Firmware update not yet implemented");
        await Task.CompletedTask;
        return false;
    }

    private async Task ReadFirmwareVersionAsync()
    {
        if (_connectionManager?.ConnectedDevice == null)
            return;

        try
        {
            var firmwareService = await _connectionManager.GetServiceAsync(DelSolConstants.FirmwareServiceGuid);
            if (firmwareService == null)
            {
                _logger.LogWarning("Firmware service not found");
                return;
            }

            var versionCharacteristic = await firmwareService.GetCharacteristicAsync(DelSolConstants.FirmwareVersionCharacteristicGuid);
            if (versionCharacteristic == null)
            {
                _logger.LogWarning("Firmware version characteristic not found");
                return;
            }

            var versionResult = await versionCharacteristic.ReadAsync();
            if (versionResult.data != null && versionResult.data.Length > 0)
            {
                FirmwareVersion = System.Text.Encoding.UTF8.GetString(versionResult.data);
                _logger.LogInformation("Read firmware version: {Version}", FirmwareVersion);
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to read firmware version");
        }
    }

    private void OnConnectionStatusChanged(object? sender, ConnectionStatusChangedEventArgs e)
    {
        _logger.LogInformation("Connection status changed: {IsConnected} - {DeviceName} - {Error}", 
            e.IsConnected, e.DeviceName, e.ErrorMessage);

        // If disconnected, clean up services
        if (!e.IsConnected)
        {
            _vehicleService?.Dispose();
            FirmwareVersion = "Unknown";
        }

        // Forward the event
        ConnectionStatusChanged?.Invoke(this, e);
    }

    private void OnVehicleStatusChanged(object? sender, VehicleStatusChangedEventArgs e)
    {
        _logger.LogDebug("Vehicle status changed: {Status}", e.Status);
        VehicleStatusChanged?.Invoke(this, e);
    }

    private void OnBatteryStatusChanged(object? sender, BatteryStatusChangedEventArgs e)
    {
        _logger.LogDebug("Battery status changed: {Voltage}V", e.Voltage);
        BatteryStatusChanged?.Invoke(this, e);
    }

    public void Dispose()
    {
        if (_disposed)
            return;

        _logger.LogInformation("Disposing DelSol device manager");

        try
        {
            // Disconnect and clean up
            DisconnectAsync().Wait(5000);

            // Dispose services
            _vehicleService?.Dispose();
            _connectionManager?.Dispose();

            // Unsubscribe from events
            if (_connectionManager != null)
                _connectionManager.ConnectionStatusChanged -= OnConnectionStatusChanged;

            if (_vehicleService != null)
            {
                _vehicleService.StatusChanged -= OnVehicleStatusChanged;
                _vehicleService.BatteryChanged -= OnBatteryStatusChanged;
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error during DelSol device manager disposal");
        }

        _disposed = true;
    }
}