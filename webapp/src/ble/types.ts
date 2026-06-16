// Domain models for the Del Sol Clock, mirroring DelSolClockAppMaui/Models.

export interface VehicleStatus {
  /** Rear window is rolled down. */
  rearWindowDown: boolean;
  /** Trunk is open. */
  trunkOpen: boolean;
  /** Convertible roof is down (stowed). */
  convertibleRoofDown: boolean;
  /** Ignition is on. */
  ignitionOn: boolean;
  /** Headlights are on. */
  headlightsOn: boolean;
  /** When this status was received by the app. */
  lastUpdated: Date;
}

export interface BatteryStatus {
  /** Battery voltage in volts. */
  voltage: number;
  /** When this reading was received by the app. */
  timestamp: Date;
}

export type ConnectionState =
  | 'disconnected'
  | 'requesting'
  | 'connecting'
  | 'connected';
