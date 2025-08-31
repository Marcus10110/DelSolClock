using RestSharp;
using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using System.Text.Json.Serialization;

namespace DelSolClockAppMaui.Services;

public class FirmwareBrowser
{
    public class Release
    {
        [JsonPropertyName( "url" )]
        public string Url { get; set; }

        [JsonPropertyName( "html_url" )]
        public string HtmlUrl { get; set; }

        [JsonPropertyName( "tag_name" )]
        public string TagName { get; set; }

        [JsonPropertyName( "name" )]
        public string Name { get; set; }

        [JsonPropertyName( "body" )]
        public string Body { get; set; }

        [JsonPropertyName( "created_at" )]
        public DateTime CreatedAt { get; set; }

        [JsonPropertyName( "published_at" )]
        public DateTime PublishedAt { get; set; }

        [JsonPropertyName( "assets_url" )]
        public string AssetsUrl { get; set; }

        [JsonPropertyName( "assets" )]
        public List<Asset> Assets { get; set; }

        public class Asset
        {
            [JsonPropertyName( "name" )]
            public string Name { get; set; }

            [JsonPropertyName( "size" )]
            public int Size { get; set; }

            [JsonPropertyName( "browser_download_url" )]
            public string BrowserDownloadUrl { get; set; }
        }
    }

    public static async Task<List<Release>> FetchReleasesAsync( string repoOwner, string repoName )
    {
        var options = new RestClientOptions( "https://api.github.com" )
        {
            ThrowOnAnyError = true
        };
        var client = new RestClient( options );

        var request = new RestRequest( $"/repos/{repoOwner}/{repoName}/releases" )
            .AddHeader( "User-Agent", "DelSolClockAppMaui" );

        var response = await client.GetAsync<List<Release>>( request );
        return response ?? [];
    }
}
