using System;
using System.Collections.Generic;

namespace DelSolClockAppMaui.Models;

public class VehicleStatus
{
    public bool RearWindowDown { get; set; }
    public bool TrunkOpen { get; set; }
    public bool ConvertibleRoofDown { get; set; }
    public bool IgnitionOn { get; set; }
    public bool HeadlightsOn { get; set; }
    
    public DateTime LastUpdated { get; set; } = DateTime.Now;
    
    public override string ToString()
    {
        var items = new List<string>();
        items.Add($"Window: {(RearWindowDown ? "DOWN" : "UP")}");
        items.Add($"Trunk: {(TrunkOpen ? "OPEN" : "CLOSED")}");
        items.Add($"Roof: {(ConvertibleRoofDown ? "DOWN" : "UP")}");
        items.Add($"Ignition: {(IgnitionOn ? "ON" : "OFF")}");
        items.Add($"Lights: {(HeadlightsOn ? "ON" : "OFF")}");
        return string.Join(", ", items);
    }
}