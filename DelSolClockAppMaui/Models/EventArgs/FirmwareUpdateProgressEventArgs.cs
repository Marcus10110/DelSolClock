using System;

namespace DelSolClockAppMaui.Models.EventArgs;

public class FirmwareUpdateProgressEventArgs : System.EventArgs
{
    public double PercentComplete { get; set; }
    public double SpeedBytesPerSecond { get; set; }
    public TimeSpan EstimatedTimeRemaining { get; set; }
    public bool IsCompleted { get; set; }
    public string? ErrorMessage { get; set; }
}