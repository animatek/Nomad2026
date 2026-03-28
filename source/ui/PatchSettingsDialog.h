#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"

class PatchSettingsDialog : public juce::Component
{
public:
    struct Result
    {
        int voices;
        int velRangeMin, velRangeMax;
        int keyRangeMin, keyRangeMax;
        int pedalMode;       // 0=Sustain, 1=On/Off
        int bendRange;       // 1-24 semitones
        bool portamento;     // false=Normal, true=Auto
        int portamentoTime;  // 0-127
        int octaveShift;     // 0-4 maps to -2..+2
        bool voiceRetriggerPoly;
        bool voiceRetriggerCommon;
    };

    using Callback = std::function<void(const Result&)>;

    PatchSettingsDialog(const PatchHeader& header, Callback onOk);

    void resized() override;

    static void show(juce::Component* parent, const PatchHeader& header, Callback onOk);

private:
    Callback okCallback;

    // Voices
    juce::Label voicesLabel { {}, "Voices" };
    juce::Label voicesReqLabel { {}, "Requested" };
    juce::Slider voicesSlider;

    // Velocity range
    juce::Label velLabel { {}, "Velocity range" };
    juce::Label velMaxLabel { {}, "Max" };
    juce::Label velMinLabel { {}, "Min" };
    juce::Slider velMaxSlider, velMinSlider;

    // Keyboard range
    juce::Label keyLabel { {}, "Keyboard range" };
    juce::Label keyMaxLabel { {}, "Max" };
    juce::Label keyMinLabel { {}, "Min" };
    juce::Slider keyMaxSlider, keyMinSlider;

    // Pedal mode
    juce::Label pedalLabel { {}, "Pedal mode" };
    juce::ToggleButton pedalSustain { "Sustain" };
    juce::ToggleButton pedalOnOff { "On/Off" };

    // Bend range
    juce::Label bendLabel { {}, "Bend range" };
    juce::Slider bendSlider;
    juce::Label bendUnitLabel { {}, "Semitones" };

    // Portamento
    juce::Label portaLabel { {}, "Portamento" };
    juce::ToggleButton portaNormal { "Normal" };
    juce::ToggleButton portaAuto { "Auto" };
    juce::Label portaTimeLabel { {}, "Time" };
    juce::Slider portaTimeSlider;

    // Octave shift
    juce::Label octaveLabel { {}, "Octave shift" };
    juce::ToggleButton octaveButtons[5];  // -2, -1, 0, +1, +2

    // Voice retrigger
    juce::Label retrigLabel { {}, "Voice retrig" };
    juce::ToggleButton retrigPoly { "Poly" };
    juce::ToggleButton retrigCommon { "Common" };

    // Buttons
    juce::TextButton okButton { "OK" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSettingsDialog)
};
