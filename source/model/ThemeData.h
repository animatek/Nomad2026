#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <map>

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
    std::vector<juce::String> labels;  // indexed by btn index
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
};

struct ThemeLight
{
    juce::String componentId;
    int x = 0, y = 0;
    int width = 7, height = 7;
    juce::String type;  // "led" or "meter"
};

struct ThemeCustomDisplay
{
    juce::String type;         // tag name, e.g. "overdrive-display", "LFODisplay"
    int x = 0, y = 0;
    int width = 40, height = 30;
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
};

class ThemeData
{
public:
    bool loadFromFile(const juce::File& xmlFile);

    const ModuleTheme* getModuleTheme(const juce::String& componentId) const;

    int getModuleThemeCount() const { return static_cast<int>(themes.size()); }

private:
    void parseModule(const juce::XmlElement& moduleElem);
    void parseConnector(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseKnob(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseSlider(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseButton(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseLabel(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseTextDisplay(const juce::XmlElement& elem, ModuleTheme& theme);
    void parseLight(const juce::XmlElement& elem, ModuleTheme& theme);

    std::map<juce::String, ModuleTheme> themes;
};
