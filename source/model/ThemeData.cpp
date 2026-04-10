#include "ThemeData.h"

bool ThemeData::loadFromFile(const juce::File& xmlFile)
{
    if (!xmlFile.existsAsFile())
    {
        DBG("ThemeData: file not found: " + xmlFile.getFullPathName());
        return false;
    }
    return loadFromXml(juce::XmlDocument::parse(xmlFile));
}

bool ThemeData::loadFromXmlString(const juce::String& xmlString)
{
    return loadFromXml(juce::XmlDocument::parse(xmlString));
}

bool ThemeData::loadFromXml(std::unique_ptr<juce::XmlElement> xml)
{
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
        else if (tag == "resetButton")
            parseResetButton(*child, theme);
        else if (tag == "image")
        {
            auto href = child->getStringAttribute("xlink:href");
            if (href.contains("groupbox"))
            {
                ThemeGroupBox gb;
                gb.x = child->getIntAttribute("x");
                gb.y = child->getIntAttribute("y");
                gb.width  = child->getIntAttribute("width", 40);
                gb.height = child->getIntAttribute("height", 30);
                theme.groupBoxes.push_back(gb);
            }
            else if (href.contains("/images/"))
            {
                // Static waveform / decoration icon (e.g. wf_sine.png)
                juce::String iconName = href.fromLastOccurrenceOf("/", false, false)
                                            .upToLastOccurrenceOf(".", false, false);
                if (iconName.isNotEmpty())
                {
                    ThemeStaticIcon si;
                    si.iconName = iconName;
                    si.x      = child->getIntAttribute("x");
                    si.y      = child->getIntAttribute("y");
                    si.width  = child->getIntAttribute("width", 11);
                    si.height = child->getIntAttribute("height", 9);
                    theme.staticIcons.push_back(si);
                }
            }
        }
        else if (tag != "name"
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
        // Silently skip: image, scrollbar
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
            auto text = btn->getAllSubText().trim();

            // Extract image href if present (e.g. wf_sine, wf_saw, env_log...)
            juce::String imageRef;
            if (auto* img = btn->getChildByName("image"))
            {
                auto href = img->getStringAttribute("xlink:href");
                // Extract filename without path and extension: "./images/slice/wf_sine.png" → "wf_sine"
                imageRef = href.fromLastOccurrenceOf("/", false, false)
                               .upToLastOccurrenceOf(".", false, false);
                if (imageRef.isEmpty())
                    imageRef = href; // fallback to full href
            }

            while (static_cast<int>(tb.labels.size()) <= idx)
                tb.labels.push_back("");
            tb.labels[static_cast<size_t>(idx)] = text;

            while (static_cast<int>(tb.imageRefs.size()) <= idx)
                tb.imageRefs.push_back("");
            tb.imageRefs[static_cast<size_t>(idx)] = imageRef;
        }
    }

    theme.buttons.push_back(tb);
}

void ThemeData::parseLabel(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeLabel tl;
    tl.x = elem.getIntAttribute("x");
    tl.y = elem.getIntAttribute("y");
    tl.text = elem.getAllSubText().trim().replace("\\n", "\n");

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
    {
        td.componentId = param->getStringAttribute("component-id");

        // Modules with note-select parameter (MIDI note name display)
        // m67 (NoteDetect): p1 — m100 (KeybSplit): p1 (lower) and p2 (upper)
        static const juce::StringArray noteSelectModules { "m67", "m100" };
        bool isNoteModule = noteSelectModules.contains(theme.componentId);
        bool isNoteParam  = (td.componentId == "p1") ||
                            (theme.componentId == "m100" && td.componentId == "p2");
        if (isNoteModule && isNoteParam)
            td.noteFormat = true;

        // Oscillator frequency displays — show note name (C-1..G9) instead of raw number
        // Main oscillators: m7 (OscA), m8 (OscB), m9 (OscC) — p2 = freq coarse
        // OscSineBank (m106): p1,p4,p7,p10,p13,p16 = osc coarse per voice
        static const juce::StringArray oscFreqModules { "m7", "m8", "m9", "m95" };
        static const juce::StringArray oscSineBankFreqParams { "p1","p4","p7","p10","p13","p16" };
        if (oscFreqModules.contains(theme.componentId) && td.componentId == "p2")
            td.noteFormat = true;
        if (theme.componentId == "m106" && oscSineBankFreqParams.contains(td.componentId))
            td.noteFormat = true;

        // Slave oscillator detune coarse → show as partial ratio (1:1, 2:1, etc.)
        static const juce::StringArray slaveOscModules { "m10", "m11", "m12", "m13", "m14", "m85" };
        if (slaveOscModules.contains(theme.componentId) && td.componentId == "p2")
            td.partialFormat = true;

        // DrumSynth (m58): MTune → fmtDrumHz, STune → fmtDrumPartials
        if (theme.componentId == "m58")
        {
            if (td.componentId == "p1") td.drumHzFormat = true;
            if (td.componentId == "p2") td.drumPartialFormat = true;
        }
    }

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
    if (elem.hasAttribute("ledOnValue"))
        tl.ledOnValue = elem.getIntAttribute("ledOnValue");

    theme.lights.push_back(tl);
}

void ThemeData::parseResetButton(const juce::XmlElement& elem, ModuleTheme& theme)
{
    ThemeResetButton rb;
    rb.x = elem.getIntAttribute("x");
    rb.y = elem.getIntAttribute("y");
    rb.width  = elem.getIntAttribute("width", 9);
    rb.height = elem.getIntAttribute("height", 6);

    if (auto* param = elem.getChildByName("parameter"))
    {
        rb.componentId = param->getStringAttribute("component-id");
        // Default value: read from descriptor if available, fallback 64
        rb.defaultValue = param->getIntAttribute("defaultValue", 64);
    }

    theme.resetButtons.push_back(rb);
}
