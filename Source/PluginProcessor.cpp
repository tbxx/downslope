#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    constexpr auto trigger      = "trigger";
    constexpr auto stopTime     = "stopTime";
    constexpr auto stopTimeMode = "stopTimeMode";
    constexpr auto stopSyncDiv   = "stopSyncDiv";
    constexpr auto returnSyncDiv = "returnSyncDiv";
    constexpr auto returnTime   = "returnTime";
    constexpr auto returnEnable = "returnEnable";
    constexpr auto stopCurve    = "stopCurve";
    constexpr auto returnCurve  = "returnCurve";
    constexpr auto floorPitch   = "floorPitch";
    constexpr auto toneRolloff  = "toneRolloff";
    constexpr auto toneAmount   = "toneAmount";
    constexpr auto triggerMode  = "triggerMode";
    constexpr auto mix          = "mix";
}

// Sync divisions, ordered by ascending duration. Triplets sit between their
// straight neighbours where their duration places them numerically.
static const std::array<float, 14> syncDivBeats {
    1.0f / 6.0f,   // 1/16t  ≈ 0.167
    0.25f,         // 1/16
    1.0f / 3.0f,   // 1/8t   ≈ 0.333
    0.5f,          // 1/8
    2.0f / 3.0f,   // 1/4t   ≈ 0.667
    1.0f,          // 1/4
    4.0f / 3.0f,   // 1/2t   ≈ 1.333
    1.5f,          // 1/4d
    2.0f,          // 1/2
    8.0f / 3.0f,   // 1 bar t ≈ 2.667
    3.0f,          // 1/2d
    4.0f,          // 1 bar
    8.0f,          // 2 bars
    16.0f          // 4 bars
};
static const juce::StringArray syncDivNames {
    "1/16t", "1/16", "1/8t", "1/8", "1/4t", "1/4",
    "1/2t",  "1/4d", "1/2",  "1 bar t", "1/2d", "1 bar", "2 bars", "4 bars"
};

juce::AudioProcessorValueTreeState::ParameterLayout
DownslopeAudioProcessor::createParameterLayout()
{
    using PF = juce::AudioParameterFloat;
    using PB = juce::AudioParameterBool;
    using PC = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<PB> (juce::ParameterID (ids::trigger, 1),
        "Trigger", false));

    p.push_back (std::make_unique<PF> (juce::ParameterID (ids::stopTime, 1),
        "Stop Time", Range (10.0f, 8000.0f, 1.0f, 0.4f), 1000.0f));

    p.push_back (std::make_unique<PC> (juce::ParameterID (ids::stopTimeMode, 1),
        "Stop Time Mode", juce::StringArray { "Free (ms)", "Sync" }, 0));

    p.push_back (std::make_unique<PC> (juce::ParameterID (ids::stopSyncDiv, 1),
        "Stop Sync", syncDivNames, 8));   // default 1/2

    p.push_back (std::make_unique<PC> (juce::ParameterID (ids::returnSyncDiv, 1),
        "Return Sync", syncDivNames, 5)); // default 1/4

    p.push_back (std::make_unique<PF> (juce::ParameterID (ids::returnTime, 1),
        "Return Time", Range (5.0f, 4000.0f, 1.0f, 0.4f), 250.0f));

    p.push_back (std::make_unique<PB> (juce::ParameterID (ids::returnEnable, 1),
        "Return Enable", true));

    const juce::StringArray curves { "Linear", "Exponential", "Logarithmic", "S-Curve" };

    p.push_back (std::make_unique<PC> (juce::ParameterID (ids::stopCurve, 1),
        "Stop Curve", curves, 1));

    p.push_back (std::make_unique<PC> (juce::ParameterID (ids::returnCurve, 1),
        "Return Curve", curves, 0));

    p.push_back (std::make_unique<PF> (juce::ParameterID (ids::floorPitch, 1),
        "Floor Pitch", Range (0.0f, 1.0f, 0.001f), 0.0f));

    p.push_back (std::make_unique<PB> (juce::ParameterID (ids::toneRolloff, 1),
        "Tone Rolloff", true));

    p.push_back (std::make_unique<PF> (juce::ParameterID (ids::toneAmount, 1),
        "Tone Amount", Range (0.0f, 1.0f, 0.001f), 0.5f));

    p.push_back (std::make_unique<PC> (juce::ParameterID (ids::triggerMode, 1),
        "Trigger Mode", juce::StringArray { "Momentary", "One-Shot" }, 0));

    p.push_back (std::make_unique<PF> (juce::ParameterID (ids::mix, 1),
        "Mix", Range (0.0f, 1.0f, 0.001f), 1.0f));

    return { p.begin(), p.end() };
}

DownslopeAudioProcessor::DownslopeAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    cacheParameterPointers();
}

void DownslopeAudioProcessor::cacheParameterPointers()
{
    pTrigger     = apvts.getRawParameterValue (ids::trigger);
    pStopTime    = apvts.getRawParameterValue (ids::stopTime);
    pStopMode    = apvts.getRawParameterValue (ids::stopTimeMode);
    pStopSync    = apvts.getRawParameterValue (ids::stopSyncDiv);
    pReturnTime  = apvts.getRawParameterValue (ids::returnTime);
    pReturnSync  = apvts.getRawParameterValue (ids::returnSyncDiv);
    pReturnEn    = apvts.getRawParameterValue (ids::returnEnable);
    pStopCurve   = apvts.getRawParameterValue (ids::stopCurve);
    pReturnCurve = apvts.getRawParameterValue (ids::returnCurve);
    pFloorPitch  = apvts.getRawParameterValue (ids::floorPitch);
    pToneOn      = apvts.getRawParameterValue (ids::toneRolloff);
    pToneAmount  = apvts.getRawParameterValue (ids::toneAmount);
    pTrigMode    = apvts.getRawParameterValue (ids::triggerMode);
    pMix         = apvts.getRawParameterValue (ids::mix);
}

bool DownslopeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out)                            return false;
    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void DownslopeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void DownslopeAudioProcessor::releaseResources()
{
    // Explicit teardown — drop ring buffers and reset state, so the host
    // sees us in a clean idle state if it polls during shutdown.
    engine.reset();
}

void DownslopeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const bool  trig         = pTrigger    ->load() > 0.5f;
    const int   timeMode     = (int) pStopMode  ->load();
    const float stopMs       = pStopTime   ->load();
    const int   stopSyncIdx  = (int) pStopSync  ->load();
    const float returnMs     = pReturnTime ->load();
    const int   retSyncIdx   = (int) pReturnSync->load();
    const bool  retEn        = pReturnEn   ->load() > 0.5f;
    const int   curveIdx     = (int) pStopCurve ->load();
    const int   retCurveIx   = (int) pReturnCurve->load();
    const float floorP       = pFloorPitch ->load();
    const bool  tone         = pToneOn     ->load() > 0.5f;
    const float toneAmt      = pToneAmount ->load();
    const int   trigMode     = (int) pTrigMode  ->load();
    const float mixV         = pMix        ->load();

    float stopSeconds   = stopMs   * 0.001f;
    float returnSeconds = returnMs * 0.001f;

    if (timeMode == 1)
    {
        double bpm = 120.0;
        if (auto* head = getPlayHead())
            if (auto pos = head->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;

        const auto divs = (int) syncDivBeats.size();
        const int sIdx = juce::jlimit (0, divs - 1, stopSyncIdx);
        const int rIdx = juce::jlimit (0, divs - 1, retSyncIdx);
        stopSeconds   = static_cast<float> (syncDivBeats[(size_t) sIdx] * 60.0 / bpm);
        returnSeconds = static_cast<float> (syncDivBeats[(size_t) rIdx] * 60.0 / bpm);
    }

    engine.setStopTimeSeconds   (stopSeconds);
    engine.setReturnTimeSeconds (returnSeconds);
    engine.setFloorPitch        (floorP);
    engine.setCurve             (static_cast<TapeStopEngine::Curve> (curveIdx));
    engine.setReturnCurve       (static_cast<TapeStopEngine::Curve> (retCurveIx));
    engine.setTriggerMode       (static_cast<TapeStopEngine::TriggerMode> (trigMode));
    engine.setReturnEnabled     (retEn);
    engine.setToneRolloff       (tone);
    engine.setToneAmount        (toneAmt);
    engine.setMix               (mixV);
    engine.setTrigger           (trig);

    engine.processBlock (buffer.getArrayOfReadPointers(),
                         buffer.getArrayOfWritePointers(),
                         buffer.getNumChannels(),
                         buffer.getNumSamples());
}

juce::AudioProcessorEditor* DownslopeAudioProcessor::createEditor()
{
    return new DownslopeAudioProcessorEditor (*this);
}

void DownslopeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DownslopeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DownslopeAudioProcessor();
}
