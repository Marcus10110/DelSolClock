using CoreBluetooth;
using DelSolClockApp.Services;
using Foundation;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UIKit;
using Xamarin.Essentials;
using Xamarin.Forms;

[assembly: Dependency(typeof(DelSolClockApp.iOS.BluetoothPermissionService))]
namespace DelSolClockApp.iOS
{
	public class BluetoothPermissionService : IBluetoothPermissionService
	{
		public async Task<PermissionStatus> CheckPermissionAsync()
		{
			switch (CBCentralManager.Authorization)
			{
				case CBManagerAuthorization.AllowedAlways:
					return PermissionStatus.Granted;
				case CBManagerAuthorization.Restricted:
					return PermissionStatus.Restricted;
				case CBManagerAuthorization.NotDetermined:
					return PermissionStatus.Unknown;
				default:
					return PermissionStatus.Denied;
			}
		}

		public async Task<PermissionStatus> RequestPermissionAsync()
		{
			// Initializing CBCentralManager will present the Bluetooth permission dialog.
			new CBCentralManager();
			PermissionStatus status;
			do
			{
				status = await this.CheckPermissionAsync();
				await Task.Delay(200);
			} while (status == PermissionStatus.Unknown);
			return status;
		}
	}
}