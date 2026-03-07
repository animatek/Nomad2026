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

### In Progress
- [ ] Verify input/output connector visuals (distinguish inputs vs outputs)
- [ ] Polish individual module rendering (compare each with original editor)

### Next Up
- [ ] Patch file I/O (.pch load/save)
- [ ] Drag & drop modules on canvas
- [ ] Cable creation/deletion by clicking connectors
- [ ] Multi-slot support (slots 1-4, currently slot 0 only)
- [ ] Bank/patch management

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
