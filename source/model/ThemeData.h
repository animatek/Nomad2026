#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <map>
#include <memory>

struct ThemeConnector
{
    juce::String componentId;  // e.g. "c1", "c2"
    int x = 0, y = 0;
    int size = 13;
    juce::String cssClass;     // e.g. "cAUDIO", "cCONTROL"
};

struct ThemeKnob
{
    juce::String componentId;  // e.g. "p1", "p2"
    int x = 0, y = 0;
    int size = 21;
};

struct ThemeSlider
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 12, height = 50;
    juce::String orientation;  // "vertical" or "horizontal"
};

struct ThemeButton
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 34, height = 16;
    bool cyclic = true;                // cyclic=toggle, !cyclic=radio selector
    bool isIncrement = false;          // mode="increment" (arrow buttons)
    bool landscape = false;            // landscape="true" (horizontal layout)
    std::vector<juce::String> labels;     // indexed by btn index
    std::vector<juce::String> imageRefs;  // image href per btn index (e.g. "wf_sine", "wf_saw")
};

struct ThemeLabel
{
    int x = 0, y = 0;
    juce::String text;
};

struct ThemeTextDisplay
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 40, height = 16;
    bool noteFormat = false;        // true → display as note name (C4, D#3, etc.)
    bool partialFormat = false;     // true → display as partial ratio (1:1, 2:1, etc.)
    bool drumHzFormat = false;      // true → fmtDrumHz: 20*2^(v/24) Hz
    bool drumPartialFormat = false; // true → fmtDrumPartials: 1:1/2:1/4:1 or x0.00
    bool oscHzFormat = false;       // true → fmtOscHz: 440*2^((v-69)/12) Hz
    bool lfoHzFormat = false;       // true → fmtLFOHz: 440*2^((v-177)/12), shows s or Hz
    bool phaseFormat = false;       // true → fmtPhase: v*2.8125-180 degrees
    bool bpmFormat = false;         // true → fmtBPM: piecewise linear, shows bpm
    bool stepFormat = false;        // true → 0=OFF, 1-128=number (PatternGen steps)
};

struct ThemeLight
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 7, height = 7;
    juce::String type;      // "led" or "meter"
    int ledOnValue = -1;    // if >= 0: LED activates when paired meter reaches this value
};

struct ThemeGroupBox
{
    int x = 0, y = 0, width = 40, height = 30;
};

struct ThemeResetButton
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 9, height = 6;
    int defaultValue = 64;   // value at which the indicator lights green
};

struct ThemeCustomDisplay
{
    juce::String type;              // tag name, e.g. "overdrive-display", "LFODisplay"
    int x = 0, y = 0;
    int width = 40, height = 30;
    // LFODisplay sub-element component IDs (empty = not present)
    juce::String phaseComponentId;  // <phase component-id="pN"> → phase param
    juce::String shapeComponentId;  // <shape component-id="pN"> → waveform shape param
    juce::String rateComponentId;   // <rate component-id="pN">  → rate param (cycles scaling)
    int fixedWaveform = -1;         // <waveform value="N"> → fixed shape (-1 = dynamic)
};

struct ThemeStaticIcon
{
    juce::String iconName;     // e.g. "wf_sine", "wf_saw", "wf_tri", "wf_square"
    int x = 0, y = 0;
    int width = 11, height = 9;
};

struct ModuleTheme
{
    juce::String componentId;  // e.g. "m62"
    int width = 255;
    int height = 30;
    juce::String name;

    std::vector<ThemeConnector> connectors;
    std::vector<ThemeKnob> knobs;
    std::vector<ThemeSlider> sliders;
    std::vector<ThemeButton> buttons;
    std::vector<ThemeLabel> labels;
    std::vector<ThemeTextDisplay> textDisplays;
    std::vector<ThemeLight> lights;
    std::vector<ThemeCustomDisplay> customDisplays;
    std::vector<ThemeResetButton> resetButtons;
    std::vector<ThemeGroupBox> groupBoxes;
    std::vector<ThemeStaticIcon> staticIcons;
};

class ThemeData
{
public:
    bool loadFromFile(const juce::File& xmlFile);
    bool loadFromXmlString(const juce::String& xmlString);

    const ModuleTheme* getModuleTheme(const juce::String& componentId) const;

    int getModuleThemeCount() const { return static_cast<int>(themes.size()); }

private:
    bool loadFromXml(std::unique_ptr<juce::XmlElement> xml);
    void parseModule(const juce::XmlElement& moduleElem);
    void parseConnector(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseKnob(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseSlider(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseButton(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseLabel(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseTextDisplay(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseLight(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseResetButton(const juce::XmlElement& elem, ModuleTheme& theme);

    std::map<juce::String, ModuleTheme> themes;
};
