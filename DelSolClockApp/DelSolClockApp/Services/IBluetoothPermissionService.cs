using System;
using System.Collections.Generic;
using System.Text;
using System.Threading.Tasks;
using Xamarin.Essentials;

namespace DelSolClockApp.Services
{
	// MSDN docs on DI: https://docs.microsoft.com/en-us/xamarin/xamarin-forms/app-fundamentals/dependency-service/introduction
	public interface IBluetoothPermissionService
	{
		Task<PermissionStatus> CheckPermissionAsync();
		Task<PermissionStatus> RequestPermissionAsync();
	}
}
