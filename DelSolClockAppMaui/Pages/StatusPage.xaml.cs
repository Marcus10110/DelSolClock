using Microsoft.Maui.Controls;
using DelSolClockAppMaui.Services;
using DelSolClockAppMaui.Models;
using DelSolClockAppMaui.Models.EventArgs;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;

namespace DelSolClockAppMaui.Pages;

public partial class StatusPage : ContentPage, INotifyPropertyChanged
{
    private readonly DelSolDevice _delSolDevice;
    private ObservableCollection<DiscoveredDevice> _discoveredDevices = new();
    private CancellationTokenSource? _scanCancellationTokenSource;

    public ObservableCollection<DiscoveredDevice> DiscoveredDevices
    {
        get => _discoveredDevices;
        set
        {
            _discoveredDevices = value;
            OnPropertyChanged();
        }
    }

    public new event PropertyChangedEventHandler? PropertyChanged;

    protected new virtual void OnPropertyChanged( [System.Runtime.CompilerServices.CallerMemberName] string? propertyName = null )
    {
        PropertyChanged?.Invoke( this, new PropertyChangedEventArgs( propertyName ) );
    }

    public StatusPage( DelSolDevice delSolDevice )
    {
        InitializeComponent();
        _delSolDevice = delSolDevice;

        // Set up data binding
        BindingContext = this;

        // Subscribe to device events
        _delSolDevice.ConnectionStatusChanged += OnConnectionStatusChanged;
        _delSolDevice.VehicleStatusChanged += OnVehicleStatusChanged;
        _delSolDevice.BatteryStatusChanged += OnBatteryStatusChanged;
    }

    protected override async void OnAppearing()
    {
        base.OnAppearing();

        // Subscribe to firmware version changes
        _delSolDevice.FirmwareVersionChanged += OnFirmwareVersionChanged;

        // Initialize the DelSol device when page appears
        await InitializeDeviceAsync();
        UpdateUIState();
    }

    private async Task InitializeDeviceAsync()
    {
        try
        {
            var initialized = await _delSolDevice.InitializeAsync();
            if( !initialized )
            {
                await DisplayAlert( "Error", "Failed to initialize Bluetooth. Please check your Bluetooth adapter.", "OK" );
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to initialize device: {ex.Message}", "OK" );
        }
    }

    private void UpdateUIState()
    {
        // Don't override connecting state if spinner is running
        if( ConnectionSpinner.IsRunning )
        {
            return; // Let the connecting state continue until connection completes
        }

        // Update connection status
        if( _delSolDevice.IsConnected )
        {
            ConnectionStatusLabel.Text = $"Connected: {_delSolDevice.ConnectedDeviceName}";
            DeviceInfoLabel.Text = $"Firmware: {_delSolDevice.FirmwareVersion ?? "--"}";
            DeviceInfoLabel.IsVisible = true;

            DisconnectButton.IsVisible = true;
            ScanButton.IsVisible = false;
            KnownDevicesButton.IsVisible = false;

            VehicleStatusCard.IsVisible = true;
            DeviceListCard.IsVisible = false;
            HelpCard.IsVisible = false;
        }
        else
        {
            ConnectionStatusLabel.Text = "Not Connected";
            DeviceInfoLabel.IsVisible = false;

            DisconnectButton.IsVisible = false;
            ScanButton.IsVisible = true;
            KnownDevicesButton.IsVisible = true;

            VehicleStatusCard.IsVisible = false;
            DeviceListCard.IsVisible = false;
            HelpCard.IsVisible = true;
        }

        ConnectionSpinner.IsVisible = false;
        ConnectionSpinner.IsRunning = false;
        CancelScanButton.IsVisible = false;
    }

    private async void OnScanButtonClicked( object sender, EventArgs e )
    {
        try
        {
            // Clear previous results
            DiscoveredDevices.Clear();

            // Set up cancellation token
            _scanCancellationTokenSource = new CancellationTokenSource();

            // Show spinner and update UI state
            ConnectionSpinner.IsVisible = true;
            ConnectionSpinner.IsRunning = true;
            ScanButton.IsEnabled = false;
            KnownDevicesButton.IsEnabled = false;
            ConnectionStatusLabel.Text = "Scanning for devices...";

            // Show device list card and cancel button
            DeviceListCard.IsVisible = true;
            ScanStatusLabel.Text = "Scanning for devices...";
            CancelScanButton.IsVisible = true;
            HelpCard.IsVisible = false;

            // Scan for devices 
            var devices = await _delSolDevice.ScanForDevicesAsync( TimeSpan.FromSeconds( 3 ) );

            // Add devices to list
            var sortedDevices = devices.OrderBy( d => d.Name );
            foreach( var device in sortedDevices )
            {
                DiscoveredDevices.Add( device );
            }

            // Update status
            if( devices.Any() )
            {
                ScanStatusLabel.Text = $"Found {devices.Count} device(s)";
            }
            else
            {
                ScanStatusLabel.Text = "No devices found";
                await DisplayAlert( "Scan Results", "No devices found. Make sure your Del Sol device is nearby and powered on.", "OK" );
            }
        }
        catch( OperationCanceledException )
        {
            ScanStatusLabel.Text = "Scan cancelled";
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Scan failed: {ex.Message}", "OK" );
            DeviceListCard.IsVisible = false;
        }
        finally
        {
            // Reset UI state
            ScanButton.IsEnabled = true;
            KnownDevicesButton.IsEnabled = true;
            ConnectionSpinner.IsVisible = false;
            ConnectionSpinner.IsRunning = false;
            CancelScanButton.IsVisible = false;
            ConnectionStatusLabel.Text = "Not Connected";
            _scanCancellationTokenSource = null;
        }
    }

    private async void OnKnownDevicesButtonClicked( object sender, EventArgs e )
    {
        try
        {
            // Clear previous results
            DiscoveredDevices.Clear();

            // Show spinner
            ConnectionSpinner.IsVisible = true;
            ConnectionSpinner.IsRunning = true;
            ConnectionStatusLabel.Text = "Looking for known devices...";

            // Show device list card
            DeviceListCard.IsVisible = true;
            ScanStatusLabel.Text = "Looking for known devices...";
            HelpCard.IsVisible = false;

            // Get known devices
            var knownDevices = await _delSolDevice.GetKnownDevicesAsync();

            // Add devices to list
            var sortedKnownDevices = knownDevices.OrderBy( d => d.Name );
            foreach( var device in sortedKnownDevices )
            {
                DiscoveredDevices.Add( device );
            }

            // Update status
            if( knownDevices.Any() )
            {
                ScanStatusLabel.Text = $"Found {knownDevices.Count} known device(s)";
            }
            else
            {
                ScanStatusLabel.Text = "No known devices found";
                await DisplayAlert( "Known Devices", "No known Del Sol devices found. Try scanning for new devices.", "OK" );
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to get known devices: {ex.Message}", "OK" );
            DeviceListCard.IsVisible = false;
        }
        finally
        {
            ConnectionSpinner.IsVisible = false;
            ConnectionSpinner.IsRunning = false;
            ConnectionStatusLabel.Text = "Not Connected";
        }
    }

    private async void OnDisconnectButtonClicked( object sender, EventArgs e )
    {
        try
        {
            // Show disconnecting state immediately
            ConnectionSpinner.IsVisible = true;
            ConnectionSpinner.IsRunning = true;
            ConnectionStatusLabel.Text = "Disconnecting...";
            DisconnectButton.IsEnabled = false;

            // Run disconnect on background thread with timeout
            var disconnectTask = Task.Run( async () =>
            {
                // note, the underlying BLE windows API does not explicitly support disconnecting, and sometimes the API call will never return.
                // in those cases, the device won't be connectable again either, the app needs to be restarted.
                await _delSolDevice.DisconnectAsync();
            } );

            var timeoutTask = Task.Delay( 10000 ); // 10 second timeout
            var completedTask = await Task.WhenAny( disconnectTask, timeoutTask );

            if( completedTask == timeoutTask )
            {
                await DisplayAlert( "Timeout", "Disconnect operation timed out, but may still complete in background", "OK" );
            }
            else if( disconnectTask.Exception != null )
            {
                throw disconnectTask.Exception.GetBaseException();
            }
            else
            {
                await DisplayAlert( "Disconnected", "Device disconnected successfully", "OK" );
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Disconnect failed: {ex.Message}", "OK" );
        }
        finally
        {
            // Always stop spinner and update UI
            ConnectionSpinner.IsVisible = false;
            ConnectionSpinner.IsRunning = false;
            DisconnectButton.IsEnabled = true;
            UpdateUIState();
        }
    }

    private void OnConnectionStatusChanged( object? sender, ConnectionStatusChangedEventArgs e )
    {
        // Update UI on main thread
        MainThread.BeginInvokeOnMainThread( () =>
        {
            UpdateUIState();

            if( !string.IsNullOrEmpty( e.ErrorMessage ) )
            {
                DisplayAlert( "Connection Error", e.ErrorMessage, "OK" );
            }
        } );
    }

    private void OnVehicleStatusChanged( object? sender, VehicleStatusChangedEventArgs e )
    {
        // Update vehicle status on main thread
        MainThread.BeginInvokeOnMainThread( () =>
        {
            if( e.Status != null )
            {
                UpdateVehicleStatusDisplay( e.Status );
            }
        } );
    }

    private void OnBatteryStatusChanged( object? sender, BatteryStatusChangedEventArgs e )
    {
        // Update battery status on main thread
        MainThread.BeginInvokeOnMainThread( () =>
        {
            BatteryStatusLabel.Text = $"Battery: {e.Voltage:F1}V";
            LastUpdatedLabel.Text = $"Last Updated: {e.Timestamp:HH:mm:ss}";
        } );
    }

    private void OnFirmwareVersionChanged( object? sender, EventArgs e )
    {
        // Update firmware info on main thread
        MainThread.BeginInvokeOnMainThread( () =>
        {
            DeviceInfoLabel.Text = $"Firmware: {_delSolDevice.FirmwareVersion ?? "--"}";
        } );
    }

    private void UpdateVehicleStatusDisplay( VehicleStatus status )
    {
        WindowStatusLabel.Text = $"Rear Window: {( status.RearWindowDown ? "DOWN" : "UP" )}";
        TrunkStatusLabel.Text = $"Trunk: {( status.TrunkOpen ? "OPEN" : "CLOSED" )}";
        RoofStatusLabel.Text = $"Roof: {( status.ConvertibleRoofDown ? "DOWN" : "UP" )}";
        IgnitionStatusLabel.Text = $"Ignition: {( status.IgnitionOn ? "ON" : "OFF" )}";
        LightsStatusLabel.Text = $"Head Lights: {( status.HeadlightsOn ? "ON" : "OFF" )}";

        LastUpdatedLabel.Text = $"Last Updated: {status.LastUpdated:HH:mm:ss}";
    }

    private async void OnConnectDeviceButtonClicked( object sender, EventArgs e )
    {
        if( sender is Button button && button.CommandParameter is DiscoveredDevice device )
        {
            try
            {
                // Show connecting state
                ConnectionSpinner.IsVisible = true;
                ConnectionSpinner.IsRunning = true;
                ConnectionStatusLabel.Text = $"Connecting to {device.Name}...";

                // Set device as connecting
                device.IsConnecting = true;

                // Disable scan buttons during connection
                ScanButton.IsEnabled = false;
                KnownDevicesButton.IsEnabled = false;

                // Attempt connection
                var success = await _delSolDevice.ConnectAsync( device );

                if( success )
                {
                    // Hide device list on successful connection
                    DeviceListCard.IsVisible = false;
                    DiscoveredDevices.Clear();

                    // Request initial vehicle status
                    _ = Task.Run( async () =>
                    {
                        await Task.Delay( 1000 ); // Give connection time to stabilize
                        await RequestInitialStatusAsync();
                    } );

                    await DisplayAlert( "Connected", $"Successfully connected to {device.Name}", "OK" );
                }
                else
                {
                    await DisplayAlert( "Connection Failed", $"Failed to connect to {device.Name}. Please try again.", "OK" );
                }
            }
            catch( Exception ex )
            {
                await DisplayAlert( "Connection Error", $"Error connecting to device: {ex.Message}", "OK" );
            }
            finally
            {
                // Reset connecting state
                device.IsConnecting = false;

                // Re-enable scan buttons
                ScanButton.IsEnabled = true;
                KnownDevicesButton.IsEnabled = true;

                // Stop connecting spinner and update UI
                ConnectionSpinner.IsVisible = false;
                ConnectionSpinner.IsRunning = false;
                UpdateUIState();
            }
        }
    }

    private void OnCancelScanButtonClicked( object sender, EventArgs e )
    {
        _scanCancellationTokenSource?.Cancel();

        // Hide device list
        DeviceListCard.IsVisible = false;
        HelpCard.IsVisible = true;

        // Reset UI
        UpdateUIState();
    }

    private async Task RequestInitialStatusAsync()
    {
        try
        {
            // Request initial vehicle status
            var vehicleStatus = await _delSolDevice.GetVehicleStatusAsync();
            if( vehicleStatus != null )
            {
                MainThread.BeginInvokeOnMainThread( () =>
                {
                    UpdateVehicleStatusDisplay( vehicleStatus );
                } );
            }

            // Request initial battery voltage
            var batteryVoltage = await _delSolDevice.GetBatteryVoltageAsync();
            if( batteryVoltage > 0 )
            {
                MainThread.BeginInvokeOnMainThread( () =>
                {
                    BatteryStatusLabel.Text = $"Battery: {batteryVoltage:F1}V";
                    LastUpdatedLabel.Text = $"Last Updated: {DateTime.Now:HH:mm:ss}";
                } );
            }
        }
        catch( Exception ex )
        {
            System.Diagnostics.Debug.WriteLine( $"Error requesting initial status: {ex.Message}" );
        }
    }

    protected override void OnDisappearing()
    {
        base.OnDisappearing();

        // Cancel any ongoing scan
        _scanCancellationTokenSource?.Cancel();

        // Unsubscribe from events
        _delSolDevice.FirmwareVersionChanged -= OnFirmwareVersionChanged;
    }
}