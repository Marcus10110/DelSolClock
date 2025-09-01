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
                StatusResultFrame.BackgroundColor = Colors.LightCoral;
            }
            else
            {
                DownloadCard.IsVisible = false;
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

            var success = await _delSolDevice.DebugService!.DownloadCrashDumpAsync( _delSolDevice.FirmwareVersion, progress );

            if( success )
            {
                await DisplayAlert( "Success", "Crash dump downloaded successfully!", "OK" );

                // Hide download section and refresh list
                DownloadCard.IsVisible = false;
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
}