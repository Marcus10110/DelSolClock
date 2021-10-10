using Android.App;
using Android.Content;
using Android.OS;
using Android.Runtime;
using Android.Views;
using Android.Widget;
using DelSolClockApp.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Xamarin.Essentials;
using Xamarin.Forms;

[assembly: Dependency(typeof(DelSolClockApp.Droid.BluetoothPermissionService))]
namespace DelSolClockApp.Droid
{
	public class BluetoothPermissionService : IBluetoothPermissionService
	{
		public async Task<PermissionStatus> CheckPermissionAsync()
		{
			return PermissionStatus.Unknown;
		}
		public async Task<PermissionStatus> RequestPermissionAsync()
		{
			return PermissionStatus.Unknown;
		}
	}
}