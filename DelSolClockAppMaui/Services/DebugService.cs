using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;
using DelSolClockAppMaui.Models;
using Plugin.BLE.Abstractions.Contracts;
using Plugin.BLE.Abstractions.EventArgs;

namespace DelSolClockAppMaui.Services;

public class DebugService : IDisposable
{
    private readonly ILogger<DebugService> _logger;
    private IDevice? _device;
    private IService? _debugService;
    private ICharacteristic? _debugStatusCharacteristic;
    private bool _disposed = false;
    private readonly string _crashDumpsDirectory;

    public DebugService(ILogger<DebugService> logger)
    {
        _logger = logger;
        _crashDumpsDirectory = Path.Combine(FileSystem.Current.AppDataDirectory, "crash_dumps");
    }

    public async Task<bool> InitializeAsync(IDevice device)
    {
        try
        {
            _device = device;
            _logger.LogInformation("Initializing Debug service for device {DeviceId}", device.Id);

            // Get debug service
            _debugService = await _device.GetServiceAsync(DelSolConstants.DebugServiceGuid);
            if (_debugService == null)
            {
                _logger.LogWarning("Debug service not found on device");
                return false;
            }

            // Get debug status characteristic
            _debugStatusCharacteristic = await _debugService.GetCharacteristicAsync(DelSolConstants.DebugStatusCharacteristicGuid);
            if (_debugStatusCharacteristic == null)
            {
                _logger.LogWarning("Debug status characteristic not found");
                return false;
            }

            // Ensure crash dumps directory exists
            if (!Directory.Exists(_crashDumpsDirectory))
            {
                Directory.CreateDirectory(_crashDumpsDirectory);
                _logger.LogInformation("Created crash dumps directory: {Directory}", _crashDumpsDirectory);
            }

            _logger.LogInformation("Debug service initialized successfully");
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to initialize debug service");
            return false;
        }
    }

    public async Task<CrashDumpStatus> CheckCrashDumpStatusAsync()
    {
        try
        {
            if (_debugStatusCharacteristic == null)
            {
                _logger.LogWarning("Debug status characteristic not available");
                return new CrashDumpStatus { HasCrash = false, SizeBytes = 0 };
            }

            _logger.LogInformation("Reading crash dump status");
            var result = await _debugStatusCharacteristic.ReadAsync();
            var statusString = Encoding.UTF8.GetString(result.data);
            
            _logger.LogInformation("Received debug status: {Status}", statusString);

            if (statusString.StartsWith("CRASH:", StringComparison.OrdinalIgnoreCase))
            {
                // Parse size from "CRASH:<size bytes in base 10>"
                var sizeString = statusString.Substring(6); // Remove "CRASH:" prefix
                if (int.TryParse(sizeString, out int sizeBytes))
                {
                    return new CrashDumpStatus { HasCrash = true, SizeBytes = sizeBytes };
                }
                else
                {
                    _logger.LogWarning("Failed to parse crash dump size from: {StatusString}", statusString);
                    return new CrashDumpStatus { HasCrash = true, SizeBytes = 0 };
                }
            }
            else if (statusString.Equals("NOCRASH", StringComparison.OrdinalIgnoreCase))
            {
                return new CrashDumpStatus { HasCrash = false, SizeBytes = 0 };
            }
            else
            {
                _logger.LogWarning("Unknown debug status format: {StatusString}", statusString);
                return new CrashDumpStatus { HasCrash = false, SizeBytes = 0 };
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error checking crash dump status");
            return new CrashDumpStatus { HasCrash = false, SizeBytes = 0 };
        }
    }

    public async Task<bool> DownloadCrashDumpAsync(string firmwareVersion, IProgress<int>? progress = null)
    {
        try
        {
            // Generate filename with firmware version and timestamp
            var timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
            var fileName = $"crash_v{firmwareVersion}_{timestamp}.dump";
            var filePath = Path.Combine(_crashDumpsDirectory, fileName);

            _logger.LogInformation("Starting stubbed crash dump download to: {FilePath}", filePath);

            // Stubbed implementation - simulate download progress
            var totalSteps = 10;
            for (int i = 0; i <= totalSteps; i++)
            {
                await Task.Delay(200); // Simulate work
                var progressPercent = (i * 100) / totalSteps;
                progress?.Report(progressPercent);
                _logger.LogDebug("Download progress: {Progress}%", progressPercent);
            }

            // Create the stubbed file
            var stubContent = "TODO: implement crash download\n\nThis is a placeholder file created by the stubbed crash dump download functionality.";
            await File.WriteAllTextAsync(filePath, stubContent);

            _logger.LogInformation("Stubbed crash dump saved to: {FilePath}", filePath);
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error during crash dump download");
            return false;
        }
    }

    public async Task<List<CrashDumpFile>> GetSavedCrashDumpsAsync()
    {
        try
        {
            var crashDumpFiles = new List<CrashDumpFile>();

            if (!Directory.Exists(_crashDumpsDirectory))
            {
                return crashDumpFiles;
            }

            var files = Directory.GetFiles(_crashDumpsDirectory, "*.dump", SearchOption.TopDirectoryOnly);
            
            foreach (var filePath in files)
            {
                var fileName = Path.GetFileName(filePath);
                var fileInfo = new FileInfo(filePath);
                
                // Parse firmware version from filename (crash_v1.2.3_20240831_143022.dump)
                var firmwareVersion = "Unknown";
                if (fileName.StartsWith("crash_v") && fileName.Contains("_"))
                {
                    var parts = fileName.Split('_');
                    if (parts.Length >= 2 && parts[0].Length > 7)
                    {
                        firmwareVersion = parts[0].Substring(7); // Remove "crash_v" prefix
                    }
                }

                crashDumpFiles.Add(new CrashDumpFile
                {
                    FileName = fileName,
                    FilePath = filePath,
                    DownloadDate = fileInfo.CreationTime,
                    FirmwareVersion = firmwareVersion,
                    FileSizeBytes = fileInfo.Length
                });
            }

            // Sort by download date, newest first
            crashDumpFiles.Sort((a, b) => b.DownloadDate.CompareTo(a.DownloadDate));

            _logger.LogInformation("Found {Count} saved crash dump files", crashDumpFiles.Count);
            return crashDumpFiles;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error retrieving saved crash dumps");
            return new List<CrashDumpFile>();
        }
    }

    public async Task<bool> DeleteCrashDumpAsync(string fileName)
    {
        try
        {
            var filePath = Path.Combine(_crashDumpsDirectory, fileName);
            
            if (File.Exists(filePath))
            {
                File.Delete(filePath);
                _logger.LogInformation("Deleted crash dump file: {FileName}", fileName);
                return true;
            }
            else
            {
                _logger.LogWarning("Crash dump file not found: {FileName}", fileName);
                return false;
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error deleting crash dump file: {FileName}", fileName);
            return false;
        }
    }

    public void Dispose()
    {
        if (!_disposed)
        {
            _debugService?.Dispose();
            _disposed = true;
        }
    }
}