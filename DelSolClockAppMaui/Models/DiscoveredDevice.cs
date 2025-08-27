using System;
using System.ComponentModel;
using Plugin.BLE.Abstractions.Contracts;

namespace DelSolClockAppMaui.Models;

public class DiscoveredDevice : INotifyPropertyChanged
{
    private bool _isConnecting = false;

    public string Name { get; set; } = string.Empty;
    public string Id { get; set; } = string.Empty;
    public Guid DeviceId { get; set; }
    public bool IsKnownDevice { get; set; }
    public int SignalStrength { get; set; }
    public DateTime LastSeen { get; set; } = DateTime.Now;
    
    public bool IsConnecting
    {
        get => _isConnecting;
        set
        {
            if (_isConnecting != value)
            {
                _isConnecting = value;
                OnPropertyChanged();
            }
        }
    }
    
    // Internal reference to Plugin.BLE device
    internal IDevice BleDevice { get; set; } = null!;
    
    public event PropertyChangedEventHandler? PropertyChanged;

    protected virtual void OnPropertyChanged([System.Runtime.CompilerServices.CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
    
    public override string ToString() => $"{Name} ({(IsKnownDevice ? "Known" : "New")})";
}