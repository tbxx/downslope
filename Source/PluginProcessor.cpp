#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ids
{
    constexpr auto trigger      = "trigger";
    constexpr auto stopTime     = "stopTime";
    constexpr auto stopTimeMode = "stopTimeMode";
    constexpr auto stopSyncDiv  = "stopSyncDiv";
    constexpr auto returnTime   = "returnTime";
    constexpr auto returnEnable = "returnEnable";
    constexpr auto stopCurve    = "stopCurve";
    constexpr auto returnCurve  = "returnCurve";
    constexpr auto floorPitch   = "floorPitch";
    constexpr auto toneRolloff  = "toneRolloff";
    constexpr auto triggerMode  = "triggerMode";
    constexpr auto mix          = "mix";
}

static const std::array<float, 9> syncDivBeats {
    0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f
};
static const juce::StringArray syncDivNames {
    "1/16", "1/8", "1/4", "1/4d", "1/2", "1/2d", "1 bar", "2 bars", "4 bars"
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
        "Stop Sync", syncDivNames, 4));

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

void DownslopeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto loadF = [this] (const char* id) {
        return apvts.getRawParameterValue (id)->load();
    };
    const auto loadB = [&] (const char* id) { return loadF (id) > 0.5f; };
    const auto loadI = [&] (const char* id) { return static_cast<int> (loadF (id)); };

    const bool  trig       = loadB (ids::trigger);
    const int   timeMode   = loadI (ids::stopTimeMode);
    const float stopMs     = loadF (ids::stopTime);
    const int   syncIdx    = loadI (ids::stopSyncDiv);
    const float returnMs   = loadF (ids::returnTime);
    const bool  retEn      = loadB (ids::returnEnable);
    const int   curveIdx   = loadI (ids::stopCurve);
    const int   retCurveIx = loadI (ids::returnCurve);
    const float floorP     = loadF (ids::floorPitch);
    const bool  tone       = loadB (ids::toneRolloff);
    const int   trigMode   = loadI (ids::triggerMode);
    const float mixV       = loadF (ids::mix);

    float stopSeconds = stopMs * 0.001f;
    if (timeMode == 1)
    {
        double bpm = 120.0;
        if (auto* head = getPlayHead())
            if (auto pos = head->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;

        const int idx = juce::jlimit (0, (int) syncDivBeats.size() - 1, syncIdx);
        stopSeconds = static_cast<float> (syncDivBeats[(size_t) idx] * 60.0 / bpm);
    }

    engine.setStopTimeSeconds   (stopSeconds);
    engine.setReturnTimeSeconds (returnMs * 0.001f);
    engine.setFloorPitch        (floorP);
    engine.setCurve             (static_cast<TapeStopEngine::Curve> (curveIdx));
    engine.setReturnCurve       (static_cast<TapeStopEngine::Curve> (retCurveIx));
    engine.setTriggerMode       (static_cast<TapeStopEngine::TriggerMode> (trigMode));
    engine.setReturnEnabled     (retEn);
    engine.setToneRolloff       (tone);
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
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
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
