using Microsoft.Maui.Controls;
using DelSolClockAppMaui.Services;
using DelSolClockAppMaui.Models;
using System.Collections.ObjectModel;
using System.ComponentModel;
using Microsoft.Extensions.Logging;

namespace DelSolClockAppMaui.Pages;

public partial class DebugPage : ContentPage, INotifyPropertyChanged
{
    private readonly DelSolDevice _delSolDevice;
    private ObservableCollection<CrashDumpFile> _crashDumpFiles = new();
    private bool _isCheckingStatus = false;
    private bool _isDownloading = false;

    public ObservableCollection<CrashDumpFile> CrashDumpFiles
    {
        get => _crashDumpFiles;
        set
        {
            _crashDumpFiles = value;
            OnPropertyChanged();
        }
    }

    public new event PropertyChangedEventHandler? PropertyChanged;

    protected new virtual void OnPropertyChanged( [System.Runtime.CompilerServices.CallerMemberName] string? propertyName = null )
    {
        PropertyChanged?.Invoke( this, new PropertyChangedEventArgs( propertyName ) );
    }

    public DebugPage( DelSolDevice delSolDevice )
    {
        InitializeComponent();
        _delSolDevice = delSolDevice;

        // Set up data binding
        BindingContext = this;
        CrashDumpsCollection.ItemsSource = CrashDumpFiles;
    }

    protected override async void OnAppearing()
    {
        base.OnAppearing();


        // Load saved crash dumps
        await RefreshCrashDumpListAsync();
    }


    private async void OnCheckStatusClicked( object sender, EventArgs e )
    {
        if( _isCheckingStatus || !_delSolDevice.IsConnected )
        {
            if( !_delSolDevice.IsConnected )
            {
                await DisplayAlert( "Not Connected", "Please connect to your Del Sol device first from the Status tab.", "OK" );
            }
            return;
        }

        try
        {
            _isCheckingStatus = true;
            CheckStatusButton.Text = "Checking...";
            CheckStatusButton.IsEnabled = false;

            if( _delSolDevice.DebugService == null )
            {
                await DisplayAlert( "Error", "Debug service not available. Please ensure your device supports crash dump debugging.", "OK" );
                return;
            }

            // Check crash dump status
            var status = await _delSolDevice.DebugService.CheckCrashDumpStatusAsync();

            // Update UI with results
            StatusResultLabel.Text = status.StatusMessage;
            StatusResultFrame.IsVisible = true;

            // Show download section if crash exists
            if( status.HasCrash )
            {
                DownloadCard.IsVisible = true;
                ClearButton.IsVisible = true;
                StatusResultFrame.BackgroundColor = Colors.LightCoral;
            }
            else
            {
                DownloadCard.IsVisible = false;
                ClearButton.IsVisible = false;
                StatusResultFrame.BackgroundColor = Colors.LightGreen;
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to check crash dump status: {ex.Message}", "OK" );
        }
        finally
        {
            _isCheckingStatus = false;
            CheckStatusButton.Text = "Check for Crash Dumps";
            CheckStatusButton.IsEnabled = true;
        }
    }

    private async void OnDownloadClicked( object sender, EventArgs e )
    {
        if( _isDownloading )
            return;

        try
        {
            _isDownloading = true;
            DownloadButton.IsEnabled = false;
            ProgressSection.IsVisible = true;

            var progress = new Progress<int>( percent =>
            {
                MainThread.BeginInvokeOnMainThread( () =>
                {
                    DownloadProgressBar.Progress = percent / 100.0;
                    ProgressLabel.Text = $"{percent}%";
                } );
            } );

            // First get the current crash dump status to get chunk count
            var status = await _delSolDevice.DebugService!.CheckCrashDumpStatusAsync();
            if (!status.HasCrash || status.ChunkCount <= 0)
            {
                await DisplayAlert("Error", "No valid crash dump available or invalid chunk count.", "OK");
                return;
            }

            var success = await _delSolDevice.DebugService!.DownloadCrashDumpAsync(_delSolDevice.FirmwareVersion, status.ChunkCount, progress);

            if( success )
            {
                await DisplayAlert( "Success", "Crash dump downloaded successfully!", "OK" );

                // Hide download section and refresh list
                DownloadCard.IsVisible = false;
                ClearButton.IsVisible = false;
                StatusResultFrame.IsVisible = false;
                await RefreshCrashDumpListAsync();
            }
            else
            {
                await DisplayAlert( "Error", "Failed to download crash dump.", "OK" );
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Download failed: {ex.Message}", "OK" );
        }
        finally
        {
            _isDownloading = false;
            DownloadButton.IsEnabled = true;
            ProgressSection.IsVisible = false;
            DownloadProgressBar.Progress = 0;
            ProgressLabel.Text = "0%";
        }
    }

    private async void OnRefreshClicked( object sender, EventArgs e )
    {
        await RefreshCrashDumpListAsync();
    }

    private async Task RefreshCrashDumpListAsync()
    {
        try
        {
            var crashDumps = await ( _delSolDevice.DebugService?.GetSavedCrashDumpsAsync() ?? Task.FromResult( new List<CrashDumpFile>() ) );

            MainThread.BeginInvokeOnMainThread( () =>
            {
                CrashDumpFiles.Clear();
                foreach( var crashDump in crashDumps )
                {
                    CrashDumpFiles.Add( crashDump );
                }

                EmptyStateLabel.IsVisible = CrashDumpFiles.Count == 0;
            } );
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to refresh crash dump list: {ex.Message}", "OK" );
        }
    }

    private async void OnShareClicked( object sender, EventArgs e )
    {
        try
        {
            if( sender is Button button && button.CommandParameter is CrashDumpFile crashDump )
            {
                await Share.RequestAsync( new ShareFileRequest
                {
                    Title = $"Share Crash Dump - {crashDump.DisplayName}",
                    File = new ShareFile( crashDump.FilePath )
                } );
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to share file: {ex.Message}", "OK" );
        }
    }

    private async void OnDeleteClicked( object sender, EventArgs e )
    {
        try
        {
            if( sender is Button button && button.CommandParameter is CrashDumpFile crashDump )
            {
                var confirm = await DisplayAlert( "Delete File",
                    $"Are you sure you want to delete {crashDump.DisplayName}?",
                    "Delete", "Cancel" );

                if( confirm && _delSolDevice.DebugService != null )
                {
                    var success = await _delSolDevice.DebugService.DeleteCrashDumpAsync( crashDump.FileName );
                    if( success )
                    {
                        await RefreshCrashDumpListAsync();
                    }
                    else
                    {
                        await DisplayAlert( "Error", "Failed to delete crash dump file.", "OK" );
                    }
                }
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to delete file: {ex.Message}", "OK" );
        }
    }

    private async void OnRebootClicked( object sender, EventArgs e )
    {
        try
        {
            if( !_delSolDevice.IsConnected )
            {
                await DisplayAlert( "Not Connected", "Please connect to your Del Sol device first from the Status tab.", "OK" );
                return;
            }

            if( _delSolDevice.DebugService == null )
            {
                await DisplayAlert( "Error", "Debug service not available. Please ensure your device supports debugging.", "OK" );
                return;
            }

            var confirm = await DisplayAlert( "Confirm Reboot", 
                "Are you sure you want to reboot the device? This will disconnect the BLE connection.", 
                "Reboot", "Cancel" );

            if( confirm )
            {
                RebootButton.IsEnabled = false;
                RebootButton.Text = "Rebooting...";

                var success = await _delSolDevice.DebugService.SendDebugControlCommandAsync( "REBOOT" );
                
                if( success )
                {
                    await DisplayAlert( "Success", "Reboot command sent successfully. The device will restart shortly.", "OK" );
                }
                else
                {
                    await DisplayAlert( "Error", "Failed to send reboot command.", "OK" );
                }
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to reboot device: {ex.Message}", "OK" );
        }
        finally
        {
            RebootButton.IsEnabled = true;
            RebootButton.Text = "Reboot Device";
        }
    }

    private async void OnClearClicked( object sender, EventArgs e )
    {
        try
        {
            if( !_delSolDevice.IsConnected )
            {
                await DisplayAlert( "Not Connected", "Please connect to your Del Sol device first from the Status tab.", "OK" );
                return;
            }

            if( _delSolDevice.DebugService == null )
            {
                await DisplayAlert( "Error", "Debug service not available. Please ensure your device supports debugging.", "OK" );
                return;
            }

            var confirm = await DisplayAlert( "Confirm Clear", 
                "Are you sure you want to clear the crash dump from the device? This action cannot be undone.", 
                "Clear", "Cancel" );

            if( confirm )
            {
                ClearButton.IsEnabled = false;
                ClearButton.Text = "Clearing...";

                var success = await _delSolDevice.DebugService.SendDebugControlCommandAsync( "CLEAR" );
                
                if( success )
                {
                    await DisplayAlert( "Success", "Crash dump cleared successfully.", "OK" );
                    
                    // Hide the clear button and download card since crash is cleared
                    ClearButton.IsVisible = false;
                    DownloadCard.IsVisible = false;
                    StatusResultFrame.IsVisible = false;
                }
                else
                {
                    await DisplayAlert( "Error", "Failed to clear crash dump.", "OK" );
                }
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to clear crash dump: {ex.Message}", "OK" );
        }
        finally
        {
            ClearButton.IsEnabled = true;
            ClearButton.Text = "Clear Crash Dump";
        }
    }

    private async void OnPrintClicked( object sender, EventArgs e )
    {
        try
        {
            if( !_delSolDevice.IsConnected )
            {
                await DisplayAlert( "Not Connected", "Please connect to your Del Sol device first from the Status tab.", "OK" );
                return;
            }

            if( _delSolDevice.DebugService == null )
            {
                await DisplayAlert( "Error", "Debug service not available. Please ensure your device supports debugging.", "OK" );
                return;
            }

            PrintButton.IsEnabled = false;
            PrintButton.Text = "Printing...";

            var success = await _delSolDevice.DebugService.SendDebugControlCommandAsync( "PRINT" );
            
            if( success )
            {
                await DisplayAlert( "Success", "Print test log command sent successfully. Check the device logs for output.", "OK" );
            }
            else
            {
                await DisplayAlert( "Error", "Failed to send print command.", "OK" );
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to send print command: {ex.Message}", "OK" );
        }
        finally
        {
            PrintButton.IsEnabled = true;
            PrintButton.Text = "Print Test Log";
        }
    }

    private async void OnCrashNowClicked( object sender, EventArgs e )
    {
        try
        {
            if( !_delSolDevice.IsConnected )
            {
                await DisplayAlert( "Not Connected", "Please connect to your Del Sol device first from the Status tab.", "OK" );
                return;
            }

            if( _delSolDevice.DebugService == null )
            {
                await DisplayAlert( "Error", "Debug service not available. Please ensure your device supports debugging.", "OK" );
                return;
            }

            var confirm = await DisplayAlert( "Confirm Crash Test", 
                "Are you sure you want to crash the device now? This will cause an immediate assert on the current thread for testing purposes.", 
                "Crash Now", "Cancel" );

            if( confirm )
            {
                CrashNowButton.IsEnabled = false;
                CrashNowButton.Text = "Crashing...";

                var success = await _delSolDevice.DebugService.SendDebugControlCommandAsync( "ASSERT" );
                
                if( success )
                {
                    await DisplayAlert( "Success", "Crash command sent successfully. The device should assert immediately.", "OK" );
                }
                else
                {
                    await DisplayAlert( "Error", "Failed to send crash command.", "OK" );
                }
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to send crash command: {ex.Message}", "OK" );
        }
        finally
        {
            CrashNowButton.IsEnabled = true;
            CrashNowButton.Text = "Crash Now";
        }
    }

    private async void OnCrashLaterClicked( object sender, EventArgs e )
    {
        try
        {
            if( !_delSolDevice.IsConnected )
            {
                await DisplayAlert( "Not Connected", "Please connect to your Del Sol device first from the Status tab.", "OK" );
                return;
            }

            if( _delSolDevice.DebugService == null )
            {
                await DisplayAlert( "Error", "Debug service not available. Please ensure your device supports debugging.", "OK" );
                return;
            }

            var confirm = await DisplayAlert( "Confirm Crash Test", 
                "Are you sure you want to schedule a crash? This will cause an assert on the main thread for testing purposes.", 
                "Crash Later", "Cancel" );

            if( confirm )
            {
                CrashLaterButton.IsEnabled = false;
                CrashLaterButton.Text = "Scheduling...";

                var success = await _delSolDevice.DebugService.SendDebugControlCommandAsync( "ASSERT_LATER" );
                
                if( success )
                {
                    await DisplayAlert( "Success", "Crash command sent successfully. The device will assert on the main thread shortly.", "OK" );
                }
                else
                {
                    await DisplayAlert( "Error", "Failed to send crash command.", "OK" );
                }
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", $"Failed to send crash command: {ex.Message}", "OK" );
        }
        finally
        {
            CrashLaterButton.IsEnabled = true;
            CrashLaterButton.Text = "Crash Later";
        }
    }
}