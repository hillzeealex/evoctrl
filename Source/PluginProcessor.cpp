#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String paramId (int i) { return "p" + juce::String (i).paddedLeft ('0', 3); }

EvoCtrlProcessor::EvoCtrlProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (const auto& p : evo::params)
    {
        auto* param = new juce::AudioParameterInt (
            juce::ParameterID { paramId (p.index), 1 }, p.name, p.min, p.max, p.def,
            juce::AudioParameterIntAttributes().withStringFromValueFunction (
                [&p] (int v, int) { return juce::String (evo::valueToText (p, v)); }));
        addParameter (param);
        param->addListener (this);
        params[(size_t) p.index] = param;
        lastValue[(size_t) p.index] = p.def;
    }

    link.onReceived = [this] { triggerAsyncUpdate(); };

    for (const auto& m : PatchLibrary::readSyx (wavesFolder().getChildFile ("evolver-cache.syx")))
        if (m.kind == evo::Parsed::Kind::Waveshape && m.index < evo::numWaves)
            waves[(size_t) m.index] = m.wave;
    for (const auto& m : PatchLibrary::readSyx (wavesFolder().getChildFile ("originals.syx")))
        if (m.kind == evo::Parsed::Kind::Waveshape && m.index >= evo::firstUserWave && m.index < evo::numWaves)
            originals[(size_t) (m.index - evo::firstUserWave)] = m.wave;
    recordOriginals();   // first run: whatever was read from the Evolver before any edit

    for (const auto& m : PatchLibrary::readSyx (wavesFolder().getParentDirectory().getChildFile ("program-names.syx")))
        if (m.kind == evo::Parsed::Kind::Name && m.index < 4 && m.value < 128)
        {
            programNames[(size_t) (m.index * 128 + m.value)] = m.name;
            nameKnown[(size_t) (m.index * 128 + m.value)] = true;
        }
    nameWatchdog.fn = [this]
    {
        if (std::exchange (nameRetried, ! nameRetried))
            ++nameNext;
        requestNextName();
    };

    waveWatchdog.fn = [this]   // no answer: ask once more, then skip that wave
    {
        if (std::exchange (waveRetried, ! waveRetried))
            ++waveNext;
        requestNextWave();
    };
}

EvoCtrlProcessor::~EvoCtrlProcessor()
{
    for (auto* p : params)
        p->removeListener (this);
    link.onReceived = nullptr;
    waveWatchdog.stopTimer();
    nameWatchdog.stopTimer();
    cancelPendingUpdate();
}

// Any thread (host automation arrives on the audio thread): atomics only.
void EvoCtrlProcessor::parameterValueChanged (int index, float normalised)
{
    const auto& p = evo::params[(size_t) index];
    const int v = juce::roundToInt ((float) p.min + normalised * (float) (p.max - p.min));
    if (lastValue[(size_t) index].exchange (v) == v || muteOutgoing.load())
        return;
    link.queueParam (index, v);
    sentAt[(size_t) index] = juce::Time::getMillisecondCounter();
}

void EvoCtrlProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();

    if (! viaAbleton.load())
        return; // incoming notes pass through untouched

    // Fallback: params with a documented CC go out through the host. Scaling is linear (unverified, see spec §0).
    const int channel = midiChannel.load();
    link.drainParams (32, [&] (int i, int v)
    {
        const auto& p = evo::params[(size_t) i];
        if (p.cc >= 0)
            midi.addEvent (juce::MidiMessage::controllerEvent (channel, p.cc, juce::roundToInt (v * 127.0 / p.max)), 0);
    });
}

void EvoCtrlProcessor::setViaAbleton (bool b)
{
    viaAbleton = b;
    link.directParams = ! b;
}

void EvoCtrlProcessor::setStatus (StatusKind kind, const juce::String& text)
{
    status = { kind, text };
    ++changeCounter;
}

juce::String EvoCtrlProcessor::connect (const juce::String& in, const juce::String& out)
{
    inputId = in;
    outputId = out;
    const auto error = link.open (in, out);

    if (error.isNotEmpty())
        setStatus (StatusKind::Error, error);
    else if (! link.hasOutput())
        setStatus (StatusKind::Idle, "No MIDI output selected");
    else if (! link.hasInput())
        setStatus (StatusKind::Warning, "Output only: no MIDI input, knob moves and dumps won't be received");
    else
        setStatus (StatusKind::Warning, "Ports open - press Test to check the Evolver answers");
    return error;
}

void EvoCtrlProcessor::requestEditBuffer (bool apply)
{
    if (! link.hasOutput() || ! link.hasInput())
        return setStatus (StatusKind::Error, "Select both a MIDI input and output first");

    pendingTest = true;
    applyNextEditBuffer = apply;
    pendingMain = true;
    link.queueSysex (evo::requestMainParams()); // current bank/program, then its name (see K::Main)
    link.queueSysex (evo::requestEditBuffer());
    setStatus (StatusKind::Warning, "Waiting for the Evolver...");
    startTimer (2000);
}

void EvoCtrlProcessor::timerCallback()
{
    stopTimer();
    if (pendingTest || pendingProgramRead)
        setStatus (StatusKind::Error, "No (complete) answer from the Evolver. Check both MIDI cables and its globals: "
                                      "MIDI Rec = ALL, MIDI Xmit = ALL, Poly Chain = OFF (see Setup tab)");
    pendingTest = pendingProgramRead = pendingMain = nameAfterDump = false;
}

void EvoCtrlProcessor::sendToEvolver()
{
    if (! link.hasOutput())
        return setStatus (StatusKind::Error, "No MIDI output open");
    link.queueSysex (evo::editBufferDump (capturePatch()));
    setStatus (StatusKind::Ok, "Patch sent to the Evolver edit buffer");
}

void EvoCtrlProcessor::readProgram (int b, int p)
{
    if (! link.hasOutput() || ! link.hasInput())
        return setStatus (StatusKind::Error, "Select both a MIDI input and output first");
    bank = b;
    program = p;
    pendingProgramRead = true;
    link.queueSysex (evo::requestProgram (b, p));
    link.queueSysex (evo::requestName (b, p));
    setStatus (StatusKind::Warning, "Reading program...");
    startTimer (2000);
}

void EvoCtrlProcessor::writeProgram (int b, int p)
{
    if (! link.hasOutput())
        return setStatus (StatusKind::Error, "No MIDI output open");
    bank = b;
    program = p;
    link.queueSysex (evo::programDump (b, p, capturePatch()));
    link.queueSysex (evo::nameDump (b, p, patchName.toStdString())); // format documented, store-on-receive unverified
    setStatus (StatusKind::Ok, "Written to bank " + juce::String (b + 1) + " program " + juce::String (p + 1));
}

void EvoCtrlProcessor::handleAsyncUpdate()
{
    using K = evo::Parsed::Kind;

    for (const auto& m : link.popReceived())
    {
        switch (m.kind)
        {
            case K::ProgramParam:
                if (m.index < evo::numParams && juce::Time::getMillisecondCounter() - sentAt[(size_t) m.index] > echoWindowMs)
                    setParamFromDevice (m.index, m.value);
                break;

            case K::SeqParam:
                if (m.index < evo::numSeqSteps && juce::Time::getMillisecondCounter() - seqSentAt[(size_t) m.index] > echoWindowMs)
                {
                    seq[(size_t) m.index] = m.value;
                    ++changeCounter;
                }
                break;

            case K::EditBuffer:
                if (pendingTest)
                {
                    stopTimer();
                    pendingTest = false;
                    if (applyNextEditBuffer)
                        applyPatch (m.patch, false);
                    setStatus (StatusKind::Ok, applyNextEditBuffer ? "Edit buffer received from the Evolver"
                                                                   : "Evolver connected and answering");
                    if (std::exchange (nameAfterDump, false))
                        link.queueSysex (evo::requestName (bank, program));
                }
                break;

            case K::Program:
                if (pendingProgramRead && m.index == bank && m.value == program)
                {
                    stopTimer();
                    pendingProgramRead = false;
                    applyPatch (m.patch, true); // so the Evolver plays what the editor shows
                    setStatus (StatusKind::Ok, "Program B" + juce::String (bank + 1) + " P" + juce::String (program + 1) + " loaded");
                }
                break;

            case K::Main:
                if (pendingMain || pendingGlobals)
                {
                    for (int i = 0; i < evo::numMainParams; ++i)
                        globals[(size_t) i] = m.main[(size_t) i];
                    globalsKnown = true;
                    bank = m.index;
                    program = m.value;
                    if (pendingMain)
                        nameAfterDump = true; // asking now would cut into the edit buffer dump the Evolver is sending
                    pendingMain = pendingGlobals = false;
                    ++changeCounter;
                }
                break;

            case K::ProgramChange: // program picked on the Evolver: its edit buffer now holds it
                patchName.clear();
                requestEditBuffer (true);
                break;

            case K::Waveshape:
                if (m.index < evo::numWaves)
                {
                    waves[(size_t) m.index] = m.wave;
                    recordOriginals();
                    if (m.index == waveNext)
                    {
                        ++waveNext;
                        waveRetried = false;
                        requestNextWave();
                    }
                    ++changeCounter;
                }
                break;

            case K::Name:
                if (m.index < 4 && m.value < 128)
                {
                    programNames[(size_t) (m.index * 128 + m.value)] = m.name;
                    nameKnown[(size_t) (m.index * 128 + m.value)] = true;
                }
                if (m.index == bank && m.value == program)
                    patchName = m.name;
                if (nameNext >= 0 && m.index == nameBank && m.value == nameNext)
                {
                    ++nameNext;
                    nameRetried = false;
                    requestNextName();
                }
                ++changeCounter;
                break;
        }
    }
}

void EvoCtrlProcessor::setParamFromDevice (int index, int value)
{
    const auto& p = evo::params[(size_t) index];
    muteOutgoing = true; // listener fires synchronously below: record the value, don't echo it back
    params[(size_t) index]->setValueNotifyingHost (params[(size_t) index]->convertTo0to1 ((float) juce::jlimit (p.min, p.max, value)));
    muteOutgoing = false;
}

void EvoCtrlProcessor::readGlobals()
{
    if (! link.hasOutput() || ! link.hasInput())
        return;
    pendingGlobals = true;
    link.queueSysex (evo::requestMainParams());
}

void EvoCtrlProcessor::setGlobal (int index, int value)
{
    if (! link.hasOutput())
        return setStatus (StatusKind::Error, "No MIDI output open");
    link.queueSysex (evo::mainParam (index, value));
    globals[(size_t) index] = value;
    ++changeCounter;
}

void EvoCtrlProcessor::selectProgram (int b, int p)
{
    if (! link.hasOutput() || ! link.hasInput())
        return setStatus (StatusKind::Error, "Select both a MIDI input and output first");
    // Main params 1 (bank) and 0 (program) switch the Evolver like its own knob (verified on hardware); the
    // edit buffer request queued right after then returns that program's sound.
    link.queueSysex (evo::mainParam (1, b));
    link.queueSysex (evo::mainParam (0, p));
    bank = b;
    program = p;
    patchName = programNames[(size_t) (b * 128 + p)];
    requestEditBuffer (true);
}

void EvoCtrlProcessor::startStopSequencer()
{
    if (! link.hasOutput())
        return setStatus (StatusKind::Error, "No MIDI output open");
    link.queueSysex (evo::startStop());
}

void EvoCtrlProcessor::resetSequencer()
{
    if (! link.hasOutput())
        return setStatus (StatusKind::Error, "No MIDI output open");
    link.queueSysex (evo::resetSequencer());
}

void EvoCtrlProcessor::readNames (int b)
{
    if (! link.hasOutput() || ! link.hasInput())
        return setStatus (StatusKind::Error, "Select both a MIDI input and output first");
    nameBank = b;
    nameNext = 0;
    nameRetried = false;
    requestNextName();
}

void EvoCtrlProcessor::requestNextName()
{
    if (nameNext >= 128)
    {
        nameNext = -1;
        nameWatchdog.stopTimer();
        saveNames();
        return setStatus (StatusKind::Ok, "Bank " + juce::String (nameBank + 1) + " names read");
    }
    link.queueSysex (evo::requestName (nameBank, nameNext));
    nameWatchdog.startTimer (600);
    setStatus (StatusKind::Warning, "Reading bank " + juce::String (nameBank + 1) + " names " + juce::String (nameNext + 1) + " / 128...");
}

void EvoCtrlProcessor::saveNames() const
{
    evo::Bytes all;
    for (int i = 0; i < 512; ++i)
        if (nameKnown[(size_t) i])
        {
            const auto d = evo::nameDump (i / 128, i % 128, programNames[(size_t) i].toStdString());
            all.insert (all.end(), d.begin(), d.end());
        }
    wavesFolder().getParentDirectory().createDirectory();
    PatchLibrary::writeSyx (wavesFolder().getParentDirectory().getChildFile ("program-names.syx"), all);
}

juce::File EvoCtrlProcessor::wavesFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EvoCtrl/Waves");
}

int EvoCtrlProcessor::wavesKnown() const
{
    return (int) std::count_if (waves.begin(), waves.end(), [] (const auto& w) { return w.has_value(); });
}

void EvoCtrlProcessor::readWaves (bool onlyMissing)
{
    waveOnlyMissing = onlyMissing;
    if (! link.hasOutput() || ! link.hasInput())
        return setStatus (StatusKind::Error, "Select both a MIDI input and output first");
    waveNext = 0;
    waveRetried = false;
    requestNextWave();
}

// One request at a time: the next goes out when this one answers (handleAsyncUpdate) or times out (watchdog).
void EvoCtrlProcessor::requestNextWave()
{
    while (waveOnlyMissing && waveNext < evo::numWaves && waves[(size_t) waveNext])
        ++waveNext;
    if (waveNext >= evo::numWaves)
    {
        waveNext = -1;
        waveWatchdog.stopTimer();
        saveWaveCache();
        return setStatus (StatusKind::Ok, juce::String (wavesKnown()) + " / 128 waveshapes read from the Evolver");
    }
    link.queueSysex (evo::requestWaveshape (waveNext));
    waveWatchdog.startTimer (800);
    setStatus (StatusKind::Warning, "Reading waveshapes " + juce::String (waveNext + 1) + " / 128...");
}

void EvoCtrlProcessor::sendWave (int num, const evo::Waveshape& w)
{
    if (num < evo::firstUserWave || num >= evo::numWaves)
        return;
    if (! link.hasOutput())
        return setStatus (StatusKind::Error, "No MIDI output open");
    recordOriginals();   // keep what was there before this first write
    link.queueSysex (evo::waveshapeDump (num, w));
    waves[(size_t) num] = w;
    saveWaveCache();
    setStatus (StatusKind::Ok, "Waveshape " + juce::String (num + 1) + " sent to the Evolver");
}

// Records each user wave the first time its content is known; never overwritten afterwards.
void EvoCtrlProcessor::recordOriginals()
{
    bool added = false;
    for (int num = evo::firstUserWave; num < evo::numWaves; ++num)
    {
        auto& o = originals[(size_t) (num - evo::firstUserWave)];
        if (! o && waves[(size_t) num])
        {
            o = waves[(size_t) num];
            added = true;
        }
    }
    if (! added)
        return;
    evo::Bytes all;
    for (int num = evo::firstUserWave; num < evo::numWaves; ++num)
        if (const auto& o = originals[(size_t) (num - evo::firstUserWave)])
        {
            const auto d = evo::waveshapeDump (num, *o);
            all.insert (all.end(), d.begin(), d.end());
        }
    wavesFolder().createDirectory();
    PatchLibrary::writeSyx (wavesFolder().getChildFile ("originals.syx"), all);
}

bool EvoCtrlProcessor::isModified (int num) const
{
    if (num < evo::firstUserWave || num >= evo::numWaves)
        return false;
    const auto& o = originals[(size_t) (num - evo::firstUserWave)];
    return o && waves[(size_t) num] && *o != *waves[(size_t) num];
}

void EvoCtrlProcessor::restoreOriginal (int num)
{
    if (num >= evo::firstUserWave && num < evo::numWaves)
        if (const auto& o = originals[(size_t) (num - evo::firstUserWave)])
            sendWave (num, *o);
}

void EvoCtrlProcessor::saveWaveCache() const
{
    evo::Bytes all;
    for (int i = 0; i < evo::numWaves; ++i)
        if (const auto& w = waves[(size_t) i])
        {
            const auto d = evo::waveshapeDump (i, *w);
            all.insert (all.end(), d.begin(), d.end());
        }
    wavesFolder().createDirectory();
    PatchLibrary::writeSyx (wavesFolder().getChildFile ("evolver-cache.syx"), all);
}

evo::Patch EvoCtrlProcessor::capturePatch() const
{
    evo::Patch patch;
    for (int i = 0; i < evo::numParams; ++i)
        patch.params[(size_t) i] = (uint8_t) params[(size_t) i]->get();
    for (int s = 0; s < evo::numSeqSteps; ++s)
        patch.seq[(size_t) s] = (uint8_t) seq[(size_t) s].load();
    return patch;
}

void EvoCtrlProcessor::applyPatch (const evo::Patch& patch, bool sendToDevice)
{
    for (int i = 0; i < evo::numParams; ++i)
        setParamFromDevice (i, patch.params[(size_t) i]);
    for (int s = 0; s < evo::numSeqSteps; ++s)
        seq[(size_t) s] = patch.seq[(size_t) s];
    ++changeCounter;

    if (sendToDevice && link.hasOutput())
        link.queueSysex (evo::editBufferDump (patch));
}

void EvoCtrlProcessor::setSeqStep (int step, int value)
{
    seq[(size_t) step] = value;
    link.queueSeq (step, value);
    seqSentAt[(size_t) step] = juce::Time::getMillisecondCounter();
}

bool EvoCtrlProcessor::importSyx (const juce::File& f)
{
    const auto added = library.import (f);
    if (added.empty())
    {
        setStatus (StatusKind::Error, "No Evolver program found in " + f.getFileName());
        return false;
    }
    patchName = f.getFileNameWithoutExtension();
    applyPatch (added.front(), true);
    setStatus (StatusKind::Ok, juce::String ((int) added.size()) + " patch(es) imported into the library, first one loaded");
    return true;
}

bool EvoCtrlProcessor::exportSyx (const juce::File& f) const
{
    return PatchLibrary::writeSyx (f, evo::editBufferDump (capturePatch()));
}

void EvoCtrlProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("EvoCtrl");
    for (auto* p : params)
        state.setProperty (p->paramID, p->get(), nullptr);

    juce::StringArray steps;
    for (const auto& s : seq)
        steps.add (juce::String (s.load()));

    state.setProperty ("seq", steps.joinIntoString (","), nullptr)
         .setProperty ("inputId", inputId, nullptr)
         .setProperty ("outputId", outputId, nullptr)
         .setProperty ("channel", midiChannel.load(), nullptr)
         .setProperty ("viaAbleton", viaAbleton.load(), nullptr)
         .setProperty ("intervalMs", link.intervalMs.load(), nullptr)
         .setProperty ("resendOnLoad", resendOnLoad, nullptr)
         .setProperty ("patchName", patchName, nullptr)
         .setProperty ("bank", bank, nullptr)
         .setProperty ("program", program, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void EvoCtrlProcessor::setStateInformation (const void* data, int size)
{
    const auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;
    const auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType ("EvoCtrl"))
        return;

    evo::Patch patch = capturePatch();
    for (int i = 0; i < evo::numParams; ++i)
        patch.params[(size_t) i] = (uint8_t) (int) state.getProperty (params[(size_t) i]->paramID, patch.params[(size_t) i]);

    auto steps = juce::StringArray::fromTokens (state["seq"].toString(), ",", {});
    for (int s = 0; s < evo::numSeqSteps && s < steps.size(); ++s)
        patch.seq[(size_t) s] = (uint8_t) steps[s].getIntValue();

    midiChannel = juce::jlimit (1, 16, (int) state.getProperty ("channel", 1));
    setViaAbleton (state["viaAbleton"]);
    link.intervalMs = juce::jlimit (EvolverLink::minIntervalMs, 50, (int) state.getProperty ("intervalMs", EvolverLink::minIntervalMs));
    resendOnLoad = state["resendOnLoad"];
    patchName = state["patchName"].toString();
    bank = state["bank"];
    program = state["program"];

    applyPatch (patch, false);
    connect (state["inputId"].toString(), state["outputId"].toString());

    if (resendOnLoad && link.hasOutput())
        sendToEvolver();
}

juce::AudioProcessorEditor* EvoCtrlProcessor::createEditor()
{
    return new EvoCtrlEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EvoCtrlProcessor();
}
