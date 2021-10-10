using DelSolClockApp.Services;
using Plugin.BLE;
using System;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Windows.Input;
using Xamarin.Essentials;
using Xamarin.Forms;



namespace DelSolClockApp.ViewModels
{
	public class AboutViewModel : BaseViewModel
	{
		public ObservableCollection<String> Logs { get; }

		private DelSolConnection DelSol = null;

		private void Log(String message)
		{
			Debug.WriteLine(message);
			Logs.Add(message);
		}
		public AboutViewModel()
		{
			Title = "About";
			Logs = new ObservableCollection<String>();
			OpenWebCommand = new Command(async () => await Browser.OpenAsync("https://aka.ms/xamarin-quickstart"));
			RecheckCommand = new Command(async () =>
			{
			});

			StopCommand = new Command(async () =>
			{
				if (DelSol != null)
				{
					DelSol.End();
				}
			});
			RunCommand = new Command(async () =>
			{
				Log("Starting run");
				if (DelSol != null)
				{
					await DelSol.Begin();
					return;
				}
				DelSol = new DelSolConnection();
				DelSol.Connected += (sender, args) =>
				{
					Log("Del Sol Connected Event");
				};

				DelSol.Disconnected += (sender, args) =>
				{
					Log("Del Sol Disconnected Event");
				};

				DelSol.StatusChanged += (sender, args) =>
				{
					Log($"status changed! {args.Status}");
				};
				await DelSol.Begin();
				Log("run started");

			});
			TestCommand = new Command(async () =>
			{
				Log("Test Command");
				var ble_permissions_service = DependencyService.Get<IBluetoothPermissionService>();
				var ble_status = await ble_permissions_service.CheckPermissionAsync();
				Log($"BLE status (initial) {ble_status}");
				if (ble_status == PermissionStatus.Unknown)
				{
					var ask_result = await ble_permissions_service.RequestPermissionAsync();
					Log($"BLE status (after prompt) {ask_result}");
				}

				var ble = CrossBluetoothLE.Current;
				var adapter = ble.Adapter;
				Log("Got bluetooth adapter");
				var state = ble.State;
				Log($"BLE State {state}");
				foreach (var dev in adapter.ConnectedDevices)
				{
					Log($"Existing connection: {dev.Name}");
				}
				ble.StateChanged += (s, e) =>
				{
					Log($"BLE State Changed to {e.NewState}");
				};

				adapter.DeviceDiscovered += async (s, a) =>
				{
					Log($"Device discovered, {a.Device.Name}. state: {a.Device.State}");
					if (a.Device.Name == "Del Sol")
					{

						// We got it!
						try
						{
							Log("attempting to connect to Del Sol...");
							await adapter.ConnectToDeviceAsync(a.Device);
							Log("connected!");
							try
							{
								var services = await a.Device.GetServicesAsync();
								foreach (var service in services)
								{
									Log($"service: {service.Id}: {service.Name}");
								}
							}
							catch (Exception ex)
							{
								Log($"failed to get services: {ex.Message}");
							}
							try
							{
								await adapter.StopScanningForDevicesAsync();
							}
							catch (Exception ex)
							{
								Log($"failed to stop scan: {ex.Message}");
							}

						}
						catch (Exception e)
						{
							Log("error connecting to DelSol");
							Log(e.Message);
						}
					}
				};

				adapter.ScanMode = Plugin.BLE.Abstractions.Contracts.ScanMode.Balanced;
				Log($"Checking for Service ID: {DelSolClockApp.Services.DelSolConnection.DelSolVehicleServiceGuid}");
				Log("checking for existing devices...");
				var known_devices = adapter.GetSystemConnectedOrPairedDevices(new Guid[] { DelSolClockApp.Services.DelSolConnection.DelSolVehicleServiceGuid });
				Log($"found known device count: {known_devices.Count}");
				foreach (var dev in known_devices)
				{
					Log(dev.Name);
					Log(dev.State.ToString());
				}
				Log("done. starting scan...");
				adapter.StartScanningForDevicesAsync(new Guid[] { DelSolClockApp.Services.DelSolConnection.DelSolVehicleServiceGuid });
			});
		}

		public ICommand OpenWebCommand { get; }
		public ICommand TestCommand { get; }

		public ICommand RecheckCommand { get; }
		public ICommand RunCommand { get; }
		public ICommand StopCommand { get; }
	}

}