#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/ModuleDescriptions.h"

// Spotlight-style popup for quickly adding modules.
// Press Enter on canvas → popup appears at mouse position.
// Type to filter, arrows to navigate, Enter to add, Escape to close.
class QuickAddPopup : public juce::Component,
                      public juce::TextEditor::Listener,
                      public juce::KeyListener
{
public:
    using OnSelectCallback = std::function<void(const ModuleDescriptor*, int gridX, int gridY)>;
    using OnDismissCallback = std::function<void()>;

    QuickAddPopup(const ModuleDescriptions& descs, juce::Point<int> screenPos,
                  int gridX, int gridY, OnSelectCallback cb, OnDismissCallback dismissCb);
    ~QuickAddPopup() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor&) override;
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void textEditorEscapeKeyPressed(juce::TextEditor&) override;

    // KeyListener (for arrow keys)
    bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;

    // Lose focus → close
    void focusLost(FocusChangeType) override {}
    void inputAttemptWhenModal() override { dismiss(); }

    void grabFocusNow() { searchField.grabKeyboardFocus(); }

    // Called by PatchCanvas destructor to prevent callbacks firing on a dead parent
    void clearCallbacks() { onSelect = nullptr; onDismiss = nullptr; }

private:
    static constexpr int popupWidth  = 320;
    static constexpr int rowHeight   = 22;
    static constexpr int maxVisible  = 12;
    static constexpr int fieldHeight = 32;

    void rebuildFiltered(const juce::String& text);
    void confirmSelection();
    void dismiss();
    int totalHeight() const;

    const ModuleDescriptions& descs;
    juce::Point<int> spawnGridPos;
    OnSelectCallback onSelect;
    OnDismissCallback onDismiss;

    juce::TextEditor searchField;

    struct Entry { const ModuleDescriptor* desc; juce::String category; };
    std::vector<Entry> filtered;
    int selectedIdx = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickAddPopup)
};
