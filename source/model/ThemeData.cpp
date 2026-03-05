#include "ThemeData.h"

bool ThemeData::loadFromFile(const juce::File& xmlFile)
{
    if (!xmlFile.existsAsFile())
    {
        DBG("ThemeData: file not found: " + xmlFile.getFullPathName());
        return false;
    }

    auto xml = juce::XmlDocument::parse(xmlFile);
    if (xml == nullptr)
    {
        DBG("ThemeData: failed to parse XML");
        return false;
    }

    // Root is <modules>, iterate <module> children
    for (auto* child = xml->getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
    {
        if (child->getTagName() == "module")
            parseModule(*child);
    }

    DBG("ThemeData: loaded " + juce::String(getModuleThemeCount()) + " module themes");
    return true;
}

const ModuleTheme* ThemeData::getModuleTheme(const juce::String& componentId) const
{
    auto it = themes.find(componentId);
    return (it != themes.end()) ? &it->second : nullptr;
}

void ThemeData::parseModule(const juce::XmlElement& moduleElem)
{
    ModuleTheme theme;
    theme.componentId = moduleElem.getStringAttribute("component-id");
    theme.width = moduleElem.getIntAttribute("width", 255);
    theme.height = moduleElem.getIntAttribute("height", 30);

    if (theme.componentId.isEmpty())
        return;

    for (auto* child = moduleElem.getFirstChildElement(); child != nullptr;
         child = child->getNextElement())
    {
        auto tag = child->getTagName();

        if (tag == "name")
            theme.name = child->getAllSubText().trim();
        else if (tag == "connector")
            parseConnector(*child, theme);
        else if (tag == "knob")
            parseKnob(*child, theme);
        else if (tag == "slider")
            parseSlider(*child, theme);
        else if (tag == "button")
            parseButton(*child, theme);
        else if (tag == "label")
            parseLabel(*child, theme);
        else if (tag == "textDisplay")
            parseTextDisplay(*child, theme);
        else if (tag == "light")
            parseLight(*child, theme);
        else if (tag != "image" && tag != "resetButton" && tag != "name"
                 && tag != "scrollbar" && tag != "defs" && tag != "style"
                 && child->hasAttribute("x") && child->hasAttribute("width"))
        {
            // Custom display element (overdrive-display, LFODisplay, etc.)
            ThemeCustomDisplay cd;
            cd.type = tag;
            cd.x = child->getIntAttribute("x");
            cd.y = child->getIntAttribute("y");
            cd.width = child->getIntAttribute("width", 40);
            cd.height = child->getIntAttribute("height", 30);
            theme.customDisplays.push_back(cd);
        }
        // Silently skip: image, resetButton, scrollbar
    }

    themes[theme.componentId] = std::move(theme);
}

void ThemeData::parseConnector(const juce::XmlElement& elem, ModuleTheme& theme)
{
    // Outer <connector> has position/size/class, inner <connector> has component-id
    ThemeConnector tc;
    tc.x = elem.getIntAttribute("x");
    tc.y = elem.getIntAttribute("y");
    tc.size = elem.getIntAttribute("size", 13);
    tc.cssClass = elem.getStringAttribute("class");

    // Find inner <connector> child for component-id
    if (auto* inner = elem.getChildByName("connector"))
        tc.componentId = inner->getStringAttribute("component-id");

    if (tc.componentId.isNotEmpty())
        theme.connectors.push_back(tc);
}

void ThemeData::parseKnob(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeKnob tk;
    tk.x = elem.getIntAttribute("x");
    tk.y = elem.getIntAttribute("y");
    tk.size = elem.getIntAttribute("size", 21);

    if (auto* param = elem.getChildByName("parameter"))
        tk.componentId = param->getStringAttribute("component-id");

    theme.knobs.push_back(tk);
}

void ThemeData::parseSlider(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeSlider ts;
    ts.x = elem.getIntAttribute("x");
    ts.y = elem.getIntAttribute("y");
    ts.width = elem.getIntAttribute("width", 12);
    ts.height = elem.getIntAttribute("height", 50);
    ts.orientation = elem.getStringAttribute("orientation", "vertical");

    if (auto* param = elem.getChildByName("parameter"))
        ts.componentId = param->getStringAttribute("component-id");

    theme.sliders.push_back(ts);
}

void ThemeData::parseButton(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeButton tb;
    tb.x = elem.getIntAttribute("x");
    tb.y = elem.getIntAttribute("y");
    tb.width = elem.getIntAttribute("width", 34);
    tb.height = elem.getIntAttribute("height", 16);
    tb.cyclic = (elem.getStringAttribute("cyclic", "true") == "true");
    tb.isIncrement = (elem.getStringAttribute("mode") == "increment");
    tb.landscape = (elem.getStringAttribute("landscape") == "true");

    if (auto* param = elem.getChildByName("parameter"))
        tb.componentId = param->getStringAttribute("component-id");

    // Collect button labels indexed by btn index
    for (auto* btn = elem.getFirstChildElement(); btn != nullptr;
         btn = btn->getNextElement())
    {
        if (btn->getTagName() == "btn")
        {
            int idx = btn->getIntAttribute("index", 0);
            // Get text content, skip if it contains an <image> child
            auto text = btn->getAllSubText().trim();
            if (text.isEmpty() && btn->getFirstChildElement() != nullptr)
                text = ""; // image-based button, leave empty

            // Ensure labels vector is large enough
            while (static_cast<int>(tb.labels.size()) <= idx)
                tb.labels.push_back("");
            tb.labels[static_cast<size_t>(idx)] = text;
        }
    }

    theme.buttons.push_back(tb);
}

void ThemeData::parseLabel(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeLabel tl;
    tl.x = elem.getIntAttribute("x");
    tl.y = elem.getIntAttribute("y");
    tl.text = elem.getAllSubText().trim();

    theme.labels.push_back(tl);
}

void ThemeData::parseTextDisplay(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeTextDisplay td;
    td.x = elem.getIntAttribute("x");
    td.y = elem.getIntAttribute("y");
    td.width = elem.getIntAttribute("width", 40);
    td.height = elem.getIntAttribute("height", 16);

    if (auto* param = elem.getChildByName("parameter"))
        td.componentId = param->getStringAttribute("component-id");

    theme.textDisplays.push_back(td);
}

void ThemeData::parseLight(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeLight tl;
    tl.componentId = elem.getStringAttribute("component-id");
    tl.x = elem.getIntAttribute("x");
    tl.y = elem.getIntAttribute("y");
    tl.width = elem.getIntAttribute("width", 7);
    tl.height = elem.getIntAttribute("height", 7);
    tl.type = elem.getStringAttribute("type", "led");

    theme.lights.push_back(tl);
}
