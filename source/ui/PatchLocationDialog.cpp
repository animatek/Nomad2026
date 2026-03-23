#include "PatchLocationDialog.h"

PatchLocationDialog::PatchLocationDialog(const std::vector<std::string>& patchList,
                                         bool showSlot,
                                         int currentSlot)
    : patchList_(patchList)
    , showSlot_(showSlot)
{
    // Slot selector (A-D)
    if (showSlot_)
    {
        slotLabel.setText("Slot:", juce::dontSendNotification);
        slotLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(slotLabel);

        slotCombo.addItem("A (Slot 1)", 1);
        slotCombo.addItem("B (Slot 2)", 2);
        slotCombo.addItem("C (Slot 3)", 3);
        slotCombo.addItem("D (Slot 4)", 4);
        slotCombo.setSelectedItemIndex(currentSlot, juce::dontSendNotification);
        addAndMakeVisible(slotCombo);
    }

    // Bank selector (1-9)
    bankLabel.setText("Bank:", juce::dontSendNotification);
    bankLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(bankLabel);

    for (int b = 1; b <= 9; ++b)
        bankCombo.addItem("Bank " + juce::String(b), b);
    bankCombo.setSelectedItemIndex(0, juce::dontSendNotification);
    bankCombo.onChange = [this]() { updatePositionItems(); };
    addAndMakeVisible(bankCombo);

    // Position selector (1-99 with patch names)
    positionLabel.setText("Position:", juce::dontSendNotification);
    positionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(positionLabel);

    addAndMakeVisible(positionCombo);
    updatePositionItems();

    // Buttons
    confirmButton.setButtonText("OK");
    confirmButton.onClick = [this]() { onConfirm(); };
    addAndMakeVisible(confirmButton);

    cancelButton.setButtonText("Cancel");
    cancelButton.onClick = [this]() { onCancel(); };
    addAndMakeVisible(cancelButton);

    setSize(400, showSlot_ ? 200 : 160);
}

void PatchLocationDialog::updatePositionItems()
{
    int bank = bankCombo.getSelectedItemIndex() + 1;  // 1-9
    int section = bank - 1;  // 0-8

    positionCombo.clear(juce::dontSendNotification);

    for (int pos = 1; pos <= 99; ++pos)
    {
        int position = pos - 1;  // 0-98
        int index = section * 99 + position;

        juce::String patchName = (index < static_cast<int>(patchList_.size()) && !patchList_[index].empty())
                                  ? juce::String(patchList_[index])
                                  : "--";

        positionCombo.addItem(juce::String(pos).paddedLeft('0', 2) + ": " + patchName, pos);
    }

    positionCombo.setSelectedItemIndex(0, juce::dontSendNotification);
}

void PatchLocationDialog::onConfirm()
{
    Result result;
    result.slot = showSlot_ ? slotCombo.getSelectedItemIndex() : 0;
    result.section = bankCombo.getSelectedItemIndex();  // 0-8
    result.position = positionCombo.getSelectedItemIndex();  // 0-98
    result.confirmed = true;

    // Close the dialog window
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
        window->exitModalState(1);

    if (callback)
        callback(result);
}

void PatchLocationDialog::onCancel()
{
    Result result;
    result.confirmed = false;

    // Close the dialog window
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
        window->exitModalState(0);

    if (callback)
        callback(result);
}

void PatchLocationDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);
    int rowHeight = 32;
    int labelWidth = 80;
    int gap = 10;

    // Slot row (if visible)
    if (showSlot_)
    {
        auto slotRow = bounds.removeFromTop(rowHeight);
        slotLabel.setBounds(slotRow.removeFromLeft(labelWidth));
        slotRow.removeFromLeft(gap);
        slotCombo.setBounds(slotRow);
        bounds.removeFromTop(gap);
    }

    // Bank row
    auto bankRow = bounds.removeFromTop(rowHeight);
    bankLabel.setBounds(bankRow.removeFromLeft(labelWidth));
    bankRow.removeFromLeft(gap);
    bankCombo.setBounds(bankRow);
    bounds.removeFromTop(gap);

    // Position row
    auto posRow = bounds.removeFromTop(rowHeight);
    positionLabel.setBounds(posRow.removeFromLeft(labelWidth));
    posRow.removeFromLeft(gap);
    positionCombo.setBounds(posRow);
    bounds.removeFromTop(gap * 2);

    // Buttons row
    auto buttonRow = bounds.removeFromTop(rowHeight);
    int buttonWidth = 80;
    int buttonGap = 10;
    buttonRow.removeFromRight((buttonRow.getWidth() - 2 * buttonWidth - buttonGap) / 2);
    cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonGap);
    confirmButton.setBounds(buttonRow.removeFromRight(buttonWidth));
}

void PatchLocationDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}
