using System;

namespace DelSolClockAppMaui.Models.EventArgs;

public class BatteryStatusChangedEventArgs : System.EventArgs
{
    public float Voltage { get; set; }
    public DateTime Timestamp { get; set; } = DateTime.Now;
}