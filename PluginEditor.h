#pragma once
#include "PluginProcessor.h"

//==============================================================================
// Neon look & feel
class XZXLook : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { 0xff00e5ff };

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float lw = juce::jmax (3.0f, radius * 0.13f);
        const float arcR = radius - lw;
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff232837));
        g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (pos > 0.002f)
        {
            juce::Path val;
            val.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
            g.setColour (accent.withAlpha (0.10f));
            g.strokePath (val, juce::PathStrokeType (lw * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent.withAlpha (0.25f));
            g.strokePath (val, juce::PathStrokeType (lw * 1.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accent);
            g.strokePath (val, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float br = arcR - lw * 1.3f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b4259), centre.x, centre.y - br,
                                                 juce::Colour (0xff12151f), centre.x, centre.y + br, false));
        g.fillEllipse (centre.x - br, centre.y - br, br * 2.0f, br * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (centre.x - br, centre.y - br, br * 2.0f, br * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.6f, -br + 2.0f, 3.2f, br * 0.5f, 1.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.setColour (juce::Colours::white);
        g.fillPath (pointer);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        const float rad = r.getHeight() * 0.5f;

        g.setColour (on ? accent.withAlpha (0.22f) : juce::Colour (0xff232837));
        g.fillRoundedRectangle (r, rad);
        g.setColour (on ? accent : juce::Colour (0xff4b5168));
        g.drawRoundedRectangle (r, rad, 1.2f);

        const float d = r.getHeight() - 6.0f;
        const float knobX = on ? r.getRight() - d - 3.0f : r.getX() + 3.0f;
        if (on)
        {
            g.setColour (accent.withAlpha (0.3f));
            g.fillEllipse (knobX - 2.0f, r.getY() + 1.0f, d + 4.0f, d + 4.0f);
        }
        g.setColour (on ? accent : juce::Colour (0xff6b7390));
        g.fillEllipse (knobX, r.getY() + 3.0f, d, d);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? accent.withAlpha (0.35f) : hover ? juce::Colour (0xff262b3d) : juce::Colour (0xff1a1e2b));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (accent.withAlpha (hover ? 0.9f : 0.45f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
        g.setColour (juce::Colour (0xff1a1e2b));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (accent.withAlpha (box.isPopupActive() ? 1.0f : 0.45f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        juce::Path arrow;
        const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
        arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour (accent);
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
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (15));
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

        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (box);
        addAndMakeVisible (label);
        attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, box);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (15));
        box.setBounds (r.removeFromTop (24));
    }

private:
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
};

//==============================================================================
// Panel with title, optional on/off switch, optional drop-downs, and knobs
class XZXSection : public juce::Component
{
public:
    XZXSection (const juce::String& t, const juce::Colour& accentRef,
                juce::AudioProcessorValueTreeState& state)
        : title (t), accent (accentRef), apvts (state) {}

    void addToggle (const juce::String& id)
    {
        toggle = std::make_unique<juce::ToggleButton>();
        toggle->setClickingTogglesState (true);
        addAndMakeVisible (*toggle);
        toggleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, id, *toggle);
    }

    void addKnob (const juce::String& id, const juce::String& text)
    {
        addAndMakeVisible (knobs.add (new XZXKnob (apvts, id, text)));
    }

    void addChoice (const juce::String& id, const juce::String& text)
    {
        addAndMakeVisible (choices.add (new XZXChoice (apvts, id, text)));
    }

    float weight() const
    {
        return juce::jmax (2.0f, (float) knobs.size(), 1.5f * (float) choices.size());
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1e2c), 0.0f, 0.0f,
                                                 juce::Colour (0xff11141d), 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 12.0f);

        // accent glow strip along the top
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.20f), b.getX(), b.getY(),
                                                 accent.withAlpha (0.0f), b.getX(), b.getY() + 34.0f, false));
        g.fillRoundedRectangle (b.withHeight (34.0f), 12.0f);

        g.setColour (accent.withAlpha (0.28f));
        g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

        g.setColour (accent);
        g.setFont (13.0f);
        g.drawText (title.toUpperCase(), getLocalBounds().reduced (14, 7).removeFromTop (16),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8);
        auto header = r.removeFromTop (22);
        if (toggle != nullptr)
            toggle->setBounds (header.removeFromRight (34).withSizeKeepingCentre (34, 17));

        r.removeFromTop (2);

        if (choices.size() > 0)
        {
            auto row = r.removeFromTop (42);
            const int w = row.getWidth() / choices.size();
            for (auto* c : choices)
                c->setBounds (row.removeFromLeft (w).reduced (3, 0));
        }

        if (knobs.size() > 0)
        {
            const int w = r.getWidth() / knobs.size();
            for (auto* k : knobs)
                k->setBounds (r.removeFromLeft (w));
        }
    }

private:
    juce::String title;
    const juce::Colour& accent;
    juce::AudioProcessorValueTreeState& apvts;
    juce::OwnedArray<XZXKnob> knobs;
    juce::OwnedArray<XZXChoice> choices;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttach;
};

//==============================================================================
// Live spectrum analyzer of the plugin output
class XZXSpectrum : public juce::Component, private juce::Timer
{
public:
    XZXSpectrum (XZXAudioProcessor& p, const juce::Colour& accentRef)
        : proc (p), accent (accentRef)
    {
        bars.fill (0.0f);
        peaks.fill (0.0f);
        fftData.fill (0.0f);
        startTimerHz (30);
    }

    void timerCallback() override
    {
        constexpr int fftSize = XZXAudioProcessor::fftSize;

        if (proc.analyzerReady.load())
        {
            std::copy (proc.analyzerBlock.begin(), proc.analyzerBlock.end(), fftData.begin());
            proc.analyzerReady.store (false);
            std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

            window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData.data());

            const float fs = (float) juce::jmax (8000.0, proc.getSampleRate());
            for (int i = 0; i < numBars; ++i)
            {
                const float f0 = 20.0f * std::pow (1000.0f, (float) i / (float) numBars);
                const float f1 = 20.0f * std::pow (1000.0f, (float) (i + 1) / (float) numBars);
                const int b0 = juce::jlimit (1, fftSize / 2 - 1, (int) std::floor (f0 * (float) fftSize / fs));
                const int b1 = juce::jlimit (b0, fftSize / 2 - 1, (int) std::ceil (f1 * (float) fftSize / fs));

                float m = 0.0f;
                for (int b = b0; b <= b1; ++b)
                    m = juce::jmax (m, fftData[(size_t) b]);

                const float db = juce::Decibels::gainToDecibels (m / ((float) fftSize * 0.25f), -100.0f);
                const float v = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -78.0f, 0.0f, 0.0f, 1.0f));
                bars[(size_t) i] = v > bars[(size_t) i] ? v : bars[(size_t) i] * 0.86f + v * 0.14f;
            }
        }
        else
        {
            for (auto& b : bars) b *= 0.94f;
        }

        for (size_t i = 0; i < bars.size(); ++i)
            peaks[i] = juce::jmax (bars[i], peaks[i] - 0.006f);

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff10131c), 0.0f, 0.0f,
                                                 juce::Colour (0xff090b11), 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle (b, 12.0f);
        g.setColour (accent.withAlpha (0.28f));
        g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

        auto plot = b.reduced (14.0f, 20.0f);

        // grid
        g.setFont (10.0f);
        for (float db : { -20.0f, -40.0f, -60.0f })
        {
            const float y = plot.getBottom() - juce::jmap (db, -78.0f, 0.0f, 0.0f, 1.0f) * plot.getHeight();
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        }
        const char* labels[] = { "100", "1k", "10k" };
        const float freqs[] = { 100.0f, 1000.0f, 10000.0f };
        for (int i = 0; i < 3; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * std::log (freqs[i] / 20.0f) / std::log (1000.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawText (labels[i], (int) x - 20, (int) plot.getBottom() + 3, 40, 12, juce::Justification::centred);
        }

        g.setColour (accent.withAlpha (0.8f));
        g.setFont (11.0f);
        g.drawText ("SPECTRUM", getLocalBounds().reduced (14, 5).removeFromTop (14), juce::Justification::centredLeft);

        // curve
        juce::Path line, fill;
        for (int i = 0; i < numBars; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * ((float) i + 0.5f) / (float) numBars;
            const float y = plot.getBottom() - bars[(size_t) i] * plot.getHeight();
            if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom()); fill.lineTo (x, y); }
            else        { line.lineTo (x, y); fill.lineTo (x, y); }
        }
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.55f), 0.0f, plot.getY(),
                                                 accent.withAlpha (0.0f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);

        const auto stroke = [] (float w) { return juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };
        g.setColour (accent.withAlpha (0.12f)); g.strokePath (line, stroke (8.0f));
        g.setColour (accent.withAlpha (0.30f)); g.strokePath (line, stroke (4.0f));
        g.setColour (accent.brighter (0.4f));   g.strokePath (line, stroke (1.6f));

        // peak dots
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        for (int i = 0; i < numBars; i += 2)
        {
            const float x = plot.getX() + plot.getWidth() * ((float) i + 0.5f) / (float) numBars;
            const float y = plot.getBottom() - peaks[(size_t) i] * plot.getHeight();
            g.fillRect (x - 1.0f, y - 1.0f, 2.0f, 2.0f);
        }
    }

private:
    static constexpr int numBars = 110;

    XZXAudioProcessor& proc;
    const juce::Colour& accent;

    juce::dsp::FFT fft { XZXAudioProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) XZXAudioProcessor::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, XZXAudioProcessor::fftSize * 2> fftData;
    std::array<float, numBars> bars, peaks;
};

//==============================================================================
class XZXEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit XZXEditor (XZXAudioProcessor&);
    ~XZXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyTheme (int index);
    void setAccent (juce::Colour c);
    void refreshPresetList (int selectId = -1);
    void stepPreset (int direction);
    void promptSavePreset();
    XZXSection* makeSection (const juce::String& title);

    XZXAudioProcessor& proc;

    XZXLook laf;
    juce::Colour accent { 0xff00e5ff };
    float cycleHue = 0.5f;
    bool cycling = false;

    juce::OwnedArray<XZXSection> sections;
    XZXSection *secIO {}, *secCrush {}, *secDrive {}, *secEQ {}, *secFilter {}, *secTrem {},
               *secChorus {}, *secDelay {}, *secReverb {};
    std::unique_ptr<XZXSpectrum> spectrum;

    juce::ComboBox presetBox, themeBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, randBtn { "RANDOMIZE" };
    juce::Array<juce::File> userFiles;
    juce::Array<int> presetIds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XZXEditor)
};
