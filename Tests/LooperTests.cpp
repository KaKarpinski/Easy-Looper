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
    IncomingMidi noteAOn { MidiMessageType::Note, 1, 36, 100 };
    IncomingMidi noteAOff { MidiMessageType::Note, 1, 36, 0 };
    IncomingMidi noteBOn { MidiMessageType::Note, 1, 37, 100 };
    IncomingMidi noteBOff { MidiMessageType::Note, 1, 37, 0 };
    IncomingMidi noteCOn { MidiMessageType::Note, 1, 38, 110 };
    IncomingMidi ccOn { MidiMessageType::ControlChange, 1, 64, 127 };
    IncomingMidi ccOff { MidiMessageType::ControlChange, 1, 64, 0 };
    IncomingMidi pcA { MidiMessageType::ProgramChange, 1, 0, 0 };
    IncomingMidi pcB { MidiMessageType::ProgramChange, 1, 1, 0 };

    mapper.startLearn (LooperCommand::Record);
    expect (! mapper.process (noteAOff).has_value(), "learn ignores note release");
    expect (! mapper.process (noteAOn).has_value(), "learn consumes the press and does not fire a command");
    expect (! mapper.isLearning(), "learn finishes after a press");

    const auto recordCmd = mapper.process (noteAOn);
    expect (recordCmd.has_value() && *recordCmd == LooperCommand::Record, "learned note press triggers Record");
    expect (! mapper.process (noteAOff).has_value(), "note release does not retrigger");

    mapper.startLearn (LooperCommand::Overdub);
    expect (! mapper.process (noteAOn).has_value(), "second learn ignores echo of the previous pedal");
    expect (! mapper.process (noteAOn).has_value(), "repeated echoes of A still do not bind Overdub");
    expect (mapper.isLearning(), "still learning Overdub after stale echo");
    expect (mapper.getBinding (LooperCommand::Record).assigned, "Record mapping survives the echo");
    expect (! mapper.process (noteBOn).has_value(), "learn B for Overdub");
    expect (! mapper.isLearning(), "Overdub learn finishes on pedal B");

    const auto stillRecord = mapper.process (noteAOn);
    expect (stillRecord.has_value() && *stillRecord == LooperCommand::Record, "A still triggers Record after learning B");
    expect (! mapper.process (noteAOff).has_value(), "A release does not fire");
    const auto overdubCmd = mapper.process (noteBOn);
    expect (overdubCmd.has_value() && *overdubCmd == LooperCommand::Overdub, "B triggers Overdub");
    expect (! mapper.process (noteBOff).has_value(), "B release does not retrigger");

    mapper.startLearn (LooperCommand::PlayStop);
    expect (! mapper.process (ccOn).has_value(), "learn CC press for Play/Stop");
    const auto playCmd = mapper.process (ccOn);
    expect (playCmd.has_value() && *playCmd == LooperCommand::PlayStop, "learned CC press triggers Play/Stop");
    expect (! mapper.process (ccOff).has_value(), "CC value 0 does not retrigger");
    expect (mapper.process (noteAOn).value_or (LooperCommand::Clear) == LooperCommand::Record, "A still Record after third learn");
    expect (mapper.process (noteBOn).value_or (LooperCommand::Clear) == LooperCommand::Overdub, "B still Overdub after third learn");

    MidiMapper stealMapper;
    stealMapper.startLearn (LooperCommand::Record);
    stealMapper.process (noteAOn);
    stealMapper.process (noteAOff);
    stealMapper.startLearn (LooperCommand::Overdub);
    stealMapper.process (noteAOn); // stale echo
    stealMapper.process (noteAOff);
    stealMapper.process (noteAOn); // real re-press of the same pedal
    expect (! stealMapper.getBinding (LooperCommand::Record).assigned, "re-learning A on Overdub clears Record");
    expect (stealMapper.getBinding (LooperCommand::Overdub).assigned, "Overdub now owns pedal A");
    const auto stolen = stealMapper.process (noteAOn);
    expect (stolen.has_value() && *stolen == LooperCommand::Overdub, "A now triggers Overdub only");

    MidiMapper pcMapper;
    pcMapper.startLearn (LooperCommand::Record);
    pcMapper.process (pcA);
    pcMapper.startLearn (LooperCommand::Undo);
    pcMapper.process (pcA); // host echo of last PC
    expect (pcMapper.isLearning(), "PC echo does not finish the next learn");
    pcMapper.process (pcB);
    expect (pcMapper.process (pcA).value_or (LooperCommand::Clear) == LooperCommand::Record, "PC A stays Record");
    expect (pcMapper.process (pcB).value_or (LooperCommand::Clear) == LooperCommand::Undo, "PC B triggers Undo");

    MidiMapper valueMapper;
    IncomingMidi ccPadA { MidiMessageType::ControlChange, 1, 80, 1 };
    IncomingMidi ccPadB { MidiMessageType::ControlChange, 1, 80, 2 };
    valueMapper.startLearn (LooperCommand::Record);
    valueMapper.process (ccPadA);
    valueMapper.startLearn (LooperCommand::Clear);
    valueMapper.process (ccPadA); // echo of pad A
    valueMapper.process (ccPadB);
    expect (valueMapper.process (ccPadA).value_or (LooperCommand::Undo) == LooperCommand::Record, "same CC number value 1 is Record");
    expect (valueMapper.process (ccPadB).value_or (LooperCommand::Undo) == LooperCommand::Clear, "same CC number value 2 is Clear");

    MidiMapper chocolateMapper;
    IncomingMidi padA { MidiMessageType::ControlChange, 1, 20, 1 };
    IncomingMidi padB { MidiMessageType::ControlChange, 1, 20, 2 };
    IncomingMidi padC { MidiMessageType::ControlChange, 1, 20, 3 };
    IncomingMidi padD { MidiMessageType::ControlChange, 1, 20, 0 };
    IncomingMidi padEcho { MidiMessageType::ControlChange, 1, 20, 1 };
    chocolateMapper.startLearn (LooperCommand::Record);
    chocolateMapper.process (padA);
    chocolateMapper.startLearn (LooperCommand::Overdub);
    chocolateMapper.process (padEcho);
    expect (chocolateMapper.isLearning(), "same type/channel/number echo is ignored when value matches last pad");
    chocolateMapper.process (padB);
    chocolateMapper.startLearn (LooperCommand::PlayStop);
    chocolateMapper.process (padB);
    chocolateMapper.process (padC);
    chocolateMapper.startLearn (LooperCommand::Undo);
    chocolateMapper.process (padC);
    chocolateMapper.process (padD);
    expect (chocolateMapper.process (padA).value_or (LooperCommand::Clear) == LooperCommand::Record, "Chocolate pad value 1 = Record");
    expect (chocolateMapper.process (padB).value_or (LooperCommand::Clear) == LooperCommand::Overdub, "Chocolate pad value 2 = Overdub");
    expect (chocolateMapper.process (padC).value_or (LooperCommand::Clear) == LooperCommand::PlayStop, "Chocolate pad value 3 = Play/Stop");
    expect (chocolateMapper.process (padD).value_or (LooperCommand::Clear) == LooperCommand::Undo, "Chocolate pad value 0 = Undo");
    expect (chocolateMapper.getBinding (LooperCommand::Record).value == 1, "Record stores learned value");
    expect (chocolateMapper.getBinding (LooperCommand::Overdub).value == 2, "Overdub stores learned value");

    IncomingMidi sameNoteA { MidiMessageType::Note, 1, 60, 10 };
    IncomingMidi sameNoteB { MidiMessageType::Note, 1, 60, 20 };
    IncomingMidi sameNoteOff { MidiMessageType::Note, 1, 60, 0 };
    MidiMapper velocityMapper;
    velocityMapper.startLearn (LooperCommand::Record);
    velocityMapper.process (sameNoteA);
    velocityMapper.startLearn (LooperCommand::Overdub);
    velocityMapper.process (sameNoteA);
    velocityMapper.process (sameNoteOff);
    expect (velocityMapper.isLearning(), "note-off of the shared note does not bind Overdub");
    velocityMapper.process (sameNoteB);
    expect (velocityMapper.process (sameNoteA).value_or (LooperCommand::Clear) == LooperCommand::Record, "same note velocity 10 = Record");
    expect (velocityMapper.process (sameNoteB).value_or (LooperCommand::Clear) == LooperCommand::Overdub, "same note velocity 20 = Overdub");
    expect (! velocityMapper.process (sameNoteOff).has_value(), "shared note-off does not fire a command");

    expect (mapper.process (noteCOn).has_value() == false, "unmapped pedal does not fire a command");

    if (gFailures != 0)
    {
        std::cerr << gFailures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
