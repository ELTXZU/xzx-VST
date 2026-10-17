#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// xzx VST - tempo-synced glitch engine by ELTXZU
//   * 16-step pattern: each step can stutter / reverse / tape-stop / pitch-hop / mute / crush
//   * drawable volume shaper + filter shaper (ShaperBox style)
class XZXAudioProcessor : public juce::AudioProcessor,
                          private juce::ValueTree::Listener
{
public:
    static constexpr int numSteps = 16;
    static constexpr int curveN   = 32;

    enum Mode { Off = 0, Stutter, Reverse, TapeStop, OctUp, OctDown, Mute, Crush, numModes };

    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;   // param id -> real value
        juce::String pattern;                                  // 16 digits, 0-7 (see Mode)
        juce::String vol, filt;                                // curve strings ("" = flat)
    };

    XZXAudioProcessor();
    ~XZXAudioProcessor() override;

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
    double getTailLengthSeconds() const override { return 0.0; }

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

    // --- Curves (0 = volume shaper, 1 = filter shaper) ---
    static juce::String curveToString (const std::array<float, curveN>&);
    static std::array<float, curveN> makeCurveShape (int kind, float flatValue);
    std::array<float, curveN> getCurveCopy (int which) const;
    void setCurve (int which, const std::array<float, curveN>&);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    std::array<std::atomic<float>, curveN> volCurve, filtCurve;

    // --- Visual feedback for the GUI ---
    std::atomic<float> visLevel { 0.f };
    std::atomic<int>   visStep { -1 };
    std::atomic<int>   visMode { 0 };
    std::atomic<float> visVolPhase { 0.f }, visFiltPhase { 0.f };
    std::atomic<bool>  visNap { false };

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { loadCurves(); }
    void valueTreeRedirected (juce::ValueTree&) override { loadCurves(); }
    void loadCurves();
    float readRing (int ch, double pos) const;

    struct Params
    {
        std::atomic<float> *stepRate, *steps, *chance, *fadeMs, *stutterDiv, *tapeCurve,
                           *crushBits, *crushRate,
                           *volDepth, *volLen,
                           *fType, *cut, *res, *range, *filtLen,
                           *mix, *outGain, *nap;
        std::atomic<float>* step[numSteps];
    } p {};

    double sr = 44100.0;
    int currentProgram = 0;

    // ring buffer (what the glitches read from)
    juce::AudioBuffer<float> ring;
    int ringMask = 0;
    int writePos = 0;

    // step engine state
    double freePpq = 0.0;
    long long lastAbsStep = std::numeric_limits<long long>::min();
    int    stepMode = 0;
    int    stepN = 0;
    double stepSamples = 1.0;
    int    loopStart = 0;
    double readPos = 0.0;
    int    crushCnt = 0;
    float  crushHold[2] { 0.f, 0.f };
    int    currentStepIdx = 0;
    juce::Random rng;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> mixSm, outSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XZXAudioProcessor)
};
