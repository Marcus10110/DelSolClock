using DelSolClockApp.Models;
using DelSolClockApp.Services;
using System;
using System.Collections.Generic;
using System.Globalization;
using Xamarin.Forms;


namespace DelSolClockApp.ViewModels
{
	class StatusViewModel : BaseViewModel
	{
		private bool connected = false;
		private CarStatus carStatus = new CarStatus();

		public bool Connected
		{
			get { return connected; }
			set
			{
				SetProperty(ref connected, value);
			}
		}

		public CarStatus CarStatus
		{
			get { return carStatus; }
			set
			{
				SetProperty(ref carStatus, value);
			}
		}
		public StatusViewModel()
		{
			Title = "Status";

			var app = App.Current as App;
			if (app == null)
			{
				Logger.WriteLine("App is null");
				throw new Exception("app not available");
			}
			var del_sol = app.DelSol;
			if (del_sol == null)
			{
				Logger.WriteLine("Del Sol is Null");
				throw new Exception("del sol not available");
			}

			Connected = del_sol.IsConnected;
			CarStatus = del_sol.GetStatus();

			del_sol.Connected += (sender, args) =>
			{
				Connected = true;
			};
			del_sol.Disconnected += (sender, args) =>
			{
				Connected = false;
			};
			del_sol.StatusChanged += (sender, args) =>
			{
				CarStatus = args.Status;
			};
		}
	}
}
