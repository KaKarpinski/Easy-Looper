#include "MidiMapper.h"

const char* MidiMapper::commandName (LooperCommand command) noexcept
{
    switch (command)
    {
        case LooperCommand::Record:   return "Record";
        case LooperCommand::PlayStop: return "Play/Stop";
        case LooperCommand::Overdub:  return "Overdub";
        case LooperCommand::Undo:     return "Undo";
        case LooperCommand::Clear:    return "Clear";
    }
    return "Record";
}

const char* MidiMapper::typeName (MidiMessageType type) noexcept
{
    switch (type)
    {
        case MidiMessageType::Note:           return "Note";
        case MidiMessageType::ControlChange:  return "CC";
        case MidiMessageType::ProgramChange:  return "PC";
        case MidiMessageType::Unknown:        return "-";
    }
    return "-";
}

void MidiMapper::startLearn (LooperCommand command) noexcept
{
    learnTarget_.store (static_cast<int> (command), std::memory_order_relaxed);
}

void MidiMapper::cancelLearn() noexcept
{
    learnTarget_.store (-1, std::memory_order_relaxed);
}

bool MidiMapper::isLearning() const noexcept
{
    return learnTarget_.load (std::memory_order_relaxed) >= 0;
}

std::optional<LooperCommand> MidiMapper::learnTarget() const noexcept
{
    const int target = learnTarget_.load (std::memory_order_relaxed);

    if (target < 0 || target >= kNumLooperCommands)
        return std::nullopt;

    return static_cast<LooperCommand> (target);
}

MidiBinding MidiMapper::getBinding (LooperCommand command) const noexcept
{
    return bindings_[static_cast<int> (command)];
}

void MidiMapper::setBinding (LooperCommand command, const MidiBinding& binding) noexcept
{
    bindings_[static_cast<int> (command)] = binding;
}

void MidiMapper::remember (const IncomingMidi& message) noexcept
{
    lastType_.store (static_cast<int> (message.type), std::memory_order_relaxed);
    lastChannel_.store (message.channel, std::memory_order_relaxed);
    lastNumber_.store (message.number, std::memory_order_relaxed);
    lastValue_.store (message.value, std::memory_order_relaxed);
    lastSequence_.fetch_add (1, std::memory_order_relaxed);
}

bool MidiMapper::isPress (const IncomingMidi& message) const noexcept
{
    switch (message.type)
    {
        case MidiMessageType::Note:
            return message.value > 0;

        case MidiMessageType::ControlChange:
            return message.value > 0;

        case MidiMessageType::ProgramChange:
            return true;

        case MidiMessageType::Unknown:
            return false;
    }
    return false;
}

bool MidiMapper::matches (const MidiBinding& binding, const IncomingMidi& message) const noexcept
{
    if (! binding.assigned)
        return false;

    if (binding.type != message.type)
        return false;

    if (binding.channel != message.channel)
        return false;

    if (binding.number != message.number)
        return false;

    return true;
}

void MidiMapper::assignLearned (const IncomingMidi& message) noexcept
{
    const int target = learnTarget_.load (std::memory_order_relaxed);

    if (target < 0 || target >= kNumLooperCommands)
        return;

    MidiBinding binding;
    binding.assigned = true;
    binding.type = message.type;
    binding.channel = message.channel;
    binding.number = message.number;
    binding.value = message.value;
    bindings_[target] = binding;
    learnTarget_.store (-1, std::memory_order_relaxed);
}

std::optional<LooperCommand> MidiMapper::process (const IncomingMidi& message) noexcept
{
    if (message.type == MidiMessageType::Unknown)
        return std::nullopt;

    remember (message);

    if (! isPress (message))
        return std::nullopt;

    if (isLearning())
    {
        assignLearned (message);
        return std::nullopt;
    }

    for (int i = 0; i < kNumLooperCommands; ++i)
    {
        if (matches (bindings_[i], message))
            return static_cast<LooperCommand> (i);
    }

    return std::nullopt;
}
