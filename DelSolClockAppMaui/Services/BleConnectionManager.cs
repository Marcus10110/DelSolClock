using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using Plugin.BLE;
using Plugin.BLE.Abstractions.Contracts;
using Plugin.BLE.Abstractions.EventArgs;
using Plugin.BLE.Abstractions;
using DelSolClockAppMaui.Models;
using DelSolClockAppMaui.Models.EventArgs;

namespace DelSolClockAppMaui.Services;

public class BleConnectionManager : IDisposable
{
    private IAdapter? _adapter;
    private IDevice? _connectedDevice;
    private readonly ILogger<BleConnectionManager> _logger;
    private bool _disposed = false;
    private bool _isInitialized = false;
    private DateTime _lastInitAttempt = DateTime.MinValue;
    private static readonly TimeSpan InitializationCooldown = TimeSpan.FromSeconds(30);

    public bool IsConnected => _connectedDevice?.State == Plugin.BLE.Abstractions.DeviceState.Connected;
    public string? ConnectedDeviceName => _connectedDevice?.Name;
    public IDevice? ConnectedDevice => _connectedDevice;

    public event EventHandler<ConnectionStatusChangedEventArgs>? ConnectionStatusChanged;

    public BleConnectionManager( ILogger<BleConnectionManager> logger )
    {
        _logger = logger;
    }

    public async Task<bool> InitializeAsync(bool forceRetry = false)
    {
        if( _isInitialized && !forceRetry )
            return true;

        // Implement cooldown to avoid rapid retry attempts
        if( !forceRetry && DateTime.Now - _lastInitAttempt < InitializationCooldown )
        {
            _logger.LogInformation( "BLE initialization attempted too recently, waiting for cooldown" );
            return false;
        }

        _lastInitAttempt = DateTime.Now;

        try
        {
            _logger.LogInformation( "Initializing BLE connection manager" );

            // Get the BLE instance
            var ble = CrossBluetoothLE.Current;
            
            // Subscribe to state changes first (for iOS permission handling)
            ble.StateChanged += OnBleStateChanged;

            // Initial state check with more detailed logging
            _logger.LogInformation( "Initial BLE state: {State}", ble.State );
            
            // For iOS, we may need to wait for permission prompt resolution
            if( ble.State == BluetoothState.Unknown )
            {
                _logger.LogInformation( "BLE state is Unknown, waiting for iOS permissions..." );
                
                // Wait up to 5 seconds for iOS to resolve permissions
                var maxWaitTime = TimeSpan.FromSeconds( 5 );
                var startTime = DateTime.Now;
                
                while( ble.State == BluetoothState.Unknown && DateTime.Now - startTime < maxWaitTime )
                {
                    await Task.Delay( 500 );
                    _logger.LogDebug( "Waiting for BLE state resolution... Current state: {State}", ble.State );
                }
                
                _logger.LogInformation( "After waiting, BLE state is: {State}", ble.State );
            }

            // Check final BLE state
            if( ble.State != BluetoothState.On )
            {
                _logger.LogWarning( "Bluetooth is not enabled. State: {State}. Please ensure Bluetooth is enabled and permissions are granted.", ble.State );
                return false;
            }
            
            _logger.LogInformation( "Bluetooth is enabled and ready" );

            // Get adapter
            _adapter = ble.Adapter;
            if( _adapter == null )
            {
                _logger.LogError( "Failed to get BLE adapter" );
                return false;
            }

            // Subscribe to adapter events
            _adapter.DeviceConnected += OnDeviceConnected;
            _adapter.DeviceDisconnected += OnDeviceDisconnected;
            _adapter.DeviceConnectionLost += OnDeviceConnectionLost;

            _isInitialized = true;
            _logger.LogInformation( "BLE connection manager initialized successfully" );
            return true;
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Failed to initialize BLE connection manager" );
            return false;
        }
    }

    public async Task<bool> RetryInitializationAsync()
    {
        _logger.LogInformation( "Retrying BLE initialization..." );
        _isInitialized = false;
        return await InitializeAsync(forceRetry: true);
    }

    public async Task<List<DiscoveredDevice>> ScanForDevicesAsync( Guid serviceId, TimeSpan timeout )
    {
        if( _adapter == null || !_isInitialized )
        {
            _logger.LogWarning( "BLE adapter not initialized" );
            return new List<DiscoveredDevice>();
        }

        var discoveredDevices = new List<DiscoveredDevice>();
        var deviceAdvertisedHandler = new EventHandler<DeviceEventArgs>( ( sender, args ) =>
        {
            var device = args.Device;
            if( device?.Name != null && !string.IsNullOrWhiteSpace( device.Name ) )
            {
                var discoveredDevice = new DiscoveredDevice
                {
                    Name = device.Name,
                    Id = device.Id.ToString(),
                    DeviceId = device.Id,
                    IsKnownDevice = false,
                    SignalStrength = args.Device.Rssi,
                    LastSeen = DateTime.Now,
                    BleDevice = device
                };

                // Avoid duplicates
                if( !discoveredDevices.Any( d => d.Id == discoveredDevice.Id ) )
                {
                    discoveredDevices.Add( discoveredDevice );
                    _logger.LogInformation( "Discovered device: {Name} ({Id})", device.Name, device.Id );
                }
            }
        } );

        try
        {
            _logger.LogInformation( "Starting BLE scan for service {ServiceId} with timeout {Timeout}", serviceId, timeout );

            _adapter.DeviceAdvertised += deviceAdvertisedHandler;

            _adapter.ScanTimeout = (int)timeout.TotalMilliseconds;
            await _adapter.StartScanningForDevicesAsync( new ScanFilterOptions { ServiceUuids = new Guid[] { serviceId } } );

            _logger.LogInformation( "Start await finished." );


            await _adapter.StopScanningForDevicesAsync();

            _logger.LogInformation( "BLE scan completed. Found {Count} devices", discoveredDevices.Count );
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error during BLE scan" );
        }
        finally
        {
            _adapter.DeviceAdvertised -= deviceAdvertisedHandler;
        }

        return discoveredDevices;
    }


    public async Task<List<DiscoveredDevice>> GetKnownDevicesAsync( Guid serviceId )
    {
        if( _adapter == null || !_isInitialized )
        {
            _logger.LogWarning( "BLE adapter not initialized" );
            return new List<DiscoveredDevice>();
        }

        try
        {
            _logger.LogInformation( "Getting known devices for service {ServiceId}", serviceId );

            var knownDevices = _adapter.GetSystemConnectedOrPairedDevices( new[] { serviceId } );
            var discoveredDevices = new List<DiscoveredDevice>();

            foreach( var device in knownDevices )
            {
                if( device?.Name != null )
                {
                    discoveredDevices.Add( new DiscoveredDevice
                    {
                        Name = device.Name,
                        Id = device.Id.ToString(),
                        DeviceId = device.Id,
                        IsKnownDevice = true,
                        SignalStrength = device.Rssi,
                        LastSeen = DateTime.Now,
                        BleDevice = device
                    } );
                }
            }

            _logger.LogInformation( "Found {Count} known devices", discoveredDevices.Count );
            return discoveredDevices;
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error getting known devices" );
            return new List<DiscoveredDevice>();
        }
    }

    public async Task<bool> ConnectAsync( DiscoveredDevice device )
    {
        if( _adapter == null || !_isInitialized )
        {
            _logger.LogWarning( "BLE adapter not initialized" );
            return false;
        }

        if( device.BleDevice == null )
        {
            _logger.LogError( "Device BLE reference is null" );
            return false;
        }

        try
        {
            _logger.LogInformation( "Connecting to device: {Name} ({Id})", device.Name, device.Id );

            // Disconnect from current device if connected
            if( _connectedDevice != null )
            {
                await DisconnectAsync();
            }

            // Connect without connection parameters for Plugin.BLE 3.x compatibility
            await _adapter.ConnectToDeviceAsync( device.BleDevice );

            if( device.BleDevice.State == Plugin.BLE.Abstractions.DeviceState.Connected )
            {
                _connectedDevice = device.BleDevice;
                _logger.LogInformation( "Successfully connected to {Name}", device.Name );

                ConnectionStatusChanged?.Invoke( this, new ConnectionStatusChangedEventArgs
                {
                    IsConnected = true,
                    DeviceName = device.Name
                } );

                return true;
            }
            else
            {
                _logger.LogWarning( "Connection attempt completed but device not in connected state: {State}", device.BleDevice.State );
                return false;
            }
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Failed to connect to device {Name}", device.Name );

            ConnectionStatusChanged?.Invoke( this, new ConnectionStatusChangedEventArgs
            {
                IsConnected = false,
                ErrorMessage = ex.Message
            } );

            return false;
        }
    }

    public async Task DisconnectAsync()
    {
        if( _connectedDevice != null && _adapter != null )
        {
            try
            {
                _logger.LogInformation( "Disconnecting from device: {Name}", _connectedDevice.Name );
                await _adapter.DisconnectDeviceAsync( _connectedDevice );
                _connectedDevice = null;

                ConnectionStatusChanged?.Invoke( this, new ConnectionStatusChangedEventArgs
                {
                    IsConnected = false
                } );
            }
            catch( Exception ex )
            {
                _logger.LogError( ex, "Error during disconnect" );
            }
        }
    }

    public async Task<IService?> GetServiceAsync( Guid serviceId )
    {
        if( _connectedDevice == null )
        {
            _logger.LogWarning( "No device connected" );
            return null;
        }

        try
        {
            _logger.LogDebug( "Getting service {ServiceId} from device", serviceId );
            var service = await _connectedDevice.GetServiceAsync( serviceId );

            if( service == null )
            {
                _logger.LogWarning( "Service {ServiceId} not found on device", serviceId );
            }

            return service;
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error getting service {ServiceId}", serviceId );
            return null;
        }
    }

    private void OnDeviceConnected( object? sender, DeviceEventArgs e )
    {
        _logger.LogInformation( "Device connected: {Name} ({Id})", e.Device.Name, e.Device.Id );
    }

    private void OnDeviceDisconnected( object? sender, DeviceEventArgs e )
    {
        _logger.LogInformation( "Device disconnected: {Name} ({Id})", e.Device.Name, e.Device.Id );

        if( e.Device.Id == _connectedDevice?.Id )
        {
            _connectedDevice = null;
            ConnectionStatusChanged?.Invoke( this, new ConnectionStatusChangedEventArgs
            {
                IsConnected = false,
                DeviceName = e.Device.Name
            } );
        }
    }

    private void OnDeviceConnectionLost( object? sender, DeviceErrorEventArgs e )
    {
        _logger.LogWarning( "Device connection lost: {Name} ({Id}) - {Error}", e.Device.Name, e.Device.Id, e.ErrorMessage );

        if( e.Device.Id == _connectedDevice?.Id )
        {
            _connectedDevice = null;
            ConnectionStatusChanged?.Invoke( this, new ConnectionStatusChangedEventArgs
            {
                IsConnected = false,
                DeviceName = e.Device.Name,
                ErrorMessage = e.ErrorMessage
            } );
        }
    }

    private void OnBleStateChanged( object? sender, BluetoothStateChangedArgs e )
    {
        _logger.LogInformation( "BLE state changed from {OldState} to {NewState}", e.OldState, e.NewState );

        if( e.NewState != BluetoothState.On && _connectedDevice != null )
        {
            _logger.LogWarning( "BLE turned off while connected to device" );
            _connectedDevice = null;
            ConnectionStatusChanged?.Invoke( this, new ConnectionStatusChangedEventArgs
            {
                IsConnected = false,
                ErrorMessage = "Bluetooth was turned off"
            } );
        }
    }

    public void Dispose()
    {
        if( _disposed )
            return;

        _logger.LogInformation( "Disposing BLE connection manager" );

        try
        {
            // Disconnect current device
            if( _connectedDevice != null && _adapter != null )
            {
                _adapter.DisconnectDeviceAsync( _connectedDevice ).Wait( 5000 );
            }

            // Unsubscribe from events
            if( _adapter != null )
            {
                _adapter.DeviceConnected -= OnDeviceConnected;
                _adapter.DeviceDisconnected -= OnDeviceDisconnected;
                _adapter.DeviceConnectionLost -= OnDeviceConnectionLost;
            }

            var ble = CrossBluetoothLE.Current;
            ble.StateChanged -= OnBleStateChanged;
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error during BLE connection manager disposal" );
        }

        _disposed = true;
    }
}