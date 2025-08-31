# MAUI App Development TODO

## Current State Analysis
The .NET MAUI app has basic structure with 3 tabs (Status, Firmware, Logs) but most functionality is stubbed out:
- **DelSolConnection**: Has UUIDs defined but no actual BLE implementation
- **StatusPage**: Shows hardcoded "Connected" status and dummy data
- **UpdatePage**: Can fetch GitHub releases but firmware update is stubbed
- **LogPage**: Shows dummy logs
- **Dependencies**: Plugin.BLE and RestSharp already included

## Phase 1: Foundation & BLE Core Implementation

### 1.1 BLE Service Implementation
- **Priority**: HIGH
- **Effort**: 3-4 days
- Replace DelSolConnection stub with actual Plugin.BLE implementation
- Implement device discovery filtering by service UUID `8fb88487-73cf-4cce-b495-505a4b54b802`
- Add connection/disconnection logic with proper error handling
- Implement connection state management and events

### 1.2 Characteristic Reading Infrastructure
- **Priority**: HIGH  
- **Effort**: 2-3 days
- Implement reading from status characteristic `40d527f5-3204-44a2-a4ee-d8d3c16f970e`
- Implement reading from battery characteristic `5c258bb8-91fc-43bb-8944-b83d0edc9b43`
- Implement reading from firmware version characteristic `a5c0d67a-9576-47ea-85c6-0347f8473cf3`
- Add notification handling for status updates
- Parse status string format "0,0,0,1,1" into meaningful data

### 1.3 Connection Management UI
- **Priority**: HIGH
- **Effort**: 2 days
- Add device discovery/pairing screen or functionality
- Update StatusPage to show actual connection status
- Add reconnection logic and error handling
- Implement proper disposal patterns for BLE resources

## Phase 2: Status Screen Implementation

### 2.1 Status Data Display
- **Priority**: HIGH
- **Effort**: 2-3 days
- Replace dummy data in StatusPage with actual BLE readings
- Implement real-time updates from device notifications
- Display battery voltage (from float characteristic)
- Display all 5 boolean status indicators:
  - Rear Window (up/down)
  - Trunk (open/closed) 
  - Convertible roof (up/down)
  - Car ignition (on/off)
  - Headlights (on/off)

### 2.2 Status UI Improvements
- **Priority**: MEDIUM
- **Effort**: 1-2 days
- Improve status display with icons (already have some in Resources/Images/icons/)
- Add visual indicators for each status item
- Implement proper error states for disconnected device
- Add refresh functionality

## Phase 3: Firmware Update Implementation

### 3.1 Firmware Update Core Logic
- **Priority**: HIGH
- **Effort**: 4-5 days
- Implement firmware download from GitHub release assets
- Implement 512-byte chunked BLE writing to characteristic `7efc013a-37b7-44da-8e1c-06e28256d83b`
- Handle response parsing ("success", "continue", "error")
- Implement proper timeout and error handling
- Handle edge case: send zero-length value if firmware is exact multiple of 512 bytes

### 3.2 Firmware Update UI
- **Priority**: MEDIUM
- **Effort**: 2-3 days
- Add progress bar for firmware updates
- Implement cancel functionality
- Show current firmware version at top of UpdatePage
- Mark installed firmware versions in release list
- Add proper error dialogs and user feedback

### 3.3 GitHub Release Integration
- **Priority**: LOW (mostly done)
- **Effort**: 1 day
- FirmwareBrowser is mostly complete, minor refinements only
- Ensure proper asset filtering (.bin files)
- Add error handling for network issues

## Phase 4: Logs Implementation

### 4.1 Log Reading Infrastructure
- **Priority**: MEDIUM
- **Effort**: 3-4 days
- **Note**: Requires firmware changes - this phase may be delayed
- Research and implement log reading mechanism (circular buffer or notification characteristic)
- Parse log format: `[timestamp] [level] [file:line] message`
- Implement real-time log streaming if using notifications

### 4.2 Log Display and Management  
- **Priority**: MEDIUM
- **Effort**: 2 days
- Replace dummy logs with actual device logs
- Implement scrollable log display with proper formatting
- Add log saving functionality with date/time stamped filenames
- Implement log clearing functionality

## Phase 5: Polish & Platform-Specific Features

### 5.1 Cross-Platform Testing
- **Priority**: HIGH
- **Effort**: 3-4 days
- Test on Windows desktop with BLE adapter
- Verify BLE permissions on Android/iOS
- Test UI responsiveness across screen sizes
- Handle platform-specific BLE quirks

### 5.2 Error Handling & UX
- **Priority**: MEDIUM  
- **Effort**: 2 days
- Improve error messages and user guidance
- Add loading states and spinners
- Implement proper navigation patterns
- Add app settings/preferences if needed

### 5.3 Final Polish
- **Priority**: LOW
- **Effort**: 1-2 days
- Update app icons and branding
- Improve color scheme and styling
- Add about screen if needed
- Performance optimization

## Development Order Recommendations

**Sprint 1 (Week 1-2)**: Phase 1 (BLE Foundation)
- Critical foundation for all other features
- Must be solid before building other functionality

**Sprint 2 (Week 3)**: Phase 2 (Status Screen)  
- Builds directly on BLE foundation
- Provides immediate user value

**Sprint 3 (Week 4-5)**: Phase 3 (Firmware Updates)
- Most complex feature requiring careful implementation
- High user value once working

**Sprint 4 (Week 6)**: Phase 4 (Logs) - Optional/Future
- May require firmware changes
- Lower priority than other features

**Sprint 5 (Week 7)**: Phase 5 (Polish)
- Final testing and refinement
- Platform-specific adjustments

## Technical Notes

### Key Dependencies
- Plugin.BLE 3.2.0-beta.1 (already added)
- RestSharp 112.1.0 (already added)
- .NET MAUI targeting net9.0

### BLE Service/Characteristic UUIDs (from plan)
```
Main Service: 8fb88487-73cf-4cce-b495-505a4b54b802
Status Characteristic: 40d527f5-3204-44a2-a4ee-d8d3c16f970e  
Battery Characteristic: 5c258bb8-91fc-43bb-8944-b83d0edc9b43
Firmware Service: 69da0f2b-43a4-4c2a-b01d-0f11564c732b
Firmware Write: 7efc013a-37b7-44da-8e1c-06e28256d83b
Firmware Version: a5c0d67a-9576-47ea-85c6-0347f8473cf3
```

### Risk Areas
- BLE reliability across platforms (especially Windows)
- Firmware update protocol complexity and error handling
- Log streaming implementation (may need firmware changes)
- Cross-platform permission handling

## Estimated Total Timeline
**6-7 weeks** for full implementation, with core functionality (Phases 1-3) available in 4-5 weeks.