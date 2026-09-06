#include "MidiMapper.h"

namespace
{
    std::uint32_t packBinding (const MidiBinding& binding) noexcept
    {
        return (binding.assigned ? 1u : 0u)
             | (static_cast<std::uint32_t> (binding.type) << 1)
             | (static_cast<std::uint32_t> (binding.channel & 0xff) << 8)
             | (static_cast<std::uint32_t> (binding.number & 0xff) << 16)
             | (static_cast<std::uint32_t> (binding.value & 0xff) << 24);
    }

    MidiBinding unpackBinding (std::uint32_t packed) noexcept
    {
        MidiBinding binding;
        binding.assigned = (packed & 1u) != 0;
        binding.type = static_cast<MidiMessageType> ((packed >> 1) & 0x7f);
        binding.channel = static_cast<int> ((packed >> 8) & 0xff);
        binding.number = static_cast<int> ((packed >> 16) & 0xff);
        binding.value = static_cast<int> ((packed >> 24) & 0xff);
        return binding;
    }
}

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

MidiBinding MidiMapper::loadBinding (int index) const noexcept
{
    if (index < 0 || index >= kNumLooperCommands)
        return {};

    return unpackBinding (packedBindings_[index].load (std::memory_order_relaxed));
}

void MidiMapper::storeBinding (int index, const MidiBinding& binding) noexcept
{
    if (index < 0 || index >= kNumLooperCommands)
        return;

    packedBindings_[index].store (packBinding (binding), std::memory_order_relaxed);
}

void MidiMapper::startLearn (LooperCommand command) noexcept
{
    const bool haveLastMessage = lastSequence_.load (std::memory_order_relaxed) > 0;
    staleType_.store (lastType_.load (std::memory_order_relaxed), std::memory_order_relaxed);
    staleChannel_.store (lastChannel_.load (std::memory_order_relaxed), std::memory_order_relaxed);
    staleNumber_.store (lastNumber_.load (std::memory_order_relaxed), std::memory_order_relaxed);
    staleValue_.store (lastValue_.load (std::memory_order_relaxed), std::memory_order_relaxed);
    staleArmed_.store (haveLastMessage, std::memory_order_relaxed);
    learnTarget_.store (static_cast<int> (command), std::memory_order_relaxed);
}

void MidiMapper::cancelLearn() noexcept
{
    learnTarget_.store (-1, std::memory_order_relaxed);
    staleArmed_.store (false, std::memory_order_relaxed);
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
    return loadBinding (static_cast<int> (command));
}

void MidiMapper::setBinding (LooperCommand command, const MidiBinding& binding) noexcept
{
    storeBinding (static_cast<int> (command), binding);
}

void MidiMapper::remember (const IncomingMidi& message) noexcept
{
    lastType_.store (static_cast<int> (message.type), std::memory_order_relaxed);
    lastChannel_.store (message.channel, std::memory_order_relaxed);
    lastNumber_.store (message.number, std::memory_order_relaxed);
    lastValue_.store (message.value, std::memory_order_relaxed);
    lastSequence_.fetch_add (1, std::memory_order_relaxed);
}

bool MidiMapper::isOnOffValue (int value) const noexcept
{
    return value == 0 || value == 127;
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

bool MidiMapper::isLearnCandidate (const IncomingMidi& message) const noexcept
{
    return isPress (message);
}

bool MidiMapper::sameControl (const IncomingMidi& a, const IncomingMidi& b) const noexcept
{
    MidiBinding binding;
    binding.assigned = true;
    binding.type = a.type;
    binding.channel = a.channel;
    binding.number = a.number;
    binding.value = a.value;
    return sameControl (binding, b);
}

bool MidiMapper::sameControl (const MidiBinding& binding, const IncomingMidi& message) const noexcept
{
    if (! binding.assigned)
        return false;

    if (binding.type != message.type || binding.channel != message.channel || binding.number != message.number)
        return false;

    if (binding.type == MidiMessageType::ControlChange
        && ! isOnOffValue (binding.value)
        && ! isOnOffValue (message.value))
    {
        return binding.value == message.value;
    }

    return true;
}

bool MidiMapper::matches (const MidiBinding& binding, const IncomingMidi& message) const noexcept
{
    if (! sameControl (binding, message))
        return false;

    if (binding.type == MidiMessageType::ControlChange && binding.value == 0)
        return true;

    if (binding.type == MidiMessageType::ControlChange
        && ! isOnOffValue (binding.value)
        && ! isOnOffValue (message.value))
    {
        return message.value == binding.value;
    }

    return isPress (message);
}

bool MidiMapper::shouldIgnoreStaleLearnMessage (const IncomingMidi& message) noexcept
{
    if (! staleArmed_.load (std::memory_order_relaxed))
        return false;

    IncomingMidi stale;
    stale.type = static_cast<MidiMessageType> (staleType_.load (std::memory_order_relaxed));
    stale.channel = staleChannel_.load (std::memory_order_relaxed);
    stale.number = staleNumber_.load (std::memory_order_relaxed);
    stale.value = staleValue_.load (std::memory_order_relaxed);

    if (! sameControl (stale, message))
        return false;

    // Keep ignoring the previous pedal (FL often repeats it) until it is released.
    // A different pedal is accepted immediately by the caller.
    if (! isPress (message))
        staleArmed_.store (false, std::memory_order_relaxed);

    return true;
}

void MidiMapper::clearConflicts (int keepIndex, const MidiBinding& binding) noexcept
{
    IncomingMidi identity { binding.type, binding.channel, binding.number, binding.value };

    for (int i = 0; i < kNumLooperCommands; ++i)
    {
        if (i == keepIndex)
            continue;

        const auto existing = loadBinding (i);
        if (sameControl (existing, identity))
        {
            MidiBinding cleared;
            storeBinding (i, cleared);
        }
    }
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
    storeBinding (target, binding);
    clearConflicts (target, binding);
    learnTarget_.store (-1, std::memory_order_relaxed);
    staleArmed_.store (false, std::memory_order_relaxed);
}

std::optional<LooperCommand> MidiMapper::process (const IncomingMidi& message) noexcept
{
    if (message.type == MidiMessageType::Unknown)
        return std::nullopt;

    remember (message);

    if (isLearning())
    {
        if (shouldIgnoreStaleLearnMessage (message))
            return std::nullopt;

        if (isLearnCandidate (message))
            assignLearned (message);

        return std::nullopt;
    }

    for (int i = 0; i < kNumLooperCommands; ++i)
    {
        const auto binding = loadBinding (i);
        if (matches (binding, message))
            return static_cast<LooperCommand> (i);
    }

    return std::nullopt;
}
