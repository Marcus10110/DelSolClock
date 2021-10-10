using DelSolClockApp.Services;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Text;
using Xamarin.Forms;

namespace DelSolClockApp.ViewModels
{
	class LogViewModel : BaseViewModel
	{
		public ObservableCollection<String> Logs { get { return Logger.Logs; } }
		public LogViewModel()
		{
			Title = "Logs";
		}
	}
}
