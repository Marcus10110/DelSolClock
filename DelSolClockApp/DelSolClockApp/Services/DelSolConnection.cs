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

		public class ConnectedEventArgs : System.EventArgs
		{
			public string Version { get; set; }
		}

		public struct FwUpdateProgress
		{
			public double PercentComplete { get; set; }
			public double SecondsRemaining { get; set; }
			public double SpeedBps { get; set; }
			public bool Started { get; set; }

		}

		public static readonly Guid DelSolVehicleServiceGuid = new Guid("8fb88487-73cf-4cce-b495-505a4b54b802");
		public static readonly Guid DelSolStatusCharacteristicGuid = new Guid("40d527f5-3204-44a2-a4ee-d8d3c16f970e");
		public static readonly Guid DelSolBatteryCharacteristicGuid = new Guid("5c258bb8-91fc-43bb-8944-b83d0edc9b43");

		public static readonly Guid DelSolLocationServiceGuid = new Guid("61d33c70-e3cd-4b31-90d8-a6e14162fffd");
		public static readonly Guid DelSolNavigationServiceGuid = new Guid("77f5d2b5-efa1-4d55-b14a-cc92b72708a0");

		public static readonly Guid DelSolFirmwareServiceGuid = new Guid("69da0f2b-43a4-4c2a-b01d-0f11564c732b");
		public static readonly Guid DelSolFirmwareWriteCharacteristicGuid = new Guid("7efc013a-37b7-44da-8e1c-06e28256d83b");
		public static readonly Guid DelSolFirmwareVersionCharacteristicGuid = new Guid("a5c0d67a-9576-47ea-85c6-0347f8473cf3");

		private IDevice DelSolDevice = null;
		private ICharacteristic StatusCharacteristic = null;
		private ICharacteristic WriteCharacteristic = null;
		private IAdapter Adapter = null;
		private bool Active = false;

		public EventHandler<StatusChangedEventArgs> StatusChanged;
		public EventHandler<ConnectedEventArgs> Connected;
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

		private TaskCompletionSource<string> WriteResultTask = null;

		public async Task<bool> UpdateFirmware(System.IO.Stream bin, Action<FwUpdateProgress> percent_update_cb)
		{
			if (!IsConnected) throw new Exception("unable to update firmware, not connected");
			if (WriteCharacteristic == null) throw new Exception("write characteristic is missing");

			DateTime upload_start = DateTime.Now;
			long total_size = bin.Length;
			const int max_length = 512; // matches firmware.
			byte[] buffer = new byte[max_length];
			int read = 0;
			long bytes_written = 0;
			do
			{
				read = await bin.ReadAsync(buffer, 0, max_length);

				byte[] write_buffer = new byte[read];
				Array.Copy(buffer, write_buffer, read);
				WriteResultTask = new TaskCompletionSource<string>();
				var write_start = DateTime.Now;
				var write_success = await WriteCharacteristic.WriteAsync(write_buffer);
				if (!write_success)
				{
					throw new Exception("failed to write firmware, write failed");
				}
				bytes_written += read;
				var write_success_time = DateTime.Now - write_start;
				string result = await WriteResultTask.Task;
				var response_time = DateTime.Now - write_start;
				var elapsed_time = DateTime.Now - upload_start;



				if (result == "continue")
				{
					double percent_done = 100 * (double)bytes_written / (double)total_size;
					Logger.WriteLine($"wrote {read}, total {bytes_written}. {percent_done}%");
					double average_bps = (double)bytes_written / elapsed_time.TotalSeconds;
					double seconds_remaining = (total_size - bytes_written) / average_bps;
					Logger.WriteLine($"{Math.Round(average_bps)}bps. write time: {write_success_time.TotalSeconds}, response time: {response_time.TotalSeconds} elapsed: {Math.Round(elapsed_time.TotalSeconds)}");
					FwUpdateProgress update = new FwUpdateProgress { SpeedBps = average_bps, PercentComplete = percent_done, SecondsRemaining = seconds_remaining, Started = true };
					percent_update_cb(update);
				}
				else if (result == "success")
				{
					Logger.WriteLine("upload success!");
					return true;
				}
				else if (result == "error")
				{
					Logger.WriteLine($"FW upload error after section of {read}, total {bytes_written}");
					return false;
				}
				else
				{
					throw new Exception($"could not understand response: \"{result}\"");
				}

			} while (read > 0);
			throw new Exception($"reached the end of the FW update file, but did not get a finished response. total: {bytes_written}");
		}

		private void WriteCharacteristic_ValueUpdated(object sender, Plugin.BLE.Abstractions.EventArgs.CharacteristicUpdatedEventArgs e)
		{

			string value = Encoding.Default.GetString(e.Characteristic.Value);
			if (WriteResultTask != null && !WriteResultTask.Task.IsCompleted)
			{
				Logger.WriteLine($"write characteristic notified: {value}, setting result.");
				WriteResultTask.SetResult(value);
			}
			else
			{
				Logger.WriteLine($"Value changed without a valid task: {value}");
			}
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

					// Firmware Update
					var firmware_service = await DelSolDevice.GetServiceAsync(DelSolFirmwareServiceGuid);
					if (firmware_service == null)
					{
						throw new Exception("Firmware Serivce missing");
					}
					var version_characteristic = await firmware_service.GetCharacteristicAsync(DelSolFirmwareVersionCharacteristicGuid);
					if (version_characteristic == null)
					{
						throw new Exception("version characteristic missing");
					}
					String version = System.Text.Encoding.Default.GetString(await version_characteristic.ReadAsync());
					Logger.WriteLine($"current FW version: {version}");

					WriteCharacteristic = await firmware_service.GetCharacteristicAsync(DelSolFirmwareWriteCharacteristicGuid);
					if (WriteCharacteristic == null)
					{
						throw new Exception("write characteristic missing");
					}
					WriteCharacteristic.ValueUpdated += WriteCharacteristic_ValueUpdated;
					await WriteCharacteristic.StartUpdatesAsync();

					Connected?.Invoke(this, new ConnectedEventArgs { Version = version });
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
			String str = Encoding.Default.GetString(value);
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
