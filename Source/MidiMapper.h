#pragma once

#include <atomic>
#include <cstdint>
#include <optional>

enum class LooperCommand : std::uint8_t
{
    Record = 0,
    PlayStop,
    Overdub,
    Undo,
    Clear
};

inline constexpr int kNumLooperCommands = 5;

enum class MidiMessageType : std::uint8_t
{
    Unknown = 0,
    Note,
    ControlChange,
    ProgramChange
};

struct IncomingMidi
{
    MidiMessageType type = MidiMessageType::Unknown;
    int channel = 1;
    int number = 0;
    int value = 0;
};

struct MidiBinding
{
    bool assigned = false;
    MidiMessageType type = MidiMessageType::Note;
    int channel = 1;
    int number = 0;
    int value = 0;
};

class MidiMapper
{
public:
    static const char* commandName (LooperCommand command) noexcept;
    static const char* typeName (MidiMessageType type) noexcept;

    std::optional<LooperCommand> process (const IncomingMidi& message) noexcept;

    void startLearn (LooperCommand command) noexcept;
    void cancelLearn() noexcept;
    bool isLearning() const noexcept;
    std::optional<LooperCommand> learnTarget() const noexcept;

    MidiBinding getBinding (LooperCommand command) const noexcept;
    void setBinding (LooperCommand command, const MidiBinding& binding) noexcept;

    MidiMessageType lastType() const noexcept { return static_cast<MidiMessageType> (lastType_.load (std::memory_order_relaxed)); }
    int lastChannel() const noexcept { return lastChannel_.load (std::memory_order_relaxed); }
    int lastNumber() const noexcept { return lastNumber_.load (std::memory_order_relaxed); }
    int lastValue() const noexcept { return lastValue_.load (std::memory_order_relaxed); }
    std::uint32_t lastSequence() const noexcept { return lastSequence_.load (std::memory_order_relaxed); }

private:
    bool isNoteRelease (const IncomingMidi& message) const noexcept;
    bool isLearnCandidate (const IncomingMidi& message) const noexcept;
    bool sameAddress (const IncomingMidi& a, const IncomingMidi& b) const noexcept;
    bool sameIdentity (const IncomingMidi& a, const IncomingMidi& b) const noexcept;
    bool sameIdentity (const MidiBinding& binding, const IncomingMidi& message) const noexcept;
    bool matches (const MidiBinding& binding, const IncomingMidi& message) const noexcept;
    bool shouldIgnoreStaleLearnMessage (const IncomingMidi& message) noexcept;
    void remember (const IncomingMidi& message) noexcept;
    void assignLearned (const IncomingMidi& message) noexcept;
    void clearConflicts (int keepIndex, const MidiBinding& binding) noexcept;

    MidiBinding loadBinding (int index) const noexcept;
    void storeBinding (int index, const MidiBinding& binding) noexcept;

    std::atomic<std::uint32_t> packedBindings_[kNumLooperCommands] {};
    std::atomic<int> learnTarget_ { -1 };
    std::atomic<bool> staleArmed_ { false };
    std::atomic<int> staleType_ { 0 };
    std::atomic<int> staleChannel_ { 0 };
    std::atomic<int> staleNumber_ { 0 };
    std::atomic<int> staleValue_ { 0 };

    std::atomic<int> lastType_ { 0 };
    std::atomic<int> lastChannel_ { 0 };
    std::atomic<int> lastNumber_ { 0 };
    std::atomic<int> lastValue_ { 0 };
    std::atomic<std::uint32_t> lastSequence_ { 0 };

    std::atomic<int> lastPressType_ { 0 };
    std::atomic<int> lastPressChannel_ { 0 };
    std::atomic<int> lastPressNumber_ { 0 };
    std::atomic<int> lastPressValue_ { 0 };
    std::atomic<std::uint32_t> lastPressSequence_ { 0 };
};
