#include "EvolverLink.h"

#if JUCE_MAC
 #include <CoreMIDI/CoreMIDI.h>

static juce::String macString (MIDIObjectRef obj, CFStringRef property)
{
    CFStringRef s = nullptr;
    if (MIDIObjectGetStringProperty (obj, property, &s) != noErr || s == nullptr)
        return {};
    const auto result = juce::String::fromCFString (s);
    CFRelease (s);
    return result;
}

// Finds the CoreMIDI destination behind a JUCE output: by unique ID (JUCE's identifier is built from
// the endpoint/entity/device IDs), else by name.
static MIDIEndpointRef findMacDestination (const juce::MidiDeviceInfo& info)
{
    const auto tokens = juce::StringArray::fromTokens (info.identifier, " ,", {});
    MIDIEndpointRef byName = 0;

    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i)
    {
        const auto dest = MIDIGetDestination (i);
        MIDIEntityRef entity = 0;
        MIDIEndpointGetEntity (dest, &entity);

        for (const MIDIObjectRef obj : { (MIDIObjectRef) dest, (MIDIObjectRef) entity })
        {
            SInt32 id = 0;
            if (obj != 0 && MIDIObjectGetIntegerProperty (obj, kMIDIPropertyUniqueID, &id) == noErr && tokens.contains (juce::String (id)))
                return dest;
        }
        if (byName == 0 && macString (dest, kMIDIPropertyDisplayName) == info.name)
            byName = dest;
    }
    return byName;
}
#endif

EvolverLink::EvolverLink() : juce::Thread ("EvoCtrl MIDI out")
{
   #if JUCE_MAC
    MIDIClientRef client = 0;
    MIDIPortRef port = 0;
    if (MIDIClientCreate (CFSTR ("EvoCtrl"), nullptr, nullptr, &client) == noErr
        && MIDIOutputPortCreate (client, CFSTR ("EvoCtrl out"), &port) == noErr)
    {
        macClient = client;
        macPort = port;
    }
   #endif
    startThread();
}

EvolverLink::~EvolverLink()
{
    stopThread (2000);
    close();
   #if JUCE_MAC
    if (macClient != 0)
        MIDIClientDispose (macClient);
   #endif
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
       #if JUCE_MAC
        else
            for (const auto& d : juce::MidiOutput::getAvailableDevices())
                if (d.identifier == outputId)
                    macDest = findMacDestination (d);
       #endif
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
   #if JUCE_MAC
    macDest = 0;
   #endif
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

   #if JUCE_MAC
    if (macPort != 0 && macDest != 0)
    {
        alignas (MIDIPacketList) Byte buffer[1024]; // largest message is a 226-byte dump
        auto* list = reinterpret_cast<MIDIPacketList*> (buffer);
       #pragma clang diagnostic push
       #pragma clang diagnostic ignored "-Wdeprecated-declarations" // the legacy API is the point
        auto* packet = MIDIPacketListAdd (list, sizeof (buffer), MIDIPacketListInit (list), 0, bytes.size(), bytes.data());
        if (packet != nullptr && MIDISend (macPort, macDest, list) == noErr)
            return;
       #pragma clang diagnostic pop
    }
   #endif
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

    if (m.isProgramChange()) // the Evolver sends one when a program is selected on it (MIDI Xmit ALL or pro, p.17)
    {
        {
            std::lock_guard lock (receivedLock);
            received.push_back ({ evo::Parsed::Kind::ProgramChange, 0, m.getProgramChangeNumber() });
        }
        if (onReceived)
            onReceived();
        return;
    }
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
