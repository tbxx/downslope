#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MinimalistLookAndFeel.h"

class DownslopeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit DownslopeAudioProcessorEditor (DownslopeAudioProcessor&);
    ~DownslopeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SAtt = APVTS::SliderAttachment;
    using BAtt = APVTS::ButtonAttachment;
    using CAtt = APVTS::ComboBoxAttachment;

    DownslopeAudioProcessor& proc;
    MinimalistLookAndFeel    lnf;

    juce::TextButton triggerBtn { "TRIGGER" };
    std::unique_ptr<BAtt> triggerAtt;

    juce::Slider stopKnob, returnKnob, floorKnob, mixKnob;
    std::unique_ptr<SAtt> stopAtt, returnAtt, floorAtt, mixAtt;

    juce::Label stopLbl, returnLbl, floorLbl, mixLbl;

    // Sync-mode siblings of stopKnob/returnKnob (overlap them, swapped by mode)
    juce::Slider stopSyncKnob, returnSyncKnob;
    std::unique_ptr<SAtt> stopSyncAtt, returnSyncAtt;

    juce::Slider toneAmountKnob;
    std::unique_ptr<SAtt> toneAmountAtt;

    juce::ComboBox timeModeBox, stopCurveBox, returnCurveBox, trigModeBox;
    std::unique_ptr<CAtt> timeModeAtt, stopCurveAtt, returnCurveAtt, trigModeAtt;

    juce::Label timeModeLbl, stopCurveLbl, returnCurveLbl, trigModeLbl;

    juce::ToggleButton returnEnableBtn { "Return Enable" };
    juce::ToggleButton toneBtn        { "Tone Rolloff" };
    std::unique_ptr<BAtt> returnEnableAtt, toneAtt;

    void styleKnob   (juce::Slider&, juce::Label&, const juce::String&);
    void styleCombo  (juce::ComboBox&, juce::Label&, const juce::String&);

    void timerCallback() override;
    void updateEnabledStates();

    // last-seen values so the timer only repaints/updates on actual change
    int  lastStopMode    = -1;
    bool lastReturnOn    = true;
    bool lastToneOn      = true;

    // Layout constants — single source of truth shared by paint() and resized()
    // so the hairlines under each band always land where the band actually ends.
    struct Layout
    {
        static constexpr int marginX        = 44;
        static constexpr int titleBandH     = 66;
        static constexpr int triggerBandH   = 116;
        static constexpr int knobBandH      = 184;
        static constexpr int comboLabelH    = 18;
        static constexpr int comboBoxH      = 30;
        static constexpr int comboRowH      = comboLabelH + comboBoxH;
        static constexpr int toggleRowH     = 40;
        static constexpr int halfGap        = 6;
        static constexpr int toggleSegmentY = 504;   // y of the two short rules under the toggle row
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DownslopeAudioProcessorEditor)
};
