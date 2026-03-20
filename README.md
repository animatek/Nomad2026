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
  - Context menu (right-click): Copy, Move, Delete patch operations
- [x] **Patch Name Editing**: Double-click patch name in header bar to rename (max 15 chars, syncs to synth immediately)
- [x] **Quick Save Button**: Diskette icon next to patch name — saves current patch back to its original bank location
- [x] **Universal Location Selector**: Consistent Slot/Bank/Position dialog for all save/copy/move operations
  - Position dropdown shows patch names from selected bank
  - Updates dynamically when bank selection changes
  - Shows "01: PatchName" or "01: --" for empty slots
- [x] **Help Menu**: Links to Nord Modular forum, Facebook group, and patch archive
- [x] **About Menu**: Links to Patreon, GitHub source code, and project website
- [x] **Send Patch to Synth** (Device menu): Full 16-section SysEx upload of editor patch to synth working slot
  - Serializes patch to PDL2 binary (modules, cables, parameters, morphs, knobs, controls, names, notes)
  - Variable-width parameter encoding derived from module descriptors
  - Sequential section upload (one section at a time, waits for ACK before sending next — matches Java protocol)
  - Optional "Store to Bank" dialog after upload
- [x] **New Patch** (File menu): Creates empty Init Patch in editor
- [x] **Split Poly/Common Canvas**: Two independent scrollable panels (Poly top, Common bottom), each 128 rows tall
- [x] **Slot Tabs (1-4)**: Tab bar at top of main layout for the 4 synth slots
- [x] **Multi-module selection**:
  - Rubber-band drag on empty canvas area to select multiple modules
  - Shift+click to add/remove modules from selection
  - Visual highlight: yellow border + semi-transparent overlay on selected modules
- [x] **Multi-module move**: Drag selected group maintaining relative positions
- [x] **Copy/Paste modules** (Ctrl+C / Ctrl+V): Copies modules with their parameters and internal cables
- [x] **Duplicate** (context menu): Duplicate selection with or without internal cables
- [x] **QuickAdd popup**: Press Space or double-click empty canvas → searchable module list, Enter/click to add
- [x] **Parameter context menu** (right-click knob/slider/button): Assign/remove Morph Group (1-4)
- [x] **MorphAssignmentMessage**: New protocol message — assigns a parameter to a morph group
- [x] **MorphRangeChangeMessage**: New protocol message — changes morph range/direction for a parameter
- [x] **Inspector morph integration**: onMorphGroupChanged / onMorphRangeChanged callbacks sync changes live to synth

### In Progress
- [ ] Visual indicator for currently loaded patch in browser
- [ ] Module drag & drop from browser to canvas

### Next Up
- [ ] Multi-slot support (load/save per slot using slot tabs)
- [ ] Undo/Redo system
- [ ] Cable creation/deletion by clicking connectors

## Roadmap

This section outlines all planned features to achieve feature parity with the original Nomad editor. Features are organized by category.

### File Operations
- [x] **New Patch** (Ctrl+N) - Create new empty patch
- [x] **Open Patch** (Ctrl+O) - Load .pch file from disk
- [ ] **Close Patch** (Ctrl+W) - Close current patch
- [ ] **Close All Patches** - Close all open patches
- [x] **Save Patch** (Ctrl+S) - Save current patch to disk
- [x] **Save Patch As** - Save patch with new filename
- [ ] **Save All Patches** - Save all modified patches
- [x] **Quit Application** (Ctrl+Q) - Exit Nomad2026

### Patch Management
- [ ] **Patch Settings Dialog** (Ctrl+P) - Edit patch metadata and configuration
- [x] **Send Patch to Slot** - Upload editor patch to synth working slot (full 16-section PDL2 upload)
- [x] **Save Patch in Synth** - Store uploaded patch to a bank location
- [x] **New Patch** - Create a new empty patch in the editor
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
- [x] **Move Modules** - Reposition modules on canvas (single and multi-move)
- [x] **Delete Modules** - Remove modules from patch (Delete key or context menu)
- [ ] **Cable Creation** - Click connectors to create cables
- [ ] **Cable Deletion** - Click/right-click cables to delete
- [x] **Module Copy/Paste** - Ctrl+C / Ctrl+V with parameter values and internal cables
- [x] **Duplicate** - Duplicate selected modules with or without cables (context menu)
- [x] **Selection Tool** - Rubber-band + Shift+click multi-selection
- [x] **QuickAdd** - Space / double-click to add modules by name search
- [x] **Parameter Context Menu** - Right-click params to assign/remove morph group

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
# Configure (Debug mode recommended for development)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run
./build/Nomad2026_artefacts/Debug/Nomad2026
```

**Note:** Currently using Debug builds during active development. Some features (like URL opening in Help/About menus) require Debug mode to work correctly on all platforms.

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
