#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    inline float shapeSample (float x, int type)
    {
        switch (type)
        {
            case 1:  return juce::jlimit (-1.0f, 1.0f, x);                          // Hard clip
            case 2:  return std::sin (x * juce::MathConstants<float>::halfPi);      // Wavefold
            case 3:  return std::tanh (x + 0.25f) - std::tanh (0.25f);              // Asymmetric tube
            case 4:  return x >= 0.0f ? 1.0f - std::exp (-x)                        // Diode
                                      : -0.6f * (1.0f - std::exp (x * 1.5f));
            default: return std::tanh (x);                                           // Soft clip
        }
    }

    inline float lfoValue (float ph, int shape)
    {
        switch (shape)
        {
            case 1:  return 4.0f * std::abs (ph - 0.5f) - 1.0f;   // Triangle
            case 2:  return ph < 0.5f ? 1.0f : -1.0f;             // Square
            case 3:  return 2.0f * ph - 1.0f;                     // Saw
            default: return std::sin (twoPi * ph);                // Sine
        }
    }

    inline float wrap01 (float v) { return v - std::floor (v); }
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
    auto addB = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // Global
    addF ("in_gain",  "Input",  -24.f, 24.f, 0.01f, 1.f, 0.f, "dB", 1);
    addF ("out_gain", "Output", -24.f, 24.f, 0.01f, 1.f, 0.f, "dB", 1);
    addF ("mix",      "Dry/Wet", 0.f, 1.f, 0.001f, 1.f, 1.f, "", 2);
    addF ("width",    "Width",   0.f, 2.f, 0.001f, 1.f, 1.f, "", 2);

    // Bitcrusher
    addB ("crush_on", "Crush On", true);
    addF ("crush_bits", "Bits", 1.f, 16.f, 0.1f, 1.f, 16.f, "bit", 1);
    addF ("crush_rate", "Downsample", 1.f, 50.f, 1.f, 0.5f, 1.f, "x", 0);
    addF ("crush_mix",  "Crush Mix", 0.f, 1.f, 0.001f, 1.f, 1.f, "", 2);

    // Drive
    addB ("drive_on", "Drive On", true);
    addF ("drive", "Drive", 0.f, 40.f, 0.01f, 1.f, 0.f, "dB", 1);
    addC ("drive_type", "Drive Type", { "Soft", "Hard", "Fold", "Tube", "Diode" }, 0);
    addF ("drive_bias", "Bias", -1.f, 1.f, 0.001f, 1.f, 0.f, "", 2);
    addF ("drive_tone", "Tone", 200.f, 20000.f, 0.f, 0.3f, 20000.f, "Hz", 0);

    // EQ
    addB ("eq_on", "EQ On", true);
    addF ("eq_low",  "Low",  -15.f, 15.f, 0.01f, 1.f, 0.f, "dB", 1);
    addF ("eq_mid",  "Mid",  -15.f, 15.f, 0.01f, 1.f, 0.f, "dB", 1);
    addF ("eq_mid_freq", "Mid Freq", 200.f, 5000.f, 0.f, 0.4f, 1000.f, "Hz", 0);
    addF ("eq_high", "High", -15.f, 15.f, 0.01f, 1.f, 0.f, "dB", 1);

    // Filter + LFO + envelope follower
    addC ("filt_type", "Filter", { "Off", "Low-pass", "High-pass", "Band-pass" }, 0);
    addF ("filt_cut", "Cutoff", 20.f, 20000.f, 0.f, 0.3f, 1000.f, "Hz", 0);
    addF ("filt_res", "Resonance", 0.4f, 12.f, 0.01f, 0.5f, 0.707f, "", 2);
    addF ("lfo_rate", "LFO Rate", 0.02f, 20.f, 0.f, 0.4f, 1.f, "Hz", 2);
    addF ("lfo_depth", "LFO Depth", 0.f, 1.f, 0.001f, 1.f, 0.f, "", 2);
    addC ("lfo_shape", "LFO Shape", { "Sine", "Triangle", "Square", "Saw" }, 0);
    addF ("env_amt", "Env Amount", -1.f, 1.f, 0.001f, 1.f, 0.f, "", 2);
    addF ("env_rel", "Env Release", 10.f, 1000.f, 0.f, 0.5f, 150.f, "ms", 0);

    // Tremolo / autopan
    addB ("trem_on", "Tremolo On", true);
    addF ("trem_rate", "Rate", 0.1f, 20.f, 0.f, 0.5f, 4.f, "Hz", 2);
    addF ("trem_depth", "Depth", 0.f, 1.f, 0.001f, 1.f, 0.f, "", 2);
    addC ("trem_shape", "Shape", { "Sine", "Triangle", "Square" }, 0);
    addF ("trem_stereo", "Stereo", 0.f, 1.f, 0.001f, 1.f, 0.f, "", 2);

    // Delay
    addB ("delay_on", "Delay On", true);
    addC ("delay_mode", "Mode", { "Stereo", "Ping-Pong" }, 0);
    addF ("delay_time", "Time", 1.f, 2000.f, 0.f, 0.4f, 350.f, "ms", 0);
    addF ("delay_fb",   "Feedback", 0.f, 0.95f, 0.001f, 1.f, 0.35f, "", 2);
    addF ("delay_mix",  "Delay Mix", 0.f, 1.f, 0.001f, 1.f, 0.f, "", 2);
    addF ("delay_spread", "Spread", 0.f, 0.5f, 0.001f, 1.f, 0.f, "", 2);
    addF ("delay_damp", "Damping", 500.f, 20000.f, 0.f, 0.3f, 20000.f, "Hz", 0);

    // Chorus / flanger
    addB ("chorus_on", "Chorus On", true);
    addF ("chorus_rate",  "Rate",  0.05f, 10.f, 0.f, 0.4f, 1.f, "Hz", 2);
    addF ("chorus_depth", "Depth", 0.f, 1.f, 0.001f, 1.f, 0.25f, "", 2);
    addF ("chorus_delay", "Delay", 1.f, 40.f, 0.01f, 0.6f, 7.f, "ms", 1);
    addF ("chorus_fb",    "Feedback", -0.9f, 0.9f, 0.001f, 1.f, 0.f, "", 2);
    addF ("chorus_mix",   "Chorus Mix", 0.f, 1.f, 0.001f, 1.f, 0.f, "", 2);

    // Reverb
    addB ("rev_on", "Reverb On", true);
    addF ("rev_size",  "Size",  0.f, 1.f, 0.001f, 1.f, 0.5f, "", 2);
    addF ("rev_damp",  "Damping", 0.f, 1.f, 0.001f, 1.f, 0.5f, "", 2);
    addF ("rev_width", "Width", 0.f, 1.f, 0.001f, 1.f, 1.f, "", 2);
    addF ("rev_mix",   "Reverb Mix", 0.f, 1.f, 0.001f, 1.f, 0.f, "", 2);

    return layout;
}

//==============================================================================
XZXAudioProcessor::XZXAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XZX", createLayout())
{
    auto g = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    p.inGain = g ("in_gain");  p.outGain = g ("out_gain"); p.mix = g ("mix"); p.width = g ("width");
    p.crushOn = g ("crush_on"); p.bits = g ("crush_bits"); p.rate = g ("crush_rate"); p.crushMix = g ("crush_mix");
    p.driveOn = g ("drive_on"); p.drive = g ("drive"); p.driveType = g ("drive_type");
    p.bias = g ("drive_bias");  p.tone = g ("drive_tone");
    p.eqOn = g ("eq_on"); p.eqLow = g ("eq_low"); p.eqMid = g ("eq_mid");
    p.eqMidFreq = g ("eq_mid_freq"); p.eqHigh = g ("eq_high");
    p.fType = g ("filt_type"); p.cut = g ("filt_cut"); p.res = g ("filt_res");
    p.lfoRate = g ("lfo_rate"); p.lfoDepth = g ("lfo_depth"); p.lfoShape = g ("lfo_shape");
    p.envAmt = g ("env_amt"); p.envRel = g ("env_rel");
    p.tremOn = g ("trem_on"); p.tremRate = g ("trem_rate"); p.tremDepth = g ("trem_depth");
    p.tremShape = g ("trem_shape"); p.tremStereo = g ("trem_stereo");
    p.delayOn = g ("delay_on"); p.dMode = g ("delay_mode"); p.dTime = g ("delay_time");
    p.dFb = g ("delay_fb"); p.dMix = g ("delay_mix"); p.dSpread = g ("delay_spread"); p.dDamp = g ("delay_damp");
    p.chOn = g ("chorus_on"); p.chRate = g ("chorus_rate"); p.chDepth = g ("chorus_depth");
    p.chDelay = g ("chorus_delay"); p.chFb = g ("chorus_fb"); p.chMix = g ("chorus_mix");
    p.rvOn = g ("rev_on"); p.rvSize = g ("rev_size"); p.rvDamp = g ("rev_damp");
    p.rvWidth = g ("rev_width"); p.rvMix = g ("rev_mix");
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
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock,
                                  (juce::uint32) juce::jmax (1, getTotalNumOutputChannels()) };

    filter.prepare (spec);  filter.reset();
    chorus.prepare (spec);  chorus.reset();
    reverb.prepare (spec);  reverb.reset();
    delay.setMaximumDelayInSamples ((int) (sampleRate * 3.1) + 4);
    delay.prepare (spec);   delay.reset();

    eqLow.prepare (spec);  eqMid.prepare (spec);  eqHigh.prepare (spec);
    eqLow.reset();  eqMid.reset();  eqHigh.reset();
    for (auto& v : lastEq) v = 1e9f;   // force coefficient refresh

    dry.setSize (2, juce::jmax (1, samplesPerBlock));
    envBuf.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.f);

    auto init = [sampleRate] (juce::SmoothedValue<float>& sm, float v, double seconds)
    {
        sm.reset (sampleRate, seconds);
        sm.setCurrentAndTargetValue (v);
    };
    init (inGainSm,  juce::Decibels::decibelsToGain (p.inGain->load()),  0.02);
    init (outGainSm, juce::Decibels::decibelsToGain (p.outGain->load()), 0.02);
    init (mixSm,     p.mix->load(), 0.02);
    init (driveSm,   juce::Decibels::decibelsToGain (p.drive->load()),   0.02);
    init (delaySm,   (float) (p.dTime->load() * 0.001 * sampleRate),     0.08);

    for (int i = 0; i < 2; ++i)
    {
        crushHeld[i] = 0.f; crushCount[i] = 0; toneState[i] = 0.f;
        dampState[i] = 0.f; dcX[i] = 0.f; dcY[i] = 0.f;
    }
    envState = 0.f;
    lfoPhase = 0.f;
    tremPhase = 0.f;
    fifoIndex = 0;
}

void XZXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    if (numCh == 0 || n == 0)
        return;

    if (dry.getNumSamples() < n)
        dry.setSize (2, n, false, false, true);
    if ((int) envBuf.size() < n)
        envBuf.resize ((size_t) n, 0.f);
    for (int ch = 0; ch < numCh; ++ch)
        dry.copyFrom (ch, 0, buffer, ch, 0, n);

    auto isOn = [] (std::atomic<float>* a) { return a->load() > 0.5f; };
    const float fs = (float) sr;

    // ---- crush ----
    const float bits     = p.bits->load();
    const int   rate     = juce::jmax (1, (int) p.rate->load());
    const float crushMix = p.crushMix->load();
    const bool  crushOn  = isOn (p.crushOn) && (bits < 15.5f || rate > 1) && crushMix > 0.001f;
    const float q        = std::pow (2.0f, bits) * 0.5f;

    // ---- drive ----
    const float driveDb  = p.drive->load();
    const bool  driveOn  = isOn (p.driveOn) && driveDb > 0.05f;
    const int   dType    = (int) p.driveType->load();
    const float bias     = p.bias->load();
    const float biasOff  = shapeSample (bias, dType);
    const float toneHz   = p.tone->load();
    const bool  toneOn   = driveOn && toneHz < 19000.0f;
    const float toneA    = std::exp (-twoPi * toneHz / fs);
    const bool  needDC   = driveOn && (dType >= 3 || std::abs (bias) > 0.001f);

    // ---- filter / modulation ----
    const int   fType   = (int) p.fType->load();
    const float baseCut = p.cut->load();
    const float lfoOct  = p.lfoDepth->load() * 4.0f;
    const int   lfoShp  = (int) p.lfoShape->load();
    const float lfoInc  = p.lfoRate->load() / fs;
    const float maxCut  = fs * 0.45f;
    const float envAmt  = p.envAmt->load();
    const float atkC    = std::exp (-1.0f / (0.001f * fs));
    const float relC    = std::exp (-1.0f / (juce::jmax (1.0f, p.envRel->load()) * 0.001f * fs));

    filter.setType (fType == 2 ? juce::dsp::StateVariableTPTFilterType::highpass
                  : fType == 3 ? juce::dsp::StateVariableTPTFilterType::bandpass
                               : juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.setResonance (p.res->load());

    // ---- tremolo ----
    const bool  tremOn    = isOn (p.tremOn) && p.tremDepth->load() > 0.001f;
    const float tremDepth = p.tremDepth->load();
    const float tremInc   = p.tremRate->load() / fs;
    const int   tremShape = (int) p.tremShape->load();
    const float tremSt    = p.tremStereo->load();

    // ---- delay ----
    const bool  delayOn  = isOn (p.delayOn);
    const bool  pingPong = numCh == 2 && (int) p.dMode->load() == 1;
    const float dFb      = p.dFb->load();
    const float dMix     = p.dMix->load();
    const float dSpread  = p.dSpread->load();
    const float dampA    = p.dDamp->load() >= 19000.0f ? 0.0f : std::exp (-twoPi * p.dDamp->load() / fs);
    const float maxDelaySamples = fs * 3.0f;

    inGainSm.setTargetValue  (juce::Decibels::decibelsToGain (p.inGain->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (p.outGain->load()));
    mixSm.setTargetValue     (p.mix->load());
    driveSm.setTargetValue   (juce::Decibels::decibelsToGain (driveDb));
    delaySm.setTargetValue   (juce::jlimit (1.0f, fs * 2.0f, p.dTime->load() * 0.001f * fs));

    // =========================== PASS 1: crush -> drive ===========================
    for (int s = 0; s < n; ++s)
    {
        const float inG = inGainSm.getNextValue();
        const float drv = driveSm.getNextValue();
        float lvl = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            float x = d[s] * inG;
            lvl = juce::jmax (lvl, std::abs (x));

            if (crushOn)
            {
                const float xin = x;
                if (crushCount[ch] == 0)
                    crushHeld[ch] = bits < 15.5f ? std::round (xin * q) / q : xin;
                crushCount[ch] = (crushCount[ch] + 1) % rate;
                x = xin + (crushHeld[ch] - xin) * crushMix;
            }

            if (driveOn)
            {
                x = shapeSample (x * drv + bias, dType) - biasOff;
                if (toneOn)
                {
                    toneState[ch] = (1.0f - toneA) * x + toneA * toneState[ch];
                    x = toneState[ch];
                }
            }

            d[s] = x;
        }

        envState = lvl > envState ? lvl + atkC * (envState - lvl)
                                  : lvl + relC * (envState - lvl);
        envBuf[(size_t) s] = envState;
    }

    // =========================== EQ (block based) ===========================
    if (isOn (p.eqOn))
    {
        const float lo = p.eqLow->load(), mid = p.eqMid->load(), hi = p.eqHigh->load();
        const float mf = p.eqMidFreq->load();

        if (std::abs (lo) > 0.05f || std::abs (mid) > 0.05f || std::abs (hi) > 0.05f)
        {
            if (! juce::exactlyEqual (lo, lastEq[0]) || ! juce::exactlyEqual (mid, lastEq[1])
                || ! juce::exactlyEqual (mf, lastEq[2]) || ! juce::exactlyEqual (hi, lastEq[3]))
            {
                lastEq[0] = lo; lastEq[1] = mid; lastEq[2] = mf; lastEq[3] = hi;
                *eqLow.state  = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                                    sr, 200.0f, 0.707f, juce::Decibels::decibelsToGain (lo));
                *eqMid.state  = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                                    sr, mf, 0.9f, juce::Decibels::decibelsToGain (mid));
                *eqHigh.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                                    sr, 4500.0f, 0.707f, juce::Decibels::decibelsToGain (hi));
            }

            juce::dsp::AudioBlock<float> blk (buffer);
            juce::dsp::ProcessContextReplacing<float> c (blk);
            eqLow.process (c);
            eqMid.process (c);
            eqHigh.process (c);
        }
    }

    // =========================== PASS 2: filter -> tremolo -> delay ===========================
    for (int s = 0; s < n; ++s)
    {
        const float lfo = lfoValue (lfoPhase, lfoShp);
        lfoPhase = wrap01 (lfoPhase + lfoInc);

        if (fType > 0 && (s & 7) == 0)
        {
            const float oct = lfo * lfoOct + envBuf[(size_t) s] * envAmt * 4.0f;
            filter.setCutoffFrequency (juce::jlimit (20.0f, maxCut, baseCut * std::pow (2.0f, oct)));
        }

        float xs[2] = { 0.f, 0.f };
        for (int ch = 0; ch < numCh; ++ch)
        {
            float x = buffer.getSample (ch, s);

            if (fType > 0)
                x = filter.processSample (ch, x);

            if (tremOn)
            {
                const float ph = wrap01 (tremPhase + (ch == 1 ? tremSt * 0.5f : 0.0f));
                x *= 1.0f - tremDepth * 0.5f * (1.0f - lfoValue (ph, tremShape));
            }
            xs[ch] = x;
        }
        tremPhase = wrap01 (tremPhase + tremInc);

        const float dL = delaySm.getNextValue();
        const float dR = juce::jmin (dL * (1.0f + dSpread), maxDelaySamples);

        if (! delayOn)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                delay.popSample (ch, dL);
                delay.pushSample (ch, 0.0f);
            }
        }
        else if (pingPong)
        {
            const float eL = delay.popSample (0, dL);
            const float eR = delay.popSample (1, dR);
            dampState[0] = (1.0f - dampA) * eL + dampA * dampState[0];
            dampState[1] = (1.0f - dampA) * eR + dampA * dampState[1];
            const float mono = 0.5f * (xs[0] + xs[1]);
            delay.pushSample (0, mono + std::tanh (dampState[1] * dFb));
            delay.pushSample (1, std::tanh (dampState[0] * dFb));
            xs[0] += eL * dMix;
            xs[1] += eR * dMix;
        }
        else
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float e = delay.popSample (ch, ch == 1 ? dR : dL);
                dampState[ch] = (1.0f - dampA) * e + dampA * dampState[ch];
                delay.pushSample (ch, xs[ch] + std::tanh (dampState[ch] * dFb));
                xs[ch] += e * dMix;
            }
        }

        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, s, xs[ch]);
    }

    // =========================== chorus -> reverb (block based) ===========================
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    const float chMix = p.chMix->load();
    if (isOn (p.chOn) && chMix > 0.001f)
    {
        chorus.setRate (p.chRate->load());
        chorus.setDepth (p.chDepth->load());
        chorus.setCentreDelay (p.chDelay->load());
        chorus.setFeedback (p.chFb->load());
        chorus.setMix (chMix);
        chorus.process (ctx);
    }

    const float rvMix = p.rvMix->load();
    if (isOn (p.rvOn) && rvMix > 0.001f)
    {
        juce::dsp::Reverb::Parameters rp;
        rp.roomSize   = p.rvSize->load();
        rp.damping    = p.rvDamp->load();
        rp.width      = p.rvWidth->load();
        rp.wetLevel   = rvMix;
        rp.dryLevel   = 1.0f - rvMix;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.process (ctx);
    }

    // =========================== width, DC, dry/wet, output, analyzer ===========================
    const float width = p.width->load();
    const bool  doWidth = numCh == 2 && std::abs (width - 1.0f) > 0.001f;

    for (int s = 0; s < n; ++s)
    {
        const float m  = mixSm.getNextValue();
        const float og = outGainSm.getNextValue();

        float w[2] = { 0.f, 0.f };
        for (int ch = 0; ch < numCh; ++ch)
            w[ch] = buffer.getSample (ch, s);

        if (doWidth)
        {
            const float mid  = 0.5f * (w[0] + w[1]);
            const float side = 0.5f * (w[0] - w[1]) * width;
            w[0] = mid + side;
            w[1] = mid - side;
        }

        float mono = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float wv = w[ch];
            if (needDC)
            {
                const float y = wv - dcX[ch] + 0.9995f * dcY[ch];
                dcX[ch] = wv;
                dcY[ch] = y;
                wv = y;
            }
            const float out = (dry.getSample (ch, s) * (1.0f - m) + wv * m) * og;
            buffer.setSample (ch, s, out);
            mono += out;
        }

        fifo[(size_t) fifoIndex++] = mono / (float) numCh;
        if (fifoIndex >= fftSize)
        {
            if (! analyzerReady.load())
            {
                analyzerBlock = fifo;
                analyzerReady.store (true);
            }
            fifoIndex = 0;
        }
    }
}

//==============================================================================
// Presets
const std::vector<XZXAudioProcessor::Preset>& XZXAudioProcessor::factoryPresets()
{
    static const std::vector<Preset> presets =
    {
        { "Init", {} },

        { "Warm Tape", {
            { "drive", 8.f }, { "drive_type", 3.f }, { "drive_tone", 9000.f },
            { "chorus_mix", 0.15f }, { "chorus_rate", 0.6f }, { "chorus_depth", 0.3f },
            { "eq_low", 2.f }, { "eq_high", -2.f }, { "out_gain", -2.f } } },

        { "Lo-Fi Crunch", {
            { "crush_bits", 8.f }, { "crush_rate", 4.f }, { "drive", 6.f }, { "drive_tone", 6000.f },
            { "delay_mix", 0.1f }, { "delay_time", 280.f }, { "eq_high", -4.f } } },

        { "Vinyl Dust", {
            { "crush_bits", 12.f }, { "crush_rate", 2.f }, { "crush_mix", 0.6f },
            { "drive", 4.f }, { "drive_type", 3.f }, { "drive_tone", 7500.f },
            { "eq_low", 3.f }, { "eq_high", -6.f }, { "width", 0.7f }, { "rev_mix", 0.08f } } },

        { "Wobble Filter", {
            { "filt_type", 1.f }, { "filt_cut", 400.f }, { "filt_res", 5.f },
            { "lfo_rate", 2.5f }, { "lfo_depth", 0.6f }, { "drive", 10.f }, { "out_gain", -3.f } } },

        { "Auto-Wah", {
            { "filt_type", 3.f }, { "filt_cut", 500.f }, { "filt_res", 6.f },
            { "env_amt", 0.7f }, { "env_rel", 120.f }, { "drive", 6.f }, { "out_gain", -2.f } } },

        { "Auto Sweep HP", {
            { "filt_type", 2.f }, { "filt_cut", 300.f }, { "filt_res", 2.f },
            { "lfo_rate", 0.3f }, { "lfo_depth", 0.8f }, { "lfo_shape", 1.f } } },

        { "Dub Echo", {
            { "delay_time", 480.f }, { "delay_fb", 0.65f }, { "delay_mix", 0.5f }, { "delay_damp", 3500.f },
            { "rev_mix", 0.15f } } },

        { "Ping-Pong Bounce", {
            { "delay_mode", 1.f }, { "delay_time", 300.f }, { "delay_fb", 0.55f }, { "delay_mix", 0.45f },
            { "delay_spread", 0.25f }, { "delay_damp", 6000.f }, { "width", 1.3f } } },

        { "Cathedral", {
            { "rev_size", 0.95f }, { "rev_damp", 0.35f }, { "rev_mix", 0.5f }, { "chorus_mix", 0.1f } } },

        { "Dream Wash", {
            { "chorus_mix", 0.5f }, { "chorus_depth", 0.6f }, { "chorus_rate", 0.4f },
            { "rev_size", 0.85f }, { "rev_mix", 0.45f }, { "delay_mix", 0.25f }, { "delay_time", 600.f },
            { "width", 1.4f } } },

        { "Flanger Jet", {
            { "chorus_mix", 0.6f }, { "chorus_rate", 0.2f }, { "chorus_depth", 0.8f },
            { "chorus_delay", 2.f }, { "chorus_fb", 0.75f } } },

        { "Helicopter Tremolo", {
            { "trem_rate", 8.f }, { "trem_depth", 1.f }, { "trem_shape", 2.f }, { "drive", 5.f } } },

        { "Autopan Widener", {
            { "trem_rate", 0.7f }, { "trem_depth", 0.7f }, { "trem_stereo", 1.f }, { "width", 1.5f },
            { "chorus_mix", 0.2f } } },

        { "Fold Destroyer", {
            { "drive", 24.f }, { "drive_type", 2.f }, { "eq_low", -8.f }, { "drive_tone", 8000.f },
            { "out_gain", -6.f } } },

        { "Telephone", {
            { "filt_type", 3.f }, { "filt_cut", 1800.f }, { "filt_res", 3.f },
            { "drive", 12.f }, { "drive_type", 1.f }, { "crush_bits", 10.f }, { "out_gain", -3.f } } },

        { "Diode Grit", {
            { "drive", 16.f }, { "drive_type", 4.f }, { "drive_bias", 0.2f }, { "drive_tone", 5000.f },
            { "eq_mid", 4.f }, { "eq_mid_freq", 1800.f }, { "out_gain", -4.f } } },
    };
    return presets;
}

void XZXAudioProcessor::resetToDefaults()
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

void XZXAudioProcessor::applyPreset (const Preset& preset)
{
    resetToDefaults();
    for (auto& [id, value] : preset.values)
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void XZXAudioProcessor::randomize()
{
    juce::Random r;
    for (auto* prm : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm);
        if (rp == nullptr) continue;

        const juce::String id = rp->paramID;
        if (id == "in_gain" || id == "out_gain" || id == "mix") continue;
        if (id.endsWith ("_on")) continue;

        float v = r.nextFloat();
        if (id.endsWith ("_mix"))          v = r.nextBool() ? r.nextFloat() * 0.7f : 0.0f;
        else if (id == "drive")            v = r.nextFloat() * 0.5f;
        else if (id == "delay_fb")         v *= 0.8f;
        else if (id == "chorus_fb")        v = 0.3f + v * 0.4f;
        else if (id == "crush_bits")       v = 0.45f + v * 0.55f;
        else if (id == "crush_rate")       v *= 0.3f;
        else if (id == "filt_res")         v *= 0.6f;
        else if (id == "eq_low" || id == "eq_mid" || id == "eq_high") v = 0.3f + v * 0.4f;
        else if (id == "env_amt")          v = 0.5f + (v - 0.5f) * 0.8f;
        else if (id == "width")            v = 0.35f + v * 0.4f;
        else if (id == "trem_depth")       v = r.nextBool() ? v * 0.8f : 0.0f;

        rp->setValueNotifyingHost (v);
    }
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
    state.removeProperty ("theme", nullptr);   // UI theme isn't part of a sound preset
    if (auto xml = state.createXml())
        xml->writeTo (getUserPresetDir().getChildFile (juce::File::createLegalFileName (name) + ".xzxpreset"));
}

bool XZXAudioProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::parseXML (file))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto theme = apvts.state.getProperty ("theme");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            if (! theme.isVoid())
                apvts.state.setProperty ("theme", theme, nullptr);
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
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* XZXAudioProcessor::createEditor()
{
    return new XZXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XZXAudioProcessor();
}
