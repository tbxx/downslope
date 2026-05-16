#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class DownslopeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit DownslopeAudioProcessorEditor (DownslopeAudioProcessor&);
    ~DownslopeAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SAtt = APVTS::SliderAttachment;
    using BAtt = APVTS::ButtonAttachment;
    using CAtt = APVTS::ComboBoxAttachment;

    DownslopeAudioProcessor& proc;

    juce::TextButton triggerBtn { "TRIGGER" };
    std::unique_ptr<BAtt> triggerAtt;

    juce::Slider stopKnob, returnKnob, floorKnob, mixKnob;
    std::unique_ptr<SAtt> stopAtt, returnAtt, floorAtt, mixAtt;

    juce::Label stopLbl, returnLbl, floorLbl, mixLbl;

    juce::ComboBox timeModeBox, syncDivBox, stopCurveBox, returnCurveBox, trigModeBox;
    std::unique_ptr<CAtt> timeModeAtt, syncDivAtt, stopCurveAtt, returnCurveAtt, trigModeAtt;

    juce::Label timeModeLbl, syncDivLbl, stopCurveLbl, returnCurveLbl, trigModeLbl;

    juce::ToggleButton returnEnableBtn { "Return Enable" };
    juce::ToggleButton toneBtn        { "Tone Rolloff" };
    std::unique_ptr<BAtt> returnEnableAtt, toneAtt;

    void styleKnob   (juce::Slider&, juce::Label&, const juce::String&);
    void styleCombo  (juce::ComboBox&, juce::Label&, const juce::String&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DownslopeAudioProcessorEditor)
};
