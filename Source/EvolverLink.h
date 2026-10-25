#pragma once

#include "EvolverSysex.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <deque>
#include <mutex>

// Direct MIDI ports to the Evolver (bypassing the host, which does not pass SysEx reliably).
//
// Threads:
//  - queueParam/queueSeq: any thread incl. audio (atomics only). Same param queued twice = only the latest value is sent.
//  - queueSysex: message thread (dumps/requests).
//  - sender thread: drains the queues, one message every `intervalMs`.
//  - MIDI input thread: parses SysEx, stores it, calls onReceived (keep it cheap, e.g. triggerAsyncUpdate).
class EvolverLink : private juce::Thread, private juce::MidiInputCallback
{
public:
    EvolverLink();
    ~EvolverLink() override;

    // Returns an empty string on success, otherwise a user-facing error. Empty id = leave that direction closed.
    juce::String open (const juce::String& inputId, const juce::String& outputId);
    void close();
    bool hasInput() const;
    bool hasOutput() const;

    void queueParam (int index, int value) noexcept
    {
        paramValue[(size_t) index].store (value);
        paramDirty[(size_t) index].store (true);
    }

    void queueSeq (int step, int value) noexcept
    {
        seqValue[(size_t) step].store (value);
        seqDirty[(size_t) step].store (true);
    }

    void queueSysex (evo::Bytes message);

    // "Via Ableton" fallback: the audio thread drains param changes instead of the sender thread.
    template <typename Fn>
    void drainParams (int budget, Fn&& fn) noexcept
    {
        for (int n = 0; n < evo::numParams && budget > 0; ++n)
        {
            const auto i = (size_t) (drainStart++ % evo::numParams);
            if (paramDirty[i].exchange (false))
            {
                fn ((int) i, paramValue[i].load());
                --budget;
            }
        }
    }

    std::vector<evo::Parsed> popReceived();

    // MIDI monitor: hex lines of what goes through the direct ports (not the audio-thread CC fallback).
    std::atomic<bool> logEnabled { false };
    juce::StringArray popLog();

    std::atomic<int> intervalMs { 1 };
    std::atomic<bool> directParams { true };
    std::function<void()> onReceived;

private:
    void run() override;
    void send (const evo::Bytes&);
    void log (const char* direction, const uint8_t* data, int size);
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;

    std::array<std::atomic<int>, evo::numParams> paramValue {};
    std::array<std::atomic<bool>, evo::numParams> paramDirty {};
    std::array<std::atomic<int>, evo::numSeqSteps> seqValue {};
    std::array<std::atomic<bool>, evo::numSeqSteps> seqDirty {};
    unsigned drainStart = 0;

    mutable std::mutex portLock;
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;

    std::mutex sysexLock;
    std::deque<evo::Bytes> sysexQueue;

    std::mutex receivedLock;
    std::vector<evo::Parsed> received;

    std::mutex logLock;
    juce::StringArray logLines;

   #if JUCE_MAC
    // JUCE 8 sends through CoreMIDI's MIDI 2.0 API, which splits SysEx into 6-byte chunks; the Evolver
    // (behind a MOTU UltraLite, verified) misreads split messages, so on macOS each message goes out
    // as one legacy CoreMIDI packet instead. Refs are MIDIClientRef / MIDIPortRef / MIDIEndpointRef.
    std::uint32_t macClient = 0, macPort = 0, macDest = 0;
   #endif
};
