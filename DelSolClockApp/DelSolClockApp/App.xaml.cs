using DelSolClockApp.Services;
using DelSolClockApp.Views;
using System;
using Xamarin.Forms;
using Xamarin.Forms.Xaml;

namespace DelSolClockApp
{
	public partial class App : Application
	{
		public DelSolConnection DelSol = null;

		public App()
		{
			InitializeComponent();

			DependencyService.Register<MockDataStore>();
			MainPage = new AppShell();
		}

		protected override void OnStart()
		{
			DelSol = new DelSolConnection();
			DelSol.Begin();
		}

		protected override void OnSleep()
		{
			DelSol.End();
		}

		protected override void OnResume()
		{
			DelSol.Begin();
		}
	}
}
