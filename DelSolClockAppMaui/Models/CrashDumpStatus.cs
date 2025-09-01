namespace DelSolClockAppMaui.Models;

public class CrashDumpStatus
{
    public bool HasCrash { get; set; }
    public int SizeBytes { get; set; }
    
    public string StatusMessage => HasCrash ? $"Crash found: {SizeBytes:N0} bytes" : "No crash dumps found";
}