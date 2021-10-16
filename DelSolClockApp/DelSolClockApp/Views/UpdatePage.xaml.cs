﻿using DelSolClockApp.Services;
using DelSolClockApp.ViewModels;
using RestSharp;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
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
		public ObservableCollection<ReleaseBrowser.Release> Releases { get; }

		public Command RefreshCommand { get; }

		public Command<ReleaseBrowser.Release> UploadRelease { get; }

		public UpdateViewModel()
		{
			Releases = new ObservableCollection<ReleaseBrowser.Release>();
			RefreshCommand = new Command(async () =>
			{
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
