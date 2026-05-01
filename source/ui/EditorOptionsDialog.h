#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FlatCloseButton.h"

struct EditorOptions
{
    enum class CableStyle  { CurvedThick = 0, StraightThick, CurvedThin, StraightThin };
    enum class KnobControl { Horizontal = 0, Circular, Vertical };

    CableStyle  cableStyle     = CableStyle::CurvedThick;
    KnobControl knobControl    = KnobControl::Horizontal;
    bool        autoUpload     = true;
    bool        recycleWindows = true;
    float       cableOpacity   = 0.80f;

    static EditorOptions load(juce::PropertiesFile* props);
    void save(juce::PropertiesFile* props) const;
};

class EditorOptionsDialog : public juce::Component
{
public:
    explicit EditorOptionsDialog(const EditorOptions& current);

    std::function<void(const EditorOptions&)> onChange;

    void paint   (juce::Graphics& g) override;
    void resized () override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown  (const juce::MouseEvent& e) override;
    void mouseDrag  (const juce::MouseEvent& e) override;

    static void show(juce::Component* parent,
                     const EditorOptions& current,
                     std::function<void(const EditorOptions&)> onChangeCb);

private:
    void close();
    void apply();

    EditorOptions options;
    juce::ComponentDragger dragger;
    FlatCloseButton closeButton;

    // Cable Style
    juce::Label    cableStyleLabel   { {}, "CABLE STYLE" };
    juce::ToggleButton cableCurvedThick   { "Curved (thick, default)" };
    juce::ToggleButton cableStraightThick { "Straight (thick)" };
    juce::ToggleButton cableCurvedThin    { "Curved (thin)" };
    juce::ToggleButton cableStraightThin  { "Straight (thin)" };

    // Knob Control
    juce::Label    knobControlLabel  { {}, "KNOB CONTROL" };
    juce::ToggleButton knobHorizontal { "Horizontal drag (default)" };
    juce::ToggleButton knobCircular   { "Circular drag" };
    juce::ToggleButton knobVertical   { "Vertical drag" };

    // Behaviour
    juce::Label    behaviourLabel    { {}, "BEHAVIOUR" };
    juce::ToggleButton autoUploadToggle   { "Auto Upload  (send parameter changes to synth immediately)" };
    juce::ToggleButton recycleWinToggle   { "Recycle Windows  (reuse patch windows)" };

    // Buttons
    juce::TextButton okButton     { "OK" };
    juce::TextButton cancelButton { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditorOptionsDialog)
};
