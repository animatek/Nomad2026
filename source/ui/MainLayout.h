#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PatchCanvasComponent.h"
#include "InspectorPanel.h"
#include "PatchBrowserPanel.h"
#include "StatusBar.h"
#include "PatchHeaderBar.h"
#include "../model/ModuleDescriptions.h"

// Custom slot selector panel — shows 4 slot buttons with patch names
class SlotBar : public juce::Component
{
public:
    SlotBar();

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void resized() override;

    void setCurrentTab(int index);
    int  getCurrentTabIndex() const { return activeIndex; }
    void setSlotName(int slot, const juce::String& patchName);

    std::function<void(int)> onSlotChanged;

private:
    static constexpr int numSlots = 4;
    int activeIndex = 0;
    juce::String slotNames[numSlots];  // patch names per slot
    juce::Rectangle<int> slotBounds[numSlots];

    static constexpr const char* slotLetters[] = { "A", "B", "C", "D" };

    // Keyboard icon SVG path (simplified synth icon)
    void drawSlotIcon(juce::Graphics& g, juce::Rectangle<int> area, bool active);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotBar)
};

class MainLayout : public juce::Component,
                   public juce::DragAndDropContainer
{
public:
    MainLayout(ModuleDescriptions& moduleDescs);

    void paint(juce::Graphics& g) override;
    void resized() override;

    PatchCanvasComponent& getCanvas()      { return canvasComponent; }
    void setTheme(const ColorScheme& cs)   { canvasComponent.setTheme(cs); }
    InspectorPanel&       getInspector()   { return inspectorPanel; }
    PatchBrowserPanel&    getPatchBrowser() { return patchBrowserPanel; }
    StatusBar&            getStatusBar()   { return statusBar; }
    PatchHeaderBar&       getHeaderBar()   { return headerBar; }
    SlotBar&              getSlotBar()     { return slotBar; }

    std::function<void(int)> onSlotChanged;  // called with slot index 0-3
    std::function<void()> onMidiSettingsClicked;
    std::function<void()> onStoreToBankClicked;

private:
    // Left column: inspector + toolbar + slots
    SlotBar           slotBar;
    InspectorPanel    inspectorPanel;

    // Toolbar buttons
    juce::TextButton midiButton { "MIDI" };
    juce::TextButton storeButton { "Store" };
    juce::Component   leftColumn;   // groups all left elements

    PatchCanvasComponent canvasComponent; // centre — patch canvas
    PatchBrowserPanel patchBrowserPanel;  // right  — synth patch browser
    StatusBar         statusBar;
    PatchHeaderBar    headerBar;

    // Stretchable layout: [leftColumn | bar | canvas | bar | patchBrowser]
    juce::StretchableLayoutManager layoutManager;
    juce::StretchableLayoutResizerBar resizerBar1 { &layoutManager, 1, true };
    juce::StretchableLayoutResizerBar resizerBar2 { &layoutManager, 3, true };

    static constexpr int statusBarHeight = 24;
    static constexpr int slotBarHeight   = 100;  // 4 rows × 25px
    static constexpr int toolbarHeight   = 28;
    static constexpr int headerBarHeight = 48;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainLayout)
};
