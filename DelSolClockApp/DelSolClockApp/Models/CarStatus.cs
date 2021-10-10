using System;
using System.Collections.Generic;
using System.Text;

namespace DelSolClockApp.Models
{
	public class CarStatus
	{
		public bool RearWindow { get; set; }
		public bool Trunk { get; set; }
		public bool Roof { get; set; }
		public bool Ignition { get; set; }
		public bool Lights { get; set; }
		/*
		public float BatteryVoltage { get; set; }
		public float IlluminationVoltage { get; set; }
		public bool HourButton { get; set; }
		public bool MinuteButton { get; set; }
		*/
		public override string ToString()
		{
			String str = $"Window {(RearWindow ? "OPEN" : "closed")}\n";
			str += $"Trunk {(Trunk ? "OPEN" : "closed")}\n";
			str += $"Roof {(Roof ? "OFF" : "on")}\n";
			str += $"Ignition {(Ignition ? "running" : "off")}\n";
			str += $"Lights {(Lights ? "ON" : "off")}";
			return str;
		}
	}
}
