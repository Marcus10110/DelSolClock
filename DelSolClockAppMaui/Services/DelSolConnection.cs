using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DelSolClockAppMaui.Services
{
    internal class DelSolConnection
    {
        public static readonly Guid DelSolVehicleServiceGuid = new Guid( "8fb88487-73cf-4cce-b495-505a4b54b802" );
        public static readonly Guid DelSolStatusCharacteristicGuid = new Guid( "40d527f5-3204-44a2-a4ee-d8d3c16f970e" );
        public static readonly Guid DelSolBatteryCharacteristicGuid = new Guid( "5c258bb8-91fc-43bb-8944-b83d0edc9b43" );

        public static readonly Guid DelSolLocationServiceGuid = new Guid( "61d33c70-e3cd-4b31-90d8-a6e14162fffd" );
        public static readonly Guid DelSolNavigationServiceGuid = new Guid( "77f5d2b5-efa1-4d55-b14a-cc92b72708a0" );

        public static readonly Guid DelSolFirmwareServiceGuid = new Guid( "69da0f2b-43a4-4c2a-b01d-0f11564c732b" );
        public static readonly Guid DelSolFirmwareWriteCharacteristicGuid = new Guid( "7efc013a-37b7-44da-8e1c-06e28256d83b" );
        public static readonly Guid DelSolFirmwareVersionCharacteristicGuid = new Guid( "a5c0d67a-9576-47ea-85c6-0347f8473cf3" );

        public bool Connected
        {
            get
            {
                // TODO
                return true;
            }
        }

        public Dictionary<string, string> GetData() => new()
        {
            [ "Key1" ] = "Value1",
            [ "Key2" ] = "Value2"
        };
        public List<string> GetLogs() => new() { "Log line 1", "Log line 2" };
        public void StartFirmwareUpdate( string url ) { /* stub */ }

    }
}
