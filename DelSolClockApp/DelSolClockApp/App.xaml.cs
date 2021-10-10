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
			DelSol = new DelSolConnection();
			DependencyService.Register<MockDataStore>();
			MainPage = new AppShell();
		}

		protected override void OnStart()
		{
			Logger.WriteLine("App OnStart");
			DelSol.Begin();
		}

		protected override void OnSleep()
		{
			Logger.WriteLine("App OnSleep");
			DelSol.End();
		}

		protected override void OnResume()
		{
			Logger.WriteLine("App OnResume");
			DelSol.Begin();
		}
	}
}
