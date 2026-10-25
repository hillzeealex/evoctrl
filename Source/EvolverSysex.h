#pragma once

// Evolver SysEx codec (docs/evolver_midi_spec.md §2–3). No JUCE dependency so it is unit-testable.
// Messages are built WITH F0/F7; parse() takes the body WITHOUT F0/F7 (what juce::MidiMessage::getSysExData gives).

#include "EvolverParams.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace evo
{
using Bytes = std::vector<uint8_t>;

// A digital oscillator waveshape: 128 signed 16-bit points (p.48). Waves 97-128 (0-based 96-127) are user writable.
using Waveshape = std::array<int16_t, 128>;
constexpr int numWaves = 128, firstUserWave = 96;

// One full program: what a Program / Edit Buffer dump carries. Also the value type for V2 undo, A/B, morph.
struct Patch
{
    std::array<uint8_t, numParams> params {};
    std::array<uint8_t, numSeqSteps> seq {};
    bool operator== (const Patch&) const = default;
};

namespace cmd
{
    constexpr uint8_t programParam = 0x01, programDump = 0x02, editBufferDump = 0x03, requestProgram = 0x05,
                      requestEditBuffer = 0x06, seqParam = 0x08, waveshapeDump = 0x0A, requestWaveshape = 0x0B,
                      requestMain = 0x0E, mainDump = 0x0F,
                      requestName = 0x10, nameDump = 0x11;
}

constexpr int packedPatchSize = 220;
constexpr int packedWaveSize = 293;  // 256 bytes = 128 words, LS byte then MS byte
constexpr int nameLength = 16;

// 8-byte packets: MS bits of 7 data bytes first (bit i = byte i), then the 7 low-7-bit bytes. Last packet may be partial.
inline Bytes pack (const uint8_t* in, size_t n)
{
    Bytes out;
    for (size_t i = 0; i < n; i += 7)
    {
        const size_t len = std::min<size_t> (7, n - i);
        uint8_t ms = 0;
        for (size_t j = 0; j < len; ++j)
            ms |= (uint8_t) (((in[i + j] >> 7) & 1) << j);
        out.push_back (ms);
        for (size_t j = 0; j < len; ++j)
            out.push_back (in[i + j] & 0x7F);
    }
    return out;
}

inline Bytes unpack (const uint8_t* in, size_t n)
{
    Bytes out;
    for (size_t i = 0; i < n; i += 8)
    {
        const uint8_t ms = in[i];
        for (size_t j = 1; j < 8 && i + j < n; ++j)
            out.push_back ((uint8_t) ((in[i + j] & 0x7F) | (((ms >> (j - 1)) & 1) << 7)));
    }
    return out;
}

inline Bytes header (uint8_t command) { return { 0xF0, 0x01, 0x20, 0x01, command }; }
inline Bytes& end (Bytes& b) { b.push_back (0xF7); return b; }

inline Bytes nibbleMessage (uint8_t command, int index, int value)
{
    auto b = header (command);
    b.insert (b.end(), { (uint8_t) (index & 0x7F), (uint8_t) (value & 0x0F), (uint8_t) ((value >> 4) & 0x0F) });
    return end (b);
}

inline Bytes programParam (int index, int value) { return nibbleMessage (cmd::programParam, index, value); }
inline Bytes seqParam (int step, int value)      { return nibbleMessage (cmd::seqParam, step, value); }
inline Bytes requestEditBuffer()                 { auto b = header (cmd::requestEditBuffer); return end (b); }
inline Bytes requestWaveshape (int num)          { auto b = header (cmd::requestWaveshape); b.push_back ((uint8_t) (num & 0x7F)); return end (b); }
inline Bytes requestMainParams()                 { auto b = header (cmd::requestMain); return end (b); }  // p.51

inline Bytes bankProgMessage (uint8_t command, int bank, int prog)
{
    auto b = header (command);
    b.insert (b.end(), { (uint8_t) (bank & 0x03), (uint8_t) (prog & 0x7F) });
    return b;
}

inline Bytes requestProgram (int bank, int prog) { auto b = bankProgMessage (cmd::requestProgram, bank, prog); return end (b); }
inline Bytes requestName (int bank, int prog)    { auto b = bankProgMessage (cmd::requestName, bank, prog); return end (b); }

inline void appendPatch (Bytes& b, const Patch& p)
{
    uint8_t raw[numParams + numSeqSteps];
    std::copy (p.params.begin(), p.params.end(), raw);
    std::copy (p.seq.begin(), p.seq.end(), raw + numParams);
    const auto packed = pack (raw, sizeof (raw));
    b.insert (b.end(), packed.begin(), packed.end());
}

inline Bytes editBufferDump (const Patch& p) { auto b = header (cmd::editBufferDump); appendPatch (b, p); return end (b); }

inline Bytes programDump (int bank, int prog, const Patch& p)
{
    auto b = bankProgMessage (cmd::programDump, bank, prog);
    appendPatch (b, p);
    return end (b);
}

inline Bytes waveshapeDump (int num, const Waveshape& w)
{
    auto b = header (cmd::waveshapeDump);
    b.push_back ((uint8_t) (num & 0x7F));
    uint8_t raw[256];
    for (size_t i = 0; i < w.size(); ++i)
    {
        raw[2 * i] = (uint8_t) ((uint16_t) w[i] & 0xFF);
        raw[2 * i + 1] = (uint8_t) ((uint16_t) w[i] >> 8);
    }
    const auto packed = pack (raw, sizeof (raw));
    b.insert (b.end(), packed.begin(), packed.end());
    return end (b);
}

inline Bytes nameDump (int bank, int prog, const std::string& name)
{
    auto b = bankProgMessage (cmd::nameDump, bank, prog);
    for (int i = 0; i < nameLength; ++i)
        b.push_back (i < (int) name.size() ? (uint8_t) (name[(size_t) i] & 0x7F) : (uint8_t) ' ');
    return end (b);
}

struct Parsed
{
    // Main = Main Parameter dump (current bank + program); ProgramChange = MIDI program change from the Evolver.
    enum class Kind { ProgramParam, SeqParam, EditBuffer, Program, Name, Main, ProgramChange, Waveshape } kind;
    int index = 0, value = 0;  // param/step number + value, or bank + program for dumps/names/main
    Patch patch {};
    std::string name;
    Waveshape wave {};  // Kind::Waveshape: index = wave number 0-127
};

inline Patch patchFromPacked (const uint8_t* d)
{
    const auto raw = unpack (d, packedPatchSize);
    Patch p;
    std::copy (raw.begin(), raw.begin() + numParams, p.params.begin());
    std::copy (raw.begin() + numParams, raw.begin() + numParams + numSeqSteps, p.seq.begin());
    return p;
}

// body = SysEx data without F0/F7. Returns nullopt for anything that is not a well-formed Evolver message we use.
inline std::optional<Parsed> parse (const uint8_t* d, size_t n)
{
    if (n < 4 || d[0] != 0x01 || d[1] != 0x20 || d[2] != 0x01)
        return std::nullopt;

    const uint8_t c = d[3];
    d += 4;
    n -= 4;

    switch (c)
    {
        case cmd::programParam:
        case cmd::seqParam:
            if (n != 3) return std::nullopt;
            return Parsed { c == cmd::programParam ? Parsed::Kind::ProgramParam : Parsed::Kind::SeqParam,
                            d[0], d[1] | (d[2] << 4) };
        case cmd::editBufferDump:
            if (n != (size_t) packedPatchSize) return std::nullopt;
            return Parsed { Parsed::Kind::EditBuffer, 0, 0, patchFromPacked (d) };
        case cmd::programDump:
            if (n != (size_t) packedPatchSize + 2) return std::nullopt;
            return Parsed { Parsed::Kind::Program, d[0], d[1], patchFromPacked (d + 2) };
        case cmd::waveshapeDump:
        {
            if (n != 1 + (size_t) packedWaveSize) return std::nullopt;
            Parsed r { Parsed::Kind::Waveshape, d[0] };
            const auto raw = unpack (d + 1, packedWaveSize);
            for (size_t i = 0; i < r.wave.size(); ++i)
                r.wave[i] = (int16_t) (uint16_t) (raw[2 * i] | (raw[2 * i + 1] << 8));
            return r;
        }
        case cmd::mainDump:
            // 16 main params as LS/MS nibble pairs (p.48, p.53): #0 = program 0-127, #1 = bank 0-3.
            if (n != 32) return std::nullopt;
            return Parsed { Parsed::Kind::Main, (d[2] | (d[3] << 4)) & 0x03, (d[0] | (d[1] << 4)) & 0x7F };
        case cmd::nameDump:
        {
            if (n != (size_t) nameLength + 2) return std::nullopt;
            Parsed r { Parsed::Kind::Name, d[0], d[1] };
            r.name.assign (d + 2, d + 2 + nameLength);
            for (auto& c : r.name)
                if (c < 0x20 || c > 0x7E) // unnamed programs are filled with 0x7F
                    c = ' ';
            r.name.erase (r.name.find_last_not_of (' ') + 1);
            return r;
        }
        default:
            return std::nullopt;
    }
}

// Splits a .syx file into message bodies (without F0/F7).
inline std::vector<Bytes> splitSysex (const uint8_t* d, size_t n)
{
    std::vector<Bytes> out;
    for (size_t i = 0; i < n; ++i)
    {
        if (d[i] != 0xF0) continue;
        size_t j = i + 1;
        while (j < n && d[j] != 0xF7) ++j;
        if (j < n) out.emplace_back (d + i + 1, d + j);
        i = j;
    }
    return out;
}
} // namespace evo
