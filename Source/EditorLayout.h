#pragma once

// What goes where in the editor: pages > columns > panels > rows of controls.
// No JUCE dependency so the tests can check every parameter is placed exactly once.

#include "EvolverParams.h"
#include <vector>

namespace layout
{
struct Ctl { int index; const char* label; };
struct Row
{
    std::vector<Ctl> ctls;
    int adsr = -1;                 // >= 0: envelope graph of params adsr..adsr+3 (A, D, S, R) instead of controls
    const char* tag = nullptr;     // optional row name at the left ("OSC 1"), with a yellow sub-line
    const char* tagSub = nullptr;
};
struct Panel { const char* title; std::vector<Row> rows; };  // "Title · sub" draws the sub part in yellow
using PanelRow = std::vector<Panel>;
struct Tab { const char* name; std::vector<PanelRow> rows; };

// SYNTH is the whole instrument on one front panel; TWEAKS holds what is rarely touched.
inline const std::vector<Tab>& tabs()
{
    static const std::vector<Tab> t {
        { "SYNTH", {
            { { "Oscillators  ·  1–2 analog / 3–4 digital", {
                  { { { 0, "Freq" }, { 1, "Fine" }, { 2, "Shape" }, { 3, "Level" }, { 64, "Glide" }, { 65, "Sync 2>1" } }, -1, "OSC 1", "ANALOG" },
                  { { { 4, "Freq" }, { 5, "Fine" }, { 6, "Shape" }, { 7, "Level" }, { 68, "Glide" } }, -1, "OSC 2", "ANALOG" },
                  { { { 8, "Freq" }, { 9, "Fine" }, { 10, "Wave" }, { 11, "Level" }, { 72, "Glide" }, { 73, "FM 4>3" }, { 75, "Ring 4>3" }, { 74, "Shape Mod" } }, -1, "OSC 3", "DIGITAL" },
                  { { { 12, "Freq" }, { 13, "Fine" }, { 14, "Wave" }, { 15, "Level" }, { 76, "Glide" }, { 77, "FM 3>4" }, { 79, "Ring 3>4" }, { 78, "Shape Mod" } }, -1, "OSC 4", "DIGITAL" } } },
              { "Common", {
                  { { { 60, "Noise" }, { 69, "Slop" } } },
                  { { { 55, "Key Xpose" }, { 70, "Bend" } } },
                  { { { 71, "Key Mode" } } } } },
              { "Low pass filter", {
                  { { { 16, "Cutoff" }, { 22, "Reso" }, { 17, "Env Amt" }, { 23, "Key Amt" }, { 80, "Poles" } } },
                  { { { 82, "Audio Mod" }, { 83, "L/R Split" }, { 81, "Velocity" }, { 84, "Highpass" } } },
                  { {}, 18 },
                  { { { 18, "Attack" }, { 19, "Decay" }, { 20, "Sustain" }, { 21, "Release" } } } } },
              { "Amplifier  ·  output", {
                  { { { 24, "Level" }, { 25, "Env Amt" }, { 89, "Velocity" }, { 88, "Env Shape" } } },
                  { { { 31, "Volume" }, { 30, "Pan" } } },
                  { {}, 26 },
                  { { { 26, "Attack" }, { 27, "Decay" }, { 28, "Sustain" }, { 29, "Release" } } } } } },
            { { "LFO  ·  1–4", {
                  { { { 40, "Freq" }, { 41, "Shape" }, { 42, "Amount" }, { 43, "Dest" } }, -1, "LFO 1" },
                  { { { 44, "Freq" }, { 45, "Shape" }, { 46, "Amount" }, { 47, "Dest" } }, -1, "LFO 2" },
                  { { { 104, "Freq" }, { 105, "Shape" }, { 106, "Amount" }, { 107, "Dest" } }, -1, "LFO 3" },
                  { { { 108, "Freq" }, { 109, "Shape" }, { 110, "Amount" }, { 111, "Dest" } }, -1, "LFO 4" } } },
              { "Envelope 3", {
                  { { { 49, "Dest" }, { 48, "Amount" }, { 113, "Velocity" }, { 112, "Delay" } } },
                  { {}, 50 },
                  { { { 50, "Attack" }, { 51, "Decay" }, { 52, "Sustain" }, { 53, "Release" } } } } },
              { "Mod matrix", {
                  { { { 85, "Source" }, { 86, "Amount" }, { 87, "Dest" } }, -1, "MOD 1" },
                  { { { 90, "Source" }, { 91, "Amount" }, { 92, "Dest" } }, -1, "MOD 2" },
                  { { { 93, "Source" }, { 94, "Amount" }, { 95, "Dest" } }, -1, "MOD 3" },
                  { { { 96, "Source" }, { 97, "Amount" }, { 98, "Dest" } }, -1, "MOD 4" } } },
              { "Delay  ·  feedback / distortion", {
                  { { { 35, "Time 1" }, { 36, "Level 1" }, { 99, "Time 2" }, { 100, "Level 2" }, { 101, "Time 3" }, { 102, "Level 3" } } },
                  { { { 37, "Fbk > Delay" }, { 38, "Fbk > Filter" }, { 32, "FB Freq" }, { 33, "FB Amount" }, { 34, "Grunge" } } },
                  { { { 103, "Distortion" }, { 39, "Output Hack" } } } } } },
        } },
        { "TWEAKS", {
            { { "Controllers", {
                  { { { 118, "Velocity" }, { 119, "Vel Dest" }, { 120, "Mod Wheel" }, { 121, "Wheel Dest" } } },
                  { { { 122, "Pressure" }, { 123, "Press Dest" }, { 124, "Breath" }, { 125, "Breath Dest" } } },
                  { { { 126, "Foot" }, { 127, "Foot Dest" } } } } },
              { "External input  ·  audio in / modulator", {
                  { { { 62, "Mode" }, { 61, "Volume" }, { 63, "Input Hack" } } },
                  { { { 114, "Peak Amt" }, { 115, "Peak Dest" }, { 116, "Follow Amt" }, { 117, "Follow Dest" } } } } } },
        } },
    };
    return t;
}

// Sequencer tab: these settings above the grid, one destination menu per sequence row.
inline const Panel& seqSettings()
{
    static const Panel p { "Sequencer", { { { { 54, "Trigger" }, { 67, "Clock Div" }, { 66, "Tempo" } } } } };
    return p;
}
inline constexpr int seqDests[4] = { 56, 57, 58, 59 };
} // namespace layout
