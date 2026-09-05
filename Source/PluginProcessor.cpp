#include "PluginProcessor.h"
#include "PluginEditor.h"

EasyLooperProcessor::EasyLooperProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Domyślnie brak mapowania MIDI — pedały przypisujesz przez Learn w GUI.
    // Jeśli znasz komunikaty Chocolate, możesz je wpisać tutaj, np.:
    //
    // MidiBinding record;
    // record.assigned = true;
    // record.type = MidiMessageType::Note;          // albo ControlChange / ProgramChange
    // record.channel = 1;                           // 1–16, taki jaki pokaże LAST MIDI
    // record.number = 36;                           // note albo CC number
    // record.value = 127;
    // midiMapper_.setBinding (LooperCommand::Record, record);
}

void EasyLooperProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = juce::jmax (1, getTotalNumInputChannels(), getTotalNumOutputChannels());
    looper_.prepare (sampleRate, samplesPerBlock, channels, 60.0);
}

void EasyLooperProcessor::releaseResources()
{
}

bool EasyLooperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut.isDisabled() || mainIn.isDisabled())
        return false;

    if (mainOut != mainIn)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

IncomingMidi EasyLooperProcessor::toIncomingMidi (const juce::MidiMessage& message)
{
    IncomingMidi incoming;
    incoming.channel = message.getChannel();

    if (message.isNoteOn())
    {
        incoming.type = MidiMessageType::Note;
        incoming.number = message.getNoteNumber();
        incoming.value = message.getVelocity();
        return incoming;
    }

    if (message.isNoteOff())
    {
        incoming.type = MidiMessageType::Note;
        incoming.number = message.getNoteNumber();
        incoming.value = 0;
        return incoming;
    }

    if (message.isController())
    {
        incoming.type = MidiMessageType::ControlChange;
        incoming.number = message.getControllerNumber();
        incoming.value = message.getControllerValue();
        return incoming;
    }

    if (message.isProgramChange())
    {
        incoming.type = MidiMessageType::ProgramChange;
        incoming.number = message.getProgramChangeNumber();
        incoming.value = 0;
        return incoming;
    }

    incoming.type = MidiMessageType::Unknown;
    return incoming;
}

Looper::Command EasyLooperProcessor::toEngineCommand (LooperCommand command) noexcept
{
    switch (command)
    {
        case LooperCommand::Record:   return Looper::Command::Record;
        case LooperCommand::PlayStop: return Looper::Command::PlayStop;
        case LooperCommand::Overdub:  return Looper::Command::Overdub;
        case LooperCommand::Undo:     return Looper::Command::Undo;
        case LooperCommand::Clear:    return Looper::Command::Clear;
    }

    return Looper::Command::None;
}

void EasyLooperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (auto metadata : midi)
    {
        const auto incoming = toIncomingMidi (metadata.getMessage());
        if (const auto command = midiMapper_.process (incoming))
            looper_.handleCommand (toEngineCommand (*command));
    }

    midi.clear();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const float* inPtrs[8] {};
    float* outPtrs[8] {};
    const int looperChannels = juce::jmin (numChannels, 8);

    for (int c = 0; c < looperChannels; ++c)
    {
        inPtrs[c] = buffer.getReadPointer (c);
        outPtrs[c] = buffer.getWritePointer (c);
    }

    looper_.process (inPtrs, outPtrs, numSamples);

    for (int c = looperChannels; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);
}

juce::AudioProcessorEditor* EasyLooperProcessor::createEditor()
{
    return new EasyLooperEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EasyLooperProcessor();
}
