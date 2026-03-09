# Nomad2026

A modern reimplementation of the **Nomad** editor for the **Clavia Nord Modular G1** synthesizer,
built with JUCE/C++ as a cross-platform native application.

## About

The Nord Modular G1 is a legendary modular synthesizer from the late 1990s that is programmed
via a computer editor. The original Nomad editor was written in Java and is no longer compatible
with modern Java runtimes. Nomad2026 aims to bring this editor back to life as a native
application that runs on macOS, Windows, and Linux without requiring Java.

## Development Status

### Completed
- [x] Original editor running and verified (JDK 8)
- [x] Technical research and protocol documentation
- [x] JUCE project setup with CMake
- [x] MIDI SysEx protocol implementation (C++)
- [x] Synth connection manager (auto-connect, IAm handshake)
- [x] Patch retrieval from synth (RequestPatch + 13-section GetPatch flow)
- [x] Patch data parser (modules, cables, parameters, morphs, knob/ctrl maps, names, notes)
- [x] Module descriptions loader (110+ modules from modules.xml)
- [x] UI framework (menu bar, module browser, patch canvas, inspector, status bar)
- [x] MIDI settings dialog with port persistence
- [x] Pixel-perfect module rendering (classic-theme.xml: connectors, knobs, sliders, labels, text displays, lights)
- [x] Radio-selector buttons (multi-option with highlighted selection)
- [x] Increment/arrow buttons
- [x] Custom display renderers (ADSR/AD/AHD envelopes, LFO waveforms, filter response, overdrive/clip curves, EQ, compressor/expander, phaser)
- [x] Cables with signal-type colors, dark outline, rendered on top of modules
- [x] Bidirectional parameter control (click knobs/buttons/sliders to modify values)
- [x] Status bar parameter indicator (show parameter name + value on hover)
- [x] Real-time parameter changes sent to synth with MIDI feedback
- [x] Patch header bar (patch name, voices spinner, load meters, morph knobs with colors, cable visibility toggles)
- [x] Cable visibility toggles (click colored circles to hide/show cables by signal type)
- [x] Morph knobs with per-morph colors (red, green, blue, yellow — matching original editor)
- [x] Correct patch name and module name parsing (null-terminated PDL2 strings)
- [x] Patch file I/O (.pch load/save) with PchFileIO
- [x] Real-time patch synchronization (PatchSynchronizer: cables, modules, parameters sync to synth)
- [x] Protocol messages: NewCable, DeleteCable, MoveModule, StorePatch, GetPatchList, LoadPatch
- [x] **Synth Patch Browser** (right panel):
  - Interactive hierarchical tree (9 banks, 99 patches each = 891 total)
  - Real-time search filter by patch name
  - "Hide Empty" toggle to show only patches (not empty slots)
  - Refresh button to reload from synth
  - Double-click any patch to load it into current slot
  - Auto-loads patch list on connection

### In Progress
- [ ] Visual indicator for currently loaded patch in browser
- [ ] Context menu (right-click): Rename, Delete, Copy, Move patches

### Next Up
- [ ] Integrate patch browser with "Save to Synth" dialog
- [ ] Drag & drop modules on canvas
- [ ] Cable creation/deletion by clicking connectors
- [ ] Multi-slot support (slots 1-4, currently hardcoded to slot 0)

## Roadmap

This section outlines all planned features to achieve feature parity with the original Nomad editor. Features are organized by category.

### File Operations
- [ ] **New Patch** (Ctrl+N) - Create new empty patch
- [ ] **Open Patch** (Ctrl+O) - Load .pch file from disk
- [ ] **Close Patch** (Ctrl+W) - Close current patch
- [ ] **Close All Patches** - Close all open patches
- [ ] **Save Patch** (Ctrl+S) - Save current patch to disk
- [ ] **Save Patch As** - Save patch with new filename
- [ ] **Save All Patches** - Save all modified patches
- [ ] **Quit Application** (Ctrl+Q) - Exit Nomad2026

### Patch Management
- [ ] **Patch Settings Dialog** (Ctrl+P) - Edit patch metadata and configuration
- [ ] **Download Patch to Slot** - Send patch from editor to synth slot (A/B/C/D)
- [ ] **Save Patch in Synth** - Store patch in synth's internal memory
- [ ] **Multi-Slot Support** - Support all 4 slots (A, B, C, D) instead of just slot 0

### Synth Communication
- [ ] **Synth Settings Dialog** (Ctrl+G) - Configure synth parameters:
  - Synth name editing
  - MIDI channel assignment per slot (1-16)
  - MIDI clock (Internal/External, BPM, Global sync)
  - Master tune (cents, Hz display)
  - Keyboard mode (Active slot / Selected slots)
  - MIDI velocity scale (min/max 0-127)
  - Knob mode (Immediate / Hook)
  - Pedal polarity (Normal / Inverted)
  - Program change send/receive
  - Local off, LEDs active
- [ ] **Upload Active Slot** (Ctrl+U) - Upload current synth patch to editor
- [ ] **Send Controller Snapshot** - Send current controller state to synth
- [ ] **Bank Upload from Synth** - Upload entire bank (99 patches) from synth to disk
  - Bank selection dropdown
  - Destination folder browser
  - Progress bar
- [ ] **Bank Download to Synth** - Download entire bank from disk to synth
  - Source selection (bank file or folder)
  - Bank number selection
  - Progress bar with overwrite warning

### Editor Preferences
- [ ] **Editor Options Dialog** - Configure editor behavior:
  - **Cable Style**: Straight 3D, Curved 3D (default), Straight Thin, Curved Thin
  - **Knob Control**: Circular, Horizontal (default), Vertical
  - **Auto Upload**: Automatically send parameter changes to synth
  - **Recycle Windows**: Reuse patch windows instead of creating new ones

### Floating Windows
- [ ] **Keyboard Floater** (Ctrl+F) - Virtual MIDI keyboard
  - Octave navigation (<<, <, >, >>)
  - Drone mode (sustain notes)
  - Repeat mode
  - Visual key press feedback
- [ ] **Knob Floater** (Ctrl+K) - Hardware knob mapper
  - 18 assignable knobs with LED indicators
  - Morph group selection arrows
  - Displays current module/parameter assignments
  - Sustain pedal icon
  - Keyboard hold icon
  - Joystick/modulation wheel icon
- [ ] **Notes Floater** - Patch notes/comments window
- [ ] **Browser** (Ctrl+B) - Patch library browser
  - Search and filter patches
  - Preview/audition patches
  - Organize patch collections

### Module Canvas Editing
- [ ] **Drag & Drop Modules** - Add modules from browser to canvas by dragging
- [ ] **Move Modules** - Reposition modules on canvas
- [ ] **Delete Modules** - Remove modules from patch
- [ ] **Cable Creation** - Click connectors to create cables
- [ ] **Cable Deletion** - Click/right-click cables to delete
- [ ] **Module Copy/Paste** - Duplicate module configurations
- [ ] **Selection Tool** - Select multiple modules/cables for batch operations

### Help System
- [ ] **Help Contents** - Integrated help documentation
- [ ] **Using Help** - Meta-help for navigating help system
- [ ] **About Dialog** - Version info, credits, license

### Quality of Life
- [ ] **Verify Input/Output Connectors** - Ensure visual distinction between inputs and outputs
- [ ] **Module Rendering Polish** - Compare each module type with original editor for pixel-perfect accuracy
- [ ] **Undo/Redo System** - Full undo/redo for all patch operations
- [ ] **Keyboard Shortcuts** - Complete keyboard shortcut system matching original editor
- [ ] **Window Management** - Remember window positions and sizes across sessions

## Building

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run
./build/Nomad2026_artefacts/Debug/Nomad2026
```

Requires JUCE (included in `JUCE/` directory) and a C++17 compiler.

## Technical Documentation

See [RESEARCH.md](RESEARCH.md) for:
- Nord Modular MIDI SysEx protocol (v3.03)
- Patch file format (.pch) specification
- Module system (110+ modules with parameters, connectors, signals)
- PDL2 binary format description language
- Architecture overview of the original editor

## Credits

This project is a reimplementation based on the work of the original nmedit/Nomad developers:

| Person | Contribution |
|--------|-------------|
| **Marcus Andersson** | Reverse-engineered the Nord Modular MIDI protocol; C++ and Java protocol libraries |
| **Christian Schneider** | Nomad Java editor (v0.2, v0.3) |
| **Ian Hoogeboom** | Nomad v0.4 update, macOS compatibility |
| **Jan Punter** | Nord Modular patch file format documentation |
| **Jelle Herold** | Original project founder |
| **Stefan Keel** | Module SVG icon designs |
| **Tobias Weinald** | Splash screen artwork |

## Original Project

- **Website**: https://nmedit.sourceforge.net/
- **Source (v0.3)**: https://github.com/wesen/nmedit
- **Source (v0.4)**: https://github.com/Airell/nmedit

## License

This project is licensed under the [GNU General Public License v3](LICENSE)
(upgraded from v2 for JUCE AGPLv3 compatibility).
