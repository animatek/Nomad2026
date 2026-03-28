#include "PatchSettingsDialog.h"

PatchSettingsDialog::PatchSettingsDialog(const PatchHeader& header, Callback onOk)
    : okCallback(std::move(onOk))
{
    // --- Voices ---
    voicesLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(voicesLabel);
    addAndMakeVisible(voicesReqLabel);
    voicesSlider.setRange(1, 32, 1);
    voicesSlider.setValue(header.voices, juce::dontSendNotification);
    voicesSlider.setSliderStyle(juce::Slider::IncDecButtons);
    voicesSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    addAndMakeVisible(voicesSlider);

    // --- Velocity range ---
    velLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(velLabel);
    addAndMakeVisible(velMaxLabel);
    addAndMakeVisible(velMinLabel);
    velMaxSlider.setRange(0, 127, 1);
    velMaxSlider.setValue(header.velRangeMax, juce::dontSendNotification);
    velMaxSlider.setSliderStyle(juce::Slider::IncDecButtons);
    velMaxSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    addAndMakeVisible(velMaxSlider);
    velMinSlider.setRange(0, 127, 1);
    velMinSlider.setValue(header.velRangeMin, juce::dontSendNotification);
    velMinSlider.setSliderStyle(juce::Slider::IncDecButtons);
    velMinSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    addAndMakeVisible(velMinSlider);

    // --- Keyboard range ---
    keyLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(keyLabel);
    addAndMakeVisible(keyMaxLabel);
    addAndMakeVisible(keyMinLabel);
    keyMaxSlider.setRange(0, 127, 1);
    keyMaxSlider.setValue(header.keyRangeMax, juce::dontSendNotification);
    keyMaxSlider.setSliderStyle(juce::Slider::IncDecButtons);
    keyMaxSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    addAndMakeVisible(keyMaxSlider);
    keyMinSlider.setRange(0, 127, 1);
    keyMinSlider.setValue(header.keyRangeMin, juce::dontSendNotification);
    keyMinSlider.setSliderStyle(juce::Slider::IncDecButtons);
    keyMinSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    addAndMakeVisible(keyMinSlider);

    // --- Pedal mode ---
    pedalLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(pedalLabel);
    pedalSustain.setRadioGroupId(1);
    pedalOnOff.setRadioGroupId(1);
    pedalSustain.setToggleState(header.pedalMode == 0, juce::dontSendNotification);
    pedalOnOff.setToggleState(header.pedalMode != 0, juce::dontSendNotification);
    addAndMakeVisible(pedalSustain);
    addAndMakeVisible(pedalOnOff);

    // --- Bend range ---
    bendLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(bendLabel);
    addAndMakeVisible(bendUnitLabel);
    bendSlider.setRange(1, 24, 1);
    bendSlider.setValue(header.bendRange, juce::dontSendNotification);
    bendSlider.setSliderStyle(juce::Slider::IncDecButtons);
    bendSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    addAndMakeVisible(bendSlider);

    // --- Portamento ---
    portaLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(portaLabel);
    portaNormal.setRadioGroupId(2);
    portaAuto.setRadioGroupId(2);
    portaNormal.setToggleState(!header.portamento, juce::dontSendNotification);
    portaAuto.setToggleState(header.portamento, juce::dontSendNotification);
    addAndMakeVisible(portaNormal);
    addAndMakeVisible(portaAuto);
    addAndMakeVisible(portaTimeLabel);
    portaTimeSlider.setRange(0, 127, 1);
    portaTimeSlider.setValue(header.portamentoTime, juce::dontSendNotification);
    portaTimeSlider.setSliderStyle(juce::Slider::IncDecButtons);
    portaTimeSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 22);
    addAndMakeVisible(portaTimeSlider);

    // --- Octave shift ---
    octaveLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(octaveLabel);
    const char* octNames[] = { "-2", "-1", "0", "+1", "+2" };
    for (int i = 0; i < 5; ++i)
    {
        octaveButtons[i].setButtonText(octNames[i]);
        octaveButtons[i].setRadioGroupId(3);
        octaveButtons[i].setToggleState(header.octaveShift == i, juce::dontSendNotification);
        addAndMakeVisible(octaveButtons[i]);
    }

    // --- Voice retrigger ---
    retrigLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(retrigLabel);
    retrigPoly.setToggleState(header.voiceRetriggerPoly != 0, juce::dontSendNotification);
    retrigCommon.setToggleState(header.voiceRetriggerCommon != 0, juce::dontSendNotification);
    addAndMakeVisible(retrigPoly);
    addAndMakeVisible(retrigCommon);

    // --- Buttons ---
    okButton.onClick = [this]()
    {
        if (okCallback)
        {
            Result r;
            r.voices = static_cast<int>(voicesSlider.getValue());
            r.velRangeMin = static_cast<int>(velMinSlider.getValue());
            r.velRangeMax = static_cast<int>(velMaxSlider.getValue());
            r.keyRangeMin = static_cast<int>(keyMinSlider.getValue());
            r.keyRangeMax = static_cast<int>(keyMaxSlider.getValue());
            r.pedalMode = pedalOnOff.getToggleState() ? 1 : 0;
            r.bendRange = static_cast<int>(bendSlider.getValue());
            r.portamento = portaAuto.getToggleState();
            r.portamentoTime = static_cast<int>(portaTimeSlider.getValue());
            r.octaveShift = 2;  // default to 0
            for (int i = 0; i < 5; ++i)
                if (octaveButtons[i].getToggleState()) { r.octaveShift = i; break; }
            r.voiceRetriggerPoly = retrigPoly.getToggleState();
            r.voiceRetriggerCommon = retrigCommon.getToggleState();
            okCallback(r);
        }
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(okButton);

    cancelButton.onClick = [this]()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(cancelButton);

    setSize(460, 340);
}

void PatchSettingsDialog::resized()
{
    auto area = getLocalBounds().reduced(12);
    const int rowH = 26;
    const int colW = 150;
    const int gap = 6;

    // Row 1: Voices | Velocity range | Keyboard range
    auto row1 = area.removeFromTop(60);
    {
        auto col = row1.removeFromLeft(colW);
        voicesLabel.setBounds(col.removeFromTop(16));
        auto r = col.removeFromTop(rowH);
        voicesReqLabel.setBounds(r.removeFromLeft(60));
        voicesSlider.setBounds(r);
    }
    row1.removeFromLeft(gap);
    {
        auto col = row1.removeFromLeft(colW);
        velLabel.setBounds(col.removeFromTop(16));
        auto r1 = col.removeFromTop(rowH);
        velMaxLabel.setBounds(r1.removeFromLeft(30));
        velMaxSlider.setBounds(r1.removeFromLeft(100));
        // Min below in next row area
    }
    row1.removeFromLeft(gap);
    {
        auto col = row1;
        keyLabel.setBounds(col.removeFromTop(16));
        auto r1 = col.removeFromTop(rowH);
        keyMaxLabel.setBounds(r1.removeFromLeft(30));
        keyMaxSlider.setBounds(r1.removeFromLeft(100));
    }

    // Row 2: vel min / key min
    auto row2 = area.removeFromTop(rowH);
    row2.removeFromLeft(colW + gap);
    {
        auto col = row2.removeFromLeft(colW);
        velMinLabel.setBounds(col.removeFromLeft(30));
        velMinSlider.setBounds(col.removeFromLeft(100));
    }
    row2.removeFromLeft(gap);
    {
        keyMinLabel.setBounds(row2.removeFromLeft(30));
        keyMinSlider.setBounds(row2.removeFromLeft(100));
    }

    area.removeFromTop(gap);

    // Row 3: Pedal mode | Bend range | Portamento
    auto row3 = area.removeFromTop(60);
    {
        auto col = row3.removeFromLeft(colW);
        pedalLabel.setBounds(col.removeFromTop(16));
        pedalSustain.setBounds(col.removeFromTop(rowH - 4));
        pedalOnOff.setBounds(col.removeFromTop(rowH - 4));
    }
    row3.removeFromLeft(gap);
    {
        auto col = row3.removeFromLeft(colW);
        bendLabel.setBounds(col.removeFromTop(16));
        auto r = col.removeFromTop(rowH);
        bendSlider.setBounds(r.removeFromLeft(100));
        bendUnitLabel.setBounds(r);
    }
    row3.removeFromLeft(gap);
    {
        auto col = row3;
        portaLabel.setBounds(col.removeFromTop(16));
        auto r = col.removeFromTop(rowH - 4);
        portaNormal.setBounds(r.removeFromLeft(70));
        portaTimeLabel.setBounds(r.removeFromLeft(35));
        portaTimeSlider.setBounds(r.removeFromLeft(100));
        portaAuto.setBounds(col.removeFromTop(rowH - 4).removeFromLeft(70));
    }

    area.removeFromTop(gap);

    // Row 4: Octave shift | Voice retrigger
    auto row4 = area.removeFromTop(40);
    {
        auto col = row4.removeFromLeft(250);
        octaveLabel.setBounds(col.removeFromTop(16));
        auto r = col.removeFromTop(rowH);
        for (int i = 0; i < 5; ++i)
            octaveButtons[i].setBounds(r.removeFromLeft(48));
    }
    row4.removeFromLeft(gap);
    {
        retrigLabel.setBounds(row4.removeFromTop(16));
        auto r = row4.removeFromTop(rowH);
        retrigPoly.setBounds(r.removeFromLeft(60));
        retrigCommon.setBounds(r.removeFromLeft(80));
    }

    area.removeFromTop(gap);

    // Bottom: OK / Cancel
    auto btnRow = area.removeFromBottom(30);
    cancelButton.setBounds(btnRow.removeFromRight(80));
    btnRow.removeFromRight(gap);
    okButton.setBounds(btnRow.removeFromRight(80));
}

void PatchSettingsDialog::show(juce::Component* parent, const PatchHeader& header, Callback onOk)
{
    auto* dialog = new PatchSettingsDialog(header, std::move(onOk));

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Patch Settings";
    options.dialogBackgroundColour = parent->getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}
