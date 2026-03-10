#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <string>
#include <functional>

/**
 * Dialog for selecting a patch location (Slot + Bank + Position).
 * Position dropdown shows patch names from the selected bank.
 */
class PatchLocationDialog : public juce::Component
{
public:
    struct Result
    {
        int slot = 0;       // 0-3
        int section = 0;    // 0-8
        int position = 0;   // 0-98
        bool confirmed = false;
    };

    using Callback = std::function<void(const Result&)>;

    /**
     * @param patchList All 891 patch names (9 banks × 99 positions)
     * @param showSlot If true, shows Slot selector (A-D); if false, hides it
     * @param currentSlot Default slot selection (0-3)
     */
    PatchLocationDialog(const std::vector<std::string>& patchList,
                        bool showSlot = true,
                        int currentSlot = 0);

    void setCallback(Callback cb) { callback = std::move(cb); }

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void updatePositionItems();
    void onConfirm();
    void onCancel();

    const std::vector<std::string>& patchList_;
    bool showSlot_;
    Callback callback;

    juce::Label slotLabel;
    juce::ComboBox slotCombo;

    juce::Label bankLabel;
    juce::ComboBox bankCombo;

    juce::Label positionLabel;
    juce::ComboBox positionCombo;

    juce::TextButton confirmButton;
    juce::TextButton cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchLocationDialog)
};
