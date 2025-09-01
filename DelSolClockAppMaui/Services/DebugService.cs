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
    private ICharacteristic? _debugDataCharacteristic;
    private ICharacteristic? _debugControlCharacteristic;
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

            // Get debug data characteristic
            _debugDataCharacteristic = await _debugService.GetCharacteristicAsync(DelSolConstants.DebugDataCharacteristicGuid);
            if (_debugDataCharacteristic == null)
            {
                _logger.LogWarning("Debug data characteristic not found");
                return false;
            }

            // Get debug control characteristic
            _debugControlCharacteristic = await _debugService.GetCharacteristicAsync(DelSolConstants.DebugControlCharacteristicGuid);
            if (_debugControlCharacteristic == null)
            {
                _logger.LogWarning("Debug control characteristic not found");
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
                // Parse size and chunk count from "CRASH:<size bytes in base 10>:<chunk count>"
                var crashInfo = statusString.Substring(6); // Remove "CRASH:" prefix
                var parts = crashInfo.Split(':');
                
                if (parts.Length >= 1 && int.TryParse(parts[0], out int sizeBytes))
                {
                    var chunkCount = 0;
                    if (parts.Length >= 2 && int.TryParse(parts[1], out int parsedChunkCount))
                    {
                        chunkCount = parsedChunkCount;
                    }
                    else
                    {
                        _logger.LogWarning("Failed to parse chunk count from: {StatusString}, defaulting to 0", statusString);
                    }
                    
                    return new CrashDumpStatus { HasCrash = true, SizeBytes = sizeBytes, ChunkCount = chunkCount };
                }
                else
                {
                    _logger.LogWarning("Failed to parse crash dump size from: {StatusString}", statusString);
                    return new CrashDumpStatus { HasCrash = true, SizeBytes = 0, ChunkCount = 0 };
                }
            }
            else if (statusString.Equals("NOCRASH", StringComparison.OrdinalIgnoreCase))
            {
                return new CrashDumpStatus { HasCrash = false, SizeBytes = 0, ChunkCount = 0 };
            }
            else
            {
                _logger.LogWarning("Unknown debug status format: {StatusString}", statusString);
                return new CrashDumpStatus { HasCrash = false, SizeBytes = 0, ChunkCount = 0 };
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error checking crash dump status");
            return new CrashDumpStatus { HasCrash = false, SizeBytes = 0, ChunkCount = 0 };
        }
    }

    public async Task<bool> DownloadCrashDumpAsync(string firmwareVersion, int chunkCount, IProgress<int>? progress = null)
    {
        try
        {
            if (_debugDataCharacteristic == null)
            {
                _logger.LogError("Debug data characteristic not available");
                return false;
            }

            if (chunkCount <= 0)
            {
                _logger.LogError("Invalid chunk count: {ChunkCount}", chunkCount);
                return false;
            }

            // Generate filename with firmware version and timestamp
            var timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
            var fileName = $"crash_v{firmwareVersion}_{timestamp}.dump";
            var filePath = Path.Combine(_crashDumpsDirectory, fileName);

            _logger.LogInformation("Starting crash dump download to: {FilePath}, expecting {ChunkCount} chunks", filePath, chunkCount);

            // Track received chunks
            var receivedChunks = new Dictionary<int, byte[]>();
            var totalDataSize = 0;

            // Read all chunks
            for (int expectedChunk = 0; expectedChunk < chunkCount; expectedChunk++)
            {
                var result = await _debugDataCharacteristic.ReadAsync();
                if (result.data == null || result.data.Length < 2)
                {
                    _logger.LogError("Invalid chunk data received for chunk {ExpectedChunk}", expectedChunk);
                    return false;
                }

                // Parse chunk index (first 2 bytes, little endian)
                var chunkIndex = BitConverter.ToUInt16(result.data, 0);
                var chunkData = new byte[result.data.Length - 2];
                Array.Copy(result.data, 2, chunkData, 0, chunkData.Length);

                _logger.LogDebug("Read chunk {ChunkIndex}, expected {ExpectedChunk}, data size: {DataSize}", 
                    chunkIndex, expectedChunk, chunkData.Length);

                // Validate chunk index
                if (chunkIndex != expectedChunk)
                {
                    _logger.LogError("Chunk index mismatch: expected {ExpectedChunk}, got {ActualChunk}", 
                        expectedChunk, chunkIndex);
                    return false;
                }

                // Check for duplicate chunks
                if (receivedChunks.ContainsKey(chunkIndex))
                {
                    _logger.LogWarning("Duplicate chunk {ChunkIndex} received, ignoring", chunkIndex);
                    continue;
                }

                // Store chunk data
                receivedChunks[chunkIndex] = chunkData;
                totalDataSize += chunkData.Length;

                // Report progress
                var progressPercent = ((expectedChunk + 1) * 100) / chunkCount;
                progress?.Report(progressPercent);
                _logger.LogDebug("Download progress: {Progress}% ({Current}/{Total} chunks)", 
                    progressPercent, expectedChunk + 1, chunkCount);
            }

            // Validate all chunks received
            for (int i = 0; i < chunkCount; i++)
            {
                if (!receivedChunks.ContainsKey(i))
                {
                    _logger.LogError("Missing chunk {ChunkIndex}", i);
                    return false;
                }
            }

            // Combine all chunks into final data
            var finalData = new byte[totalDataSize];
            var offset = 0;
            for (int i = 0; i < chunkCount; i++)
            {
                var chunkData = receivedChunks[i];
                Array.Copy(chunkData, 0, finalData, offset, chunkData.Length);
                offset += chunkData.Length;
            }

            // Write to file
            await File.WriteAllBytesAsync(filePath, finalData);

            _logger.LogInformation("Crash dump downloaded successfully: {FilePath}, {TotalBytes} bytes", 
                filePath, finalData.Length);
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

    public async Task<bool> SendDebugControlCommandAsync(string command)
    {
        try
        {
            if (_debugControlCharacteristic == null)
            {
                _logger.LogError("Debug control characteristic not available");
                return false;
            }

            if (string.IsNullOrWhiteSpace(command))
            {
                _logger.LogError("Invalid command: command cannot be null or empty");
                return false;
            }

            // Validate command
            var validCommands = new[] { "REBOOT", "CLEAR", "PRINT", "ASSERT", "ASSERT_LATER" };
            if (!validCommands.Contains(command.ToUpperInvariant()))
            {
                _logger.LogError("Invalid command: {Command}. Valid commands are: {ValidCommands}", 
                    command, string.Join(", ", validCommands));
                return false;
            }

            _logger.LogInformation("Sending debug control command: {Command}", command);

            // Convert command to bytes and write to characteristic
            var commandBytes = Encoding.UTF8.GetBytes(command.ToUpperInvariant());
            await _debugControlCharacteristic.WriteAsync(commandBytes);

            _logger.LogInformation("Debug control command sent successfully: {Command}", command);
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error sending debug control command: {Command}", command);
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