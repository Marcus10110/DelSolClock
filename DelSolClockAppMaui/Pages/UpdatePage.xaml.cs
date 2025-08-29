using Microsoft.Maui.Controls;
using DelSolClockAppMaui.Services;
using System.Collections.ObjectModel;
using System.Diagnostics;

namespace DelSolClockAppMaui.Pages;

public partial class UpdatePage : ContentPage
{
    private DelSolConnection _ble = new();
    private bool _loaded = false;

    public ObservableCollection<UpdateItem> Items { get; set; } = new();

    public UpdatePage()
    {
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
            // Check if device is connected (temporarily disabled until we integrate DelSolDevice)
            if( !_ble.Connected )
            {
                await DisplayAlert( "Not Connected", "Please connect to a device first on the Status page", "OK" );
                return;
            }
            
            // Show update progress
            UpdateProgressCard.IsVisible = true;
            UpdateProgressBar.Progress = 0;
            UpdateStatusLabel.Text = "Starting firmware update...";
            
            try
            {
                // TODO: Replace with DelSolDevice integration
                _ble.StartFirmwareUpdate( url );
            }
            catch( Exception ex )
            {
                await DisplayAlert( "Update Error", $"Failed to start firmware update: {ex.Message}", "OK" );
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

    public class UpdateItem
    {
        public required string Name { get; set; }
        public required string Body { get; set; }
        public DateTime PublishedAt { get; set; }
        public required string AssetUrl { get; set; }
        public required string HtmlUrl { get; set; }
    }
}
