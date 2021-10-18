using DelSolClockApp.Services;
using DelSolClockApp.ViewModels;
using RestSharp;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

using Xamarin.Forms;
using Xamarin.Forms.Xaml;

namespace DelSolClockApp.Views
{
	public class ReleaseBrowser
	{
		public struct Release
		{
			public string url { get; set; }
			public string html_url { get; set; }

			public string tag_name { get; set; }
			public string name { get; set; }
			public string body { get; set; }
			public DateTime created_at { get; set; }
			public DateTime published_at { get; set; }

			public string assets_url { get; set; }

			public List<Asset> assets { get; set; }


			public struct Asset
			{
				public string name { get; set; }
				public int size { get; set; }
				public string browser_download_url { get; set; }
			}

		}

		public static async Task<List<Release>> FetchReleases()
		{
			var client = new RestClient("https://api.github.com/repos/Marcus10110/DelSolClock/releases");
			client.Timeout = -1;
			var request = new RestRequest(Method.GET);
			var response = await client.ExecuteAsync<List<Release>>(request);
			return response.Data;
		}
	}
	public class UpdateViewModel : BaseViewModel
	{
		static readonly HttpClient Client = new HttpClient();

		private DelSolConnection.FwUpdateProgress fwProgress = new DelSolConnection.FwUpdateProgress();
		public ObservableCollection<ReleaseBrowser.Release> Releases { get; }

		public Command RefreshCommand { get; }

		public Command<ReleaseBrowser.Release> UploadRelease { get; }

		public DelSolConnection.FwUpdateProgress FwProgress
		{
			get { return fwProgress; }
			set
			{
				SetProperty(ref fwProgress, value);
			}
		}

		public UpdateViewModel()
		{
			fwProgress.Started = false;
			Releases = new ObservableCollection<ReleaseBrowser.Release>();
			RefreshCommand = new Command(async () =>
			{
				Logger.WriteLine("Refreshing...");
				IsBusy = true;
				try
				{
					Releases.Clear();

					var releases = await ReleaseBrowser.FetchReleases();
					foreach (var release in releases)
					{
						Releases.Add(release);
					}
				}
				catch (Exception ex)
				{
					Logger.WriteLine($"Error fetching firmware releases:");
					Logger.WriteLine(ex.Message);
				}
				finally
				{
					IsBusy = false;
				}
			});

			UploadRelease = new Command<ReleaseBrowser.Release>(async (ReleaseBrowser.Release release) =>
			{
				Logger.WriteLine($"uploading {release.name}...");
				// download the *.bin file!
				System.IO.Stream fw_stream;
				try
				{
					var bin_url = release.assets.First(x => x.name.EndsWith(".bin")).browser_download_url;
					Logger.WriteLine($"downloading {bin_url}");
					var response = await Client.GetAsync(bin_url);
					response.EnsureSuccessStatusCode();
					fw_stream = await response.Content.ReadAsStreamAsync();
					Logger.WriteLine("response stream ready. Uploading...");
				}
				catch (Exception ex)
				{
					Logger.WriteLine($"failed to download fw image");
					Logger.WriteLine(ex.Message);
					return;
				}

				try
				{
					var app = App.Current as App;
					var del_sol = app.DelSol;
					if (!del_sol.IsConnected)
					{
						throw new Exception("Del Sol BLE not connected");
					}
					var result = await del_sol.UpdateFirmware(fw_stream, (progress) => { FwProgress = progress; });
					if (!result)
					{
						throw new Exception("UpdateFirmware returned false");
					}
				}
				catch (Exception ex)
				{
					Logger.WriteLine($"Error uploading FW: {ex.Message}");
				}

			});
		}

	}

	[XamlCompilation(XamlCompilationOptions.Compile)]
	public partial class UpdatePage : ContentPage
	{
		UpdateViewModel _viewModel;
		bool StartedFirstRefresh = false;
		public UpdatePage()
		{
			InitializeComponent();
			BindingContext = _viewModel = new UpdateViewModel();
		}

		protected override void OnAppearing()
		{
			base.OnAppearing();
			if (StartedFirstRefresh)
				return;
			StartedFirstRefresh = true;
			// setting IsBusy will cause the RefreshView to start refreshing.
			_viewModel.IsBusy = true;
		}

	}
}
