#include "MainComponent.h"

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();
    readerThread.startThread();

    {
        juce::PropertiesFile::Options o;
        o.applicationName     = "DownslopeHost";
        o.filenameSuffix      = ".settings";
        o.folderName          = "Mine/DownslopeHost";
        o.osxLibrarySubFolder = "Application Support";
        appProperties.setStorageParameters (o);
    }

    // --- UI -----------------------------------------------------------------
    openBtn.onClick = [this] { openFile(); };
    addAndMakeVisible (openBtn);

    playBtn.onClick = [this] { togglePlay(); };
    playBtn.setEnabled (false);
    addAndMakeVisible (playBtn);

    loopBtn.setClickingTogglesState (true);
    {
        const bool initialLoop = appProperties.getUserSettings() != nullptr
            ? appProperties.getUserSettings()->getBoolValue ("loop", true)
            : true;
        loopBtn.setToggleState (initialLoop, juce::dontSendNotification);
    }
    loopBtn.onClick = [this]
    {
        if (readerSource != nullptr)
            readerSource->setLooping (loopBtn.getToggleState());
        if (auto* p = appProperties.getUserSettings())
        {
            p->setValue ("loop", loopBtn.getToggleState());
            p->saveIfNeeded();
        }
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

    // Restore plugin params BEFORE creating the editor so the UI reflects them.
    if (auto* p = appProperties.getUserSettings())
    {
        if (auto xml = p->getXmlValue ("pluginState"))
            if (xml->hasTagName (downslope->apvts.state.getType()))
                downslope->apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    downslope->apvts.state.addListener (this);

    editor.reset (downslope->createEditor());
    if (editor != nullptr)
        addAndMakeVisible (editor.get());

    // --- Audio device -------------------------------------------------------
    // Restore the saved device selection / sample rate / buffer size, if any.
    std::unique_ptr<juce::XmlElement> audioState;
    if (auto* p = appProperties.getUserSettings())
        audioState = p->getXmlValue ("audioDeviceState");

    // 0 input channels, 2 output channels.
    setAudioChannels (0, 2, audioState.get());

    deviceManager.addChangeListener (this);

    const int topBar = 76;
    const int eW = editor ? editor->getWidth()  : 600;
    const int eH = editor ? editor->getHeight() : 420;
    setSize (juce::jmax (640, eW + 40), topBar + eH + 20);

    startTimerHz (15);

    // Restore the file from the last session, if it still exists.
    const auto last = recallLastFile();
    if (last != juce::File{} && last.existsAsFile())
        loadFile (last);
}

MainComponent::~MainComponent()
{
    saveAll();

    if (downslope != nullptr)
        downslope->apvts.state.removeListener (this);
    deviceManager.removeChangeListener (this);

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
    else if (src == &deviceManager)
        saveAudioDeviceState();
}

void MainComponent::timerCallback()
{
    updateTransportText();
    updateFileStatusText();  // catches device sample-rate changes from the settings panel

    if (paramsDirty)
    {
        savePluginState();
        paramsDirty = false;
    }
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
    loadedFile      = file;
    rememberFile (file);

    updateFileStatusText();
    playBtn.setEnabled (true);
    transportSource.setPosition (0);
}

void MainComponent::rememberFile (const juce::File& f)
{
    if (auto* p = appProperties.getUserSettings())
    {
        p->setValue ("lastFile", f.getFullPathName());
        p->saveIfNeeded();
    }
}

void MainComponent::savePluginState()
{
    auto* p = appProperties.getUserSettings();
    if (p == nullptr || downslope == nullptr) return;

    auto state = downslope->apvts.copyState();
    if (auto xml = state.createXml())
    {
        p->setValue ("pluginState", xml.get());
        p->saveIfNeeded();
    }
}

void MainComponent::saveAudioDeviceState()
{
    auto* p = appProperties.getUserSettings();
    if (p == nullptr) return;

    if (auto xml = deviceManager.createStateXml())
    {
        p->setValue ("audioDeviceState", xml.get());
        p->saveIfNeeded();
    }
}

void MainComponent::saveAll()
{
    auto* p = appProperties.getUserSettings();
    if (p == nullptr) return;

    if (loadedFile != juce::File{})
        p->setValue ("lastFile", loadedFile.getFullPathName());
    p->setValue ("loop", loopBtn.getToggleState());
    if (auto audioXml = deviceManager.createStateXml())
        p->setValue ("audioDeviceState", audioXml.get());
    if (downslope != nullptr)
    {
        auto state = downslope->apvts.copyState();
        if (auto xml = state.createXml())
            p->setValue ("pluginState", xml.get());
    }
    p->saveIfNeeded();
}

juce::File MainComponent::recallLastFile()
{
    if (auto* p = appProperties.getUserSettings())
    {
        const auto path = p->getValue ("lastFile");
        if (path.isNotEmpty())
            return juce::File (path);
    }
    return {};
}

void MainComponent::updateFileStatusText()
{
    if (loadedFile == juce::File{})
    {
        fileNameLbl.setText ("(no file loaded)", juce::dontSendNotification);
        return;
    }

    double deviceRate = 0.0;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        deviceRate = dev->getCurrentSampleRate();

    constexpr auto bullet = u8"  ·  ";   // middle dot
    constexpr auto arrow  = u8"  →  ";   // right arrow

    juce::String s = loadedFile.getFileName()
        + juce::String (reinterpret_cast<const char*> (bullet))
        + juce::String (fileChannels) + " ch"
        + juce::String (reinterpret_cast<const char*> (bullet))
        + juce::String::formatted ("%.0f Hz", fileSampleRate);

    if (deviceRate > 0.0 && std::abs (deviceRate - fileSampleRate) > 0.5)
        s += juce::String (reinterpret_cast<const char*> (arrow))
           + juce::String::formatted ("%.0f Hz (resampling)", deviceRate);
    else if (deviceRate > 0.0)
        s += juce::String (reinterpret_cast<const char*> (arrow))
           + juce::String::formatted ("%.0f Hz", deviceRate);

    fileNameLbl.setText (s, juce::dontSendNotification);
}

void MainComponent::togglePlay()
{
    if (transportSource.isPlaying())
        transportSource.stop();
    else
        transportSource.start();
}
