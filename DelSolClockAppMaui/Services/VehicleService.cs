using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using Plugin.BLE.Abstractions.Contracts;
using Plugin.BLE.Abstractions.EventArgs;
using DelSolClockAppMaui.Models;
using DelSolClockAppMaui.Models.EventArgs;

namespace DelSolClockAppMaui.Services;

public class VehicleService : IDisposable
{
    private ICharacteristic? _statusCharacteristic;
    private ICharacteristic? _batteryCharacteristic;
    private readonly ILogger<VehicleService> _logger;
    private bool _disposed = false;

    public event EventHandler<VehicleStatusChangedEventArgs>? StatusChanged;
    public event EventHandler<BatteryStatusChangedEventArgs>? BatteryChanged;

    public VehicleService(ILogger<VehicleService> logger)
    {
        _logger = logger;
    }

    public async Task<bool> InitializeAsync(IDevice device)
    {
        try
        {
            _logger.LogInformation("Initializing vehicle service for device: {DeviceName}", device.Name);

            // Get vehicle service from device
            var vehicleService = await device.GetServiceAsync(DelSolConstants.VehicleServiceGuid);
            if (vehicleService == null)
            {
                _logger.LogWarning("Vehicle service not found on device");
                return false;
            }

            // Discover status characteristic
            _statusCharacteristic = await vehicleService.GetCharacteristicAsync(DelSolConstants.StatusCharacteristicGuid);
            if (_statusCharacteristic == null)
            {
                _logger.LogWarning("Status characteristic not found");
                return false;
            }

            // Discover battery characteristic
            _batteryCharacteristic = await vehicleService.GetCharacteristicAsync(DelSolConstants.BatteryCharacteristicGuid);
            if (_batteryCharacteristic == null)
            {
                _logger.LogWarning("Battery characteristic not found");
            }

            // Automatically start notifications for status updates
            if (_statusCharacteristic.CanUpdate)
            {
                _statusCharacteristic.ValueUpdated += OnStatusUpdated;
                await _statusCharacteristic.StartUpdatesAsync();
                _logger.LogInformation("Started status notifications");
            }
            else
            {
                _logger.LogWarning("Status characteristic does not support notifications");
            }

            // Start battery notifications if supported
            if (_batteryCharacteristic?.CanUpdate == true)
            {
                _batteryCharacteristic.ValueUpdated += OnBatteryUpdated;
                await _batteryCharacteristic.StartUpdatesAsync();
                _logger.LogInformation("Started battery notifications");
            }

            _logger.LogInformation("Vehicle service initialized successfully");
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to initialize vehicle service");
            return false;
        }
    }

    public async Task<VehicleStatus?> GetStatusAsync()
    {
        if (_statusCharacteristic == null)
        {
            _logger.LogWarning("Status characteristic not available");
            return null;
        }

        try
        {
            // Read current value from characteristic
            var result = await _statusCharacteristic.ReadAsync();
            return ParseStatusBytes(result.data);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to read vehicle status");
            return null;
        }
    }

    public async Task<float> GetBatteryVoltageAsync()
    {
        if (_batteryCharacteristic == null)
        {
            _logger.LogWarning("Battery characteristic not available");
            return 0f;
        }

        try
        {
            // Read 4-byte float from characteristic
            var result = await _batteryCharacteristic.ReadAsync();
            if (result.data == null || result.data.Length != 4)
            {
                _logger.LogWarning("Invalid battery data length: {Length}", result.data?.Length ?? 0);
                return 0f;
            }

            return BitConverter.ToSingle(result.data, 0);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to read battery voltage");
            return 0f;
        }
    }

    private VehicleStatus? ParseStatusBytes(byte[] value)
    {
        if (value == null || value.Length == 0)
        {
            _logger.LogWarning("Received empty status data");
            return null;
        }

        try
        {
            // Parse "0,0,0,1,1" format into VehicleStatus
            var statusString = Encoding.UTF8.GetString(value);
            _logger.LogDebug("Received status string: {StatusString}", statusString);

            var parts = statusString.Split(',');
            if (parts.Length != 5)
            {
                _logger.LogWarning("Invalid status string format. Expected 5 parts, got {Count}: {StatusString}", parts.Length, statusString);
                return null;
            }

            var status = new VehicleStatus
            {
                RearWindowDown = parts[0].Trim() == "1",
                TrunkOpen = parts[1].Trim() == "1",
                ConvertibleRoofDown = parts[2].Trim() == "1",
                IgnitionOn = parts[3].Trim() == "1",
                HeadlightsOn = parts[4].Trim() == "1",
                LastUpdated = DateTime.Now
            };

            _logger.LogDebug("Parsed vehicle status: {Status}", status);
            return status;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to parse status bytes");
            return null;
        }
    }

    private void OnStatusUpdated(object? sender, CharacteristicUpdatedEventArgs e)
    {
        _logger.LogDebug("Status characteristic updated");
        
        var status = ParseStatusBytes(e.Characteristic.Value);
        if (status != null)
        {
            StatusChanged?.Invoke(this, new VehicleStatusChangedEventArgs { Status = status });
        }
    }

    private void OnBatteryUpdated(object? sender, CharacteristicUpdatedEventArgs e)
    {
        _logger.LogDebug("Battery characteristic updated");
        
        try
        {
            if (e.Characteristic.Value != null && e.Characteristic.Value.Length == 4)
            {
                var voltage = BitConverter.ToSingle(e.Characteristic.Value, 0);
                BatteryChanged?.Invoke(this, new BatteryStatusChangedEventArgs 
                { 
                    Voltage = voltage,
                    Timestamp = DateTime.Now 
                });
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to process battery update");
        }
    }

    public void Dispose()
    {
        if (_disposed)
            return;

        _logger.LogInformation("Disposing vehicle service");

        try
        {
            // Stop notifications and clean up
            if (_statusCharacteristic != null)
            {
                _statusCharacteristic.ValueUpdated -= OnStatusUpdated;
                if (_statusCharacteristic.CanUpdate)
                {
                    _statusCharacteristic.StopUpdatesAsync().Wait(5000);
                }
                _statusCharacteristic = null;
            }

            if (_batteryCharacteristic != null)
            {
                _batteryCharacteristic.ValueUpdated -= OnBatteryUpdated;
                if (_batteryCharacteristic.CanUpdate)
                {
                    _batteryCharacteristic.StopUpdatesAsync().Wait(5000);
                }
                _batteryCharacteristic = null;
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error during vehicle service disposal");
        }

        _disposed = true;
    }
}