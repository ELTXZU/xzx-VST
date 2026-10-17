#pragma once
#include "PluginProcessor.h"

//==============================================================================
namespace xzxcol
{
    const juce::Colour bg      { 0xffe9e6eb };
    const juce::Colour panel   { 0xfff5f3f6 };
    const juce::Colour border  { 0xffcfcad4 };
    const juce::Colour ink     { 0xff3a3840 };
    const juce::Colour soft    { 0xff8d8994 };
    const juce::Colour track   { 0xffd9d5dd };

    inline juce::Colour mood (int i)
    {
        static const juce::uint32 c[] = { 0xfff08cb4, 0xffb59cf0, 0xff7fd6b6, 0xffffa97a, 0xff7cc4f0 };
        return juce::Colour (c[juce::jlimit (0, 4, i)]);
    }

    inline juce::Colour mode (int m)
    {
        static const juce::uint32 c[] = { 0xffe2dee6,   // off
                                          0xfff08cb4,   // stutter
                                          0xffb59cf0,   // reverse
                                          0xffffa97a,   // tape stop
                                          0xff7fd6b6,   // octave up
                                          0xff7cc4f0,   // octave down
                                          0xff8d8994,   // mute
                                          0xffffd66b }; // crush
        return juce::Colour (c[juce::jlimit (0, 7, m)]);
    }
}

//==============================================================================
// Soft, minimal look & feel
class XZXLook : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { 0xfff08cb4 };

    juce::Font getLabelFont (juce::Label&) override           { return juce::Font (juce::FontOptions (12.0f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override     { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (13.0f)); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float lw = juce::jmax (3.0f, radius * 0.11f);
        const float arcR = radius - lw * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (xzxcol::track);
        g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (pos > 0.002f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
            g.setColour (accent);
            g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float fr = arcR - lw * 1.4f;
        g.setColour (juce::Colour (0xfffbfafc));
        g.fillEllipse (c.x - fr, c.y - fr, fr * 2.0f, fr * 2.0f);
        g.setColour (xzxcol::ink);
        g.drawEllipse (c.x - fr, c.y - fr, fr * 2.0f, fr * 2.0f, 1.5f);

        juce::Path ptr;
        ptr.addRoundedRectangle (-1.2f, -fr + 3.0f, 2.4f, fr * 0.42f, 1.2f);
        ptr.applyTransform (juce::AffineTransform::rotation (angle).translated (c.x, c.y));
        g.setColour (xzxcol::ink);
        g.fillPath (ptr);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent.withAlpha (0.55f) : down ? accent.withAlpha (0.35f)
                                                         : hover ? juce::Colour (0xfff0edf2) : juce::Colour (0xffe8e4ea));
        g.fillRoundedRectangle (r, r.getHeight() * 0.35f);
        g.setColour (juce::Colour (0xff9a95a1));
        g.drawRoundedRectangle (r, r.getHeight() * 0.35f, 1.2f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colour (0xffe8e4ea));
        g.fillRoundedRectangle (r, r.getHeight() * 0.35f);
        g.setColour (box.isPopupActive() ? accent : juce::Colour (0xff9a95a1));
        g.drawRoundedRectangle (r, r.getHeight() * 0.35f, 1.2f);

        juce::Path arrow;
        const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
        arrow.addTriangle (cx - 3.5f, cy - 2.0f, cx + 3.5f, cy - 2.0f, cx, cy + 2.5f);
        g.setColour (xzxcol::ink);
        g.fillPath (arrow);
    }
};

//==============================================================================
class XZXKnob : public juce::Component
{
public:
    XZXKnob (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& text)
        : attach (apvts, id, slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 15);
        label.setText (text.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, xzxcol::soft);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (14));
        slider.setBounds (r);
    }

private:
    juce::Slider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attach;
};

//==============================================================================
class XZXChoice : public juce::Component
{
public:
    XZXChoice (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, const juce::String& text)
    {
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            box.addItemList (cp->choices, 1);

        label.setText (text.toUpperCase(), juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, xzxcol::soft);
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (box);
        addAndMakeVisible (label);
        attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        box.setBounds (r.removeFromTop (24));
    }

private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
};

//==============================================================================
// Rounded light panel with a small lowercase title and a flexible control layout
class XZXPanel : public juce::Component
{
public:
    XZXPanel (const juce::String& t, juce::AudioProcessorValueTreeState& state)
        : title (t), apvts (state) {}

    void addKnob (const juce::String& id, const juce::String& text)
    {
        addAndMakeVisible (knobs.add (new XZXKnob (apvts, id, text)));
    }

    void addChoice (const juce::String& id, const juce::String& text)
    {
        addAndMakeVisible (choices.add (new XZXChoice (apvts, id, text)));
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (xzxcol::panel);
        g.fillRoundedRectangle (b, 14.0f);
        g.setColour (xzxcol::border);
        g.drawRoundedRectangle (b, 14.0f, 1.2f);
        g.setColour (xzxcol::soft);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (title, 16, 8, 200, 18, juce::Justification::centredLeft);
    }

    void layoutControls (juce::Rectangle<int> area, int knobCols, bool singleRow)
    {
        if (singleRow)
        {
            const int items = choices.size() + knobs.size();
            if (items == 0) return;
            const int w = area.getWidth() / items;
            for (auto* c : choices)
            {
                auto cell = area.removeFromLeft (w);
                c->setBounds (cell.reduced (8, 0).withSizeKeepingCentre (juce::jmax (20, w - 16), 40));
            }
            for (auto* k : knobs)
                k->setBounds (area.removeFromLeft (w));
            return;
        }

        if (choices.size() > 0)
        {
            auto row = area.removeFromTop (42);
            const int w = row.getWidth() / choices.size();
            for (auto* c : choices)
                c->setBounds (row.removeFromLeft (w).reduced (4, 0));
            area.removeFromTop (4);
        }

        if (knobs.size() > 0)
        {
            const int cols = juce::jmax (1, juce::jmin (knobCols, knobs.size()));
            const int rows = (knobs.size() + cols - 1) / cols;
            const int cw = area.getWidth() / cols, ch = area.getHeight() / rows;
            for (int i = 0; i < knobs.size(); ++i)
                knobs[i]->setBounds (area.getX() + (i % cols) * cw, area.getY() + (i / cols) * ch, cw, ch);
        }
    }

protected:
    juce::String title;
    juce::AudioProcessorValueTreeState& apvts;
    juce::OwnedArray<XZXKnob> knobs;
    juce::OwnedArray<XZXChoice> choices;
};

//==============================================================================
// 16 clickable pattern cells
class XZXStepGrid : public juce::Component, private juce::Timer
{
public:
    XZXStepGrid (XZXAudioProcessor& p, const int& brushRef) : proc (p), brush (brushRef)
    {
        for (int i = 0; i < XZXAudioProcessor::numSteps; ++i)
            modeP[i] = proc.apvts.getRawParameterValue ("step_" + juce::String (i + 1));
        lenP = proc.apvts.getRawParameterValue ("steps");
        startTimerHz (30);
    }

    void timerCallback() override
    {
        const int cur = proc.visStep.load();
        if (cur != lastStep) { lastStep = cur; }
        repaint();
    }

    juce::Rectangle<float> cellBounds (int i) const
    {
        const float gap = 6.0f;
        const float w = ((float) getWidth() - gap * 7.0f) / 8.0f;
        const float h = ((float) getHeight() - gap) / 2.0f;
        return { (float) (i % 8) * (w + gap), (float) (i / 8) * (h + gap), w, h };
    }

    void paint (juce::Graphics& g) override
    {
        const int len = (int) lenP->load();
        const bool running = proc.visStep.load() >= 0;
        for (int i = 0; i < XZXAudioProcessor::numSteps; ++i)
        {
            auto r = cellBounds (i);
            const int m = (int) modeP[i]->load();
            const bool inRange = i < len;
            const bool playing = running && i == lastStep && inRange;

            g.setColour (xzxcol::mode (m).withAlpha (inRange ? 1.0f : 0.35f));
            g.fillRoundedRectangle (r, 9.0f);

            g.setColour (playing ? xzxcol::ink : xzxcol::border);
            g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, playing ? 2.4f : 1.2f);

            if (m > 0)
            {
                static const char* glyph[] = { "", "st", "rv", "tp", "up", "dn", "mt", "cr" };
                g.setColour (xzxcol::ink.withAlpha (inRange ? 0.8f : 0.4f));
                g.setFont (juce::Font (juce::FontOptions (13.0f)));
                g.drawText (glyph[m], r, juce::Justification::centred);
            }
            else if (playing)
            {
                g.setColour (xzxcol::ink.withAlpha (0.5f));
                g.fillEllipse (r.getCentreX() - 3.0f, r.getCentreY() - 3.0f, 6.0f, 6.0f);
            }
        }
    }

    int cellAt (juce::Point<float> pt) const
    {
        for (int i = 0; i < XZXAudioProcessor::numSteps; ++i)
            if (cellBounds (i).expanded (3.0f).contains (pt)) return i;
        return -1;
    }

    void setStep (int i, int mode)
    {
        if (i < 0) return;
        if (auto* prm = proc.apvts.getParameter ("step_" + juce::String (i + 1)))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) mode));
            prm->endChangeGesture();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.position);
        if (i < 0) return;
        const int cur = (int) modeP[i]->load();
        paintValue = e.mods.isPopupMenu() ? 0 : (cur == brush ? 0 : brush);
        setStep (i, paintValue);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int i = cellAt (e.position);
        if (i >= 0 && (int) modeP[i]->load() != paintValue)
            setStep (i, paintValue);
    }

private:
    XZXAudioProcessor& proc;
    const int& brush;
    std::atomic<float>* modeP[XZXAudioProcessor::numSteps] {};
    std::atomic<float>* lenP = nullptr;
    int lastStep = -1;
    int paintValue = 1;
};

//==============================================================================
// Brush chips: pick which effect you are painting into the grid
class XZXBrushChips : public juce::Component
{
public:
    explicit XZXBrushChips (int& brushRef) : brush (brushRef) {}

    juce::Rectangle<float> chipBounds (int i) const
    {
        const float gap = 6.0f;
        const float w = ((float) getWidth() - gap * 7.0f) / 8.0f;
        return { (float) i * (w + gap), 0.0f, w, (float) getHeight() };
    }

    void paint (juce::Graphics& g) override
    {
        static const char* names[] = { "off", "stutter", "reverse", "stop", "up", "down", "mute", "crush" };
        for (int i = 0; i < 8; ++i)
        {
            auto r = chipBounds (i);
            const bool sel = i == brush;
            g.setColour (sel ? xzxcol::mode (i) : xzxcol::mode (i).withAlpha (0.35f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.4f);
            g.setColour (sel ? xzxcol::ink : xzxcol::border);
            g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.4f, sel ? 1.8f : 1.0f);
            g.setColour (xzxcol::ink.withAlpha (sel ? 1.0f : 0.6f));
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.drawText (names[i], r, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 8; ++i)
            if (chipBounds (i).contains (e.position)) { brush = i; repaint(); return; }
    }

private:
    int& brush;
};

//==============================================================================
// Drawable curve (ShaperBox style). which: 0 = volume, 1 = filter
class XZXCurve : public juce::Component, private juce::Timer
{
public:
    XZXCurve (XZXAudioProcessor& p, int whichCurve, const juce::Colour& accentRef)
        : proc (p), which (whichCurve), accent (accentRef)
    {
        startTimerHz (30);
    }

    void timerCallback() override { repaint(); }

    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (6.0f); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xfffbfafc));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (xzxcol::border);
        g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.2f);

        auto a = area();
        g.setColour (xzxcol::track.withAlpha (0.8f));
        for (int i = 1; i < 4; ++i)
            g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / 4.0f), a.getY(), a.getBottom());
        g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());

        const auto c = proc.getCurveCopy (which);
        juce::Path line, fill;
        for (int i = 0; i < XZXAudioProcessor::curveN; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / (float) (XZXAudioProcessor::curveN - 1);
            const float y = a.getBottom() - c[(size_t) i] * a.getHeight();
            if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, a.getBottom()); fill.lineTo (x, y); }
            else        { line.lineTo (x, y); fill.lineTo (x, y); }
        }
        fill.lineTo (a.getRight(), a.getBottom());
        fill.closeSubPath();

        g.setColour (accent.withAlpha (0.28f));
        g.fillPath (fill);
        g.setColour (accent);
        g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float ph = which == 0 ? proc.visVolPhase.load() : proc.visFiltPhase.load();
        const float px = a.getX() + a.getWidth() * ph;
        g.setColour (xzxcol::ink.withAlpha (0.55f));
        g.drawVerticalLine ((int) px, a.getY(), a.getBottom());
    }

    void editAt (const juce::MouseEvent& e)
    {
        auto a = area();
        const int i = juce::jlimit (0, XZXAudioProcessor::curveN - 1,
                          (int) std::round ((e.position.x - a.getX()) / a.getWidth() * (float) (XZXAudioProcessor::curveN - 1)));
        const float v = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - a.getY()) / a.getHeight());

        if (lastI < 0) { work[(size_t) i] = v; }
        else
        {
            const int lo = juce::jmin (lastI, i), hi = juce::jmax (lastI, i);
            for (int k = lo; k <= hi; ++k)
            {
                const float t = hi == lo ? 1.0f : (float) (k - lo) / (float) (hi - lo);
                work[(size_t) k] = (lastI <= i) ? lastV + (v - lastV) * t : v + (lastV - v) * t;
            }
        }
        lastI = i; lastV = v;
        proc.setCurve (which, work);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        work = proc.getCurveCopy (which);
        lastI = -1;
        editAt (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override { editAt (e); }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        proc.setCurve (which, XZXAudioProcessor::makeCurveShape (0, which == 0 ? 1.0f : 0.5f));
    }

    void applyShape (int kind)
    {
        proc.setCurve (which, XZXAudioProcessor::makeCurveShape (kind, which == 0 ? 1.0f : 0.5f));
    }

private:
    XZXAudioProcessor& proc;
    int which;
    const juce::Colour& accent;
    std::array<float, XZXAudioProcessor::curveN> work {};
    int lastI = -1;
    float lastV = 0.f;
};

//==============================================================================
// Shaper panel = drawable curve + quick shape buttons + parameters
class XZXShaperPanel : public XZXPanel
{
public:
    XZXShaperPanel (const juce::String& t, XZXAudioProcessor& p, int which, const juce::Colour& accentRef)
        : XZXPanel (t, p.apvts), curve (p, which, accentRef)
    {
        addAndMakeVisible (curve);
        static const char* names[] = { "flat", "saw", "ramp", "sine", "sqr", "rnd" };
        for (int i = 0; i < 6; ++i)
        {
            auto* b = shapeBtns.add (new juce::TextButton (names[i]));
            b->onClick = [this, i] { curve.applyShape (i == 0 ? 0 : i); };
            addAndMakeVisible (b);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        auto header = r.removeFromTop (24);
        const int bw = 40;
        for (int i = shapeBtns.size() - 1; i >= 0; --i)
        {
            shapeBtns[i]->setBounds (header.removeFromRight (bw).reduced (2, 1));
        }
        r.removeFromTop (4);

        auto left = r.removeFromLeft ((int) ((float) r.getWidth() * 0.56f));
        curve.setBounds (left);
        r.removeFromLeft (8);
        layoutControls (r, 2, false);
    }

private:
    XZXCurve curve;
    juce::OwnedArray<juce::TextButton> shapeBtns;
};

//==============================================================================
// Pattern panel = step grid + brushes + parameters
class XZXPatternPanel : public XZXPanel
{
public:
    XZXPatternPanel (XZXAudioProcessor& p)
        : XZXPanel ("pattern - paint glitches onto the steps", p.apvts),
          grid (p, brush), chips (brush)
    {
        addAndMakeVisible (grid);
        addAndMakeVisible (chips);
        addChoice ("step_rate", "rate");
        addKnob ("steps", "steps");
        addKnob ("chance", "chance");
        addKnob ("fade_ms", "smooth");
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 12);
        r.removeFromTop (22);

        auto right = r.removeFromRight ((int) ((float) r.getWidth() * 0.34f));
        r.removeFromRight (10);

        auto chipRow = r.removeFromBottom (28);
        r.removeFromBottom (8);
        grid.setBounds (r);
        chips.setBounds (chipRow);

        layoutControls (right, 3, false);
    }

private:
    int brush = 1;
    XZXStepGrid grid;
    XZXBrushChips chips;
};

//==============================================================================
// Little accent-colour dots
class XZXMoodDots : public juce::Component
{
public:
    std::function<void (int)> onPick;
    int selected = 0;

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < 5; ++i)
        {
            auto r = dotBounds (i);
            g.setColour (xzxcol::mood (i));
            g.fillEllipse (r);
            if (i == selected)
            {
                g.setColour (xzxcol::ink);
                g.drawEllipse (r.expanded (3.0f), 1.5f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 5; ++i)
            if (dotBounds (i).expanded (4.0f).contains (e.position))
            {
                selected = i;
                if (onPick) onPick (i);
                repaint();
                return;
            }
    }

private:
    juce::Rectangle<float> dotBounds (int i) const
    {
        const float d = 14.0f;
        return { 4.0f + (float) i * 22.0f, ((float) getHeight() - d) * 0.5f, d, d };
    }
};

//==============================================================================
// The little guy: reacts to the audio and to whichever glitch is firing
class XZXCreature : public juce::Component, private juce::Timer
{
public:
    XZXCreature (XZXAudioProcessor& p, const juce::Colour& accentRef)
        : proc (p), accent (accentRef)
    {
        startTimerHz (60);
    }

    void timerCallback() override
    {
        const float lvl = proc.visLevel.load();
        level += (lvl - level) * (lvl > level ? 0.5f : 0.10f);
        mode = proc.visMode.load();
        napping = proc.visNap.load();

        float stretch = 0.0f;
        if (mode == XZXAudioProcessor::OctUp)    stretch = 0.20f;
        if (mode == XZXAudioProcessor::OctDown)  stretch = -0.12f;
        if (mode == XZXAudioProcessor::TapeStop) stretch = -0.26f;
        if (napping)                             stretch = -0.06f;

        const float target = (napping ? 0.0f : level * 0.22f) + stretch;
        vel += (target - squash) * 0.25f;
        vel *= 0.72f;
        squash += vel;

        t += 1.0f / 60.0f;
        if (--blinkTimer <= 0) { blink = 7; blinkTimer = 100 + rng.nextInt (220); }
        if (blink > 0) --blink;

        jitter = (mode == XZXAudioProcessor::Stutter) ? (rng.nextFloat() - 0.5f) * 7.0f : jitter * 0.5f;
        glitchSeed = rng.nextInt (1000);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX() + jitter;
        const float floorY = b.getBottom() - 64.0f;

        const float bodyW = juce::jmin (b.getWidth() * 0.66f, b.getHeight() * 0.46f);
        const float bodyH = bodyW * 1.22f;
        const float bob = napping ? 0.0f : std::sin (t * 3.2f) * 2.5f;
        const float bodyBottom = floorY - 12.0f - bob;

        // shadow
        g.setColour (xzxcol::ink.withAlpha (0.10f));
        const float sw = bodyW * (0.95f - squash * 0.4f);
        g.fillEllipse (cx - sw * 0.5f, floorY - 2.0f, sw, 12.0f);

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (1.0f - squash * 0.5f, 1.0f + squash, cx, floorY));

        const float x0 = cx - bodyW * 0.5f;
        const float rTop = bodyW * 0.5f;
        const float yTop = bodyBottom - bodyH;
        const float yc = yTop + rTop;
        const float bump = bodyW * 0.09f;
        const float yb = bodyBottom - bump;

        // arms (tiny nubs)
        const float wave = napping ? 0.0f : std::sin(t * 5.0f) * (0.15f + level * 0.5f);
        g.setColour (juce::Colour (0xffa9a5b2));
        for (int side = -1; side <= 1; side += 2)
        {
            juce::Path arm;
            arm.addRoundedRectangle (-4.0f, 0.0f, 8.0f, bodyW * 0.22f, 4.0f);
            arm.applyTransform (juce::AffineTransform::rotation ((float) side * (0.9f + wave))
                                    .translated (cx + (float) side * (bodyW * 0.5f - 2.0f), yc + rTop * 0.55f));
            g.fillPath (arm);
            g.setColour (xzxcol::ink);
            g.strokePath (arm, juce::PathStrokeType (1.6f));
            g.setColour (juce::Colour (0xffa9a5b2));
        }

        // body = gumdrop ghost with a scalloped hem
        juce::Path body;
        body.addCentredArc (cx, yc, rTop, rTop, 0.0f, -juce::MathConstants<float>::halfPi,
                            juce::MathConstants<float>::halfPi, true);
        body.lineTo (x0 + bodyW, yb);
        const float seg = bodyW / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float xr = x0 + bodyW - (float) i * seg;
            body.quadraticTo (xr - seg * 0.5f, yb + bump * 2.0f, xr - seg, yb);
        }
        body.closeSubPath();

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfcbd5), cx, yTop,
                                                 juce::Colour (0xffb2aebb), cx, bodyBottom, false));
        g.fillPath (body);
        g.setColour (xzxcol::ink);
        g.strokePath (body, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // glitch slices over the body
        if (mode != XZXAudioProcessor::Off && ! napping)
        {
            juce::Random r ((juce::int64) glitchSeed);
            g.setColour (accent.withAlpha (0.45f));
            for (int i = 0; i < 3; ++i)
            {
                const float sy = yTop + r.nextFloat() * bodyH * 0.85f;
                const float sx = x0 + r.nextFloat() * bodyW * 0.4f;
                g.fillRect (sx, sy, bodyW * (0.3f + r.nextFloat() * 0.5f), 3.0f);
            }
        }

        // tuft: three little pink curls
        for (int i = -1; i <= 1; ++i)
        {
            const float ang = std::sin (t * 4.0f + (float) i) * (0.18f + level * 0.5f) + (float) i * 0.45f;
            juce::Path stem;
            stem.startNewSubPath (cx + (float) i * bodyW * 0.07f, yTop + 3.0f);
            const float len = bodyW * (0.17f + 0.04f * (float) (1 - std::abs (i)));
            const float ex = cx + (float) i * bodyW * 0.07f + std::sin (ang) * len;
            const float ey = yTop + 3.0f - std::cos (ang) * len;
            stem.quadraticTo (cx + (float) i * bodyW * 0.07f, yTop - len * 0.5f, ex, ey);
            g.setColour (xzxcol::ink);
            g.strokePath (stem, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (stem, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.fillEllipse (ex - 4.0f, ey - 4.0f, 8.0f, 8.0f);
        }

        // cheeks
        g.setColour (accent.withAlpha (0.75f));
        const float cheekY = yc + rTop * 0.52f;
        g.fillEllipse (cx - bodyW * 0.30f - bodyW * 0.07f, cheekY - bodyW * 0.045f, bodyW * 0.14f, bodyW * 0.09f);
        g.fillEllipse (cx + bodyW * 0.30f - bodyW * 0.07f, cheekY - bodyW * 0.045f, bodyW * 0.14f, bodyW * 0.09f);

        // eyes
        const float eyeY = yc + rTop * 0.12f;
        const float eyeDX = bodyW * 0.17f;
        const bool shut = napping || blink > 0 || mode == XZXAudioProcessor::Mute || mode == XZXAudioProcessor::TapeStop;
        float eyeR = bodyW * 0.052f;
        if (mode == XZXAudioProcessor::OctUp)   eyeR *= 1.35f;
        if (mode == XZXAudioProcessor::OctDown) eyeR *= 0.75f;
        float look = 0.0f;
        if (mode == XZXAudioProcessor::Reverse) look = -eyeR * 0.7f;
        if (mode == XZXAudioProcessor::Stutter) look = (rng.nextFloat() - 0.5f) * eyeR;

        g.setColour (xzxcol::ink);
        for (int side = -1; side <= 1; side += 2)
        {
            const float ex = cx + (float) side * eyeDX;
            if (mode == XZXAudioProcessor::Crush && ! napping)
            {
                juce::Path xe;
                xe.startNewSubPath (ex - eyeR, eyeY - eyeR); xe.lineTo (ex + eyeR, eyeY + eyeR);
                xe.startNewSubPath (ex + eyeR, eyeY - eyeR); xe.lineTo (ex - eyeR, eyeY + eyeR);
                g.strokePath (xe, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else if (shut)
            {
                juce::Path lid;
                lid.startNewSubPath (ex - eyeR * 1.1f, eyeY);
                lid.quadraticTo (ex, eyeY + eyeR * 1.1f, ex + eyeR * 1.1f, eyeY);
                g.strokePath (lid, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
            {
                g.fillEllipse (ex - eyeR + look, eyeY - eyeR, eyeR * 2.0f, eyeR * 2.0f);
                g.setColour (juce::Colours::white);
                g.fillEllipse (ex - eyeR * 0.15f + look, eyeY - eyeR * 0.65f, eyeR * 0.6f, eyeR * 0.6f);
                g.setColour (xzxcol::ink);
            }
        }

        // mouth
        const float mouthY = yc + rTop * 0.55f;
        const float mh = napping ? 2.0f : 3.0f + level * bodyW * 0.20f;
        const float mw = bodyW * 0.07f + level * bodyW * 0.03f;
        g.setColour (xzxcol::ink);
        if (napping)
            g.fillRoundedRectangle (cx - mw, mouthY, mw * 2.0f, 2.5f, 1.2f);
        else
            g.fillEllipse (cx - mw, mouthY - mh * 0.5f, mw * 2.0f, mh);

        g.restoreState();

        // zzz when napping
        if (napping)
        {
            g.setColour (accent);
            g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
            const float zy = std::sin (t * 2.0f) * 4.0f;
            g.drawText ("z", (int) (cx + bodyW * 0.45f), (int) (yTop + zy), 20, 20, juce::Justification::centred);
            g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
            g.drawText ("z", (int) (cx + bodyW * 0.62f), (int) (yTop - 18 + zy), 20, 20, juce::Justification::centred);
        }

        // caption
        static const char* captions[] = { "just vibing", "stuttering!", "backwards!", "slowing down...",
                                          "up up up!", "down down", "shhh...", "crunchy!" };
        g.setColour (xzxcol::soft);
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText (napping ? "napping... zzz" : captions[juce::jlimit (0, 7, mode)],
                    getLocalBounds().removeFromBottom (56).removeFromTop (22), juce::Justification::centred);
    }

private:
    XZXAudioProcessor& proc;
    const juce::Colour& accent;
    juce::Random rng;
    float level = 0.f, squash = 0.f, vel = 0.f, t = 0.f, jitter = 0.f;
    int mode = 0, blink = 0, blinkTimer = 120, glitchSeed = 0;
    bool napping = false;
};

//==============================================================================
class XZXEditor : public juce::AudioProcessorEditor
{
public:
    explicit XZXEditor (XZXAudioProcessor&);
    ~XZXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setMood (int index);
    void refreshPresetList (int selectId = -1);
    void stepPreset (int direction);
    void promptSavePreset();

    XZXAudioProcessor& proc;

    XZXLook laf;
    juce::Colour accent { 0xfff08cb4 };

    std::unique_ptr<XZXCreature> creature;
    std::unique_ptr<XZXPatternPanel> pattern;
    std::unique_ptr<XZXShaperPanel> volPanel, filtPanel;
    std::unique_ptr<XZXPanel> tuningPanel, masterPanel;
    XZXMoodDots moodDots;

    juce::TextButton napBtn { "nap" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> napAttach;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "save" }, diceBtn { "dice" };
    juce::Array<juce::File> userFiles;
    juce::Array<int> presetIds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XZXEditor)
};
