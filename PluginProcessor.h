#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class XZXAudioProcessor : public juce::AudioProcessor
{
public:
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values; // param id -> real value
    };

    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;

    XZXAudioProcessor();
    ~XZXAudioProcessor() override = default;

    // --- AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "xzx VST"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- Presets ---
    static const std::vector<Preset>& factoryPresets();
    void resetToDefaults();
    void applyPreset (const Preset&);
    void randomize();
    juce::File getUserPresetDir() const;
    juce::Array<juce::File> getUserPresetFiles() const;
    void saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File&);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Analyzer data (audio thread -> GUI)
    std::array<float, fftSize> analyzerBlock {};
    std::atomic<bool> analyzerReady { false };

private:
    struct Params
    {
        std::atomic<float> *inGain, *outGain, *mix, *width,
                           *crushOn, *bits, *rate, *crushMix,
                           *driveOn, *drive, *driveType, *bias, *tone,
                           *eqOn, *eqLow, *eqMid, *eqMidFreq, *eqHigh,
                           *fType, *cut, *res, *lfoRate, *lfoDepth, *lfoShape, *envAmt, *envRel,
                           *tremOn, *tremRate, *tremDepth, *tremShape, *tremStereo,
                           *delayOn, *dMode, *dTime, *dFb, *dMix, *dSpread, *dDamp,
                           *chOn, *chRate, *chDepth, *chDelay, *chFb, *chMix,
                           *rvOn, *rvSize, *rvDamp, *rvWidth, *rvMix;
    } p {};

    using EQFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                    juce::dsp::IIR::Coefficients<float>>;

    double sr = 44100.0;
    int currentProgram = 0;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 4 };
    EQFilter eqLow, eqMid, eqHigh;
    float lastEq[4] { 1e9f, 1e9f, 1e9f, 1e9f };

    juce::AudioBuffer<float> dry;
    std::vector<float> envBuf;
    juce::SmoothedValue<float> inGainSm, outGainSm, mixSm, driveSm, delaySm;

    float crushHeld[2]  { 0.f, 0.f };
    int   crushCount[2] { 0, 0 };
    float toneState[2]  { 0.f, 0.f };
    float dampState[2]  { 0.f, 0.f };
    float dcX[2] { 0.f, 0.f }, dcY[2] { 0.f, 0.f };
    float envState = 0.f;
    float lfoPhase = 0.f, tremPhase = 0.f;

    std::array<float, fftSize> fifo {};
    int fifoIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XZXAudioProcessor)
};
