#pragma once

#include <JuceHeader.h>
#include "TapeStopEngine.h"

class DownslopeAudioProcessor : public juce::AudioProcessor
{
public:
    DownslopeAudioProcessor();
    ~DownslopeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override            { return "Downslope"; }
    bool acceptsMidi()  const override                     { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    // Worst-case tail = the longest stop time we expose (8s) — used during the
    // slowdown ring-buffer read. Keep this tight so the host doesn't hold the
    // audio thread open unnecessarily on stop/transport changes.
    double getTailLengthSeconds() const override           { return 8.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    TapeStopEngine engine;

    // Cached atomic pointers — avoids a hash lookup per param per block.
    std::atomic<float>* pTrigger     = nullptr;
    std::atomic<float>* pStopTime    = nullptr;
    std::atomic<float>* pStopMode    = nullptr;
    std::atomic<float>* pStopSync    = nullptr;
    std::atomic<float>* pReturnTime  = nullptr;
    std::atomic<float>* pReturnSync  = nullptr;
    std::atomic<float>* pReturnEn    = nullptr;
    std::atomic<float>* pStopCurve   = nullptr;
    std::atomic<float>* pReturnCurve = nullptr;
    std::atomic<float>* pFloorPitch  = nullptr;
    std::atomic<float>* pToneOn      = nullptr;
    std::atomic<float>* pToneAmount  = nullptr;
    std::atomic<float>* pTrigMode    = nullptr;
    std::atomic<float>* pMix         = nullptr;
    void cacheParameterPointers();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DownslopeAudioProcessor)
};
