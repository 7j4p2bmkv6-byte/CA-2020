#pragma once
#include <JuceHeader.h>
#include <mutex>
#include "engine/ChordArpEngine.h"
#include "engine/PreviewSynth.h"

class CA2020Processor : public juce::AudioProcessor
{
public:
    CA2020Processor();
    ~CA2020Processor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CA-2020"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Factory styles are exposed as host programs
    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentStyle.load()); }
    void setCurrentProgram (int index) override { applyStyle (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    ca::Engine engine;

    void applyStyle (int index);
    std::atomic<int> currentStyle { -1 };

    // user progressions (8 slots) - edited by the UI, copied to the engine safely
    ca::Progression getUserProgression (int slot) const;
    void setUserProgression (int slot, const ca::Progression&);
    std::atomic<int> userVersion { 1 };

    // presets
    static juce::File presetFolder();
    bool savePreset (const juce::File&);
    bool loadPreset (const juce::File&);

    std::atomic<double> lastBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };
    bool isStandalone() const { return wrapperType == wrapperType_Standalone; }

    static juce::StringArray progressionNames();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout (bool standalone);
    ca::Settings readSettings() const;
    juce::ValueTree buildFullState();
    void restoreFullState (const juce::ValueTree&);
    void setParam (const juce::String& id, float plainValue);

    mutable std::mutex userMutex;
    ca::Progression userProgs[ca::kNumUserProgs];
    int engineUserVersion = 0;

    ca::PreviewSynth synth;
    int lastPatch = -1;
    bool lastPreview = false;

    std::vector<ca::MidiEv> inEvents, outEvents;
    std::vector<float> scratchRight;

    struct P { std::atomic<float>* v = nullptr; float get() const { return v ? v->load() : 0.0f; } };
    P pMode, pTrigger, pLatch, pFollow, pKey, pScale, pChordType, pExt, pProg, pSpeed, pOutput, pPattern,
      pArpMode, pArpRate, pArpOct, pGate, pSwing, pInv, pSpread, pOct, pBass, pLead, pStrum, pVelMode, pVel,
      pHuman, pTempo, pPreview, pPatch, pLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CA2020Processor)
};
