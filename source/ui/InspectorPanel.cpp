#include "InspectorPanel.h"
#include <cmath>

// ─── Morph group colours (same as canvas) ────────────────────────────────────
static const juce::Colour kMorphColors[4] = {
    juce::Colour(0xffCB4F4F),  // 1 – red
    juce::Colour(0xff9AC899),  // 2 – green
    juce::Colour(0xff5A5FB3),  // 3 – blue
    juce::Colour(0xffE5DE45),  // 4 – yellow
};
static const char* kGroupNames[4] = { "Macro 1", "Macro 2", "Macro 3", "Macro 4" };

// ─── MorphListComponent ───────────────────────────────────────────────────────
// Paints the 4 morph groups with their assigned parameters.
// Each row: [×]  ParamName  [  amount  ]
// Drag up/down on the amount to change morphRange.
class MorphListComponent : public juce::Component
{
public:
    // Row data
    struct Row
    {
        int   group;        // 0-3
        int   paramIndex;   // descriptor index
        juce::String name;
        Parameter*   param = nullptr;
    };

    // Callbacks set by InspectorPanel
    std::function<void(int paramIndex, int group)>        onRemove;      // group=-1 removes
    std::function<void(int paramIndex, int span, int dir)> onRangeChange;

    MorphListComponent() { setInterceptsMouseClicks(true, false); }

    void setModule(Module* m, int /*section*/)
    {
        module = m;
        rebuild();
    }

    void rebuild()
    {
        rows.clear();
        if (module == nullptr) { setSize(1, 1); repaint(); return; }

        for (auto& p : module->getParameters())
        {
            int g = p.getMorphGroup();
            if (g < 0 || g > 3) continue;
            auto* pd = p.getDescriptor();
            if (pd == nullptr) continue;
            rows.push_back({ g, pd->index, pd->name, const_cast<Parameter*>(&p) });
        }

        // Sort by group then by paramIndex
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){
            return a.group != b.group ? a.group < b.group : a.paramIndex < b.paramIndex;
        });

        // Compute required height
        int h = topPad;
        int prevGroup = -1;
        for (auto& r : rows)
        {
            if (r.group != prevGroup) { h += groupHeaderH; prevGroup = r.group; }
            h += rowH;
        }
        h += topPad;
        setSize(getWidth() > 0 ? getWidth() : 200, juce::jmax(h, 10));
        repaint();
    }

    void resized() override { rebuild(); }

    // ── Layout helpers ────────────────────────────────────────────────────────
    static constexpr int topPad      = 4;
    static constexpr int groupHeaderH = 20;
    static constexpr int rowH         = 22;
    static constexpr int xBtnW        = 18;
    static constexpr int amountW      = 52;
    static constexpr int marginX      = 6;

    // Returns y of a row (skipping headers)
    // Fills `hitGroup` and `isXBtn` / `isAmount`
    struct HitResult { int rowIdx = -1; bool isXBtn = false; bool isAmount = false; };

    HitResult findHit(juce::Point<int> pos) const
    {
        int y = topPad;
        int prevGroup = -1;
        for (int i = 0; i < (int)rows.size(); ++i)
        {
            const auto& r = rows[size_t(i)];
            if (r.group != prevGroup) { y += groupHeaderH; prevGroup = r.group; }
            juce::Rectangle<int> rowRect(0, y, getWidth(), rowH);
            if (rowRect.contains(pos))
            {
                HitResult hr;
                hr.rowIdx   = i;
                int amX     = getWidth() - marginX - amountW;
                hr.isXBtn   = (pos.x >= marginX && pos.x < marginX + xBtnW);
                hr.isAmount = (pos.x >= amX);
                return hr;
            }
            y += rowH;
        }
        return {};
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff16162a));

        if (rows.empty())
        {
            g.setColour(juce::Colour(0xff444466));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText("No morph assignments", getLocalBounds().reduced(marginX),
                       juce::Justification::centredTop);
            return;
        }

        int y = topPad;
        int prevGroup = -1;
        int w = getWidth();

        for (int i = 0; i < (int)rows.size(); ++i)
        {
            const auto& r = rows[size_t(i)];

            // Group header
            if (r.group != prevGroup)
            {
                prevGroup = r.group;
                juce::Colour gc = kMorphColors[r.group];
                g.setColour(gc.withAlpha(0.18f));
                g.fillRect(0, y, w, groupHeaderH);
                g.setColour(gc);
                g.fillRect(0, y, 3, groupHeaderH);   // left accent bar
                g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
                g.setColour(gc.brighter(0.3f));
                g.drawText(kGroupNames[r.group], marginX + 6, y, w - marginX * 2, groupHeaderH,
                           juce::Justification::centredLeft);
                y += groupHeaderH;
            }

            // Row background (alternating)
            g.setColour(juce::Colour(i % 2 == 0 ? 0xff1e1e38 : 0xff1a1a30));
            g.fillRect(0, y, w, rowH);

            // X remove button
            g.setColour(juce::Colour(0xff666688));
            juce::Rectangle<int> xRect(marginX, y + (rowH - xBtnW) / 2, xBtnW, xBtnW);
            g.drawRoundedRectangle(xRect.toFloat(), 3.0f, 1.0f);
            g.setFont(juce::FontOptions(10.0f));
            g.drawText("x", xRect, juce::Justification::centred);

            // Param name
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::FontOptions(11.0f));
            int nameX  = marginX + xBtnW + 4;
            int amX    = w - marginX - amountW;
            int nameW2 = amX - nameX - 4;
            g.drawText(r.name, nameX, y, nameW2, rowH, juce::Justification::centredLeft, true);

            // Amount bar
            if (r.param != nullptr)
            {
                int morphRange = r.param->getMorphRange();  // -127..127
                juce::Colour gc = kMorphColors[r.group];

                // Background of amount area
                juce::Rectangle<int> amRect(amX, y + 3, amountW, rowH - 6);
                g.setColour(juce::Colour(0xff0d0d20));
                g.fillRoundedRectangle(amRect.toFloat(), 3.0f);

                // Filled portion from centre
                float fraction = static_cast<float>(morphRange) / 127.0f;
                float midX = amRect.getX() + amRect.getWidth() * 0.5f;
                float barW = std::abs(fraction) * (amRect.getWidth() * 0.5f);
                float barX = (fraction >= 0.0f) ? midX : midX - barW;
                g.setColour(gc.withAlpha(0.75f));
                g.fillRoundedRectangle(barX, float(amRect.getY()),
                                       barW, float(amRect.getHeight()), 2.0f);

                // Centre line
                g.setColour(juce::Colour(0xff444466));
                g.drawVerticalLine(int(midX), float(amRect.getY()), float(amRect.getBottom()));

                // Value text
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.setFont(juce::FontOptions(10.0f));
                g.drawText(juce::String(morphRange), amRect, juce::Justification::centred);

                // Outline
                g.setColour(gc.withAlpha(0.4f));
                g.drawRoundedRectangle(amRect.toFloat(), 3.0f, 1.0f);
            }

            y += rowH;
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto hr = findHit(e.getPosition());
        if (hr.rowIdx < 0) return;

        if (hr.isXBtn)
        {
            // Save what we need before rebuild() invalidates rows[]
            auto& r = rows[size_t(hr.rowIdx)];
            int savedParamIndex = r.paramIndex;
            Parameter* p = r.param;
            if (p)
            {
                p->setMorphGroup(-1);
                p->setMorphRange(0);
            }
            rebuild();  // Rebuild first so rows is in consistent state
            if (onRemove) onRemove(savedParamIndex, -1);
            return;
        }

        if (hr.isAmount)
        {
            dragRowIdx   = hr.rowIdx;
            dragStartY   = e.getPosition().y;
            dragStartVal = rows[size_t(hr.rowIdx)].param
                         ? rows[size_t(hr.rowIdx)].param->getMorphRange() : 0;
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragRowIdx < 0 || dragRowIdx >= (int)rows.size()) return;
        auto& r = rows[size_t(dragRowIdx)];
        if (r.param == nullptr) return;

        // dy is bounded by screen pixel range (~0-4000), well within int.
        // jlimit clamps the final value to the valid morph range [-127, 127].
        int dy  = dragStartY - e.getPosition().y;  // up = increase
        int val = juce::jlimit(-127, 127, dragStartVal + dy);
        r.param->setMorphRange(val);

        int span = std::abs(val);
        int dir  = (val >= 0) ? 0 : 1;
        if (onRangeChange) onRangeChange(r.paramIndex, span, dir);
        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { dragRowIdx = -1; }

private:
    Module*       module    = nullptr;
    std::vector<Row> rows;
    int dragRowIdx  = -1;
    int dragStartY  = 0;
    int dragStartVal = 0;
};

// ─── InspectorPanel ──────────────────────────────────────────────────────────

InspectorPanel::InspectorPanel()
{
    titleLabel.setText("Inspector", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaacc));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    nameLabel.setText("Name", juce::dontSendNotification);
    nameLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888899));
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(nameLabel);

    nameEditor.setFont(juce::Font(juce::FontOptions(13.0f)));
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff252540));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a3a60));
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff6666cc));
    nameEditor.setInputRestrictions(16);
    nameEditor.addListener(this);
    nameEditor.setEnabled(false);
    addAndMakeVisible(nameEditor);

    sectionLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    sectionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666688));
    sectionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sectionLabel);

    // Morph list
    morphList = std::make_unique<MorphListComponent>();
    morphList->onRemove = [this](int paramIndex, int /*group*/)
    {
        if (onMorphGroupChanged && currentModule)
            onMorphGroupChanged(currentSection, currentModule, paramIndex, -1);
        repaint();
    };
    morphList->onRangeChange = [this](int paramIndex, int span, int dir)
    {
        if (onMorphRangeChanged && currentModule)
            onMorphRangeChanged(currentSection, currentModule, paramIndex, span, dir);
        repaint();
    };

    morphViewport.setViewedComponent(morphList.get(), false);
    morphViewport.setScrollBarsShown(true, false);
    morphViewport.setScrollBarThickness(6);
    addAndMakeVisible(morphViewport);
}

InspectorPanel::~InspectorPanel() = default;

void InspectorPanel::setModule(Module* module, int section)
{
    currentModule  = module;
    currentSection = section;

    if (module == nullptr) { clearModule(); return; }

    auto* desc = module->getDescriptor();
    titleLabel.setText(desc ? desc->fullname : "Module", juce::dontSendNotification);
    sectionLabel.setText(section == 1 ? "Poly" : "Common", juce::dontSendNotification);
    nameEditor.setEnabled(true);
    nameEditor.setText(module->getTitle(), juce::dontSendNotification);

    morphList->setModule(module, section);
    resized();
    repaint();
}

void InspectorPanel::clearModule()
{
    currentModule  = nullptr;
    currentSection = -1;
    titleLabel.setText("Inspector", juce::dontSendNotification);
    sectionLabel.setText("", juce::dontSendNotification);
    nameEditor.setText("", juce::dontSendNotification);
    nameEditor.setEnabled(false);
    morphList->setModule(nullptr, -1);
    repaint();
}

void InspectorPanel::refreshMorphList()
{
    morphList->rebuild();
    resized();
}

void InspectorPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a30));

    // Separator below name row
    g.setColour(juce::Colour(0xff2a2a50));
    g.fillRect(0, margin + rowH + 2 + 14 + 14 + margin * 2 + rowH + 4, getWidth(), 1);

    if (currentModule == nullptr)
    {
        g.setColour(juce::Colour(0xff444466));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("Select a module\nto inspect",
                   getLocalBounds().withTrimmedTop(80).reduced(margin),
                   juce::Justification::centredTop);
    }
}

void InspectorPanel::resized()
{
    int x = margin;
    int w = getWidth() - margin * 2;
    int y = margin;

    titleLabel.setBounds(x, y, w, rowH);   y += rowH + 2;
    sectionLabel.setBounds(x, y, w, 14);   y += 14 + 4;

    nameLabel.setBounds(x, y, w, 14);      y += 16;
    nameEditor.setBounds(x, y, w, rowH);   y += rowH + margin;

    // Separator
    y += 1 + margin;

    // Morph list fills the rest
    int remaining = getHeight() - y - margin;
    if (remaining > 0)
    {
        morphViewport.setBounds(0, y, getWidth(), remaining);
        morphList->setSize(getWidth(), morphList->getHeight());
    }
}

void InspectorPanel::textEditorReturnKeyPressed(juce::TextEditor&)
{
    commitName();
    grabKeyboardFocus();
}

void InspectorPanel::textEditorFocusLost(juce::TextEditor&)
{
    commitName();
}

void InspectorPanel::commitName()
{
    if (currentModule == nullptr) return;
    juce::String newName = nameEditor.getText().trim();
    if (newName.isEmpty()) { nameEditor.setText(currentModule->getTitle(), juce::dontSendNotification); return; }
    if (newName == currentModule->getTitle()) return;
    currentModule->setTitle(newName);
    if (onNameChanged) onNameChanged(currentSection, currentModule, newName);
}
