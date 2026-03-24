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
- [x] **Code robustness review** (14 bugs fixed):
  - *Critical*: `QuickAddPopup` safe destructor — `PatchCanvas::~PatchCanvas` clears callbacks before deleting popup, prevents dangling-pointer crash on window close
  - *Critical*: `dragState` reset on `setPatch()` — raw pointers into old patch cleared before loading new patch
  - *Critical*: `const_cast<Parameter*>` eliminated — `findParameter()` non-const overload returns `Parameter*` directly
  - *High*: `uploadCompleteCallback` and all dialog callbacks use `SafePointer<MainComponent>` — safe if window closed mid-upload
  - *High*: `synthErrorCallback` captured by value in async lambda — safe if `ConnectionManager` destroyed before async fires
  - *High*: `MorphListComponent` saves `paramIndex` before `rebuild()` invalidates rows vector
  - *High*: `ParameterEncoder` logs unknown module types instead of silently returning empty vector
  - *Medium*: `onPatchDelete/Copy/Move` browser callbacks use `SafePointer` — safe against MainComponent destruction while dialog open
  - *Medium*: `dragState.module` null guard added to `MorphRange` drag handler
  - *Medium*: Dead code loop removed from `duplicateSelection()`
  - *Low*: Timeout constants documented with rationale (8s patch, 2s stale, 10s patch list)
  - *Low*: `duplicateSelection` stores section in `oldToNew` map — eliminates O(N²) re-search after module creation
- [x] **Morph system bug fixes** (6 bugs fixed):
  - *Critical*: Inspector dangling pointer crash — `clearModule()` before replacing patch prevents SIGSEGV when `refreshMorphList()` accesses destroyed module (all 3 patch-replace paths: synth load, file load, new patch)
  - *Critical*: Morph assignments lost after "Send Patch to Synth" — suppress auto-refetch (`NewPatchInSlot`) after upload; `currentPatch` is already authoritative
  - *High*: `getParameter(int)` matched wrong parameter on modules with overlapping indices (e.g. FilterF: custom "freq display units" and regular "frequency" both index=0) — now filters to `paramClass == "parameter"` only
  - *Medium*: "Zero Morph" now fully removes morph assignment (group + range) instead of only zeroing the range
  - *Medium*: Default morph range changed from 64 to 0 on new assignments — matches synth expectation
  - *Low*: Bidirectional morph range sync between canvas (Ctrl+drag) and inspector panel
- [x] **Hardware Knob Assignment** (parameter context menu → Knob):
  - Assign any parameter to Knob 1-18, Pedal, After touch, or On/Off switch
  - Shows current assignment with tick mark and "(used)" indicator
  - Reassign between knobs or disable assignment
  - KnobAssignmentMessage protocol: SysEx sc=0x25 (new) / sc=0x26 (reassign/deassign)
- [x] **MIDI Controller Assignment** (parameter context menu → MIDI Ctrl):
  - Assign any parameter to MIDI CC 0-119
  - Shows current assignment with tick mark
  - Reassign between CCs or disable assignment
  - MidiCtrlAssignmentMessage protocol: SysEx sc=0x22 (new) / sc=0x23 (reassign/deassign)
- [x] **Inspector Panel: Assignments View** (3-section unified display):
  - **Morphs** (purple): morph group headers, X remove button, drag-to-adjust range bar
  - **Knobs** (blue): badge label ("Knob 3", "Pedal"), X remove button with SysEx deassign
  - **MIDI CC** (gold): badge label ("CC 74"), X remove button with SysEx deassign
  - Dual mode: single-module view (when module selected) or patch-wide view (when no module selected)
  - Auto-refresh on every assignment change
- [x] **Resizable 3-panel layout**: Module browser, canvas, and inspector with draggable dividers

- [x] **Cable creation/deletion**: Drag between connectors to create cables, right-click to delete
- [x] **Module drag & drop**: Add modules from browser to canvas
- [x] **Shake Cables**: "S" button in header bar randomizes cable curvature to reduce overlap with controls; also available via canvas context menu with "Reset Cables" option
- [x] **Hidden cable connector indicator**: When cables are hidden via color filter, connected connectors show a "capped" visual (filled center) instead of the open hole, indicating a hidden connection exists
- [x] **Undo/Redo system** (Ctrl+Z / Ctrl+Y):
  - Add/delete modules, move modules, add/delete cables
  - Parameter changes (with coalescing for rapid knob turns)
  - Morph assignments/ranges, hardware knob assignments, MIDI CC assignments
  - Rename patch
  - Structural undos (add/delete module) trigger a debounced full patch upload to synth
  - Multi-module grouped undo (e.g. delete selection) handled correctly — single upload after all actions settle

- [x] **Visual indicator for loaded patch in browser**: Currently loaded bank patch highlighted with ▶ icon and amber color
- [x] **Module Help System** (F1):
  - Press F1 with a module selected or hovered to open a floating help popup
  - Displays module description and per-parameter documentation from the original Nord Modular Editor v3.03 help file (157 modules)
  - Fuzzy name matching (fullname, short name, normalized hyphens/spaces)
  - Draggable popup, close with Escape/F1/X button
- [x] **Multi-slot support (A/B/C/D)**:
  - 4 independent slots with separate patches, undo managers, synchronizers
  - Slot bar in left column (below inspector) with synth icons and patch names
  - Switching slots sends ActivateSlot to synth and requests patch if empty
  - New Patch uploads empty patch to synth to reset the slot
- [x] **Beta warning popup**: Styled floating popup at startup (matches F1 help style), "Don't show again" option, re-show from Help menu
- [x] **Bug reporting**: "Report a bug" button in header bar linking to GitHub Issues
- [x] **MIDI Connect and Store to Bank buttons**: Toolbar buttons in left column above slot bar
- [x] **Canvas hint watermark**: "Press Enter to add modules" shown on empty canvas sections
- [x] **Poly/Common default split**: 90/10 default divider position (poly dominant)

### Next Up
- [x] **macOS menu bar**: File/Edit/Device/Help/About now appear in the system menu bar; Device menu items enable correctly on synth connect
- [x] **macOS SysEx communication**: working correctly — previous issues were caused by a faulty USB hub, not a software bug

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
- [x] **Multi-Slot Support** - All 4 slots (A, B, C, D) with independent patches, undo, and sync

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
- [x] **Drag & Drop Modules** - Add modules from browser to canvas by dragging
- [x] **Move Modules** - Reposition modules on canvas (single and multi-move)
- [x] **Delete Modules** - Remove modules from patch (Delete key or context menu)
- [x] **Cable Creation** - Drag between connectors to create cables
- [x] **Cable Deletion** - Right-click cables to delete
- [x] **Module Copy/Paste** - Ctrl+C / Ctrl+V with parameter values and internal cables
- [x] **Duplicate** - Duplicate selected modules with or without cables (context menu)
- [x] **Selection Tool** - Rubber-band + Shift+click multi-selection
- [x] **QuickAdd** - Space / double-click to add modules by name search
- [x] **Parameter Context Menu** - Right-click params to assign/remove morph group

### Help System
- [x] **Module Help** (F1) - Context-sensitive module help from original v3.03 help file
- [ ] **Help Contents** - Integrated help documentation
- [ ] **About Dialog** - Version info, credits, license

### Quality of Life
- [ ] **Verify Input/Output Connectors** - Ensure visual distinction between inputs and outputs
- [ ] **Module Rendering Polish** - Compare each module type with original editor for pixel-perfect accuracy
- [x] **Undo/Redo System** - Full undo/redo for all patch operations
- [ ] **Keyboard Shortcuts** - Complete keyboard shortcut system matching original editor
- [ ] **Window Management** - Remember window positions and sizes across sessions

## Building

```bash
# Clone with submodules (JUCE is a git submodule)
git clone --recurse-submodules https://github.com/animatek/Nomad2026.git
# Or if already cloned:
git submodule update --init --recursive

# Configure (Debug mode recommended for development)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run (Linux/Windows)
./build/Nomad2026_artefacts/Debug/Nomad2026

# Run (macOS)
build/Nomad2026_artefacts/Debug/Nomad2026.app/Contents/MacOS/Nomad2026
```

**macOS Universal Binary** (Intel + Apple Silicon, for distribution):
```bash
cmake -B build-universal -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-universal -j$(sysctl -n hw.logicalcpu)
# Result: build-universal/Nomad2026_artefacts/Release/Nomad2026.app
```

**Windows** (requires [VS 2022 Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) with the "Desktop development with C++" workload):
```bash
cmake -B build-win-release -G "Visual Studio 17 2022" -A x64
cmake --build build-win-release --config Release
# Result: build-win-release/Nomad2026_artefacts/Release/Nomad2026.exe
```

**Note:** Currently using Debug builds during active development. Some features (like URL opening in Help/About menus) require Debug mode to work correctly on all platforms.

Nord Modular data files (modules, theme) are embedded in the binary via JUCE BinaryData — no external data files required.

Requires JUCE (included as a git submodule in `JUCE/`) and a C++17 compiler.

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
