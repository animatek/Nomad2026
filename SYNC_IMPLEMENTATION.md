# Real-Time Patch Synchronization Implementation

## Overview

Nomad2026 now implements **real-time patch synchronization** with the Nord Modular G1 synthesizer. Every edit you make in the UI (adding/deleting cables, moving modules) is immediately sent to the synthesizer via SysEx messages.

## Architecture

### 1. Protocol Messages (`source/protocol/`)

New message classes for patch editing operations:

| Class | Command | Purpose |
|-------|---------|---------|
| `SysExMessage` | Base class | Common SysEx operations (checksum, header/footer) |
| `NewCableMessage` | cc=0x17, sc=0x50 | Add a cable between connectors |
| `DeleteCableMessage` | cc=0x17, sc=0x51 | Remove a cable |
| `MoveModuleMessage` | cc=0x17, sc=0x34 | Update module position |
| `StorePatchMessage` | cc=0x17:0x41, sc=0x0b | Save patch to synth flash |

All messages follow the Nord Modular SysEx protocol:
- Header: `F0 33 <slot|06>`
- Payload: Command-specific bit-packed data
- Checksum: Sum of all bytes from F0 through payload, modulo 128
- Footer: `F7`

### 2. Event System (`source/model/Patch.h/cpp`)

The `Patch` model now fires events when modified:

**ModuleContainer events:**
- `CableAddedCallback` - Fired when `addConnection()` is called
- `CableRemovedCallback` - Fired when `removeConnection()` or `removeConnectionsForConnector()` is called

**Module events:**
- `ModuleMovedCallback` - Fired when `setPosition()` is called with a new position

### 3. Patch Synchronizer (`source/sync/PatchSynchronizer.h/cpp`)

The `PatchSynchronizer` class:
1. Listens to all patch events
2. Translates events into protocol messages
3. Sends messages to the synth via `ConnectionManager::sendRawSysEx()`

**Key features:**
- Automatically enabled when a patch is loaded and connection is active
- Automatically disabled on disconnect
- Only sends messages when connected
- Handles both poly and common voice areas

### 4. UI Integration

**MainComponent changes:**
- Creates `PatchSynchronizer` when:
  - Patch is loaded from synth (via request)
  - Patch is loaded from file (if connected)
  - Connection is established (if patch already loaded)
- Destroys synchronizer on disconnect

**Menu: Device → Send Patch to Synth**
- Sends `StorePatchMessage` to save current patch to synth flash
- Currently saves to current slot, bank 0, position 0
- Shows confirmation dialog after sending

### 5. ConnectionManager Enhancement

Added `sendRawSysEx()` method:
- Bypasses the NmProtocol queue system
- Used by PatchSynchronizer for immediate transmission
- Directly calls `MidiDeviceManager::sendSysEx()`

## Usage

### Real-Time Editing

1. **Connect to Synth:** File → Device → MIDI Settings
2. **Load a Patch:**
   - From synth: Device → Request Patch from Synth
   - From file: File → Open
3. **Edit the Patch:**
   - **Add Cable:** Drag from output connector to input connector
   - **Delete Cable:** Right-click a connector
   - **Move Module:** Drag module header
4. **Changes are sent immediately** to the synth (check debug log for confirmation)

### Saving to Permanent Memory

After editing:
1. **Device → Send Patch to Synth** (or Ctrl+Shift+S equivalent)
2. Confirmation dialog appears
3. Patch is written to synth flash memory
4. You can now power-cycle the synth and the patch will persist

**IMPORTANT:** Without "Send Patch to Synth", edits only exist in RAM. They will be lost if you:
- Switch patches on the synth
- Power cycle the synth
- Request a different patch

## Technical Details

### Cable Message Format

Both `NewCable` and `DeleteCable` use identical payload structure:

```
Byte 0: section:1 | color:4    (section: 0=common, 1=poly)
Byte 1: module1:7
Byte 2: type1:2 | connector1:6  (type: 0=input, 1=output)
Byte 3: module2:7
Byte 4: type2:2 | connector2:6
```

All values are 7-bit encoded (MSB = 0).

### Module Move Format

```
Byte 0: section:1 | 0:6
Byte 1: module:7
Byte 2: xpos:7
Byte 3: ypos:7
```

Position values are in grid units (see MEMORY.md for pixel conversion).

### Store Patch Format

```
Byte 0: slot:2 | section:1 | 0:5
Byte 1: position:7 | 0:1
```

Section value:
- 0 = Save both common and poly areas
- 1 = Save poly area only
- 2 = Save common area only (?)

## Testing

### Test 1: Cable Sync
1. Connect to synth
2. Request patch from synth
3. Add a cable in UI (drag output to input)
4. **Expected:** Debug log shows "Sent NewCable: section=..."
5. Delete cable (right-click connector)
6. **Expected:** Debug log shows "Sent DeleteCable: section=..."
7. Device → Send Patch to Synth
8. Device → Request Patch from Synth (reload)
9. **Expected:** Cables persist

### Test 2: Module Move Sync
1. Connect to synth
2. Load a patch
3. Drag a module to a new position
4. Release mouse
5. **Expected:** Debug log shows "Sent MoveModule: section=... pos=(...)"
6. Save to synth
7. Reload patch
8. **Expected:** Module position persists

### Test 3: Disconnected Behavior
1. Load patch from file (no synth)
2. Edit patch (add cables, move modules)
3. **Expected:** No errors, changes apply locally
4. Connect to synth
5. **Expected:** Synchronizer enables, future edits sync
6. Disconnect
7. **Expected:** Synchronizer disables cleanly

## Known Limitations

1. **Save destination:** Currently hardcoded to bank 0, position 0
   - **TODO:** Add dialog to choose bank and position
2. **Module add/delete:** Not yet implemented
   - Need UI for module browser integration
3. **Module rename:** Not yet implemented
   - Need UI for module title editing
4. **Undo/Redo:** Not yet implemented
   - Would require buffering sent messages
5. **Parameter sync:** Already implemented separately via `ConnectionManager::sendParameter()`

## Future Enhancements

### Priority 1: Save Destination Dialog
Allow user to choose:
- Bank (0-99 for user banks, or ROM banks)
- Position within bank
- Whether to save to current slot or different slot

### Priority 2: Module Management
- Add module browser with drag-and-drop
- Implement `NewModuleMessage` (cc=0x1f)
- Implement `DeleteModuleMessage` (cc=0x17, sc=0x32)
- Implement `SetModuleTitleMessage` (cc=0x17, sc=0x33)

### Priority 3: Advanced Features
- Undo/Redo with SysEx message buffering
- Batch operations (group multiple edits into single transaction)
- Conflict detection (synth edited externally during editing session)
- Patch diff viewer (show changes since last save)

## Debug Log Examples

Successful cable add:
```
Sent NewCable: section=1 modules=3->5 connectors=0->2
```

Successful module move:
```
Sent MoveModule: section=0 module=2 pos=(3,1)
```

Successful save:
```
Sent StorePatch to slot 0 position 0
```

Connection status:
```
Patch synchronizer enabled
Patch synchronizer disabled on disconnect
```

## Code Structure Summary

```
source/
├── protocol/              # SysEx message classes
│   ├── SysExMessage.cpp/h
│   ├── NewCableMessage.cpp/h
│   ├── DeleteCableMessage.cpp/h
│   ├── MoveModuleMessage.cpp/h
│   └── StorePatchMessage.cpp/h
├── sync/                  # Patch synchronization
│   └── PatchSynchronizer.cpp/h
├── model/
│   └── Patch.cpp/h        # Event callbacks added
├── midi/
│   └── ConnectionManager.cpp/h  # sendRawSysEx() added
└── MainComponent.cpp/h    # Integration + savePatchToSynth()
```

## References

- `RESEARCH.md` - Full protocol specification
- `MEMORY.md` - Project memory (lessons learned, protocol gotchas)
- `nmedit/libs/codecs/midi.pdl2` - Binary protocol spec
- Original Java: `nmedit/nomad/nomad-source/plugins/net.sf.nmedit.nordmodular/src/.../NmPatchSynchronizer.java`
