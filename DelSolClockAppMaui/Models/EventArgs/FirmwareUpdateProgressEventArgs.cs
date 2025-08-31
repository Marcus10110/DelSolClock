using System;

namespace DelSolClockAppMaui.Models.EventArgs;

public class FirmwareUpdateProgressEventArgs : System.EventArgs
{
    public double PercentComplete { get; set; }
    public string Message { get; set; } = string.Empty;
    public double SpeedBytesPerSecond { get; set; }
    public TimeSpan EstimatedTimeRemaining { get; set; }
    public bool IsCompleted { get; set; }
    public string? ErrorMessage { get; set; }

    public FirmwareUpdateProgressEventArgs() { }

    public FirmwareUpdateProgressEventArgs(double percentComplete, string message)
    {
        PercentComplete = percentComplete;
        Message = message;
    }
}