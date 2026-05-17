#include "PluginEditor.h"

DownslopeAudioProcessorEditor::DownslopeAudioProcessorEditor (DownslopeAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (820, 600);

    auto& s = proc.apvts;

    // --- TRIGGER ------------------------------------------------------------
    triggerBtn.setClickingTogglesState (true);
    addAndMakeVisible (triggerBtn);
    triggerAtt = std::make_unique<BAtt> (s, "trigger", triggerBtn);

    // --- KNOBS (display order: STOP, FLOOR, RETURN, MIX) --------------------
    styleKnob (stopKnob,   stopLbl,   "STOP");
    styleKnob (floorKnob,  floorLbl,  "FLOOR");
    styleKnob (returnKnob, returnLbl, "RETURN");
    styleKnob (mixKnob,    mixLbl,    "MIX");

    const auto formatMs = [] (double v) {
        return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " s"
                           : juce::String ((int) v)      + " ms";
    };
    const auto formatPercent = [] (double v) {
        return juce::String ((int) std::round (v * 100.0)) + "%";
    };
    stopKnob  .textFromValueFunction = formatMs;
    returnKnob.textFromValueFunction = formatMs;
    floorKnob .textFromValueFunction = formatPercent;
    mixKnob   .textFromValueFunction = formatPercent;

    stopAtt   = std::make_unique<SAtt> (s, "stopTime",   stopKnob);
    returnAtt = std::make_unique<SAtt> (s, "returnTime", returnKnob);
    floorAtt  = std::make_unique<SAtt> (s, "floorPitch", floorKnob);
    mixAtt    = std::make_unique<SAtt> (s, "mix",        mixKnob);

    // Sync-mode counterparts for STOP and RETURN: rotary "steppers" through
    // the sync-division choices. Overlay the ms knobs; visibility swapped
    // by mode in updateEnabledStates().
    auto setupSyncKnob = [this] (juce::Slider& sl)
    {
        sl.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sl.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 130, 28);
        sl.setColour (juce::Slider::textBoxTextColourId,      lnf.p.ink);
        sl.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        sl.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        addChildComponent (sl);   // start hidden; mode listener flips visibility
    };
    setupSyncKnob (stopSyncKnob);
    setupSyncKnob (returnSyncKnob);
    stopSyncAtt   = std::make_unique<SAtt> (s, "stopSyncDiv",   stopSyncKnob);
    returnSyncAtt = std::make_unique<SAtt> (s, "returnSyncDiv", returnSyncKnob);

    // --- COMBOS -------------------------------------------------------------
    styleCombo (timeModeBox,    timeModeLbl,    "STOP MODE");
    styleCombo (stopCurveBox,   stopCurveLbl,   "STOP CURVE");
    styleCombo (returnCurveBox, returnCurveLbl, "RETURN CURVE");
    styleCombo (trigModeBox,    trigModeLbl,    "TRIGGER MODE");

    auto populate = [&s] (juce::ComboBox& box, const juce::String& paramId)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (paramId)))
            box.addItemList (choice->choices, 1);
    };
    populate (timeModeBox,    "stopTimeMode");
    populate (stopCurveBox,   "stopCurve");
    populate (returnCurveBox, "returnCurve");
    populate (trigModeBox,    "triggerMode");

    timeModeAtt    = std::make_unique<CAtt> (s, "stopTimeMode", timeModeBox);
    stopCurveAtt   = std::make_unique<CAtt> (s, "stopCurve",    stopCurveBox);
    returnCurveAtt = std::make_unique<CAtt> (s, "returnCurve",  returnCurveBox);
    trigModeAtt    = std::make_unique<CAtt> (s, "triggerMode",  trigModeBox);

    // --- TOGGLES + TONE AMOUNT ---------------------------------------------
    addAndMakeVisible (returnEnableBtn);
    addAndMakeVisible (toneBtn);
    returnEnableAtt = std::make_unique<BAtt> (s, "returnEnable", returnEnableBtn);
    toneAtt         = std::make_unique<BAtt> (s, "toneRolloff",  toneBtn);

    toneAmountKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    toneAmountKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (toneAmountKnob);
    toneAmountAtt = std::make_unique<SAtt> (s, "toneAmount", toneAmountKnob);

    // Poll the few params that drive enable/disable on a timer instead of
    // registering an APVTS::Listener. This avoids cross-thread message
    // dispatch during host shutdown, which has caused issues in some hosts.
    updateEnabledStates();
    startTimerHz (20);
}

DownslopeAudioProcessorEditor::~DownslopeAudioProcessorEditor()
{
    stopTimer();

    // Explicitly tear down all APVTS attachments NOW, while we know the
    // processor is still alive. Relying on member-destruction order can race
    // with hosts that tear down processor and editor concurrently.
    triggerAtt.reset();
    stopAtt.reset();    returnAtt.reset();   floorAtt.reset();    mixAtt.reset();
    stopSyncAtt.reset(); returnSyncAtt.reset();
    toneAmountAtt.reset();
    timeModeAtt.reset(); stopCurveAtt.reset();
    returnCurveAtt.reset(); trigModeAtt.reset();
    returnEnableAtt.reset(); toneAtt.reset();

    setLookAndFeel (nullptr);
}

void DownslopeAudioProcessorEditor::timerCallback()
{
    auto& s = proc.apvts;
    const int  stopMode = (int) s.getRawParameterValue ("stopTimeMode")->load();
    const bool returnOn =       s.getRawParameterValue ("returnEnable")->load() > 0.5f;
    const bool toneOn   =       s.getRawParameterValue ("toneRolloff") ->load() > 0.5f;

    if (stopMode == lastStopMode && returnOn == lastReturnOn && toneOn == lastToneOn)
        return;

    lastStopMode = stopMode;
    lastReturnOn = returnOn;
    lastToneOn   = toneOn;
    updateEnabledStates();
}

void DownslopeAudioProcessorEditor::updateEnabledStates()
{
    auto& s = proc.apvts;
    const bool syncMode = (int) s.getRawParameterValue ("stopTimeMode")->load() == 1;
    const bool returnOn = s.getRawParameterValue ("returnEnable")->load() > 0.5f;
    const bool toneOn   = s.getRawParameterValue ("toneRolloff")->load() > 0.5f;

    // Swap which knob shows for STOP and RETURN based on the time mode.
    stopKnob      .setVisible (! syncMode);
    stopSyncKnob  .setVisible (  syncMode);
    returnKnob    .setVisible (! syncMode);
    returnSyncKnob.setVisible (  syncMode);

    // Return disabled greys out the return knob/curve regardless of mode.
    returnKnob    .setEnabled (returnOn);
    returnSyncKnob.setEnabled (returnOn);
    returnCurveBox.setEnabled (returnOn);
    returnCurveLbl.setColour  (juce::Label::textColourId,
                               returnOn ? lnf.p.inkMuted : lnf.p.inkMuted.withAlpha (0.45f));
    returnLbl.setColour (juce::Label::textColourId,
                         returnOn ? lnf.p.ink : lnf.p.ink.withAlpha (0.35f));

    toneAmountKnob.setEnabled (toneOn);
}

void DownslopeAudioProcessorEditor::styleKnob (juce::Slider& sl, juce::Label& lb, const juce::String& name)
{
    sl.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    sl.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 130, 28);
    sl.setColour (juce::Slider::textBoxTextColourId,      lnf.p.ink);
    sl.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    sl.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    addAndMakeVisible (sl);

    lb.setText (name, juce::dontSendNotification);
    lb.setJustificationType (juce::Justification::centred);
    lb.setFont (MinimalistLookAndFeel::caps (13.5f, 0.22f));
    addAndMakeVisible (lb);
}

void DownslopeAudioProcessorEditor::styleCombo (juce::ComboBox& cb, juce::Label& lb, const juce::String& name)
{
    addAndMakeVisible (cb);
    lb.setText (name, juce::dontSendNotification);
    lb.setJustificationType (juce::Justification::centredLeft);
    lb.setFont (MinimalistLookAndFeel::caps (12.5f, 0.22f));
    lb.setColour (juce::Label::textColourId, lnf.p.inkMuted);
    addAndMakeVisible (lb);
}

void DownslopeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (lnf.p.bg);

    const int marginX = Layout::marginX;
    const int W       = getWidth();
    const int innerW  = W - marginX * 2;
    const int colW    = (innerW - Layout::halfGap * 2) / 2;
    const int rightX  = marginX + colW + Layout::halfGap * 2;

    // Title — all-caps, spaced, bold
    g.setColour (lnf.p.ink);
    g.setFont (juce::Font (juce::FontOptions (20.0f).withKerningFactor (0.20f).withStyle ("Bold")));
    g.drawText ("DOWNSLOPE AUDIO BENDER",
                getLocalBounds().removeFromTop (Layout::titleBandH)
                    .withTrimmedLeft (marginX).withTrimmedRight (marginX),
                juce::Justification::centredLeft);

    // Section rules — y positions derived from the same band heights resized() uses.
    g.setColour (lnf.p.hairline);
    const int yAfterTitle   = Layout::titleBandH;
    const int yAfterTrigger = yAfterTitle   + Layout::triggerBandH;
    const int yAfterKnobs   = yAfterTrigger + Layout::knobBandH;

    g.fillRect (marginX, yAfterTitle,   innerW, 1);
    g.fillRect (marginX, yAfterTrigger, innerW, 1);
    g.fillRect (marginX, yAfterKnobs,   innerW, 1);

    // Two short rules under the toggle row, aligned with the two-column grid.
    g.fillRect (marginX, Layout::toggleSegmentY, colW, 1);
    g.fillRect (rightX,  Layout::toggleSegmentY, colW, 1);
}

void DownslopeAudioProcessorEditor::resized()
{
    const int marginX = Layout::marginX;
    const int innerW  = getWidth() - marginX * 2;
    const int colW    = (innerW - Layout::halfGap * 2) / 2;
    const int rightX  = marginX + colW + Layout::halfGap * 2;

    auto r = getLocalBounds().reduced (marginX, 0);

    r.removeFromTop (Layout::titleBandH);

    triggerBtn.setBounds (r.removeFromTop (Layout::triggerBandH).reduced (0, 22));

    // Knob band — STOP, FLOOR, RETURN, MIX
    auto knobBand = r.removeFromTop (Layout::knobBandH).reduced (0, 14);
    const int kw = knobBand.getWidth() / 4;
    auto layKnob = [] (juce::Slider& sl, juce::Label& lb, juce::Rectangle<int> b)
    {
        lb.setBounds (b.removeFromTop (18));
        sl.setBounds (b.reduced (12, 0));
    };
    layKnob (stopKnob,   stopLbl,   knobBand.removeFromLeft (kw));
    layKnob (floorKnob,  floorLbl,  knobBand.removeFromLeft (kw));
    layKnob (returnKnob, returnLbl, knobBand.removeFromLeft (kw));
    layKnob (mixKnob,    mixLbl,    knobBand);

    // Sync-mode knobs overlay the ms knobs exactly — same slot.
    stopSyncKnob  .setBounds (stopKnob  .getBounds());
    returnSyncKnob.setBounds (returnKnob.getBounds());

    auto layCombo = [] (juce::ComboBox& cb, juce::Label& lb, juce::Rectangle<int> a)
    {
        lb.setBounds (a.removeFromTop (Layout::comboLabelH));
        cb.setBounds (a.removeFromTop (Layout::comboBoxH));
    };

    // Row 1 (top): STOP CURVE / RETURN CURVE
    r.removeFromTop (18);
    auto curveRow = r.removeFromTop (Layout::comboRowH);
    layCombo (stopCurveBox,   stopCurveLbl,
              juce::Rectangle<int> (marginX, curveRow.getY(), colW, curveRow.getHeight()));
    layCombo (returnCurveBox, returnCurveLbl,
              juce::Rectangle<int> (rightX,  curveRow.getY(), colW, curveRow.getHeight()));

    // Row 2: Toggle row, same grid. Tone section LEFT, Return Enable RIGHT.
    // Tone amount knob is centred on the FLOOR knob's X axis so the two line up.
    r.removeFromTop (18);
    auto togRow = r.removeFromTop (Layout::toggleRowH);
    const int togY = togRow.getY();
    const int rowH = togRow.getHeight();

    constexpr int toneKnobBox = 40;
    const int floorCenterX = floorKnob.getBounds().getCentreX();
    toneAmountKnob.setBounds (floorCenterX - toneKnobBox / 2, togY, toneKnobBox, toneKnobBox);
    toneBtn       .setBounds (marginX, togY,
                              (floorCenterX - toneKnobBox / 2) - marginX - 10, rowH);
    returnEnableBtn.setBounds (rightX, togY, colW, rowH);

    // Row 3 (bottom): STOP MODE / TRIGGER MODE
    r.removeFromTop (24);
    auto modeRow = r.removeFromTop (Layout::comboRowH);
    layCombo (timeModeBox, timeModeLbl,
              juce::Rectangle<int> (marginX, modeRow.getY(), colW, modeRow.getHeight()));
    layCombo (trigModeBox, trigModeLbl,
              juce::Rectangle<int> (rightX,  modeRow.getY(), colW, modeRow.getHeight()));
}
