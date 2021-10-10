using DelSolClockApp.Models;
using Plugin.BLE;
using Plugin.BLE.Abstractions.Contracts;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Xamarin.Essentials;
using Xamarin.Forms;

namespace DelSolClockApp.Services
{
	public class DelSolConnection
	{
		public class BluetoothDeniedException : Exception
		{

		}

		public class StatusChangedEventArgs : System.EventArgs
		{
			public CarStatus Status { get; set; }
		}

		public static readonly Guid DelSolVehicleServiceGuid = new Guid("8fb88487-73cf-4cce-b495-505a4b54b802");
		public static readonly Guid DelSolStatusCharacteristicGuid = new Guid("40d527f5-3204-44a2-a4ee-d8d3c16f970e");
		public static readonly Guid DelSolBatteryCharacteristicGuid = new Guid("5c258bb8-91fc-43bb-8944-b83d0edc9b43");

		public static readonly Guid DelSolLocationServiceGuid = new Guid("61d33c70-e3cd-4b31-90d8-a6e14162fffd");
		public static readonly Guid DelSolNavigationServiceGuid = new Guid("77f5d2b5-efa1-4d55-b14a-cc92b72708a0");

		private IDevice DelSolDevice = null;
		private ICharacteristic StatusCharacteristic = null;
		private IAdapter Adapter = null;
		private bool Active = false;

		public EventHandler<StatusChangedEventArgs> StatusChanged;
		public EventHandler Connected;
		public EventHandler Disconnected;

		private void StartAsyncTimer(TimeSpan period, Func<Task<bool>> callback)
		{
			Action<TimeSpan> run = null;
			run = (TimeSpan adjusted_period) =>
			{
				Device.StartTimer(adjusted_period, () =>
				{
					DateTime next = DateTime.Now + period;
					Task.Run(async () =>
					{
						bool run_again = await callback();

						if (run_again)
						{
							TimeSpan delay = next - DateTime.Now;
							if (delay < TimeSpan.FromSeconds(0))
							{
								delay = TimeSpan.FromSeconds(0);
							}
							run(delay);
						}
					});
					return false;
				});
			};
			// run the first time.
			run(period);
		}


		public async Task Begin()
		{
			if (Active)
			{
				return;
			}
			Active = true;
			// check for permissions first.
			var ble_permissions_service = DependencyService.Get<IBluetoothPermissionService>();
			var ble_status = await ble_permissions_service.CheckPermissionAsync();
			if (ble_status != PermissionStatus.Granted)
			{
				var ask_result = await ble_permissions_service.RequestPermissionAsync();
				if (ask_result != PermissionStatus.Granted)
				{
					Logger.WriteLine("Bluetooth not allowed");
					Active = false;
					return;
				}
			}

			CrossBluetoothLE.Current.StateChanged += Ble_StateChanged;
			Adapter = CrossBluetoothLE.Current.Adapter;
			Adapter.DeviceConnected += Adapter_DeviceConnected;
			Adapter.DeviceDisconnected += Adapter_DeviceDisconnected;
			Adapter.DeviceConnectionLost += Adapter_DeviceConnectionLost;

			await Service();

			StartConnectionTimer();
		}

		public void End()
		{
			if (!Active)
			{
				return;
			}
			Active = false;
			CrossBluetoothLE.Current.StateChanged -= Ble_StateChanged;
			Adapter.DeviceConnected -= Adapter_DeviceConnected;
			Adapter.DeviceDisconnected -= Adapter_DeviceDisconnected;
			Adapter.DeviceConnectionLost -= Adapter_DeviceConnectionLost;
			Adapter = null;
			CleanupDevice();
		}

		public bool IsConnected
		{
			get
			{
				return Active && DelSolDevice != null && DelSolDevice.State == Plugin.BLE.Abstractions.DeviceState.Connected;
			}
		}

		public CarStatus GetStatus()
		{
			if (!IsConnected || StatusCharacteristic == null)
			{
				return null;
			}
			return ParseStatus(StatusCharacteristic.Value);
		}
		private async Task Service()
		{

			if (DelSolDevice != null && DelSolDevice.State == Plugin.BLE.Abstractions.DeviceState.Disconnected)
			{
				Logger.WriteLine("known device no longer connected. clearing.");
				CleanupDevice();
			}

			if (DelSolDevice != null)
			{
				Logger.WriteLine($"known device looks valid. nothing to do: {DelSolDevice.State}");
				return;
			}

			var known_devices = CrossBluetoothLE.Current.Adapter.GetSystemConnectedOrPairedDevices(new Guid[] { DelSolClockApp.Services.DelSolConnection.DelSolVehicleServiceGuid });
			// known devices should actively be connected to the phone.
			if (known_devices.Count > 0)
			{
				try
				{
					DelSolDevice = known_devices[0];
					if (DelSolDevice.State != Plugin.BLE.Abstractions.DeviceState.Connected)
					{
						try
						{
							Logger.WriteLine($"connecting to {DelSolDevice.Name}");
							await CrossBluetoothLE.Current.Adapter.ConnectToDeviceAsync(DelSolDevice);
						}
						catch (Exception ex)
						{
							Logger.WriteLine($"failed to connect to device: {ex.Message}");
							return;
						}
					}
					if (DelSolDevice.State != Plugin.BLE.Abstractions.DeviceState.Connected)
					{
						throw new Exception("device should be connected here.");
					}
					Logger.WriteLine($"Del Sol connected. {DelSolDevice.Name}, {DelSolDevice.State}");
					var vehicle_service = await DelSolDevice.GetServiceAsync(DelSolVehicleServiceGuid);
					if (vehicle_service == null)
					{
						throw new Exception("Vehicle Serivce missing");
					}
					StatusCharacteristic = await vehicle_service.GetCharacteristicAsync(DelSolStatusCharacteristicGuid);
					if (StatusCharacteristic == null)
					{
						throw new Exception("Status Characteristic missing");
					}
					StatusCharacteristic.ValueUpdated += Status_characteristic_ValueUpdated;
					await StatusCharacteristic.StartUpdatesAsync();
					Logger.WriteLine($"Initial Status: {System.Text.Encoding.Default.GetString(StatusCharacteristic.Value)}");
					Connected?.Invoke(this, new EventArgs());
					StatusChangedEventArgs args = new StatusChangedEventArgs();
					args.Status = ParseStatus(StatusCharacteristic.Value);
					StatusChanged?.Invoke(this, args);
				}
				catch (Exception ex)
				{
					Logger.WriteLine($"Error probing device. {ex.Message}");
				}
			}
			else
			{
				Logger.WriteLine("device not found");
			}
		}

		private CarStatus ParseStatus(byte[] value)
		{
			CarStatus status = new CarStatus();
			if (value == null) return null;
			String str = System.Text.Encoding.Default.GetString(value);
			var items = str.Split(new char[] { ',' });
			if (items.Length != 5) return null;
			status.RearWindow = int.Parse(items[0]) != 0;
			status.Trunk = int.Parse(items[1]) != 0;
			status.Roof = int.Parse(items[2]) != 0;
			status.Ignition = int.Parse(items[3]) != 0;
			status.Lights = int.Parse(items[4]) != 0;

			return status;
		}

		private bool IsConnectionTimerNeeded
		{
			get
			{
				return Active && (DelSolDevice == null || DelSolDevice.State != Plugin.BLE.Abstractions.DeviceState.Connected);
			}
		}

		private void StartConnectionTimer()
		{
			if (!IsConnectionTimerNeeded) return;
			StartAsyncTimer(TimeSpan.FromSeconds(3), async () =>
			{
				if (!IsConnectionTimerNeeded)
				{
					return false;
				}

				await Service();

				return IsConnectionTimerNeeded;
			});
		}

		private void Status_characteristic_ValueUpdated(object sender, Plugin.BLE.Abstractions.EventArgs.CharacteristicUpdatedEventArgs e)
		{
			Logger.WriteLine($"Status update: {e.Characteristic.Value}");
			StatusChangedEventArgs args = new StatusChangedEventArgs();
			args.Status = ParseStatus(e.Characteristic.Value);
			StatusChanged?.Invoke(this, args);
		}

		private void CleanupDevice()
		{
			if (StatusCharacteristic != null)
			{
				StatusCharacteristic.ValueUpdated -= Status_characteristic_ValueUpdated;
				StatusCharacteristic = null;

			}
			DelSolDevice = null;
		}

		private void Adapter_DeviceConnectionLost(object sender, Plugin.BLE.Abstractions.EventArgs.DeviceErrorEventArgs e)
		{
			if (e.Device == DelSolDevice)
			{
				Logger.WriteLine($"DelSol device lost");
				CleanupDevice();
				Disconnected(this, new EventArgs());
				StartConnectionTimer();
			}
		}

		private void Adapter_DeviceDisconnected(object sender, Plugin.BLE.Abstractions.EventArgs.DeviceEventArgs e)
		{
			if (e.Device == DelSolDevice)
			{
				Logger.WriteLine($"device disconnected: {e.Device.Name}");
				CleanupDevice();
				Disconnected(this, new EventArgs());
				StartConnectionTimer();
			}
		}

		private void Adapter_DeviceConnected(object sender, Plugin.BLE.Abstractions.EventArgs.DeviceEventArgs e)
		{
			Logger.WriteLine($"device connected: {e.Device.Name}");
		}

		private void Ble_StateChanged(object sender, Plugin.BLE.Abstractions.EventArgs.BluetoothStateChangedArgs e)
		{
			Logger.WriteLine($"BLE State Changed. {e.OldState} -> {e.NewState}");
		}
	}
}
