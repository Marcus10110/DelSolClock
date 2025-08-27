using System;

namespace DelSolClockAppMaui.Models.EventArgs;

public class VehicleStatusChangedEventArgs : System.EventArgs
{
    public VehicleStatus? Status { get; set; }
}