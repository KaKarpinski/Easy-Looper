#include "PluginEditor.h"

namespace
{
    void styleMainButton (juce::TextButton& button, juce::Colour colour)
    {
        button.setColour (juce::TextButton::buttonColourId, colour);
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    }
}

EasyLooperEditor::EasyLooperEditor (EasyLooperProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor_ (p)
{
    setSize (420, 720);

    styleMainButton (recordButton_, juce::Colour (0xffb42318));
    styleMainButton (playStopButton_, juce::Colour (0xff175cd3));
    styleMainButton (overdubButton_, juce::Colour (0xffb54708));
    styleMainButton (undoButton_, juce::Colour (0xff344054));
    styleMainButton (clearButton_, juce::Colour (0xff1d2939));

    addAndMakeVisible (recordButton_);
    addAndMakeVisible (playStopButton_);
    addAndMakeVisible (overdubButton_);
    addAndMakeVisible (undoButton_);
    addAndMakeVisible (clearButton_);

    recordButton_.onClick = [this] { processor_.requestCommand (Looper::Command::Record); };
    playStopButton_.onClick = [this] { processor_.requestCommand (Looper::Command::PlayStop); };
    overdubButton_.onClick = [this] { processor_.requestCommand (Looper::Command::Overdub); };
    undoButton_.onClick = [this] { processor_.requestCommand (Looper::Command::Undo); };
    clearButton_.onClick = [this] { processor_.requestCommand (Looper::Command::Clear); };

    const auto setupLabel = [this] (juce::Label& label, int fontHeight, juce::Justification justification)
    {
        label.setJustificationType (justification);
        label.setColour (juce::Label::textColourId, juce::Colours::white);
        label.setFont (juce::FontOptions (static_cast<float> (fontHeight), juce::Font::bold));
        addAndMakeVisible (label);
    };

    setupLabel (statusLabel_, 22, juce::Justification::centred);
    setupLabel (lengthLabel_, 16, juce::Justification::centred);
    setupLabel (midiTitleLabel_, 16, juce::Justification::centredLeft);
    setupLabel (midiTypeLabel_, 15, juce::Justification::centredLeft);
    setupLabel (midiChannelLabel_, 15, juce::Justification::centredLeft);
    setupLabel (midiNumberLabel_, 15, juce::Justification::centredLeft);
    setupLabel (midiValueLabel_, 15, juce::Justification::centredLeft);
    setupLabel (learnStatusLabel_, 14, juce::Justification::centred);

    midiTitleLabel_.setText ("LAST MIDI", juce::dontSendNotification);

    addAndMakeVisible (learnRecordButton_);
    addAndMakeVisible (learnPlayStopButton_);
    addAndMakeVisible (learnOverdubButton_);
    addAndMakeVisible (learnUndoButton_);
    addAndMakeVisible (learnClearButton_);

    learnRecordButton_.onClick = [this] { processor_.requestLearn (LooperCommand::Record); };
    learnPlayStopButton_.onClick = [this] { processor_.requestLearn (LooperCommand::PlayStop); };
    learnOverdubButton_.onClick = [this] { processor_.requestLearn (LooperCommand::Overdub); };
    learnUndoButton_.onClick = [this] { processor_.requestLearn (LooperCommand::Undo); };
    learnClearButton_.onClick = [this] { processor_.requestLearn (LooperCommand::Clear); };

    startTimerHz (20);
}

void EasyLooperEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101828));
}

void EasyLooperEditor::layoutColumn (juce::Rectangle<int> area)
{
    auto place = [&area] (juce::Component& component, int height, int gap = 8)
    {
        component.setBounds (area.removeFromTop (height));
        area.removeFromTop (gap);
    };

    place (recordButton_, 52);
    place (playStopButton_, 52);
    place (overdubButton_, 52);
    place (undoButton_, 44);
    place (clearButton_, 44);
    place (statusLabel_, 32);
    place (lengthLabel_, 24, 16);
    place (midiTitleLabel_, 24);
    place (midiTypeLabel_, 22);
    place (midiChannelLabel_, 22);
    place (midiNumberLabel_, 22);
    place (midiValueLabel_, 22, 12);
    place (learnStatusLabel_, 22);
    place (learnRecordButton_, 32);
    place (learnPlayStopButton_, 32);
    place (learnOverdubButton_, 32);
    place (learnUndoButton_, 32);
    place (learnClearButton_, 32);
}

void EasyLooperEditor::resized()
{
    layoutColumn (getLocalBounds().reduced (20));
}

void EasyLooperEditor::timerCallback()
{
    auto& looper = processor_.getLooper();
    auto& mapper = processor_.getMidiMapper();

    statusLabel_.setText (Looper::stateName (looper.getState()), juce::dontSendNotification);
    lengthLabel_.setText ("LOOP LENGTH: " + juce::String (looper.getLoopLengthSeconds(), 2) + " s",
                          juce::dontSendNotification);

    const auto seq = mapper.lastSequence();
    if (seq != lastMidiSeq_ || seq == 0)
    {
        lastMidiSeq_ = seq;
        midiTypeLabel_.setText ("Type: " + juce::String (MidiMapper::typeName (mapper.lastType())), juce::dontSendNotification);
        midiChannelLabel_.setText ("Channel: " + juce::String (mapper.lastChannel()), juce::dontSendNotification);
        midiNumberLabel_.setText ("Number: " + juce::String (mapper.lastNumber()), juce::dontSendNotification);
        midiValueLabel_.setText ("Value: " + juce::String (mapper.lastValue()), juce::dontSendNotification);
    }

    if (const auto target = mapper.learnTarget())
        learnStatusLabel_.setText ("LEARNING: " + juce::String (MidiMapper::commandName (*target)) + " — press a pedal",
                                   juce::dontSendNotification);
    else
        learnStatusLabel_.setText ("MIDI LEARN: idle", juce::dontSendNotification);
}
