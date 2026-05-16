#include "PluginEditor.h"

DownslopeAudioProcessorEditor::DownslopeAudioProcessorEditor (DownslopeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (600, 420);

    auto& s = proc.apvts;

    triggerBtn.setClickingTogglesState (true);
    triggerBtn.setColour (juce::TextButton::buttonColourId,    juce::Colour (0xff2a2a30));
    triggerBtn.setColour (juce::TextButton::buttonOnColourId,  juce::Colour (0xffe04020));
    triggerBtn.setColour (juce::TextButton::textColourOnId,    juce::Colours::white);
    triggerBtn.setColour (juce::TextButton::textColourOffId,   juce::Colours::lightgrey);
    addAndMakeVisible (triggerBtn);
    triggerAtt = std::make_unique<BAtt> (s, "trigger", triggerBtn);

    styleKnob (stopKnob,   stopLbl,   "Stop");
    styleKnob (returnKnob, returnLbl, "Return");
    styleKnob (floorKnob,  floorLbl,  "Floor");
    styleKnob (mixKnob,    mixLbl,    "Mix");

    stopAtt   = std::make_unique<SAtt> (s, "stopTime",   stopKnob);
    returnAtt = std::make_unique<SAtt> (s, "returnTime", returnKnob);
    floorAtt  = std::make_unique<SAtt> (s, "floorPitch", floorKnob);
    mixAtt    = std::make_unique<SAtt> (s, "mix",        mixKnob);

    styleCombo (timeModeBox,    timeModeLbl,    "Stop Mode");
    styleCombo (syncDivBox,     syncDivLbl,     "Sync");
    styleCombo (stopCurveBox,   stopCurveLbl,   "Stop Curve");
    styleCombo (returnCurveBox, returnCurveLbl, "Return Curve");
    styleCombo (trigModeBox,    trigModeLbl,    "Trig Mode");

    timeModeAtt    = std::make_unique<CAtt> (s, "stopTimeMode", timeModeBox);
    syncDivAtt     = std::make_unique<CAtt> (s, "stopSyncDiv",  syncDivBox);
    stopCurveAtt   = std::make_unique<CAtt> (s, "stopCurve",    stopCurveBox);
    returnCurveAtt = std::make_unique<CAtt> (s, "returnCurve",  returnCurveBox);
    trigModeAtt    = std::make_unique<CAtt> (s, "triggerMode",  trigModeBox);

    addAndMakeVisible (returnEnableBtn);
    addAndMakeVisible (toneBtn);
    returnEnableAtt = std::make_unique<BAtt> (s, "returnEnable", returnEnableBtn);
    toneAtt         = std::make_unique<BAtt> (s, "toneRolloff",  toneBtn);
}

void DownslopeAudioProcessorEditor::styleKnob (juce::Slider& sl, juce::Label& lb, const juce::String& name)
{
    sl.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    sl.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    sl.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe04020));
    sl.setColour (juce::Slider::textBoxOutlineColourId,   juce::Colours::transparentBlack);
    addAndMakeVisible (sl);
    lb.setText (name, juce::dontSendNotification);
    lb.setJustificationType (juce::Justification::centred);
    lb.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (lb);
}

void DownslopeAudioProcessorEditor::styleCombo (juce::ComboBox& cb, juce::Label& lb, const juce::String& name)
{
    addAndMakeVisible (cb);
    lb.setText (name, juce::dontSendNotification);
    lb.setJustificationType (juce::Justification::centredLeft);
    lb.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (lb);
}

void DownslopeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141418));

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));
    g.drawText ("DOWNSLOPE", getLocalBounds().removeFromTop (40),
                juce::Justification::centred);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("tape stop", getLocalBounds().removeFromTop (54).removeFromBottom (16),
                juce::Justification::centred);
}

void DownslopeAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (44);

    auto triggerArea = r.removeFromTop (56);
    triggerBtn.setBounds (triggerArea.reduced (120, 6));

    r.removeFromTop (6);
    auto knobRow = r.removeFromTop (130);
    const int kw = knobRow.getWidth() / 4;
    auto layKnob = [] (juce::Slider& sl, juce::Label& lb, juce::Rectangle<int> b)
    {
        lb.setBounds (b.removeFromTop (18));
        sl.setBounds (b.reduced (6, 0));
    };
    layKnob (stopKnob,   stopLbl,   knobRow.removeFromLeft (kw));
    layKnob (returnKnob, returnLbl, knobRow.removeFromLeft (kw));
    layKnob (floorKnob,  floorLbl,  knobRow.removeFromLeft (kw));
    layKnob (mixKnob,    mixLbl,    knobRow);

    r.removeFromTop (6);
    auto layCombo = [] (juce::ComboBox& cb, juce::Label& lb, juce::Rectangle<int> a)
    {
        lb.setBounds (a.removeFromLeft (96));
        cb.setBounds (a);
    };

    auto row1 = r.removeFromTop (28);
    const int colW1 = row1.getWidth() / 3;
    layCombo (timeModeBox, timeModeLbl, row1.removeFromLeft (colW1).reduced (4, 0));
    layCombo (syncDivBox,  syncDivLbl,  row1.removeFromLeft (colW1).reduced (4, 0));
    layCombo (trigModeBox, trigModeLbl, row1.reduced (4, 0));

    r.removeFromTop (4);
    auto row2 = r.removeFromTop (28);
    const int colW2 = row2.getWidth() / 2;
    layCombo (stopCurveBox,   stopCurveLbl,   row2.removeFromLeft (colW2).reduced (4, 0));
    layCombo (returnCurveBox, returnCurveLbl, row2.reduced (4, 0));

    r.removeFromTop (6);
    auto togRow = r.removeFromTop (28);
    returnEnableBtn.setBounds (togRow.removeFromLeft (togRow.getWidth() / 2).reduced (4, 0));
    toneBtn.setBounds (togRow.reduced (4, 0));
}
