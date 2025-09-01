namespace DelSolClockAppMaui.Models;

public class CrashDumpFile
{
    public string FileName { get; set; } = string.Empty;
    public string FilePath { get; set; } = string.Empty;
    public DateTime DownloadDate { get; set; }
    public string FirmwareVersion { get; set; } = string.Empty;
    public long FileSizeBytes { get; set; }
    
    public string DisplayName => FileName;
    public string SizeText => $"{FileSizeBytes:N0} bytes";
    public string DateText => DownloadDate.ToString("yyyy-MM-dd HH:mm");
}