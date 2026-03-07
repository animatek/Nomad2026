#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/Patch.h"
#include <functional>

class PatchHeaderBar : public juce::Component
{
public:
    PatchHeaderBar();

    void setPatch(Patch* p);

    // Set DSP load values (0.0-1.0, or -1 for unknown)
    void setLoadValues(float pva, float e) { loadPva = pva; loadE = e; repaint(); }

    using MorphChangeCallback = std::function<void(int morphIndex, int value)>;
    using VoiceChangeCallback = std::function<void(int voices)>;
    using CableVisibilityCallback = std::function<void()>;

    void setMorphChangeCallback(MorphChangeCallback cb) { morphChangeCallback = std::move(cb); }
    void setVoiceChangeCallback(VoiceChangeCallback cb) { voiceChangeCallback = std::move(cb); }
    void setCableVisibilityCallback(CableVisibilityCallback cb) { cableVisCallback = std::move(cb); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void drawMorphKnob(juce::Graphics& g, float cx, float cy, float size,
                       float normalized, const juce::String& label, juce::Colour colour);
    void drawLoadBar(juce::Graphics& g, int x, int y, int w, int h,
                     float percent, const juce::String& label);

    int getMorphKnobAt(juce::Point<int> pos) const;
    juce::Rectangle<float> getMorphKnobBounds(int i) const;

    int getCableToggleAt(juce::Point<int> pos) const;
    juce::Rectangle<float> getCableToggleBounds(int i) const;

    enum class ArrowHit { None, Up, Down };
    ArrowHit getVoiceArrowAt(juce::Point<int> pos) const;

    void toggleCableVisibility(int index);
    bool getCableVisibility(int index) const;

    Patch* patch = nullptr;
    float loadPva = -1.0f;
    float loadE = -1.0f;

    struct DragState
    {
        int morphIndex = -1;
        int startValue = 0;
        juce::Point<int> startPos;
        int lastSentValue = -1;
        juce::int64 lastSendTime = 0;
    };
    DragState dragState;

    MorphChangeCallback morphChangeCallback;
    VoiceChangeCallback voiceChangeCallback;
    CableVisibilityCallback cableVisCallback;

    // Cached section X positions (computed in resized)
    int patchSecX_ = 0;
    int voicesSecX_ = 0;
    int loadSecX_ = 0;
    int morphSecX_ = 0;
    int cableSecX_ = 0;

    static constexpr int morphKnobSize = 26;
    static constexpr int morphKnobSpacing = 8;
    static constexpr int cableToggleSize = 14;
    static constexpr int cableToggleSpacing = 5;
    static constexpr int numCableTypes = 7;
    static constexpr int paramSendIntervalMs = 50;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchHeaderBar)
};
