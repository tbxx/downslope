#pragma once

#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

class MainComponent : public juce::AudioAppComponent,
                      public juce::ChangeListener,
                      private juce::Timer
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
