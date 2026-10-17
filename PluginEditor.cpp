#include "PluginEditor.h"

XZXEditor::XZXEditor (XZXAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    // ---- panels ----
    creature = std::make_unique<XZXCreature> (proc, accent);
    addAndMakeVisible (*creature);

    pattern = std::make_unique<XZXPatternPanel> (proc);
    addAndMakeVisible (*pattern);

    volPanel = std::make_unique<XZXShaperPanel> ("volume shaper - draw it", proc, 0, accent);
    volPanel->addChoice ("vol_len", "length");
    volPanel->addKnob ("vol_depth", "depth");
    addAndMakeVisible (*volPanel);

    filtPanel = std::make_unique<XZXShaperPanel> ("filter shaper - draw it", proc, 1, accent);
    filtPanel->addChoice ("filt_type", "filter");
    filtPanel->addChoice ("filt_len", "length");
    filtPanel->addKnob ("filt_cut", "cutoff");
    filtPanel->addKnob ("filt_res", "reso");
    filtPanel->addKnob ("filt_range", "range");
    addAndMakeVisible (*filtPanel);

    tuningPanel = std::make_unique<XZXPanel> ("glitch tuning", proc.apvts);
    tuningPanel->addChoice ("stutter_div", "stutter size");
    tuningPanel->addKnob ("tape_curve", "tape stop");
    tuningPanel->addKnob ("crush_bits", "crush bits");
    tuningPanel->addKnob ("crush_rate", "crush rate");
    addAndMakeVisible (*tuningPanel);

    masterPanel = std::make_unique<XZXPanel> ("master", proc.apvts);
    masterPanel->addKnob ("mix", "dry / wet");
    masterPanel->addKnob ("out_gain", "output");
    addAndMakeVisible (*masterPanel);

    // ---- nap button ----
    napBtn.setClickingTogglesState (true);
    napAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "nap", napBtn);
    addAndMakeVisible (napBtn);

    // ---- header ----
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &saveBtn, &diceBtn, &moodDots })
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
    diceBtn.onClick = [this] { proc.randomize(); };

    moodDots.onPick = [this] (int i)
    {
        proc.apvts.state.setProperty ("mood", i, nullptr);
        setMood (i);
    };

    laf.setColour (juce::Label::textColourId, xzxcol::ink);
    laf.setColour (juce::Slider::textBoxTextColourId, xzxcol::ink);
    laf.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    laf.setColour (juce::ComboBox::textColourId, xzxcol::ink);
    laf.setColour (juce::TextButton::textColourOffId, xzxcol::ink);
    laf.setColour (juce::TextButton::textColourOnId, xzxcol::ink);
    laf.setColour (juce::PopupMenu::backgroundColourId, xzxcol::panel);
    laf.setColour (juce::PopupMenu::textColourId, xzxcol::ink);
    laf.setColour (juce::PopupMenu::highlightedTextColourId, xzxcol::ink);

    setMood ((int) proc.apvts.state.getProperty ("mood", 0));
    refreshPresetList (proc.getCurrentProgram() + 1);

    setResizable (true, true);
    setResizeLimits (880, 600, 1600, 1100);
    setSize (1060, 700);
}

XZXEditor::~XZXEditor()
{
    setLookAndFeel (nullptr);
}

void XZXEditor::setMood (int index)
{
    index = juce::jlimit (0, 4, index);
    accent = xzxcol::mood (index);
    laf.accent = accent;
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.45f));
    moodDots.selected = index;
    sendLookAndFeelChange();
    repaint();
}

void XZXEditor::refreshPresetList (int selectId)
{
    presetBox.clear (juce::dontSendNotification);
    presetIds.clear();

    presetBox.addSectionHeading ("factory");
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
        presetBox.addSectionHeading ("yours");
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
    auto* w = new juce::AlertWindow ("save preset", "name your preset:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "my glitch");
    w->addButton ("save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

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
    g.fillAll (xzxcol::bg);

    g.setColour (xzxcol::ink);
    g.setFont (juce::Font (juce::FontOptions (46.0f, juce::Font::bold)));
    g.drawText ("xzx", 26, 14, 200, 50, juce::Justification::centredLeft);

    g.setColour (xzxcol::soft);
    g.setFont (juce::Font (juce::FontOptions (14.0f)));
    g.drawText ("a tiny glitch gremlin   -   by eltxzu", 30, 62, 400, 18, juce::Justification::centredLeft);

    g.setColour (xzxcol::border);
    g.drawHorizontalLine (92, 22.0f, (float) getWidth() - 22.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (26.0f, 92.0f - 1.0f, 70.0f, 3.0f, 1.5f);
}

void XZXEditor::resized()
{
    auto r = getLocalBounds().reduced (20, 14);

    // ---- header ----
    auto header = r.removeFromTop (70);
    {
        auto h = header.withTrimmedTop (14).withHeight (30);
        diceBtn.setBounds (h.removeFromRight (62));
        h.removeFromRight (8);
        saveBtn.setBounds (h.removeFromRight (62));
        h.removeFromRight (8);
        nextBtn.setBounds (h.removeFromRight (32));
        h.removeFromRight (4);
        presetBox.setBounds (h.removeFromRight (210));
        h.removeFromRight (4);
        prevBtn.setBounds (h.removeFromRight (32));
        h.removeFromRight (16);
        moodDots.setBounds (h.removeFromRight (122));
    }
    r.removeFromTop (20);   // room for divider

    // ---- body ----
    const int leftW = juce::jlimit (210, 300, (int) ((float) r.getWidth() * 0.22f));
    auto left = r.removeFromLeft (leftW);
    r.removeFromLeft (12);

    creature->setBounds (left.withTrimmedBottom (44));
    napBtn.setBounds (left.removeFromBottom (32).withSizeKeepingCentre (96, 30));

    const int gap = 12;
    const int patH = (int) ((float) r.getHeight() * 0.34f);
    const int botH = (int) ((float) r.getHeight() * 0.22f);

    pattern->setBounds (r.removeFromTop (patH));
    r.removeFromTop (gap);

    auto bottom = r.removeFromBottom (botH);
    r.removeFromBottom (gap);

    auto shapers = r;
    const int sw = (shapers.getWidth() - gap) / 2;
    volPanel->setBounds (shapers.removeFromLeft (sw));
    shapers.removeFromLeft (gap);
    filtPanel->setBounds (shapers);

    const int tw = (int) ((float) (bottom.getWidth() - gap) * 0.68f);
    tuningPanel->setBounds (bottom.removeFromLeft (tw));
    bottom.removeFromLeft (gap);
    masterPanel->setBounds (bottom);

    tuningPanel->layoutControls (tuningPanel->getBounds().withZeroOrigin().reduced (14, 0)
                                     .withTrimmedTop (22).withTrimmedBottom (6), 0, true);
    masterPanel->layoutControls (masterPanel->getBounds().withZeroOrigin().reduced (14, 0)
                                     .withTrimmedTop (22).withTrimmedBottom (6), 0, true);
}
