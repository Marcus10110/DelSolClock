using System;

namespace DelSolClockAppMaui.Models.EventArgs;

public class ConnectionStatusChangedEventArgs : System.EventArgs
{
    public bool IsConnected { get; set; }
    public string? DeviceName { get; set; }
    public string? ErrorMessage { get; set; }
}