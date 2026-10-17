#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Identifier volCurveId  { "volCurve" };
    const juce::Identifier filtCurveId { "filtCurve" };

    const double stepBeatsTable[4]  = { 1.0, 0.5, 0.25, 0.125 };       // 1/4, 1/8, 1/16, 1/32
    const double shapeBeatsTable[6] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0 };
    const int    sliceDivTable[4]   = { 1, 2, 4, 8 };

    inline float curveAt (const std::array<std::atomic<float>, XZXAudioProcessor::curveN>& c, double phase)
    {
        const float x = (float) juce::jlimit (0.0, 1.0, phase) * (float) (XZXAudioProcessor::curveN - 1);
        const int i = juce::jlimit (0, XZXAudioProcessor::curveN - 2, (int) x);
        const float fr = x - (float) i;
        const float a = c[(size_t) i].load(), b = c[(size_t) i + 1].load();
        return a + (b - a) * fr;
    }

    inline double frac (double v) { return v - std::floor (v); }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout XZXAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addF = [&] (const char* id, const char* name, float lo, float hi, float step,
                     float skew, float def, const String& unit, int decimals)
    {
        auto attrs = AudioParameterFloatAttributes().withStringFromValueFunction (
            [unit, decimals] (float v, int)
            {
                return String (v, decimals) + (unit.isEmpty() ? String() : " " + unit);
            });
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                           NormalisableRange<float> (lo, hi, step, skew),
                                                           def, attrs));
    };
    auto addC = [&] (const char* id, const char* name, StringArray choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };

    // 16 pattern steps
    const StringArray modes { "off", "stutter", "reverse", "tape stop", "octave up", "octave down", "mute", "crush" };
    for (int i = 0; i < numSteps; ++i)
    {
        const String id = "step_" + String (i + 1);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, "Step " + String (i + 1), modes, 0));
    }

    // Pattern controls
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "steps", 1 }, "Steps", 2, 16, 16));
    addC ("step_rate", "Rate", { "1/4", "1/8", "1/16", "1/32" }, 2);
    addF ("chance", "Chance", 0.f, 1.f, 0.001f, 1.f, 1.f, "", 2);
    addF ("fade_ms", "Smooth", 0.5f, 30.f, 0.f, 0.5f, 3.f, "ms", 1);

    // Glitch tuning
    addC ("stutter_div", "Stutter Size", { "1/1", "1/2", "1/4", "1/8" }, 1);
    addF ("tape_curve", "Tape", 0.4f, 4.f, 0.01f, 1.f, 1.5f, "", 2);
    addF ("crush_bits", "Crush Bits", 2.f, 12.f, 0.1f, 1.f, 5.f, "bit", 1);
    addF ("crush_rate", "Crush Rate", 1.f, 32.f, 1.f, 0.6f, 6.f, "x", 0);

    // Volume shaper
    addF ("vol_depth", "Vol Depth", 0.f, 1.f, 0.001f, 1.f, 0.f, "", 2);
    addC ("vol_len", "Vol Length", { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars" }, 4);

    // Filter shaper
    addC ("filt_type", "Filter", { "off", "low-pass", "high-pass", "band-pass" }, 0);
    addF ("filt_cut", "Cutoff", 40.f, 16000.f, 0.f, 0.3f, 900.f, "Hz", 0);
    addF ("filt_res", "Reso", 0.5f, 10.f, 0.01f, 0.5f, 1.5f, "", 2);
    addF ("filt_range", "Range", 0.f, 5.f, 0.01f, 1.f, 2.5f, "oct", 1);
    addC ("filt_len", "Filt Length", { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars" }, 4);

    // Master
    addF ("mix", "Mix", 0.f, 1.f, 0.001f, 1.f, 1.f, "", 2);
    addF ("out_gain", "Out", -24.f, 12.f, 0.01f, 1.f, 0.f, "dB", 1);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "nap", 1 }, "Nap", false));

    return layout;
}

//==============================================================================
XZXAudioProcessor::XZXAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XZX", createLayout())
{
    auto g = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    for (int i = 0; i < numSteps; ++i) p.step[i] = g ("step_" + juce::String (i + 1));
    p.stepRate = g ("step_rate"); p.steps = g ("steps"); p.chance = g ("chance"); p.fadeMs = g ("fade_ms");
    p.stutterDiv = g ("stutter_div"); p.tapeCurve = g ("tape_curve");
    p.crushBits = g ("crush_bits"); p.crushRate = g ("crush_rate");
    p.volDepth = g ("vol_depth"); p.volLen = g ("vol_len");
    p.fType = g ("filt_type"); p.cut = g ("filt_cut"); p.res = g ("filt_res");
    p.range = g ("filt_range"); p.filtLen = g ("filt_len");
    p.mix = g ("mix"); p.outGain = g ("out_gain"); p.nap = g ("nap");

    for (auto& v : volCurve)  v.store (1.0f);
    for (auto& v : filtCurve) v.store (0.5f);

    apvts.state.addListener (this);
}

XZXAudioProcessor::~XZXAudioProcessor()
{
    apvts.state.removeListener (this);
}

bool XZXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void XZXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    int size = 1;
    while ((double) size < sampleRate * 8.0) size <<= 1;
    ring.setSize (2, size);
    ring.clear();
    ringMask = size - 1;
    writePos = 0;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                  (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };
    filter.prepare (spec);
    filter.reset();

    mixSm.reset (sampleRate, 0.02);  mixSm.setCurrentAndTargetValue (p.mix->load());
    outSm.reset (sampleRate, 0.02);  outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p.outGain->load()));

    lastAbsStep = std::numeric_limits<long long>::min();
    stepMode = 0; stepN = 0;
    crushCnt = 0;
    crushHold[0] = crushHold[1] = 0.f;
    loadCurves();
}

float XZXAudioProcessor::readRing (int ch, double pos) const
{
    const double fl = std::floor (pos);
    const int i0 = (int) fl;
    const float fr = (float) (pos - fl);
    const float a = ring.getSample (ch, i0 & ringMask);
    const float b = ring.getSample (ch, (i0 + 1) & ringMask);
    return a + (b - a) * fr;
}

void XZXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    if (numCh == 0 || n == 0)
        return;

    // ---- transport ----
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { ppq = *q; havePpq = true; }
        }
    }
    bpm = juce::jlimit (20.0, 400.0, bpm);
    const double bps = bpm / (60.0 * sr);              // beats per sample
    if (! (playing && havePpq)) ppq = freePpq;         // free-run so it still works when stopped
    freePpq = ppq + bps * (double) n;

    // ---- nap = bypass ----
    visNap.store (p.nap->load() > 0.5f);
    if (p.nap->load() > 0.5f)
    {
        float pk = 0.f;
        for (int ch = 0; ch < numCh; ++ch) pk = juce::jmax (pk, buffer.getMagnitude (ch, 0, n));
        visLevel.store (pk);
        visMode.store (0);
        lastAbsStep = std::numeric_limits<long long>::min();
        stepMode = 0;
        return;
    }

    // ---- parameters ----
    const double stepLenBeats = stepBeatsTable[juce::jlimit (0, 3, (int) p.stepRate->load())];
    const int    patLen       = juce::jlimit (2, numSteps, (int) p.steps->load());
    const float  chance       = p.chance->load();
    const double fadeSamples  = juce::jmax (8.0, (double) p.fadeMs->load() * 0.001 * sr);
    const int    sliceDiv     = sliceDivTable[juce::jlimit (0, 3, (int) p.stutterDiv->load())];
    const float  tapeCurve    = p.tapeCurve->load();
    const float  bits         = p.crushBits->load();
    const int    crushRate    = juce::jmax (1, (int) p.crushRate->load());
    const float  crushQ       = std::pow (2.0f, bits) * 0.5f;

    const float  volDepth     = p.volDepth->load();
    const double volLen       = shapeBeatsTable[juce::jlimit (0, 5, (int) p.volLen->load())];
    const int    fType        = (int) p.fType->load();
    const float  baseCut      = p.cut->load();
    const float  rangeOct     = p.range->load();
    const double filtLen      = shapeBeatsTable[juce::jlimit (0, 5, (int) p.filtLen->load())];
    const float  maxCut       = (float) (sr * 0.45);

    filter.setType (fType == 2 ? juce::dsp::StateVariableTPTFilterType::highpass
                  : fType == 3 ? juce::dsp::StateVariableTPTFilterType::bandpass
                               : juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setResonance (p.res->load());

    mixSm.setTargetValue (p.mix->load());
    outSm.setTargetValue (juce::Decibels::decibelsToGain (p.outGain->load()));

    float peak = 0.f;
    float lastVolPhase = 0.f, lastFiltPhase = 0.f;

    for (int s = 0; s < n; ++s)
    {
        const double ppqNow = ppq + bps * (double) s;
        const double stepF  = ppqNow / stepLenBeats;
        const long long absStep = (long long) std::floor (stepF);

        // ---------- new step? ----------
        if (absStep != lastAbsStep)
        {
            lastAbsStep = absStep;
            const int idx = (int) (((absStep % patLen) + patLen) % patLen);
            currentStepIdx = idx;

            const int m = (int) p.step[idx]->load();
            const bool fire = m > 0 && (chance >= 0.999f || rng.nextFloat() < chance);
            stepMode = fire ? m : 0;
            stepN = 0;
            stepSamples = juce::jmax (64.0, stepLenBeats * 60.0 / bpm * sr);
            loopStart = writePos;
            readPos = (double) loopStart;
            if (stepMode == OctUp) readPos = (double) loopStart - stepSamples;
            crushCnt = 0;
        }

        // ---------- write input to ring ----------
        float in[2] = { 0.f, 0.f };
        for (int ch = 0; ch < numCh; ++ch)
        {
            in[ch] = buffer.getSample (ch, s);
            ring.setSample (ch, writePos, in[ch]);
        }
        if (numCh == 1) in[1] = in[0];

        // crossfade envelope for this step (0 at the step edges, 1 in the middle)
        const float f = (float) juce::jlimit (0.0, 1.0,
                            juce::jmin ((double) stepN, stepSamples - (double) stepN) / fadeSamples);

        float wet[2] = { in[0], in[1] };

        switch (stepMode)
        {
            case Stutter:
            {
                const int L = juce::jmax (32, (int) (stepSamples / (double) sliceDiv));
                const int pos = stepN % L;
                const int idx = loopStart - L + pos;
                const float loopFade = juce::jmax (1.0f, juce::jmin ((float) fadeSamples, (float) L * 0.2f));
                const float lf = juce::jmin (1.0f, (float) juce::jmin (pos, L - 1 - pos) / loopFade);
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float g = ring.getSample (ch, idx & ringMask) * lf;
                    wet[ch] = in[ch] * (1.0f - f) + g * f;
                }
                break;
            }
            case Reverse:
            {
                const int idx = loopStart - 1 - stepN;
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float g = ring.getSample (ch, idx & ringMask);
                    wet[ch] = in[ch] * (1.0f - f) + g * f;
                }
                break;
            }
            case TapeStop:
            {
                const double ph = juce::jlimit (0.0, 1.0, (double) stepN / stepSamples);
                const float speed = juce::jmax (0.0f, 1.0f - std::pow ((float) ph, tapeCurve));
                const float gain = std::sqrt (speed);
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float g = readRing (ch, readPos) * gain;
                    wet[ch] = in[ch] * (1.0f - f) + g * f;
                }
                readPos += (double) speed;
                break;
            }
            case OctUp:
            case OctDown:
            {
                for (int ch = 0; ch < numCh; ++ch)
                {
                    const float g = readRing (ch, readPos);
                    wet[ch] = in[ch] * (1.0f - f) + g * f;
                }
                readPos += stepMode == OctUp ? 2.0 : 0.5;
                break;
            }
            case Mute:
            {
                for (int ch = 0; ch < numCh; ++ch)
                    wet[ch] = in[ch] * (1.0f - f);
                break;
            }
            case Crush:
            {
                if (crushCnt == 0)
                    for (int ch = 0; ch < numCh; ++ch)
                        crushHold[ch] = std::round (in[ch] * crushQ) / crushQ;
                crushCnt = (crushCnt + 1) % crushRate;
                for (int ch = 0; ch < numCh; ++ch)
                    wet[ch] = in[ch] * (1.0f - f) + crushHold[ch] * f;
                break;
            }
            default: break;
        }

        // ---------- volume shaper ----------
        const double volPhase = frac (ppqNow / volLen);
        lastVolPhase = (float) volPhase;
        if (volDepth > 0.001f)
        {
            const float c = curveAt (volCurve, volPhase);
            const float gain = 1.0f - volDepth * (1.0f - c);
            wet[0] *= gain;
            wet[1] *= gain;
        }

        // ---------- filter shaper ----------
        const double filtPhase = frac (ppqNow / filtLen);
        lastFiltPhase = (float) filtPhase;
        if (fType > 0)
        {
            if ((s & 7) == 0)
            {
                const float c = curveAt (filtCurve, filtPhase);
                const float oct = (c - 0.5f) * 2.0f * rangeOct;
                filter.setCutoffFrequency (juce::jlimit (30.0f, maxCut, baseCut * std::pow (2.0f, oct)));
            }
            for (int ch = 0; ch < numCh; ++ch)
                wet[ch] = filter.processSample (ch, wet[ch]);
        }

        // ---------- dry/wet + output ----------
        const float m  = mixSm.getNextValue();
        const float og = outSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float out = (in[ch] * (1.0f - m) + wet[ch] * m) * og;
            buffer.setSample (ch, s, out);
            peak = juce::jmax (peak, std::abs (out));
        }

        ++stepN;
        writePos = (writePos + 1) & ringMask;
    }

    visLevel.store (peak);
    visStep.store (currentStepIdx);
    visMode.store (stepMode);
    visVolPhase.store (lastVolPhase);
    visFiltPhase.store (lastFiltPhase);
}

//==============================================================================
// Curves
juce::String XZXAudioProcessor::curveToString (const std::array<float, curveN>& c)
{
    juce::StringArray parts;
    for (float v : c) parts.add (juce::String (juce::jlimit (0.0f, 1.0f, v), 3));
    return parts.joinIntoString (",");
}

std::array<float, XZXAudioProcessor::curveN> XZXAudioProcessor::makeCurveShape (int kind, float flatValue)
{
    std::array<float, curveN> c {};
    juce::Random r;
    float held = 0.5f;
    for (int i = 0; i < curveN; ++i)
    {
        const float x = (float) i / (float) (curveN - 1);
        switch (kind)
        {
            case 1:  c[(size_t) i] = 1.0f - x; break;                                        // saw down
            case 2:  c[(size_t) i] = x; break;                                               // ramp up
            case 3:  c[(size_t) i] = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * x); break;
            case 4:  c[(size_t) i] = x < 0.5f ? 1.0f : 0.0f; break;                          // square
            case 5:  if (i % 4 == 0) held = r.nextFloat(); c[(size_t) i] = held; break;     // random steps
            default: c[(size_t) i] = flatValue; break;
        }
    }
    return c;
}

std::array<float, XZXAudioProcessor::curveN> XZXAudioProcessor::getCurveCopy (int which) const
{
    std::array<float, curveN> a {};
    const auto& src = which == 0 ? volCurve : filtCurve;
    for (int i = 0; i < curveN; ++i) a[(size_t) i] = src[(size_t) i].load();
    return a;
}

void XZXAudioProcessor::setCurve (int which, const std::array<float, curveN>& c)
{
    apvts.state.setProperty (which == 0 ? volCurveId : filtCurveId, curveToString (c), nullptr);
    loadCurves();
}

void XZXAudioProcessor::loadCurves()
{
    auto load = [this] (const juce::Identifier& id, std::array<std::atomic<float>, curveN>& dst, float flat)
    {
        juce::StringArray tokens;
        tokens.addTokens (apvts.state.getProperty (id).toString(), ",", "");
        tokens.removeEmptyStrings();
        const bool ok = tokens.size() == curveN;
        for (int i = 0; i < curveN; ++i)
            dst[(size_t) i].store (ok ? juce::jlimit (0.0f, 1.0f, tokens[i].getFloatValue()) : flat);
    };
    load (volCurveId, volCurve, 1.0f);
    load (filtCurveId, filtCurve, 0.5f);
}

//==============================================================================
// Presets
const std::vector<XZXAudioProcessor::Preset>& XZXAudioProcessor::factoryPresets()
{
    static const std::vector<Preset> presets = []
    {
        auto curve = [] (std::function<float (float)> fn)
        {
            std::array<float, curveN> c {};
            for (int i = 0; i < curveN; ++i) c[(size_t) i] = fn ((float) i / (float) (curveN - 1));
            return curveToString (c);
        };
        const float tp = juce::MathConstants<float>::twoPi;

        std::vector<Preset> v;
        v.push_back ({ "init", {}, "0000000000000000", "", "" });

        v.push_back ({ "stutter fills", { { "stutter_div", 2.f }, { "step_rate", 2.f } },
                       "0001000100010111", "", "" });

        v.push_back ({ "robot stutter", { { "stutter_div", 3.f }, { "chance", 0.8f } },
                       "1010101010101010", "", "" });

        v.push_back ({ "tape stop outro", { { "step_rate", 1.f }, { "tape_curve", 1.2f } },
                       "0000000000000003", "", "" });

        v.push_back ({ "reverse riser", { { "step_rate", 1.f } },
                       "0000202000002222", "", "" });

        v.push_back ({ "pitch hops", { { "step_rate", 1.f } },
                       "0400050004000500", "", "" });

        v.push_back ({ "crunch glitch", { { "crush_bits", 4.f }, { "crush_rate", 8.f } },
                       "0700070007000777", "", "" });

        v.push_back ({ "trance gate", { { "vol_depth", 1.f }, { "vol_len", 4.f } },
                       "0000000000000000",
                       curve ([] (float x) { return std::fmod (x * 8.0f, 1.0f) < 0.55f ? 1.0f : 0.0f; }), "" });

        v.push_back ({ "sidechain pump", { { "vol_depth", 0.9f }, { "vol_len", 2.f } },
                       "0000000000000000",
                       curve ([] (float x) { return std::pow (x, 0.45f); }), "" });

        v.push_back ({ "wobble lp", { { "filt_type", 1.f }, { "filt_cut", 700.f }, { "filt_res", 4.f },
                                      { "filt_range", 3.f }, { "filt_len", 3.f } },
                       "0000000000000000", "",
                       curve ([tp] (float x) { return 0.5f + 0.5f * std::sin (tp * x); }) });

        v.push_back ({ "riser sweep", { { "filt_type", 2.f }, { "filt_cut", 400.f }, { "filt_res", 3.f },
                                        { "filt_range", 4.f }, { "filt_len", 5.f } },
                       "0000000000000000", "",
                       curve ([] (float x) { return x; }) });

        v.push_back ({ "half-time mute", { { "step_rate", 1.f } },
                       "0606060606060606", "", "" });

        v.push_back ({ "gremlin mix", { { "chance", 0.85f }, { "crush_bits", 6.f }, { "stutter_div", 2.f },
                                        { "vol_depth", 0.5f }, { "vol_len", 3.f } },
                       "1230475612034756",
                       curve ([] (float x) { return x < 0.5f ? 1.0f : 0.4f; }), "" });

        v.push_back ({ "sleepy tape", { { "step_rate", 0 }, { "tape_curve", 0.8f }, { "filt_type", 1.f },
                                        { "filt_cut", 3000.f }, { "filt_range", 1.f } },
                       "3000300030003000", "", "" });
        return v;
    }();
    return presets;
}

void XZXAudioProcessor::resetToDefaults()
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    setCurve (0, makeCurveShape (0, 1.0f));
    setCurve (1, makeCurveShape (0, 0.5f));
}

void XZXAudioProcessor::applyPreset (const Preset& preset)
{
    resetToDefaults();

    for (auto& [id, value] : preset.values)
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));

    for (int i = 0; i < numSteps && i < preset.pattern.length(); ++i)
    {
        const int mode = juce::jlimit (0, numModes - 1, preset.pattern.substring (i, i + 1).getIntValue());
        if (auto* prm = apvts.getParameter ("step_" + juce::String (i + 1)))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) mode));
    }

    if (preset.vol.isNotEmpty())  apvts.state.setProperty (volCurveId, preset.vol, nullptr);
    if (preset.filt.isNotEmpty()) apvts.state.setProperty (filtCurveId, preset.filt, nullptr);
    loadCurves();
}

void XZXAudioProcessor::randomize()
{
    juce::Random r;
    auto setP = [this] (const juce::String& id, float realValue)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (realValue));
    };
    auto setNorm = [this] (const juce::String& id, float norm)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    };

    for (int i = 0; i < numSteps; ++i)
        setP ("step_" + juce::String (i + 1), r.nextFloat() < 0.55f ? 0.0f : (float) (1 + r.nextInt (numModes - 1)));

    setP ("step_rate", r.nextBool() ? 2.0f : 1.0f);
    setP ("chance", 0.7f + r.nextFloat() * 0.3f);
    setP ("stutter_div", (float) r.nextInt (4));
    setNorm ("tape_curve", r.nextFloat());
    setNorm ("crush_bits", r.nextFloat() * 0.6f);
    setNorm ("crush_rate", r.nextFloat() * 0.5f);

    setP ("vol_depth", r.nextBool() ? 0.0f : 0.5f + r.nextFloat() * 0.5f);
    setP ("vol_len", (float) (2 + r.nextInt (4)));
    setP ("filt_type", (float) r.nextInt (4));
    setNorm ("filt_cut", 0.3f + r.nextFloat() * 0.5f);
    setNorm ("filt_res", r.nextFloat() * 0.5f);
    setNorm ("filt_range", 0.3f + r.nextFloat() * 0.5f);
    setP ("filt_len", (float) (2 + r.nextInt (4)));

    setCurve (0, makeCurveShape (1 + r.nextInt (5), 1.0f));
    setCurve (1, makeCurveShape (1 + r.nextInt (5), 0.5f));
}

int XZXAudioProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

void XZXAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms()) return;
    currentProgram = index;
    applyPreset (factoryPresets()[(size_t) index]);
}

const juce::String XZXAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms()) return {};
    return factoryPresets()[(size_t) index].name;
}

juce::File XZXAudioProcessor::getUserPresetDir() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("XZX").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::Array<juce::File> XZXAudioProcessor::getUserPresetFiles() const
{
    auto files = getUserPresetDir().findChildFiles (juce::File::findFiles, false, "*.xzxpreset");
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });
    return files;
}

void XZXAudioProcessor::saveUserPreset (const juce::String& name)
{
    auto state = apvts.copyState();
    state.removeProperty ("mood", nullptr);   // accent colour isn't part of a sound preset
    if (auto xml = state.createXml())
        xml->writeTo (getUserPresetDir().getChildFile (juce::File::createLegalFileName (name) + ".xzxpreset"));
}

bool XZXAudioProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::parseXML (file))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto mood = apvts.state.getProperty ("mood");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            if (! mood.isVoid())
                apvts.state.setProperty ("mood", mood, nullptr);
            loadCurves();
            return true;
        }
    }
    return false;
}

//==============================================================================
void XZXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void XZXAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadCurves();
        }
}

juce::AudioProcessorEditor* XZXAudioProcessor::createEditor()
{
    return new XZXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XZXAudioProcessor();
}
