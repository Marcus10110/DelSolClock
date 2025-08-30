using Microsoft.Maui.Controls;
using DelSolClockAppMaui.Services;
using DelSolClockAppMaui.Models.EventArgs;
using System.Collections.ObjectModel;
using System.Diagnostics;

namespace DelSolClockAppMaui.Pages;

public partial class UpdatePage : ContentPage
{
    private readonly DelSolDevice _delSolDevice;
    private bool _loaded = false;

    public ObservableCollection<UpdateItem> Items { get; set; } = new();

    public UpdatePage(DelSolDevice delSolDevice)
    {
        _delSolDevice = delSolDevice;
        InitializeComponent();
        FirmwareCollection.ItemsSource = Items;
    }

    protected override void OnAppearing()
    {
        base.OnAppearing();
        if( !_loaded )
        {
            _loaded = true;
            LoadReleases();
        }
        // Subscribe to firmware update progress
        _delSolDevice.FirmwareUpdateProgress += OnFirmwareUpdateProgress;
    }

    private async void LoadReleases()
    {
        Debug.WriteLine( "Loading releases..." );
        
        // Show loading state
        LoadingCard.IsVisible = true;
        LoadingSpinner.IsRunning = true;
        EmptyStateCard.IsVisible = false;
        RefreshButton.IsEnabled = false;
        
        try
        {
            Items.Clear();
            var releases = await FirmwareBrowser.FetchReleasesAsync( "Marcus10110", "DelSolClock" );
            Debug.WriteLine( releases );
            
            foreach( var r in releases )
            {
                var asset = r.Assets.FirstOrDefault( a => a.Name.EndsWith( ".bin" ) );
                if( asset != null )
                {
                    Items.Add( new UpdateItem
                    {
                        Name = r.Name,
                        Body = r.Body,
                        PublishedAt = r.PublishedAt,
                        AssetUrl = asset.BrowserDownloadUrl,
                        HtmlUrl = r.HtmlUrl,
                    } );
                }
            }
            
            // Show empty state if no releases found
            if( Items.Count == 0 )
            {
                EmptyStateCard.IsVisible = true;
            }
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error Loading Releases", ex.Message, "OK" );
            EmptyStateCard.IsVisible = true;
        }
        finally
        {
            // Hide loading state
            LoadingCard.IsVisible = false;
            LoadingSpinner.IsRunning = false;
            RefreshButton.IsEnabled = true;
        }
    }

    private async void OnUpdateClicked( object sender, EventArgs e )
    {
        if( sender is Button button && button.CommandParameter is string url )
        {
            // Check if device is connected
            if( !_delSolDevice.IsConnected )
            {
                await DisplayAlert( "Not Connected", "Please connect to a device first on the Status page", "OK" );
                return;
            }
            
            // Show update progress
            UpdateProgressCard.IsVisible = true;
            UpdateProgressBar.Progress = 0;
            UpdateStatusLabel.Text = "Starting firmware update...";
            
            // Disable refresh button during update
            RefreshButton.IsEnabled = false;
            
            try
            {
                var success = await _delSolDevice.StartFirmwareUpdateAsync( url );
                
                if( !success )
                {
                    await DisplayAlert( "Update Failed", "Firmware update failed. Check the logs for details.", "OK" );
                }
                else
                {
                    await DisplayAlert( "Update Successful", "Firmware update completed successfully!", "OK" );
                }
            }
            catch( Exception ex )
            {
                await DisplayAlert( "Update Error", $"Failed to start firmware update: {ex.Message}", "OK" );
            }
            finally
            {
                // Re-enable refresh button
                RefreshButton.IsEnabled = true;
                
                // Hide progress after a delay if update completed
                await Task.Delay(3000);
                UpdateProgressCard.IsVisible = false;
            }
        }
    }

    private async void OnViewOnlineClicked( object sender, EventArgs e )
    {
        if( sender is Button button && button.CommandParameter is string url )
        {
            try 
            { 
                await Launcher.Default.OpenAsync( url ); 
            } 
            catch( Exception ex )
            {
                await DisplayAlert( "Error", $"Could not open URL: {ex.Message}", "OK" );
            }
        }
    }

    private void OnRefreshClicked( object sender, EventArgs e )
    {
        _loaded = false;
        LoadReleases();
    }

    private void OnFirmwareUpdateProgress(object? sender, FirmwareUpdateProgressEventArgs e)
    {
        MainThread.BeginInvokeOnMainThread(() =>
        {
            try
            {
                var progressValue = e.PercentComplete / 100.0;
                Debug.WriteLine($"Firmware progress update: {e.PercentComplete}% - {e.Message}");
                
                UpdateProgressBar.Progress = progressValue;
                UpdateStatusLabel.Text = e.Message;
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Error updating progress UI: {ex.Message}");
            }
        });
    }
    
    protected override void OnDisappearing()
    {
        base.OnDisappearing();

        // Unsubscribe from events
        _delSolDevice.FirmwareUpdateProgress -= OnFirmwareUpdateProgress;
    }

    public class UpdateItem
    {
        public required string Name { get; set; }
        public required string Body { get; set; }
        public DateTime PublishedAt { get; set; }
        public required string AssetUrl { get; set; }
        public required string HtmlUrl { get; set; }
    }
}
