using DelSolClockApp.ViewModels;
using System.ComponentModel;
using Xamarin.Forms;

namespace DelSolClockApp.Views
{
	public partial class ItemDetailPage : ContentPage
	{
		public ItemDetailPage()
		{
			InitializeComponent();
			BindingContext = new ItemDetailViewModel();
		}
	}
}