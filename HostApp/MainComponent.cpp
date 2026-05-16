#include "MainComponent.h"

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();
    readerThread.startThread();

    // --- UI -----------------------------------------------------------------
    openBtn.onClick = [this] { openFile(); };
    addAndMakeVisible (openBtn);

    playBtn.onClick = [this] { togglePlay(); };
    playBtn.setEnabled (false);
    addAndMakeVisible (playBtn);

    loopBtn.setClickingTogglesState (true);
    loopBtn.setToggleState (true, juce::dontSendNotification);
    loopBtn.onClick = [this]
    {
        if (readerSource != nullptr)
            readerSource->setLooping (loopBtn.getToggleState());
    };
    addAndMakeVisible (loopBtn);

    deviceBtn.onClick = [this]
    {
        auto* selector = new juce::AudioDeviceSelectorComponent (
            deviceManager,
            0, 0,           // no inputs
            1, 2,           // 1-2 outputs
            false, false,   // no midi in/out
            true, false);
        selector->setSize (500, 320);

        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (selector);
        o.dialogTitle = "Audio Output";
        o.dialogBackgroundColour = juce::Colour (0xff141418);
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    };
    addAndMakeVisible (deviceBtn);

    fileNameLbl.setText ("(no file loaded)", juce::dontSendNotification);
    fileNameLbl.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (fileNameLbl);

    transportLbl.setColour (juce::Label::textColourId, juce::Colours::grey);
    transportLbl.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (transportLbl);

    transportSource.addChangeListener (this);

    // --- Plugin -------------------------------------------------------------
    downslope = std::make_unique<DownslopeAudioProcessor>();
    editor.reset (downslope->createEditor());
    if (editor != nullptr)
        addAndMakeVisible (editor.get());

    // --- Audio device -------------------------------------------------------
    // 0 input channels, 2 output channels — system default device.
    setAudioChannels (0, 2);

    const int topBar = 76;
    const int eW = editor ? editor->getWidth()  : 600;
    const int eH = editor ? editor->getHeight() : 420;
    setSize (juce::jmax (640, eW + 40), topBar + eH + 20);

    startTimerHz (15);
}

MainComponent::~MainComponent()
{
    transportSource.setSource (nullptr);
    shutdownAudio();
    editor.reset();
    downslope.reset();
    readerThread.stopThread (2000);
}

void MainComponent::prepareToPlay (int samplesPerBlock, double sampleRate)
{
    transportSource.prepareToPlay (samplesPerBlock, sampleRate);

    downslope->setPlayConfigDetails (2, 2, sampleRate, samplesPerBlock);
    downslope->prepareToPlay (sampleRate, samplesPerBlock);

    scratch.setSize (2, samplesPerBlock, false, true, true);
}

void MainComponent::releaseResources()
{
    transportSource.releaseResources();
    if (downslope) downslope->releaseResources();
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (readerSource == nullptr)
    {
        info.clearActiveBufferRegion();
        return;
    }

    if (scratch.getNumSamples() < info.numSamples)
        scratch.setSize (2, info.numSamples, false, false, true);

    scratch.clear (0, info.numSamples);

    juce::AudioSourceChannelInfo si (&scratch, 0, info.numSamples);
    transportSource.getNextAudioBlock (si);

    // mono file → duplicate to right channel so Downslope sees true stereo
    if (fileChannels == 1)
        scratch.copyFrom (1, 0, scratch, 0, 0, info.numSamples);

    // run Downslope over a stereo view of the scratch buffer
    juce::AudioBuffer<float> view (scratch.getArrayOfWritePointers(), 2, info.numSamples);
    juce::MidiBuffer midi;
    downslope->processBlock (view, midi);

    // copy processed audio into the device buffer
    info.buffer->clear (info.startSample, info.numSamples);
    const int outCh = info.buffer->getNumChannels();
    for (int ch = 0; ch < outCh; ++ch)
    {
        const int src = juce::jmin (ch, 1);
        info.buffer->copyFrom (ch, info.startSample, scratch, src, 0, info.numSamples);
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e0e10));

    g.setColour (juce::Colour (0xff202024));
    g.fillRect (getLocalBounds().removeFromTop (76));
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    auto bar = r.removeFromTop (76).reduced (10, 10);
    auto topRow = bar.removeFromTop (28);
    openBtn   .setBounds (topRow.removeFromLeft (110).reduced (2));
    playBtn   .setBounds (topRow.removeFromLeft (80) .reduced (2));
    loopBtn   .setBounds (topRow.removeFromLeft (80) .reduced (2));
    deviceBtn .setBounds (topRow.removeFromRight (150).reduced (2));

    bar.removeFromTop (4);
    auto bottomRow = bar.removeFromTop (22);
    transportLbl.setBounds (bottomRow.removeFromRight (180).reduced (2, 0));
    fileNameLbl .setBounds (bottomRow.reduced (2, 0));

    if (editor != nullptr)
    {
        const int eW = editor->getWidth();
        const int eH = editor->getHeight();
        editor->setBounds (r.withSizeKeepingCentre (eW, eH));
    }
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* src)
{
    if (src == &transportSource)
        playBtn.setButtonText (transportSource.isPlaying() ? "Stop" : "Play");
}

void MainComponent::timerCallback()
{
    updateTransportText();
}

void MainComponent::updateTransportText()
{
    const double pos = transportSource.getCurrentPosition();
    const double len = transportSource.getLengthInSeconds();
    auto fmt = [] (double s) {
        if (s < 0 || ! std::isfinite (s)) s = 0.0;
        const int total = static_cast<int> (s * 10.0);
        const int mins = total / 600;
        const int secs = (total / 10) % 60;
        const int tnth = total % 10;
        return juce::String::formatted ("%02d:%02d.%d", mins, secs, tnth);
    };
    transportLbl.setText (fmt (pos) + " / " + fmt (len), juce::dontSendNotification);
}

void MainComponent::openFile()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Select an audio file",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    const auto flags = juce::FileBrowserComponent::openMode
                     | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file != juce::File{})
            loadFile (file);
    });
}

void MainComponent::loadFile (const juce::File& file)
{
    auto* reader = formatManager.createReaderFor (file);
    if (reader == nullptr)
    {
        fileNameLbl.setText ("(unsupported file)", juce::dontSendNotification);
        return;
    }

    fileChannels   = static_cast<int> (reader->numChannels);
    fileSampleRate = reader->sampleRate;

    transportSource.stop();
    transportSource.setSource (nullptr);

    auto rs = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    rs->setLooping (loopBtn.getToggleState());

    // background-thread disk buffering for glitch-free playback
    auto bs = std::make_unique<juce::BufferingAudioSource> (
        rs.get(), readerThread, false,
        /*bufferSizeSamples*/ 32768,
        /*numChannels*/ juce::jmax (1, fileChannels));

    transportSource.setSource (bs.get(),
                               /*bufferSizeSamples*/ 0,
                               /*readAheadThread*/ nullptr,
                               fileSampleRate,
                               /*numChannels*/ juce::jmax (1, fileChannels));

    readerSource    = std::move (rs);
    bufferingSource = std::move (bs);

    fileNameLbl.setText (file.getFileName()
                         + juce::String::formatted ("  [%d ch, %.0f Hz]",
                                                    fileChannels, fileSampleRate),
                         juce::dontSendNotification);
    playBtn.setEnabled (true);
    transportSource.setPosition (0);
}

void MainComponent::togglePlay()
{
    if (transportSource.isPlaying())
        transportSource.stop();
    else
        transportSource.start();
}
