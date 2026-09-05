#include "Looper.h"
#include "MidiMapper.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    int gFailures = 0;

    void expect (bool condition, const char* name)
    {
        if (! condition)
        {
            std::cerr << "FAIL: " << name << '\n';
            ++gFailures;
        }
        else
        {
            std::cout << "PASS: " << name << '\n';
        }
    }

    struct Buffers
    {
        std::vector<float> inL, inR, outL, outR;

        explicit Buffers (int n, float left = 0.0f, float right = 0.0f)
            : inL (static_cast<std::size_t> (n), left),
              inR (static_cast<std::size_t> (n), right),
              outL (static_cast<std::size_t> (n), 0.0f),
              outR (static_cast<std::size_t> (n), 0.0f)
        {
        }

        void process (Looper& looper)
        {
            const float* in[2] { inL.data(), inR.data() };
            float* out[2] { outL.data(), outR.data() };
            looper.process (in, out, static_cast<int> (inL.size()));
        }
    };
}

int main()
{
    Looper looper;
    looper.prepare (44100.0, 64, 2, 8.0);

    expect (looper.getState() == Looper::State::Empty, "starts empty");

    looper.handleCommand (Looper::Command::Record);
    expect (looper.getState() == Looper::State::Recording, "Empty -> Recording");

    Buffers rec (64, 0.5f, 0.25f);
    rec.process (looper);
    rec.process (looper);

    looper.handleCommand (Looper::Command::Record);
    expect (looper.getState() == Looper::State::Playing, "Recording -> Playing");
    expect (looper.getLoopLengthSamples() == 128, "loop length equals recorded samples");

    const int recordedLength = looper.getLoopLengthSamples();

    looper.handleCommand (Looper::Command::Overdub);
    expect (looper.getState() == Looper::State::Overdubbing, "Playing -> Overdubbing");

    Buffers od (64, 0.1f, 0.1f);
    od.process (looper);
    expect (looper.getLoopLengthSamples() == recordedLength, "overdub does not change loop length");

    looper.handleCommand (Looper::Command::Overdub);
    expect (looper.getState() == Looper::State::Playing, "Overdubbing -> Playing");
    expect (looper.getLoopLengthSamples() == recordedLength, "loop length still unchanged after overdub");

    const int undoBefore = looper.getUndoCount();
    expect (undoBefore == 1, "undo stack has the overdub snapshot");

    looper.handleCommand (Looper::Command::Undo);
    expect (looper.getUndoCount() == 0, "undo consumes one snapshot");
    expect (looper.getLoopLengthSamples() == recordedLength, "undo keeps loop length");

    Buffers afterUndo (128);
    afterUndo.process (looper);
    expect (std::fabs (afterUndo.outL[0] - 0.5f) < 0.0001f, "undo restored original left sample at playhead");
    expect (std::fabs (afterUndo.outR[0] - 0.25f) < 0.0001f, "undo restored original right sample at playhead");

    looper.handleCommand (Looper::Command::PlayStop);
    expect (looper.getState() == Looper::State::Stopped, "Playing -> Stopped");

    looper.handleCommand (Looper::Command::PlayStop);
    expect (looper.getState() == Looper::State::Playing, "Stopped -> Playing");

    looper.handleCommand (Looper::Command::Clear);
    expect (looper.getState() == Looper::State::Empty, "Playing -> Empty via Clear");
    expect (looper.getLoopLengthSamples() == 0, "clear zeros loop length");

    looper.handleCommand (Looper::Command::Record);
    Buffers rec2 (32, 0.3f, 0.3f);
    rec2.process (looper);
    looper.handleCommand (Looper::Command::Record);
    looper.handleCommand (Looper::Command::Clear);
    expect (looper.getState() == Looper::State::Empty, "Clear always returns to Empty");

    MidiMapper mapper;
    IncomingMidi noteOn { MidiMessageType::Note, 3, 40, 100 };
    IncomingMidi noteOff { MidiMessageType::Note, 3, 40, 0 };
    IncomingMidi ccOn { MidiMessageType::ControlChange, 1, 64, 127 };
    IncomingMidi ccOff { MidiMessageType::ControlChange, 1, 64, 0 };

    mapper.startLearn (LooperCommand::Record);
    expect (! mapper.process (noteOff).has_value(), "learn ignores note release");
    expect (! mapper.process (noteOn).has_value(), "learn consumes the press and does not fire a command");
    expect (! mapper.isLearning(), "learn finishes after a press");

    const auto recordCmd = mapper.process (noteOn);
    expect (recordCmd.has_value() && *recordCmd == LooperCommand::Record, "learned note press triggers Record");
    expect (! mapper.process (noteOff).has_value(), "note release does not retrigger");

    mapper.startLearn (LooperCommand::Overdub);
    expect (! mapper.process (ccOn).has_value(), "learn CC press");
    const auto overdubCmd = mapper.process (ccOn);
    expect (overdubCmd.has_value() && *overdubCmd == LooperCommand::Overdub, "learned CC press triggers Overdub");
    expect (! mapper.process (ccOff).has_value(), "CC value 0 does not retrigger");

    if (gFailures != 0)
    {
        std::cerr << gFailures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
