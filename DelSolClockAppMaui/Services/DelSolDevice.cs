using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using DelSolClockAppMaui.Models;
using DelSolClockAppMaui.Models.EventArgs;
using Plugin.BLE.Abstractions.Contracts;

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
    public event EventHandler<EventArgs>? FirmwareVersionChanged;

    public DelSolDevice( ILogger<DelSolDevice> logger )
    {
        _logger = logger;
    }

    public async Task<bool> InitializeAsync()
    {
        if( _isInitialized )
            return true;

        try
        {
            _logger.LogInformation( "Initializing DelSol device manager" );

            // Create BLE connection manager
            var bleLogger = LoggerFactory.Create( builder => builder.AddDebug() ).CreateLogger<BleConnectionManager>();
            _connectionManager = new BleConnectionManager( bleLogger );

            // Initialize BLE
            var bleInitialized = await _connectionManager.InitializeAsync();
            if( !bleInitialized )
            {
                _logger.LogError( "Failed to initialize BLE connection manager" );
                return false;
            }

            // Subscribe to connection events
            _connectionManager.ConnectionStatusChanged += OnConnectionStatusChanged;

            // Create vehicle service
            var vehicleLogger = LoggerFactory.Create( builder => builder.AddDebug() ).CreateLogger<VehicleService>();
            _vehicleService = new VehicleService( vehicleLogger );

            // Subscribe to vehicle events
            _vehicleService.StatusChanged += OnVehicleStatusChanged;
            _vehicleService.BatteryChanged += OnBatteryStatusChanged;

            _isInitialized = true;
            _logger.LogInformation( "DelSol device manager initialized successfully" );
            return true;
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Failed to initialize DelSol device manager" );
            return false;
        }
    }

    public async Task<List<DiscoveredDevice>> ScanForDevicesAsync( TimeSpan timeout = default)
    {
        if( _connectionManager == null || !_isInitialized )
        {
            _logger.LogWarning( "Device manager not initialized" );
            return new List<DiscoveredDevice>();
        }

        if( timeout == default )
            timeout = DelSolConstants.DefaultScanTimeout;

        try
        {
            _logger.LogInformation( "Scanning for DelSol devices" );
            return await _connectionManager.ScanForDevicesAsync( DelSolConstants.VehicleServiceGuid, timeout );
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error during device scan" );
            return new List<DiscoveredDevice>();
        }
    }

    public async Task<List<DiscoveredDevice>> GetKnownDevicesAsync()
    {
        if( _connectionManager == null || !_isInitialized )
        {
            _logger.LogWarning( "Device manager not initialized" );
            return new List<DiscoveredDevice>();
        }

        try
        {
            _logger.LogInformation( "Getting known DelSol devices" );
            return await _connectionManager.GetKnownDevicesAsync( DelSolConstants.VehicleServiceGuid );
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error getting known devices" );
            return new List<DiscoveredDevice>();
        }
    }

    public async Task<bool> ConnectAsync( DiscoveredDevice device )
    {
        if( _connectionManager == null || _vehicleService == null || !_isInitialized )
        {
            _logger.LogWarning( "Device manager not initialized" );
            return false;
        }

        try
        {
            _logger.LogInformation( "Connecting to DelSol device: {DeviceName}", device.Name );

            // Connect via BLE
            var connected = await _connectionManager.ConnectAsync( device );
            if( !connected )
            {
                _logger.LogError( "Failed to establish BLE connection" );
                return false;
            }

            // Initialize vehicle service
            if( _connectionManager.ConnectedDevice != null )
            {
                var serviceInitialized = await _vehicleService.InitializeAsync( _connectionManager.ConnectedDevice );
                if( !serviceInitialized )
                {
                    _logger.LogError( "Failed to initialize vehicle service" );
                    await _connectionManager.DisconnectAsync();
                    return false;
                }

                // wait 2 seconds for service to stabilize
                await Task.Delay( 2000 );

                // Try to get firmware version
                await ReadFirmwareVersionAsync();
            }

            _logger.LogInformation( "Successfully connected to DelSol device" );
            return true;
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error connecting to DelSol device" );
            return false;
        }
    }

    public async Task DisconnectAsync()
    {
        if( _connectionManager == null )
            return;

        try
        {
            _logger.LogInformation( "Disconnecting from DelSol device" );

            // Dispose vehicle service to stop notifications
            _vehicleService?.Dispose();

            // Disconnect BLE
            await _connectionManager.DisconnectAsync();

            FirmwareVersion = "Unknown";
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error during disconnect" );
        }
    }

    public async Task<VehicleStatus?> GetVehicleStatusAsync()
    {
        if( _vehicleService == null || !IsConnected )
        {
            _logger.LogWarning( "Vehicle service not available or not connected" );
            return null;
        }

        try
        {
            return await _vehicleService.GetStatusAsync();
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error reading vehicle status" );
            return null;
        }
    }

    public async Task<float> GetBatteryVoltageAsync()
    {
        if( _vehicleService == null || !IsConnected )
        {
            _logger.LogWarning( "Vehicle service not available or not connected" );
            return 0f;
        }

        try
        {
            return await _vehicleService.GetBatteryVoltageAsync();
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error reading battery voltage" );
            return 0f;
        }
    }

    public async Task<bool> StartFirmwareUpdateAsync( string downloadUrl )
    {
        if( !IsConnected )
        {
            _logger.LogWarning( "Cannot start firmware update: device not connected" );
            return false;
        }

        try
        {
            _logger.LogInformation( "Starting firmware update from URL: {Url}", downloadUrl );

            // Raise progress event
            FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, "Downloading firmware..." ) );

            // Download firmware file
            using var httpClient = new HttpClient();
            using var firmwareStream = await httpClient.GetStreamAsync( downloadUrl );
            using var memoryStream = new MemoryStream();
            await firmwareStream.CopyToAsync( memoryStream );
            memoryStream.Position = 0;

            // Start the actual firmware update
            return await UpdateFirmwareAsync( memoryStream );
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Failed to start firmware update" );
            FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, $"Download failed: {ex.Message}" ) );
            return false;
        }
    }

    private TaskCompletionSource<string>? _firmwareResponseTcs;

    public async Task<bool> UpdateFirmwareAsync( Stream firmwareStream, CancellationToken cancellationToken = default )
    {
        if( !IsConnected || _connectionManager?.ConnectedDevice == null )
        {
            _logger.LogWarning( "Cannot update firmware: device not connected" );
            return false;
        }

        try
        {
            _logger.LogInformation( "Starting firmware update process" );
            FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 5, "Preparing firmware update..." ) );

            // Wait a bit for connection to stabilize
            await Task.Delay( 1000, cancellationToken );

            // Validate connection state
            var connectionState = _connectionManager.ConnectedDevice.State;
            _logger.LogInformation( "Device connection state: {State}", connectionState );

            if( connectionState != Plugin.BLE.Abstractions.DeviceState.Connected )
            {
                _logger.LogError( "Device not in connected state: {State}", connectionState );
                FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, $"Device not connected: {connectionState}" ) );
                return false;
            }

            // Get firmware service with retry logic
            IService? firmwareService = null;
            for( int retry = 0; retry < 3; retry++ )
            {
                try
                {
                    _logger.LogDebug( "Attempting to get firmware service, attempt {Retry}", retry + 1 );
                    firmwareService = await _connectionManager.GetServiceAsync( DelSolConstants.FirmwareServiceGuid );
                    if( firmwareService != null )
                        break;

                    if( retry < 2 )
                    {
                        _logger.LogWarning( "Firmware service not found, retrying in 500ms..." );
                        await Task.Delay( 500, cancellationToken );
                    }
                }
                catch( Exception ex ) when( retry < 2 )
                {
                    _logger.LogWarning( ex, "Error getting firmware service, attempt {Retry}, retrying...", retry + 1 );
                    await Task.Delay( 500, cancellationToken );
                }
            }

            if( firmwareService == null )
            {
                _logger.LogError( "Firmware service not found after retries" );
                FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, "Firmware service not found" ) );
                return false;
            }

            // Get firmware update characteristic with retry logic
            ICharacteristic? updateCharacteristic = null;
            for( int retry = 0; retry < 3; retry++ )
            {
                try
                {
                    _logger.LogDebug( "Attempting to get firmware characteristic, attempt {Retry}", retry + 1 );
                    updateCharacteristic = await firmwareService.GetCharacteristicAsync( DelSolConstants.FirmwareWriteCharacteristicGuid );
                    if( updateCharacteristic != null )
                        break;

                    if( retry < 2 )
                    {
                        _logger.LogWarning( "Firmware characteristic not found, retrying in 500ms..." );
                        await Task.Delay( 500, cancellationToken );
                    }
                }
                catch( Exception ex ) when( retry < 2 )
                {
                    _logger.LogWarning( ex, "Error getting firmware characteristic, attempt {Retry}, retrying...", retry + 1 );
                    await Task.Delay( 500, cancellationToken );
                }
            }

            if( updateCharacteristic == null )
            {
                _logger.LogError( "Firmware update characteristic not found after retries" );
                FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, "Update characteristic not found" ) );
                return false;
            }

            // Subscribe to notifications for responses
            updateCharacteristic.ValueUpdated += OnFirmwareResponseReceived;
            await updateCharacteristic.StartUpdatesAsync();

            try
            {
                // Read firmware data
                var firmwareData = new byte[ firmwareStream.Length ];
                await firmwareStream.ReadAsync( firmwareData, 0, firmwareData.Length, cancellationToken );

                _logger.LogInformation( "Firmware size: {Size} bytes", firmwareData.Length );
                FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 10, $"Uploading firmware ({firmwareData.Length} bytes)..." ) );

                // Send firmware in 512-byte chunks
                const int chunkSize = 512;
                var totalChunks = ( firmwareData.Length + chunkSize - 1 ) / chunkSize;
                var needsZeroLengthChunk = ( firmwareData.Length % chunkSize ) == 0;

                for( int chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++ )
                {
                    if( cancellationToken.IsCancellationRequested )
                    {
                        _logger.LogWarning( "Firmware update cancelled" );
                        FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, "Update cancelled" ) );
                        return false;
                    }

                    var offset = chunkIndex * chunkSize;
                    var currentChunkSize = Math.Min( chunkSize, firmwareData.Length - offset );
                    var chunk = new byte[ currentChunkSize ];
                    Array.Copy( firmwareData, offset, chunk, 0, currentChunkSize );

                    _logger.LogDebug( "Sending chunk {ChunkIndex}/{TotalChunks} ({ChunkSize} bytes)",
                        chunkIndex + 1, totalChunks, currentChunkSize );

                    // Create TaskCompletionSource for this chunk's response
                    _firmwareResponseTcs = new TaskCompletionSource<string>();

                    // Write chunk to characteristic
                    await updateCharacteristic.WriteAsync( chunk );

                    // Wait for notification response (with timeout)
                    var responseTask = _firmwareResponseTcs.Task;
                    var timeoutTask = Task.Delay( DelSolConstants.DefaultTimeoutSeconds * 1000, cancellationToken );
                    var completedTask = await Task.WhenAny( responseTask, timeoutTask );

                    if( completedTask == timeoutTask )
                    {
                        _logger.LogError( "Timeout waiting for firmware response at chunk {ChunkIndex}", chunkIndex + 1 );
                        FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, $"Timeout at chunk {chunkIndex + 1}" ) );
                        return false;
                    }

                    var response = await responseTask;
                    _logger.LogDebug( "Firmware update response: {Response}", response );

                    if( response == "error" )
                    {
                        _logger.LogError( "Firmware update failed at chunk {ChunkIndex}", chunkIndex + 1 );
                        FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, $"Update failed at chunk {chunkIndex + 1}" ) );
                        return false;
                    }
                    else if( response == "success" )
                    {
                        _logger.LogInformation( "Firmware update completed successfully" );
                        FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 100, "Update completed successfully" ) );
                        return true;
                    }
                    else if( response != "continue" )
                    {
                        _logger.LogWarning( "Unexpected firmware update response: {Response}", response );
                    }

                    // Update progress
                    var progressPercent = 10 + (int)( ( chunkIndex + 1 ) * 85.0 / totalChunks );
                    var progressMessage = $"Uploading... {chunkIndex + 1}/{totalChunks} chunks";

                    _logger.LogDebug( "Firmware progress: {ProgressPercent}% - {Message}", progressPercent, progressMessage );
                    FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( progressPercent, progressMessage ) );

                    // Small delay to allow UI updates to be visible
                    // await Task.Delay(50, cancellationToken);
                }

                // Send zero-length chunk if firmware size is exact multiple of 512 bytes
                if( needsZeroLengthChunk )
                {
                    _logger.LogDebug( "Sending zero-length chunk (firmware size is exact multiple of 512)" );

                    // Create TaskCompletionSource for final response
                    _firmwareResponseTcs = new TaskCompletionSource<string>();

                    await updateCharacteristic.WriteAsync( new byte[ 0 ] );

                    // Wait for final notification response
                    var responseTask = _firmwareResponseTcs.Task;
                    var timeoutTask = Task.Delay( DelSolConstants.DefaultTimeoutSeconds * 1000, cancellationToken );
                    var completedTask = await Task.WhenAny( responseTask, timeoutTask );

                    if( completedTask == timeoutTask )
                    {
                        _logger.LogError( "Timeout waiting for final firmware response" );
                        FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, "Timeout at final verification" ) );
                        return false;
                    }

                    var response = await responseTask;
                    _logger.LogDebug( "Final firmware update response: {Response}", response );

                    if( response == "success" )
                    {
                        _logger.LogInformation( "Firmware update completed successfully" );
                        FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 100, "Update completed successfully" ) );
                        return true;
                    }
                    else if( response == "error" )
                    {
                        _logger.LogError( "Firmware update failed at final chunk" );
                        FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, "Update failed at final verification" ) );
                        return false;
                    }
                }

                // If we get here, all chunks sent but no final success response
                _logger.LogWarning( "Firmware update completed but no success response received" );
                FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 95, "Update completed, verifying..." ) );
                return true;
            }
            finally
            {
                // Cleanup: unsubscribe from notifications
                try
                {
                    updateCharacteristic.ValueUpdated -= OnFirmwareResponseReceived;
                    await updateCharacteristic.StopUpdatesAsync();
                }
                catch( Exception ex )
                {
                    _logger.LogWarning( ex, "Error cleaning up firmware update notifications" );
                }

                _firmwareResponseTcs = null;
            }
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Firmware update failed with exception" );
            FirmwareUpdateProgress?.Invoke( this, new FirmwareUpdateProgressEventArgs( 0, $"Update failed: {ex.Message}" ) );
            return false;
        }
    }

    private void OnFirmwareResponseReceived( object? sender, Plugin.BLE.Abstractions.EventArgs.CharacteristicUpdatedEventArgs e )
    {
        try
        {
            if( e.Characteristic.Value != null && e.Characteristic.Value.Length > 0 )
            {
                var response = System.Text.Encoding.UTF8.GetString( e.Characteristic.Value );
                _logger.LogDebug( "Received firmware response notification: {Response}", response );

                // Complete the current TaskCompletionSource with the response
                _firmwareResponseTcs?.TrySetResult( response );
            }
            else
            {
                _logger.LogWarning( "Received empty firmware response notification" );
                _firmwareResponseTcs?.TrySetResult( "" );
            }
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error processing firmware response notification" );
            _firmwareResponseTcs?.TrySetException( ex );
        }
    }

    private async Task ReadFirmwareVersionAsync()
    {
        if( _connectionManager?.ConnectedDevice == null )
            return;

        try
        {
            var firmwareService = await _connectionManager.GetServiceAsync( DelSolConstants.FirmwareServiceGuid );
            if( firmwareService == null )
            {
                _logger.LogWarning( "Firmware service not found" );
                return;
            }

            var versionCharacteristic = await firmwareService.GetCharacteristicAsync( DelSolConstants.FirmwareVersionCharacteristicGuid );
            if( versionCharacteristic == null )
            {
                _logger.LogWarning( "Firmware version characteristic not found" );
                return;
            }

            var versionResult = await versionCharacteristic.ReadAsync();
            if( versionResult.data != null && versionResult.data.Length > 0 )
            {
                FirmwareVersion = System.Text.Encoding.UTF8.GetString( versionResult.data );
                _logger.LogInformation( "Read firmware version: {Version}", FirmwareVersion );
                
                // Fire firmware version changed event
                FirmwareVersionChanged?.Invoke(this, EventArgs.Empty);
            }
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Failed to read firmware version" );
        }
    }

    private void OnConnectionStatusChanged( object? sender, ConnectionStatusChangedEventArgs e )
    {
        _logger.LogInformation( "Connection status changed: {IsConnected} - {DeviceName} - {Error}",
            e.IsConnected, e.DeviceName, e.ErrorMessage );

        // If disconnected, clean up services
        if( !e.IsConnected )
        {
            _vehicleService?.Dispose();
            FirmwareVersion = "Unknown";
        }

        // Forward the event
        ConnectionStatusChanged?.Invoke( this, e );
    }

    private void OnVehicleStatusChanged( object? sender, VehicleStatusChangedEventArgs e )
    {
        _logger.LogDebug( "Vehicle status changed: {Status}", e.Status );
        VehicleStatusChanged?.Invoke( this, e );
    }

    private void OnBatteryStatusChanged( object? sender, BatteryStatusChangedEventArgs e )
    {
        _logger.LogDebug( "Battery status changed: {Voltage}V", e.Voltage );
        BatteryStatusChanged?.Invoke( this, e );
    }

    public void Dispose()
    {
        if( _disposed )
            return;

        _logger.LogInformation( "Disposing DelSol device manager" );

        try
        {
            // Disconnect and clean up
            // Don't use .Wait() as it can cause deadlocks - let disposal happen naturally
            _ = Task.Run(async () => {
                try 
                {
                    await DisconnectAsync();
                }
                catch (Exception ex)
                {
                    _logger.LogWarning(ex, "Error during async disconnect in disposal");
                }
            });

            // Dispose services
            _vehicleService?.Dispose();
            _connectionManager?.Dispose();

            // Unsubscribe from events
            if( _connectionManager != null )
                _connectionManager.ConnectionStatusChanged -= OnConnectionStatusChanged;

            if( _vehicleService != null )
            {
                _vehicleService.StatusChanged -= OnVehicleStatusChanged;
                _vehicleService.BatteryChanged -= OnBatteryStatusChanged;
            }
        }
        catch( Exception ex )
        {
            _logger.LogError( ex, "Error during DelSol device manager disposal" );
        }

        _disposed = true;
    }
}