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
        }
        catch( Exception ex )
        {
            await DisplayAlert( "Error", ex.Message, "OK" );
        }
        finally
        {
            FirmwareRefreshView.IsRefreshing = false;
        }
    }

    private void OnUpdateClicked( object sender, EventArgs e )
    {
        if( sender is Button button && button.CommandParameter is string url && _ble.Connected )
        {
            _ble.StartFirmwareUpdate( url );
        }
    }

    private void OnViewOnlineClicked( object sender, EventArgs e )
    {
        if( sender is Button button && button.CommandParameter is string url )
        {
            try { Launcher.Default.OpenAsync( url ); } catch { }
        }
    }

    private void OnRefreshing( object sender, EventArgs e )
    {
        _loaded = false;
        LoadReleases();
    }

    public class UpdateItem
    {
        public string Name { get; set; }
        public string Body { get; set; }
        public DateTime PublishedAt { get; set; }
        public string AssetUrl { get; set; }
        public string HtmlUrl { get; set; }
    }
}
