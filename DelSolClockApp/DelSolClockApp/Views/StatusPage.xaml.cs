using DelSolClockApp.Models;
using DelSolClockApp.ViewModels;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Xamarin.Forms;
using Xamarin.Forms.Xaml;

namespace DelSolClockApp
{
	public struct CarStatusListItem
	{
		public String Label { get; set; }
		public String Value { get; set; }
	}
	public class CarStatusConverter : IValueConverter
	{
		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{

			List<CarStatusListItem> items = new List<CarStatusListItem>();
			if (value == null) return items;
			CarStatus car_status = value as CarStatus;
			items.Add(new CarStatusListItem { Label = "Ignition", Value = car_status.Ignition ? "Running" : "Off" });
			items.Add(new CarStatusListItem { Label = "Lights", Value = car_status.Lights ? "On" : "Off" });
			items.Add(new CarStatusListItem { Label = "Rear Window", Value = car_status.RearWindow ? "Open" : "Closed" });
			items.Add(new CarStatusListItem { Label = "Trunk", Value = car_status.Trunk ? "Open" : "Closed" });
			items.Add(new CarStatusListItem { Label = "Roof", Value = car_status.Roof ? "Unlatched" : "Latched" });
			return items;
		}

		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			throw new NotImplementedException();
		}
	}
}


namespace DelSolClockApp.Views
{
	[XamlCompilation(XamlCompilationOptions.Compile)]
	public partial class StatusPage : ContentPage
	{
		StatusViewModel _viewModel;
		public StatusPage()
		{
			InitializeComponent();

			BindingContext = _viewModel = new StatusViewModel();
		}
	}
}
