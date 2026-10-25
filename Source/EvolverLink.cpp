#include "EvolverLink.h"

EvolverLink::EvolverLink() : juce::Thread ("EvoCtrl MIDI out")
{
    startThread();
}

EvolverLink::~EvolverLink()
{
    stopThread (2000);
    close();
}

template <typename Device>
static juce::String portError (const juce::String& direction, const juce::String& id)
{
    juce::String name = id;
    for (const auto& d : Device::getAvailableDevices())
        if (d.identifier == id)
            name = d.name;

    return "Cannot open MIDI " + direction + " \"" + name + "\". "
           "It is probably already in use by Ableton Live (Windows opens MIDI ports exclusively). "
           "In Live: Preferences > Link, Tempo & MIDI > turn off Track and Remote for this port "
           "(input AND output), then retry - or switch to \"Via Ableton\" mode in Setup.";
}

juce::String EvolverLink::open (const juce::String& inputId, const juce::String& outputId)
{
    close();
    std::lock_guard lock (portLock);
    juce::StringArray errors;

    if (outputId.isNotEmpty())
    {
        output = juce::MidiOutput::openDevice (outputId);
        if (output == nullptr)
            errors.add (portError<juce::MidiOutput> ("output", outputId));
    }

    if (inputId.isNotEmpty())
    {
        input = juce::MidiInput::openDevice (inputId, this);
        if (input != nullptr)
            input->start();
        else
            errors.add (portError<juce::MidiInput> ("input", inputId));
    }

    return errors.joinIntoString ("\n");
}

void EvolverLink::close()
{
    std::lock_guard lock (portLock);
    if (input != nullptr)
        input->stop();
    input.reset();
    output.reset();
}

bool EvolverLink::hasInput() const  { std::lock_guard lock (portLock); return input != nullptr; }
bool EvolverLink::hasOutput() const { std::lock_guard lock (portLock); return output != nullptr; }

void EvolverLink::queueSysex (evo::Bytes message)
{
    std::lock_guard lock (sysexLock);
    sysexQueue.push_back (std::move (message));
}

std::vector<evo::Parsed> EvolverLink::popReceived()
{
    std::lock_guard lock (receivedLock);
    return std::exchange (received, {});
}

juce::StringArray EvolverLink::popLog()
{
    std::lock_guard lock (logLock);
    return std::exchange (logLines, {});
}

void EvolverLink::log (const char* direction, const uint8_t* data, int size)
{
    if (! logEnabled.load())
        return;
    const auto line = juce::Time::getCurrentTime().formatted ("%H:%M:%S ") + direction + "  "
                    + juce::String::toHexString (data, size).toUpperCase();
    std::lock_guard lock (logLock);
    logLines.add (line);
    if (logLines.size() > 2000) // editor not draining (closed): keep only the most recent lines
        logLines.removeRange (0, 1000);
}

void EvolverLink::send (const evo::Bytes& bytes)
{
    log ("OUT", bytes.data(), (int) bytes.size());
    std::lock_guard lock (portLock);
    if (output != nullptr)
        output->sendMessageNow (juce::MidiMessage (bytes.data(), (int) bytes.size()));
}

void EvolverLink::run()
{
    // Sends one message, then waits intervalMs so automation bursts don't flood the Evolver.
    const auto sendPaced = [this] (const evo::Bytes& b)
    {
        send (b);
        wait (intervalMs.load());
    };

    while (! threadShouldExit())
    {
        std::optional<evo::Bytes> bulk;
        {
            std::lock_guard lock (sysexLock);
            if (! sysexQueue.empty())
            {
                bulk = std::move (sysexQueue.front());
                sysexQueue.pop_front();
            }
        }
        if (bulk)
            sendPaced (*bulk);

        if (directParams.load())
            for (int i = 0; i < evo::numParams; ++i)
                if (paramDirty[(size_t) i].exchange (false))
                    sendPaced (evo::programParam (i, paramValue[(size_t) i].load()));

        for (int s = 0; s < evo::numSeqSteps; ++s)
            if (seqDirty[(size_t) s].exchange (false))
                sendPaced (evo::seqParam (s, seqValue[(size_t) s].load()));

        // shortcut: 1 ms polling instead of signalling from the audio thread (no RT-safe event in JUCE); fine for MIDI rates.
        wait (1);
    }
}

void EvolverLink::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m)
{
    if (m.isMidiClock() || m.isActiveSense())
        return;
    log ("IN ", m.getRawData(), m.getRawDataSize());
    if (! m.isSysEx())
        return;

    if (auto parsed = evo::parse (m.getSysExData(), (size_t) m.getSysExDataSize()))
    {
        {
            std::lock_guard lock (receivedLock);
            received.push_back (std::move (*parsed));
        }
        if (onReceived)
            onReceived();
    }
}
