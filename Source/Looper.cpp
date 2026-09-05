#include "Looper.h"

#include <algorithm>
#include <cmath>

const char* Looper::stateName (State state) noexcept
{
    switch (state)
    {
        case State::Empty:       return "EMPTY";
        case State::Recording:   return "RECORDING";
        case State::Playing:     return "PLAYING";
        case State::Stopped:     return "STOPPED";
        case State::Overdubbing: return "OVERDUBBING";
    }
    return "EMPTY";
}

void Looper::prepare (double sampleRate, int, int numChannels, double maxSeconds)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    numChannels_ = std::max (1, numChannels);
    maxSamples_ = std::max (1, static_cast<int> (std::ceil (sampleRate_ * maxSeconds)));

    loop_.assign (static_cast<std::size_t> (numChannels_), std::vector<float> (static_cast<std::size_t> (maxSamples_), 0.0f));

    for (int u = 0; u < kMaxUndo; ++u)
        undo_[u].assign (static_cast<std::size_t> (numChannels_), std::vector<float> (static_cast<std::size_t> (maxSamples_), 0.0f));

    reset();
}

void Looper::reset() noexcept
{
    state_.store (State::Empty, std::memory_order_relaxed);
    loopLength_.store (0, std::memory_order_relaxed);
    pending_.store (Command::None, std::memory_order_relaxed);
    writePos_ = 0;
    playPos_ = 0;
    undoWrite_ = 0;
    undoCount_ = 0;
}

void Looper::submitCommand (Command command) noexcept
{
    pending_.store (command, std::memory_order_relaxed);
}

double Looper::getLoopLengthSeconds() const noexcept
{
    return static_cast<double> (getLoopLengthSamples()) / sampleRate_;
}

void Looper::passThrough (const float* const* input, float* const* output, int i) const noexcept
{
    for (int c = 0; c < numChannels_; ++c)
        output[c][i] = input[c][i];
}

void Looper::copyLoop (std::vector<std::vector<float>>& dest) noexcept
{
    const int length = loopLength_.load (std::memory_order_relaxed);
    const int n = std::min (length, maxSamples_);

    for (int c = 0; c < numChannels_; ++c)
        std::copy_n (loop_[static_cast<std::size_t> (c)].data(), static_cast<std::size_t> (n), dest[static_cast<std::size_t> (c)].data());
}

void Looper::restoreLoop (const std::vector<std::vector<float>>& src) noexcept
{
    const int length = loopLength_.load (std::memory_order_relaxed);
    const int n = std::min (length, maxSamples_);

    for (int c = 0; c < numChannels_; ++c)
        std::copy_n (src[static_cast<std::size_t> (c)].data(), static_cast<std::size_t> (n), loop_[static_cast<std::size_t> (c)].data());
}

void Looper::snapshotForUndo() noexcept
{
    copyLoop (undo_[undoWrite_]);
    undoWrite_ = (undoWrite_ + 1) % kMaxUndo;

    if (undoCount_ < kMaxUndo)
        ++undoCount_;
}

void Looper::applySpliceFade() noexcept
{
    const int length = loopLength_.load (std::memory_order_relaxed);

    if (length < 4)
        return;

    const int fade = std::min (kSpliceFadeSamples, length / 4);

    for (int i = 0; i < fade; ++i)
    {
        const float inGain = static_cast<float> (i) / static_cast<float> (fade);
        const float outGain = 1.0f - static_cast<float> (i) / static_cast<float> (fade);

        for (int c = 0; c < numChannels_; ++c)
        {
            loop_[static_cast<std::size_t> (c)][static_cast<std::size_t> (i)] *= inGain;
            loop_[static_cast<std::size_t> (c)][static_cast<std::size_t> (length - 1 - i)] *= outGain;
        }
    }
}

void Looper::handleCommand (Command command) noexcept
{
    const auto state = state_.load (std::memory_order_relaxed);

    switch (command)
    {
        case Command::None:
            break;

        case Command::Record:
            if (state == State::Empty)
            {
                writePos_ = 0;
                playPos_ = 0;
                loopLength_.store (0, std::memory_order_relaxed);
                state_.store (State::Recording, std::memory_order_relaxed);
            }
            else if (state == State::Recording)
            {
                if (writePos_ < 2)
                {
                    writePos_ = 0;
                    loopLength_.store (0, std::memory_order_relaxed);
                    state_.store (State::Empty, std::memory_order_relaxed);
                    break;
                }

                loopLength_.store (writePos_, std::memory_order_relaxed);
                applySpliceFade();
                playPos_ = 0;
                state_.store (State::Playing, std::memory_order_relaxed);
            }
            break;

        case Command::PlayStop:
            if (state == State::Playing || state == State::Overdubbing)
            {
                state_.store (State::Stopped, std::memory_order_relaxed);
            }
            else if (state == State::Stopped && loopLength_.load (std::memory_order_relaxed) > 0)
            {
                state_.store (State::Playing, std::memory_order_relaxed);
            }
            break;

        case Command::Overdub:
            if (state == State::Playing)
            {
                snapshotForUndo();
                state_.store (State::Overdubbing, std::memory_order_relaxed);
            }
            else if (state == State::Overdubbing)
            {
                state_.store (State::Playing, std::memory_order_relaxed);
            }
            break;

        case Command::Undo:
            if (undoCount_ > 0 && loopLength_.load (std::memory_order_relaxed) > 0)
            {
                undoWrite_ = (undoWrite_ + kMaxUndo - 1) % kMaxUndo;
                restoreLoop (undo_[undoWrite_]);
                --undoCount_;

                if (state == State::Overdubbing)
                    state_.store (State::Playing, std::memory_order_relaxed);
            }
            break;

        case Command::Clear:
            reset();
            break;
    }
}

void Looper::process (const float* const* input, float* const* output, int numSamples) noexcept
{
    const auto pending = pending_.exchange (Command::None, std::memory_order_relaxed);

    if (pending != Command::None)
        handleCommand (pending);

    if (numSamples <= 0 || loop_.empty())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto state = state_.load (std::memory_order_relaxed);

        switch (state)
        {
            case State::Empty:
            case State::Stopped:
                passThrough (input, output, i);
                break;

            case State::Recording:
            {
                if (writePos_ < maxSamples_)
                {
                    for (int c = 0; c < numChannels_; ++c)
                        loop_[static_cast<std::size_t> (c)][static_cast<std::size_t> (writePos_)] = input[c][i];

                    ++writePos_;
                }
                else
                {
                    loopLength_.store (maxSamples_, std::memory_order_relaxed);
                    applySpliceFade();
                    playPos_ = 0;
                    state_.store (State::Playing, std::memory_order_relaxed);
                }

                passThrough (input, output, i);
                break;
            }

            case State::Playing:
            {
                const int length = loopLength_.load (std::memory_order_relaxed);

                if (length <= 0)
                {
                    passThrough (input, output, i);
                    break;
                }

                if (playPos_ >= length)
                    playPos_ = 0;

                for (int c = 0; c < numChannels_; ++c)
                    output[c][i] = input[c][i] + loop_[static_cast<std::size_t> (c)][static_cast<std::size_t> (playPos_)];

                ++playPos_;
                if (playPos_ >= length)
                    playPos_ = 0;

                break;
            }

            case State::Overdubbing:
            {
                const int length = loopLength_.load (std::memory_order_relaxed);

                if (length <= 0)
                {
                    passThrough (input, output, i);
                    break;
                }

                if (playPos_ >= length)
                    playPos_ = 0;

                for (int c = 0; c < numChannels_; ++c)
                {
                    const float mixed = loop_[static_cast<std::size_t> (c)][static_cast<std::size_t> (playPos_)] + input[c][i];
                    loop_[static_cast<std::size_t> (c)][static_cast<std::size_t> (playPos_)] = mixed;
                    output[c][i] = mixed;
                }

                ++playPos_;
                if (playPos_ >= length)
                    playPos_ = 0;

                break;
            }
        }
    }
}
