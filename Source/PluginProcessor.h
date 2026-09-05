#pragma once

#include "Looper.h"
#include "MidiMapper.h"

#include <juce_audio_processors/juce_audio_processors.h>

class EasyLooperProcessor final : public juce::AudioProcessor
{
public:
    EasyLooperProcessor();
    ~EasyLooperProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Easy Looper"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    Looper& getLooper() noexcept { return looper_; }
    MidiMapper& getMidiMapper() noexcept { return midiMapper_; }

    void requestCommand (Looper::Command command) noexcept { looper_.submitCommand (command); }
    void requestLearn (LooperCommand command) noexcept { midiMapper_.startLearn (command); }

private:
    static IncomingMidi toIncomingMidi (const juce::MidiMessage& message);
    static Looper::Command toEngineCommand (LooperCommand command) noexcept;

    Looper looper_;
    MidiMapper midiMapper_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EasyLooperProcessor)
};
