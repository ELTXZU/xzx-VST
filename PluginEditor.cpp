#include "PluginEditor.h"

XZXEditor::XZXEditor (XZXAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    // ---------- Row 2: IO, Crush, Drive, EQ ----------
    secIO = makeSection ("Input / Output");
    secIO->addKnob ("in_gain", "Input");
    secIO->addKnob ("out_gain", "Output");
    secIO->addKnob ("mix", "Dry / Wet");
    secIO->addKnob ("width", "Width");

    secCrush = makeSection ("Bitcrusher");
    secCrush->addToggle ("crush_on");
    secCrush->addKnob ("crush_bits", "Bits");
    secCrush->addKnob ("crush_rate", "Downsample");
    secCrush->addKnob ("crush_mix", "Mix");

    secDrive = makeSection ("Drive");
    secDrive->addToggle ("drive_on");
    secDrive->addChoice ("drive_type", "Type");
    secDrive->addKnob ("drive", "Drive");
    secDrive->addKnob ("drive_bias", "Bias");
    secDrive->addKnob ("drive_tone", "Tone");

    secEQ = makeSection ("3-Band EQ");
    secEQ->addToggle ("eq_on");
    secEQ->addKnob ("eq_low", "Low");
    secEQ->addKnob ("eq_mid", "Mid");
    secEQ->addKnob ("eq_mid_freq", "Mid Freq");
    secEQ->addKnob ("eq_high", "High");

    // ---------- Row 3: Filter, Tremolo, Chorus ----------
    secFilter = makeSection ("Filter / LFO / Envelope");
    secFilter->addChoice ("filt_type", "Filter");
    secFilter->addChoice ("lfo_shape", "LFO Shape");
    secFilter->addKnob ("filt_cut", "Cutoff");
    secFilter->addKnob ("filt_res", "Resonance");
    secFilter->addKnob ("lfo_rate", "LFO Rate");
    secFilter->addKnob ("lfo_depth", "LFO Depth");
    secFilter->addKnob ("env_amt", "Env Amt");
    secFilter->addKnob ("env_rel", "Env Rel");

    secTrem = makeSection ("Tremolo / Autopan");
    secTrem->addToggle ("trem_on");
    secTrem->addChoice ("trem_shape", "Shape");
    secTrem->addKnob ("trem_rate", "Rate");
    secTrem->addKnob ("trem_depth", "Depth");
    secTrem->addKnob ("trem_stereo", "Stereo");

    secChorus = makeSection ("Chorus / Flanger");
    secChorus->addToggle ("chorus_on");
    secChorus->addKnob ("chorus_rate", "Rate");
    secChorus->addKnob ("chorus_depth", "Depth");
    secChorus->addKnob ("chorus_delay", "Delay");
    secChorus->addKnob ("chorus_fb", "Feedback");
    secChorus->addKnob ("chorus_mix", "Mix");

    // ---------- Row 4: Delay, Reverb ----------
    secDelay = makeSection ("Delay");
    secDelay->addToggle ("delay_on");
    secDelay->addChoice ("delay_mode", "Mode");
    secDelay->addKnob ("delay_time", "Time");
    secDelay->addKnob ("delay_fb", "Feedback");
    secDelay->addKnob ("delay_damp", "Damping");
    secDelay->addKnob ("delay_spread", "Spread");
    secDelay->addKnob ("delay_mix", "Mix");

    secReverb = makeSection ("Reverb");
    secReverb->addToggle ("rev_on");
    secReverb->addKnob ("rev_size", "Size");
    secReverb->addKnob ("rev_damp", "Damping");
    secReverb->addKnob ("rev_width", "Width");
    secReverb->addKnob ("rev_mix", "Mix");

    spectrum = std::make_unique<XZXSpectrum> (proc, accent);
    addAndMakeVisible (*spectrum);

    // ---------- top bar ----------
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &saveBtn, &randBtn, &themeBox })
        addAndMakeVisible (c);

    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 1 && id < 1000)
            proc.setCurrentProgram (id - 1);
        else if (id >= 1000 && id - 1000 < userFiles.size())
            proc.loadUserPreset (userFiles[id - 1000]);
    };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    saveBtn.onClick = [this] { promptSavePreset(); };
    randBtn.onClick = [this] { proc.randomize(); };

    themeBox.addItemList ({ "Cyan", "Magenta", "Lime", "Orange", "Violet", "Red", "Ice", "Gold", "Color Cycle" }, 1);
    themeBox.onChange = [this]
    {
        const int idx = themeBox.getSelectedId() - 1;
        proc.apvts.state.setProperty ("theme", idx, nullptr);
        applyTheme (idx);
    };

    const int savedTheme = juce::jlimit (0, 8, (int) proc.apvts.state.getProperty ("theme", 0));
    themeBox.setSelectedId (savedTheme + 1, juce::dontSendNotification);
    applyTheme (savedTheme);

    refreshPresetList (proc.getCurrentProgram() + 1);

    setResizable (true, true);
    setResizeLimits (900, 640, 1800, 1100);
    setSize (1180, 800);
}

XZXEditor::~XZXEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

XZXSection* XZXEditor::makeSection (const juce::String& title)
{
    auto* s = sections.add (new XZXSection (title, accent, proc.apvts));
    addAndMakeVisible (s);
    return s;
}

void XZXEditor::setAccent (juce::Colour c)
{
    accent = c;
    laf.accent = c;
}

void XZXEditor::applyTheme (int index)
{
    static const juce::uint32 colours[] = { 0xff00e5ff, 0xffff2bd6, 0xffb6ff00, 0xffff9f1c,
                                            0xff9d7bff, 0xffff3b4e, 0xffa8e6ff, 0xffffd23f };
    cycling = (index >= 8);
    if (cycling)
    {
        setAccent (juce::Colour::fromHSV (cycleHue, 0.7f, 1.0f, 1.0f));
        startTimerHz (20);
    }
    else
    {
        stopTimer();
        setAccent (juce::Colour (colours[juce::jlimit (0, 7, index)]));
    }

    laf.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));
    laf.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    laf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    laf.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    laf.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    laf.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    laf.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141720));
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    laf.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    laf.setColour (juce::PopupMenu::textColourId, juce::Colours::white);

    sendLookAndFeelChange();
    repaint();
}

void XZXEditor::timerCallback()
{
    if (! cycling) return;
    cycleHue += 0.004f;
    if (cycleHue >= 1.0f) cycleHue -= 1.0f;
    setAccent (juce::Colour::fromHSV (cycleHue, 0.7f, 1.0f, 1.0f));
    repaint();
}

void XZXEditor::refreshPresetList (int selectId)
{
    presetBox.clear (juce::dontSendNotification);
    presetIds.clear();

    presetBox.addSectionHeading ("Factory");
    const auto& factory = XZXAudioProcessor::factoryPresets();
    for (size_t i = 0; i < factory.size(); ++i)
    {
        presetBox.addItem (factory[i].name, (int) i + 1);
        presetIds.add ((int) i + 1);
    }

    userFiles = proc.getUserPresetFiles();
    if (! userFiles.isEmpty())
    {
        presetBox.addSeparator();
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userFiles.size(); ++i)
        {
            presetBox.addItem (userFiles[i].getFileNameWithoutExtension(), 1000 + i);
            presetIds.add (1000 + i);
        }
    }

    if (selectId > 0)
        presetBox.setSelectedId (selectId, juce::dontSendNotification);
}

void XZXEditor::stepPreset (int dir)
{
    if (presetIds.isEmpty()) return;
    int idx = presetIds.indexOf (presetBox.getSelectedId());
    idx = idx < 0 ? 0 : (idx + dir + presetIds.size()) % presetIds.size();
    presetBox.setSelectedId (presetIds[idx], juce::sendNotificationSync);
}

void XZXEditor::promptSavePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My Preset");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<XZXEditor> safe (this);
    w->enterModalState (true,
        juce::ModalCallbackFunction::create ([safe, w] (int result)
        {
            if (result != 1 || safe == nullptr) return;
            auto name = w->getTextEditorContents ("name").trim();
            if (name.isEmpty()) return;

            safe->proc.saveUserPreset (name);
            safe->refreshPresetList();
            for (int i = 0; i < safe->userFiles.size(); ++i)
                if (safe->userFiles[i].getFileNameWithoutExtension() == juce::File::createLegalFileName (name))
                    safe->presetBox.setSelectedId (1000 + i, juce::dontSendNotification);
        }),
        true);
}

void XZXEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.fillAll (juce::Colour (0xff0a0c12));

    // ambient colour glows
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.16f), 0.0f, 0.0f,
                                             accent.withAlpha (0.0f), b.getWidth() * 0.55f, b.getHeight() * 0.5f, true));
    g.fillRect (b);
    g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.10f), b.getRight(), b.getBottom(),
                                             accent.withAlpha (0.0f), b.getWidth() * 0.4f, b.getHeight() * 0.5f, true));
    g.fillRect (b);

    // glowing logo
    const juce::Rectangle<int> logo (20, 8, 120, 44);
    g.setFont (juce::Font (juce::FontOptions (36.0f, juce::Font::bold)));
    for (int i = 6; i >= 1; --i)
    {
        g.setColour (accent.withAlpha (0.035f * (float) (7 - i)));
        for (int dx = -i; dx <= i; dx += i)
            for (int dy = -i; dy <= i; dy += i)
                g.drawText ("XZX", logo.translated (dx, dy), juce::Justification::centredLeft);
    }
    g.setColour (juce::Colours::white);
    g.drawText ("XZX", logo, juce::Justification::centredLeft);

    g.setColour (accent);
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("VST", 112, 22, 50, 20, juce::Justification::centredLeft);
}

void XZXEditor::resized()
{
    auto r = getLocalBounds().reduced (12);

    auto top = r.removeFromTop (44);
    top.removeFromLeft (170);                         // logo
    themeBox.setBounds (top.removeFromRight (110).reduced (0, 6));
    top.removeFromRight (8);
    randBtn.setBounds (top.removeFromRight (100).reduced (0, 6));
    top.removeFromRight (8);
    saveBtn.setBounds (top.removeFromRight (60).reduced (0, 6));
    top.removeFromRight (8);
    nextBtn.setBounds (top.removeFromRight (34).reduced (0, 6));
    top.removeFromRight (4);
    presetBox.setBounds (top.removeFromRight (juce::jmin (top.getWidth() - 40, 320)).reduced (0, 6));
    top.removeFromRight (4);
    prevBtn.setBounds (top.removeFromRight (34).reduced (0, 6));

    r.removeFromTop (6);

    const int specH = juce::roundToInt ((float) r.getHeight() * 0.20f);
    spectrum->setBounds (r.removeFromTop (specH).reduced (4));

    const int rowH = r.getHeight() / 3;
    auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<XZXSection*> secs)
    {
        float total = 0.0f;
        for (auto* s : secs) total += s->weight();
        for (auto* s : secs)
        {
            const int w = juce::roundToInt ((float) row.getWidth() * s->weight() / total);
            s->setBounds (row.removeFromLeft (w).reduced (4));
        }
    };

    layoutRow (r.removeFromTop (rowH), { secIO, secCrush, secDrive, secEQ });
    layoutRow (r.removeFromTop (rowH), { secFilter, secTrem, secChorus });
    layoutRow (r,                      { secDelay, secReverb });
}
