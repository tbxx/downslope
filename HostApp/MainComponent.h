#pragma once

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::ChangeListener,
                      private juce::Timer,
                      private juce::ValueTree::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay (int samplesPerBlock, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    void timerCallback() override;
    void openFile();
    void loadFile (const juce::File&);
    void togglePlay();
    void updateTransportText();
    void updateFileStatusText();

    juce::File loadedFile;

    // file playback
    juce::AudioFormatManager                       formatManager;
    juce::TimeSliceThread                          readerThread { "audio-read" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::BufferingAudioSource>    bufferingSource;
    juce::AudioTransportSource                     transportSource;
    int                                            fileChannels = 0;
    double                                         fileSampleRate = 0.0;

    // scratch buffer reused per audio block
    juce::AudioBuffer<float>                       scratch;

    // the plugin itself + its real editor
    std::unique_ptr<DownslopeAudioProcessor>       downslope;
    std::unique_ptr<juce::AudioProcessorEditor>    editor;

    // UI controls
    juce::TextButton    openBtn     { "Open File..." };
    juce::TextButton    playBtn     { "Play" };
    juce::ToggleButton  loopBtn     { "Loop" };
    juce::TextButton    deviceBtn   { "Audio Settings..." };
    juce::Label         fileNameLbl;
    juce::Label         transportLbl;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::ApplicationProperties appProperties;
    bool paramsDirty = false;

    void saveAll();
    void savePluginState();
    void saveAudioDeviceState();
    void rememberFile (const juce::File&);
    juce::File recallLastFile();

    // ValueTree::Listener — used to know when plugin params change so we can persist them.
    void valueTreePropertyChanged   (juce::ValueTree&, const juce::Identifier&) override { paramsDirty = true; }
    void valueTreeChildAdded        (juce::ValueTree&, juce::ValueTree&) override        { paramsDirty = true; }
    void valueTreeChildRemoved      (juce::ValueTree&, juce::ValueTree&, int) override   { paramsDirty = true; }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                {}
    void valueTreeParentChanged     (juce::ValueTree&) override                          {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
