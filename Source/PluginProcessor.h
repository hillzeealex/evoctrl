#pragma once

#include "EvolverLink.h"
#include "PatchLibrary.h"
#include <juce_audio_processors/juce_audio_processors.h>

class EvoCtrlProcessor : public juce::AudioProcessor,
                         private juce::AudioProcessorParameter::Listener,
                         private juce::AsyncUpdater,
                         private juce::Timer
{
public:
    enum class StatusKind { Idle, Ok, Warning, Error };
    struct Status { StatusKind kind; juce::String text; };

    EvoCtrlProcessor();
    ~EvoCtrlProcessor() override;

    // --- Everything below is message-thread only, except where noted.
    juce::String connect (const juce::String& inputId, const juce::String& outputId);
    void testConnection()                 { requestEditBuffer (false); }
    void getFromEvolver()                 { requestEditBuffer (true); }
    void sendToEvolver();
    void readProgram (int bank, int prog);
    void writeProgram (int bank, int prog);
    bool importSyx (const juce::File&);
    bool exportSyx (const juce::File&) const;

    evo::Patch capturePatch() const;
    void applyPatch (const evo::Patch&, bool sendToDevice);
    int getSeqStep (int step) const       { return seq[(size_t) step].load(); }
    void setSeqStep (int step, int value);

    juce::AudioParameterInt& param (int index) const { return *params[(size_t) index]; }

    Status status { StatusKind::Idle, "Not connected" };
    juce::String inputId, outputId, patchName;
    int bank = 0, program = 0;
    bool resendOnLoad = false;
    std::atomic<int> midiChannel { 1 };       // read on the audio thread (fallback CCs)
    std::atomic<bool> viaAbleton { false };   // read on the audio thread
    std::atomic<int> changeCounter { 0 };     // bumped when seq/name/status change so the editor can refresh
    void setViaAbleton (bool);

    EvolverLink link;
    PatchLibrary library;

    // --- juce::AudioProcessor
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EvoCtrl"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

private:
    void parameterValueChanged (int index, float normalised) override;
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void requestEditBuffer (bool apply);
    void setStatus (StatusKind, const juce::String&);
    void setParamFromDevice (int index, int value);

    std::array<juce::AudioParameterInt*, evo::numParams> params {};
    std::array<std::atomic<int>, evo::numParams> lastValue {};   // last value known on both sides: anti-echo + dedupe
    std::array<std::atomic<int>, evo::numSeqSteps> seq {};
    // The Evolver echoes every parameter it receives; values arriving this soon after we sent that
    // parameter are echoes (often of an older value mid-drag) and are ignored.
    static constexpr juce::uint32 echoWindowMs = 400;
    std::array<std::atomic<juce::uint32>, evo::numParams> sentAt {};
    std::array<juce::uint32, evo::numSeqSteps> seqSentAt {};
    std::atomic<bool> muteOutgoing { false };                    // true while applying device/state values
    bool pendingTest = false, applyNextEditBuffer = false, pendingProgramRead = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EvoCtrlProcessor)
};
