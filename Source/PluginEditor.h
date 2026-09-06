#pragma once

#include "PluginProcessor.h"

class EasyLooperEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit EasyLooperEditor (EasyLooperProcessor&);
    ~EasyLooperEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutColumn (juce::Rectangle<int> area);

    EasyLooperProcessor& processor_;

    juce::TextButton recordButton_ { "RECORD" };
    juce::TextButton playStopButton_ { "PLAY / STOP" };
    juce::TextButton overdubButton_ { "OVERDUB" };
    juce::TextButton undoButton_ { "UNDO" };
    juce::TextButton clearButton_ { "CLEAR" };

    juce::Label statusLabel_;
    juce::Label lengthLabel_;
    juce::Label midiTitleLabel_;
    juce::Label midiTypeLabel_;
    juce::Label midiChannelLabel_;
    juce::Label midiNumberLabel_;
    juce::Label midiValueLabel_;
    juce::Label learnStatusLabel_;
    juce::Label versionLabel_;

    juce::TextButton learnRecordButton_ { "Learn Record" };
    juce::TextButton learnPlayStopButton_ { "Learn Play/Stop" };
    juce::TextButton learnOverdubButton_ { "Learn Overdub" };
    juce::TextButton learnUndoButton_ { "Learn Undo" };
    juce::TextButton learnClearButton_ { "Learn Clear" };

    std::uint32_t lastMidiSeq_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EasyLooperEditor)
};
