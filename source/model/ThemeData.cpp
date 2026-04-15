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
                // Only accept icons we can actually draw: wf_* waveforms and decoration-N
                juce::String iconName = href.fromLastOccurrenceOf("/", false, false)
                                            .upToLastOccurrenceOf(".", false, false);
                bool canDraw = iconName.startsWith("wf_")
                            || (iconName.startsWith("decoration-") && !iconName.contains("."))
                            || iconName == "ds-2-7" || iconName == "ds-2-8";
                if (canDraw)
                {
                    ThemeStaticIcon si;
                    si.iconName = iconName;
                    si.x      = child->getIntAttribute("x");
                    si.y      = child->getIntAttribute("y");
                    int sizeAttr = child->getIntAttribute("size", 0);
                    si.width  = child->getIntAttribute("width",  sizeAttr > 0 ? sizeAttr : 11);
                    si.height = child->getIntAttribute("height", sizeAttr > 0 ? sizeAttr : 9);
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
            // Parse sub-elements: LFO (phase/shape/rate/waveform) + envelope (attack/decay/sustain/release/hold)
            for (auto* sub = child->getFirstChildElement(); sub != nullptr; sub = sub->getNextElement())
            {
                auto subTag = sub->getTagName();
                if (subTag == "phase")
                    cd.phaseComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "shape")
                    cd.shapeComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "rate")
                    cd.rateComponentId  = sub->getStringAttribute("component-id");
                else if (subTag == "waveform")
                    cd.fixedWaveform = sub->getIntAttribute("value", -1);
                else if (subTag == "attack")
                    cd.attackComponentId  = sub->getStringAttribute("component-id");
                else if (subTag == "decay")
                    cd.decayComponentId   = sub->getStringAttribute("component-id");
                else if (subTag == "sustain")
                    cd.sustainComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "release")
                    cd.releaseComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "hold")
                    cd.holdComponentId    = sub->getStringAttribute("component-id");
                else if (subTag == "inverse")
                    cd.inverseComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "l0") cd.levelIds[0] = sub->getStringAttribute("component-id");
                else if (subTag == "l1") cd.levelIds[1] = sub->getStringAttribute("component-id");
                else if (subTag == "l2") cd.levelIds[2] = sub->getStringAttribute("component-id");
                else if (subTag == "l3") cd.levelIds[3] = sub->getStringAttribute("component-id");
                else if (subTag == "t0") cd.timeIds[0] = sub->getStringAttribute("component-id");
                else if (subTag == "t1") cd.timeIds[1] = sub->getStringAttribute("component-id");
                else if (subTag == "t2") cd.timeIds[2] = sub->getStringAttribute("component-id");
                else if (subTag == "t3") cd.timeIds[3] = sub->getStringAttribute("component-id");
                else if (subTag == "t4") cd.timeIds[4] = sub->getStringAttribute("component-id");
                else if (subTag == "curve")
                    cd.curveComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "bandwidth")
                    cd.bwComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "frequency")
                    cd.freqComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "gain")
                    cd.gainComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "cutoff")
                    cd.cutoffComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "resonance")
                    cd.resonanceComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "type")
                    cd.typeComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "slope")
                    cd.slopeComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "gain-control")
                    cd.gainControlComponentId = sub->getStringAttribute("component-id");
                else if (subTag == "in-pos")
                {
                    cd.mmInX = sub->getIntAttribute("x");
                    cd.mmInY = sub->getIntAttribute("y");
                }
                else if (subTag == "out-pos")
                {
                    cd.mmOutX = sub->getIntAttribute("x");
                    cd.mmHpY  = sub->getIntAttribute("hp");
                    cd.mmBpY  = sub->getIntAttribute("bp");
                    cd.mmLpY  = sub->getIntAttribute("lp");
                }
                else if (subTag.startsWith("band") && subTag.length() <= 6)
                {
                    int idx = subTag.substring(4).getIntValue();
                    if (idx >= 0 && idx < 16)
                        cd.bandIds[idx] = sub->getStringAttribute("component-id");
                }
            }
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
    tb.reversed = (elem.getStringAttribute("reverse") == "true");

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

    // Detect <call component="..." method="rnd"> (Vocoder Rnd button)
    if (auto* call = elem.getChildByName("call"))
    {
        tb.isCall     = true;
        tb.callMethod = call->getStringAttribute("method");
        tb.callValue  = call->getIntAttribute("value", 0);
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
        static const juce::StringArray oscNoteModules { "m7", "m8", "m9" };
        if (oscNoteModules.contains(theme.componentId) && td.componentId == "p2")
            td.noteFormat = true;

        // Advanced oscillators: Hz display (440*2^((v-69)/12))
        // m95=PercOsc, m96=FormantOsc, m97=MasterOsc, m107=SpectralOsc
        static const juce::StringArray oscHzModules { "m95", "m96", "m97", "m107" };
        if (oscHzModules.contains(theme.componentId) && td.componentId == "p2")
            td.oscHzFormat = true;

        // OscSineBank (m106): tune knobs → partial ratio (1:1, 2:1, etc.)
        static const juce::StringArray oscSineBankFreqParams { "p1","p4","p7","p10","p13","p16" };
        if (theme.componentId == "m106" && oscSineBankFreqParams.contains(td.componentId))
            td.partialFormat = true;

        // Slave oscillator detune coarse → partial ratio
        static const juce::StringArray slaveOscModules { "m10", "m11", "m12", "m13", "m14", "m85" };
        if (slaveOscModules.contains(theme.componentId) && td.componentId == "p2")
            td.partialFormat = true;

        // LFO slave rate → partial ratio (m80=LFOSlvA, m27=B, m28=C, m29=D, m30=E)
        static const juce::StringArray lfoSlvModules { "m80", "m27", "m28", "m29", "m30" };
        if (lfoSlvModules.contains(theme.componentId) && td.componentId == "p2")
            td.partialFormat = true;

        // Random generators rate → partial ratio (m34=RndStepGen, m110=RandomGen)
        static const juce::StringArray rndGenModules { "m34", "m110" };
        if (rndGenModules.contains(theme.componentId) && td.componentId == "p2")
            td.partialFormat = true;

        // LFO rate → fmtLFOHz (440*2^((v-177)/12), shows s or Hz)
        // m24=LFOA, m25=LFOB, m26=LFOC
        static const juce::StringArray lfoModules { "m24", "m25", "m26" };
        if (lfoModules.contains(theme.componentId) && td.componentId == "p1")
            td.lfoHzFormat = true;

        // Phase display → fmtPhase: v*2.8125-180 degrees
        // m24=LFOA p7, m25=LFOB p3, m80=LFOSlvA p3
        bool isPhaseParam = (theme.componentId == "m24" && td.componentId == "p7") ||
                            (theme.componentId == "m25" && td.componentId == "p3") ||
                            (theme.componentId == "m80" && td.componentId == "p3");
        if (isPhaseParam)
            td.phaseFormat = true;

        // BPM display → fmtBPM (ClkGen m68 p1)
        if (theme.componentId == "m68" && td.componentId == "p1")
            td.bpmFormat = true;

        // DrumSynth (m58): MTune → fmtDrumHz, STune → fmtDrumPartials
        if (theme.componentId == "m58")
        {
            if (td.componentId == "p1") td.drumHzFormat = true;
            if (td.componentId == "p2") td.drumPartialFormat = true;
        }

        // PatternGen (m99): step p4 → 0=OFF, 1-128=number
        if (theme.componentId == "m99" && td.componentId == "p4")
            td.stepFormat = true;

        // fmtAdsrTime: m20(ADSR), m23(Mod-Env), m46(AHD), m52(Multi-Env), m84(AD-Env)
        {
            static const std::map<juce::String, juce::StringArray> adsrTimeMap {
                { "m20", { "p2", "p3", "p5" } },
                { "m23", { "p1", "p2", "p4" } },
                { "m46", { "p1", "p2", "p3" } },
                { "m52", { "p5", "p6", "p7", "p8", "p9" } },
                { "m84", { "p1", "p2" } }
            };
            auto it = adsrTimeMap.find(theme.componentId);
            if (it != adsrTimeMap.end() && it->second.contains(td.componentId))
                td.adsrTimeFormat = true;
        }

        // fmtEnvelopeAttack / fmtEnvelopeRelease: m71 (EnvFollower)
        if (theme.componentId == "m71")
        {
            if (td.componentId == "p1") td.envAttackFormat  = true;
            if (td.componentId == "p2") td.envReleaseFormat = true;
        }

        // fmtFilterHz1: 504*2^((v-64)/12) — FilterA (m86/p1), FilterB (m87/p1)
        static const juce::StringArray filterHz1Modules { "m86", "m87" };
        if (filterHz1Modules.contains(theme.componentId) && td.componentId == "p1")
            td.filterHz1Format = true;

        // fmtFilterHz2: 330*2^((v-60)/12) — FilterC(m50/p2), FilterD(m49/p2),
        //                                     FilterE(m51/p5), FilterF(m92/p2)
        if ((theme.componentId == "m50" && td.componentId == "p2") ||
            (theme.componentId == "m49" && td.componentId == "p2") ||
            (theme.componentId == "m51" && td.componentId == "p5") ||
            (theme.componentId == "m92" && td.componentId == "p2"))
            td.filterHz2Format = true;

        // fmtEqHz: 471*2^((v-60)/12) — EqMid (m103/p1), EqShelving (m104/p1)
        static const juce::StringArray eqModules { "m103", "m104" };
        if (eqModules.contains(theme.componentId) && td.componentId == "p1")
            td.eqHzFormat = true;

        // EqGain: (v-64)*0.28125 dB — EqMid (m103/p2), EqShelving (m104/p2)
        if (eqModules.contains(theme.componentId) && td.componentId == "p2")
            td.eqGainFormat = true;

        // EqBandwidth: v/75.0 Oct — EqMid only (m103/p3)
        if (theme.componentId == "m103" && td.componentId == "p3")
            td.eqBwFormat = true;

        // Vowels: VocalFilter (m45) p1=left, p2=middle, p3=right
        static const juce::StringArray vowelParams { "p1", "p2", "p3" };
        if (theme.componentId == "m45" && vowelParams.contains(td.componentId))
            td.vowelFormat = true;
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
