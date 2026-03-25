# Nomad2026 Roadmap

Future implementation ideas and architecture plans. These are not yet started — just documented for when the time comes.

---

## VST3/CLAP Plugin Architecture

### Dual MIDI Design

The Nord Modular has two independent MIDI port pairs:
- **MIDI standard** (In/Out) — notes, CC, performance data
- **PC MIDI** (In/Out) — dedicated SysEx traffic for editor communication

These are hardware-independent and can operate simultaneously without interference.

### Plugin MIDI Flow

```
DAW automation lanes ──→ VST params ──→ SysEx (PC port) ──→ Nord Modular
DAW MIDI track ─────────→ MIDI standard ─────────────────→ Nord Modular
                          (notes, CC)
```

- The plugin opens the PC MIDI port directly via `juce::MidiInput/MidiOutput` (bypasses DAW MIDI bus)
- DAW sends notes/CC to the Nord through its normal MIDI routing
- Both paths are fully independent — they don't interfere

### Parameter Automation Strategy

**Option A — VST parameter slots (recommended)**
- Expose module parameters as native VST/CLAP parameters
- DAW automates them via automation lanes
- Plugin translates changes to SysEx Parameter Change (cc=0x13) over PC port
- Challenge: parameters are dynamic (each patch has different modules/params)
- Solution: fixed pool of ~256 parameter slots, mapped dynamically per patch

**Option B — MIDI CC passthrough**
- Plugin receives MIDI CC from DAW input bus
- Maps CCs to Nord parameters and sends as SysEx
- More flexible but harder to configure per patch

**Option C — Hybrid (knobs + morphs only)**
- Expose only the 23 hardware knobs + 4 morph sources as plugin parameters
- Simpler, matches the physical interface
- Users assign knobs to parameters in the editor, DAW automates the knobs

### Known Issues (parked)
- Experimental VST3/CLAP builds exist but have a crash-on-close bug
- See commit `48323ae` for current state

---

## Ideas Backlog

_Add future implementation ideas here as they come up._
