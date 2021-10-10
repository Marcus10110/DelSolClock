using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Text;

namespace DelSolClockApp.Services
{
	public static class Logger
	{
		public static ObservableCollection<String> Logs = new ObservableCollection<string>();

		public static void WriteLine(String message)
		{
			Debug.WriteLine(message);
			Logs.Add($"[{DateTime.Now.ToLongTimeString()}] {message}");
		}
	}
}
