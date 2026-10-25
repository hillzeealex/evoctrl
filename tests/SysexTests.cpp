// Self-checks for the SysEx codec against sizes and examples from the manual (spec §2–3).
#undef NDEBUG // asserts must run in Release builds too
#include "../Source/EditorLayout.h"
#include "../Source/EvolverSysex.h"
#include <cassert>
#include <cstdio>

using namespace evo;

int main()
{
    // Packing sizes announced by the manual: 192 -> 220, 256 -> 293.
    uint8_t raw[256];
    for (int i = 0; i < 256; ++i) raw[i] = (uint8_t) (i * 37 + 11);
    assert (pack (raw, 192).size() == 220);
    assert (pack (raw, 256).size() == 293);
    const auto packed = pack (raw, 256);
    assert (unpack (packed.data(), packed.size()) == Bytes (raw, raw + 256));
    for (auto b : packed) assert (b < 0x80);

    // Bit layout of the first packet: MS byte bit i = MSB of data byte i.
    const uint8_t sample[7] = { 0x80, 0x01, 0x80, 0x01, 0x01, 0x01, 0xFF };
    const auto p = pack (sample, 7);
    assert (p[0] == 0b1000101 && p[1] == 0x00 && p[7] == 0x7F);

    // Program Parameter: F0 01 20 01 01 pp ls ms F7, value 198 -> nibbles 6, C.
    assert (programParam (17, 198) == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x01, 17, 0x06, 0x0C, 0xF7 }));
    assert (requestEditBuffer() == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x06, 0xF7 }));
    assert (requestProgram (3, 127) == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x05, 3, 127, 0xF7 }));

    const auto body = [] (const Bytes& m) { return Bytes (m.begin() + 1, m.end() - 1); };

    auto pp = body (programParam (17, 198));
    auto r = parse (pp.data(), pp.size());
    assert (r && r->kind == Parsed::Kind::ProgramParam && r->index == 17 && r->value == 198);

    // Dump round trips.
    Patch patch;
    for (int i = 0; i < numParams; ++i) patch.params[(size_t) i] = (uint8_t) params[(size_t) i].max;
    for (int i = 0; i < numSeqSteps; ++i) patch.seq[(size_t) i] = (uint8_t) i;
    const auto eb = editBufferDump (patch);
    assert (eb.size() == 5 + 220 + 1);
    auto ebBody = body (eb);
    r = parse (ebBody.data(), ebBody.size());
    assert (r && r->kind == Parsed::Kind::EditBuffer && r->patch == patch);

    auto pd = body (programDump (2, 5, patch));
    r = parse (pd.data(), pd.size());
    assert (r && r->kind == Parsed::Kind::Program && r->index == 2 && r->value == 5 && r->patch == patch);

    Bytes unnamed { 0x01, 0x20, 0x01, 0x11, 0, 5 };
    unnamed.insert (unnamed.end(), nameLength, 0x7F);
    r = parse (unnamed.data(), unnamed.size());
    assert (r && r->kind == Parsed::Kind::Name && r->name.empty());

    auto nd = body (nameDump (0, 1, "Bass"));
    r = parse (nd.data(), nd.size());
    assert (r && r->kind == Parsed::Kind::Name && r->name == "Bass");

    // Main Parameter dump: param 0 = program, param 1 = bank, LS then MS nibble.
    Bytes main { 0x01, 0x20, 0x01, 0x0F };
    const uint8_t mainValues[16] = { 66, 2 };
    for (auto v : mainValues) { main.push_back (v & 0x0F); main.push_back (v >> 4); }
    r = parse (main.data(), main.size());
    assert (r && r->kind == Parsed::Kind::Main && r->value == 66 && r->index == 2);
    assert (requestMainParams() == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x0E, 0xF7 }));
    assert (r->main[0] == 66 && r->main[1] == 2 && r->main[2] == 0);
    assert (mainParam (9, 0) == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x09, 9, 0, 0, 0xF7 }));   // Poly Chain off
    assert (mainParam (4, 132) == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x09, 4, 0x04, 0x08, 0xF7 }));
    assert (startStop() == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x12, 0xF7 }));
    assert (resetSequencer() == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x04, 0xF7 }));

    // Waveshape: 300-byte dump (5 header + number + 293 packed + F7), full 16-bit round trip.
    Waveshape wave;
    for (int i = 0; i < 128; ++i) wave[(size_t) i] = (int16_t) ((i * 517) % 65536 - 32768);
    const auto wd = waveshapeDump (100, wave);
    assert (wd.size() == 300);
    auto wdBody = body (wd);
    r = parse (wdBody.data(), wdBody.size());
    assert (r && r->kind == Parsed::Kind::Waveshape && r->index == 100 && r->wave == wave);
    assert (requestWaveshape (127) == (Bytes { 0xF0, 0x01, 0x20, 0x01, 0x0B, 127, 0xF7 }));
    assert (! parse (wdBody.data(), wdBody.size() - 1));

    // Rejects foreign / truncated messages.
    const Bytes foreign { 0x01, 0x21, 0x01, 0x01, 0, 0, 0 };
    assert (! parse (foreign.data(), foreign.size()));
    assert (! parse (ebBody.data(), ebBody.size() - 1));

    // .syx splitting (two messages + junk).
    Bytes file { 0x00 };
    file.insert (file.end(), eb.begin(), eb.end());
    const auto pm = programParam (1, 2);
    file.insert (file.end(), pm.begin(), pm.end());
    assert (splitSysex (file.data(), file.size()).size() == 2);

    // Every display string resolves within range (catches table/list mismatches).
    for (const auto& prm : params)
    {
        assert (prm.min <= prm.def && prm.def <= prm.max);
        for (int v = prm.min; v <= prm.max; ++v)
            assert (! valueToText (prm, v).empty());
    }

    // Every parameter has exactly one control in the editor, and envelope graphs point at A/D/S/R quadruples.
    int placed[numParams] = {};
    const auto count = [&] (const layout::Panel& panel)
    {
        for (const auto& row : panel.rows)
        {
            for (const auto& c : row.ctls)
                ++placed[c.index];
            assert (row.adsr < 0 || (std::string (params[(size_t) row.adsr].name).find ("Attack") != std::string::npos
                                     && std::string (params[(size_t) row.adsr + 3].name).find ("Release") != std::string::npos));
        }
    };
    for (const auto& tab : layout::tabs())
        for (const auto& row : tab.rows)
            for (const auto& panel : row)
                count (panel);
    count (layout::seqSettings());
    for (int i : layout::seqDests)
        ++placed[i];
    for (int i = 0; i < numParams; ++i)
        assert (placed[i] == 1);

    std::puts ("SysexTests: all passed");
    return 0;
}
