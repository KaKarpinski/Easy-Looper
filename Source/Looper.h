#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

class Looper
{
public:
    enum class State : std::uint8_t
    {
        Empty = 0,
        Recording,
        Playing,
        Stopped,
        Overdubbing
    };

    enum class Command : std::uint8_t
    {
        None = 0,
        Record,
        PlayStop,
        Overdub,
        Undo,
        Clear
    };

    static const char* stateName (State state) noexcept;

    void prepare (double sampleRate, int samplesPerBlock, int numChannels, double maxSeconds = 60.0);
    void reset() noexcept;

    void process (const float* const* input, float* const* output, int numSamples) noexcept;

    void submitCommand (Command command) noexcept;
    void handleCommand (Command command) noexcept;

    State getState() const noexcept { return state_.load (std::memory_order_relaxed); }
    int getLoopLengthSamples() const noexcept { return loopLength_.load (std::memory_order_relaxed); }
    double getLoopLengthSeconds() const noexcept;
    int getUndoCount() const noexcept { return undoCount_; }
    double getSampleRate() const noexcept { return sampleRate_; }

private:
    static constexpr int kMaxUndo = 8;
    static constexpr int kSpliceFadeSamples = 64;

    void snapshotForUndo() noexcept;
    void applySpliceFade() noexcept;
    void copyLoop (std::vector<std::vector<float>>& dest) noexcept;
    void restoreLoop (const std::vector<std::vector<float>>& src) noexcept;
    void passThrough (const float* const* input, float* const* output, int i) const noexcept;

    double sampleRate_ = 44100.0;
    int numChannels_ = 2;
    int maxSamples_ = 0;

    std::vector<std::vector<float>> loop_;
    std::vector<std::vector<float>> undo_[kMaxUndo];

    std::atomic<State> state_ { State::Empty };
    std::atomic<int> loopLength_ { 0 };
    std::atomic<Command> pending_ { Command::None };

    int writePos_ = 0;
    int playPos_ = 0;
    int undoWrite_ = 0;
    int undoCount_ = 0;
};
