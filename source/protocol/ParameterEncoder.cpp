#include "ParameterEncoder.h"
#include <iostream>
#include <set>

std::vector<int> ParameterEncoder::getParameterBitWidths(int moduleType)
{
    // PDL2 Param<N> structures from patch.pdl2
    // Returns the bit width for each parameter in order

    switch (moduleType)
    {
        // Param3 - Out (module 3)
        case 3:
            // level:7
            return {7};

        // Param4 - Out2 (module 4)
        case 4:
            // level:7 destination:2 mute:1
            return {7, 2, 1};

        // Param5 - Out4 (module 5)
        case 5:
            // level:7 destination:3 mute:1
            return {7, 3, 1};

        // Param7 - OscA (module 7)
        case 7:
            // freqCoarse:7 freqFine:7 freqKbt:7 pulseWidth:7 waveform:2
            // pitchMod1:7 pitchMod2:7 fmaMod:7 pwMod:7 mute:1
            return {7, 7, 7, 7, 2, 7, 7, 7, 7, 1};

        // Param8 - OscB (module 8)
        case 8:
            // freqCoarse:7 freqFine:7 freqKbt:7 waveform:2
            // pitchMod1:7 pitchMod2:7 fmaMod:7 pulseWidth:7 Mute:1
            return {7, 7, 7, 2, 7, 7, 7, 7, 1};

        // Param9 - OscSlvA (module 9)
        case 9:
            // pitchCoarse:7 pitchFine:7 pitchKbt:1 pitchModAmount:7 fma:7 mute:1
            return {7, 7, 1, 7, 7, 1};

        // Param10 - OscSlvB (module 10)
        case 10:
            // detuneCoarse:7 detuneFine:7 pulseWidth:7 pwMod:7 mute:1
            return {7, 7, 7, 7, 1};

        // Param11 - OscSlvC (module 11)
        case 11:
            // detuneCoarse:7 detuneFine:7 fmaMod:7 mute:1
            return {7, 7, 7, 1};

        // Param12 - OscSlvD (module 12)
        case 12:
            // detuneCoarse:7 detuneFine:7 fmaMod:7 mute:1
            return {7, 7, 7, 1};

        // Param13 - OscC (module 13)
        case 13:
            // detuneCoarse:7 detuneFine:7 fmaMod:7 mute:1
            return {7, 7, 7, 1};

        // Param14 - OscD (module 14)
        case 14:
            // detuneCoarse:7 detuneFine:7 shape:2 fmaMod:7 mute:1
            return {7, 7, 2, 7, 1};

        // Param15 - NoteSeqA (module 15)
        case 15:
            // step1:7..step16:7 stepCount:7 editPosition:5 record:1 pause:1 active:1
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                    7, 5, 1, 1, 1};

        // Param16 - EnvD (module 16)
        case 16:
            // time:7
            return {7};

        // Param17 - EventSeq (module 17)
        case 17:
            // stepcount:7 active:1 gate1:1 gate2:1
            // seq1step1:1..seq1step16:1 seq2step1:1..seq2step16:1
            return {7, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

        // Param18 - Xfade (module 18)
        case 18:
            // modulation:7 crossfade:7
            return {7, 7};

        // Param19 - Mix3-1 (module 19)
        case 19:
            // inSense1:7 inSense2:7 inSense3:7
            return {7, 7, 7};

        // Param20 - ADSR (module 20)
        case 20:
            // attackShape:2 attack:7 decay:7 sustain:7 release:7 invert:1
            return {2, 7, 7, 7, 7, 1};

        // Param21 - Compressor (module 21)
        case 21:
            // attack:7 release:7 threshold:6 ratio:7 refLevel:6 limiter:5 act:1 mon:1 bypass:1
            return {7, 7, 6, 7, 6, 5, 1, 1, 1};

        // Param22 - ClipAmp (module 22)
        case 22:
            // range:7
            return {7};

        // Param23 - EnvADSR (module 23)
        case 23:
            // attack:7 decay:7 sustain:7 release:7 attackMod:7 decayMod:7 sustainMod:7 releaseMod:7 invert:1
            return {7, 7, 7, 7, 7, 7, 7, 7, 1};

        // Param24 - LFOA (module 24)
        case 24:
            // rate:7 range:2 waveform:3 rateMod:7 mono:1 rateKbt:7 phase:7 mute:1
            return {7, 2, 3, 7, 1, 7, 7, 1};

        // Param25 - LFOB (module 25)
        case 25:
            // rate:7 range:2 phase:7 rateMod:7 mono:1 rateKbt:7 pwMod:7 pulseWidth:7
            return {7, 2, 7, 7, 1, 7, 7, 7};

        // Param26 - LFOC (module 26)
        case 26:
            // rate:7 range:2 waveform:3 rateMod:7 mono:1 mute:1
            return {7, 2, 3, 7, 1, 1};

        // Param40 - Mixer8 (module 40)
        case 40:
            // inSense1:7 inSense2:7 inSense3:7 inSense4:7 inSense5:7 inSense6:7 inSense7:7 inSense8:7 attenuate:1
            return {7, 7, 7, 7, 7, 7, 7, 7, 1};

        // Param44 - GainControl (module 44)
        case 44:
            // shift:1
            return {1};

        // Param47 - Pan (module 47)
        case 47:
            // panMod:7 pan:7
            return {7, 7};

        // Param49 - FilterD (module 49)
        case 49:
            // freq:7 kbt:7 resonance:7 freqMod:7
            return {7, 7, 7, 7};

        // Param50 - FilterC (module 50)
        case 50:
            // freq:7 resonance:7 gainControl:1
            return {7, 7, 1};

        // Param51 - FilterE (module 51)
        case 51:
            // filterType:2 gainControl:1 frequencyMod:7 frequency:7 kbt:7 resonanceMod:7 resonance:7 slope:1 frequencyMod2:7 bypass:1
            return {2, 1, 7, 7, 7, 7, 7, 1, 7, 1};

        // Param52 - Multi-Env (module 52)
        case 52:
            // level1:7 level2:7 level3:7 level4:7 time1:7 time2:7 time3:7 time4:7 time5:7 sustain:3 curve:2
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 3, 2};

        // Param90 - NoteSeqB (module 90)
        case 90:
            // note1:7..note16:7 step:7 currentstep:5 record:1 play:1 loop:1
            return {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
                    7, 5, 1, 1, 1};

        // Add more common modules as needed...
        default:
            // Unknown module type — will fall back to all-7-bit encoding in caller.
            // Log once per unknown type to help identify missing entries.
            static std::set<int> warnedTypes;
            if (warnedTypes.find(moduleType) == warnedTypes.end())
            {
                warnedTypes.insert(moduleType);
                std::cout << "[ParameterEncoder] WARNING: unknown module type " << moduleType
                          << " — falling back to 7-bit encoding (add entry to fix)" << std::endl;
            }
            return {};
    }
}

std::vector<int> ParameterEncoder::getEncodingInfo(int moduleType)
{
    return getParameterBitWidths(moduleType);
}
