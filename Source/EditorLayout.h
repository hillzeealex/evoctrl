#pragma once

// What goes where in the editor: pages > columns > panels > rows of controls.
// No JUCE dependency so the tests can check every parameter is placed exactly once.

#include "EvolverParams.h"
#include <vector>

namespace layout
{
struct Ctl { int index; const char* label; };
struct Row { std::vector<Ctl> ctls; int adsr = -1; };  // adsr >= 0: envelope graph of params adsr..adsr+3 (A, D, S, R)
struct Panel { const char* title; std::vector<Row> rows; };
using Column = std::vector<Panel>;
struct Tab { const char* name; std::vector<Column> columns; };

inline const std::vector<Tab>& tabs()
{
    static const std::vector<Tab> t {
        { "OSC | MIX", {
            { { "Oscillator 1  ·  analog",  { { { { 2, "Shape" }, { 0, "Freq" }, { 1, "Fine" }, { 3, "Level" }, { 64, "Glide" }, { 65, "Sync 2>1" } } } } },
              { "Oscillator 2  ·  analog",  { { { { 6, "Shape" }, { 4, "Freq" }, { 5, "Fine" }, { 7, "Level" }, { 68, "Glide" } } } } },
              { "Common",                   { { { { 60, "Noise" }, { 69, "Slop" }, { 55, "Key Xpose" }, { 70, "Bend Range" }, { 71, "Key Mode" } } } } } },
            { { "Oscillator 3  ·  digital", { { { { 10, "Wave" }, { 8, "Freq" }, { 9, "Fine" }, { 11, "Level" }, { 72, "Glide" } } },
                                              { { { 73, "FM 4>3" }, { 75, "Ring 4>3" }, { 74, "Shape Mod" } } } } },
              { "Oscillator 4  ·  digital", { { { { 14, "Wave" }, { 12, "Freq" }, { 13, "Fine" }, { 15, "Level" }, { 76, "Glide" } } },
                                              { { { 77, "FM 3>4" }, { 79, "Ring 3>4" }, { 78, "Shape Mod" } } } } } },
        } },
        { "FILTER | AMP", {
            { { "Low pass filter", { { { { 16, "Cutoff" }, { 22, "Reso" }, { 17, "Env Amt" }, { 23, "Key Amt" }, { 82, "Audio Mod" }, { 83, "L/R Split" } } },
                                     { { { 80, "Poles" }, { 81, "Velocity" } } },
                                     { {}, 18 },
                                     { { { 18, "Attack" }, { 19, "Decay" }, { 20, "Sustain" }, { 21, "Release" } } } } },
              { "High pass filter", { { { { 84, "Freq" } } } } } },
            { { "Amplifier", { { { { 24, "Level" }, { 25, "Env Amt" }, { 89, "Velocity" }, { 88, "Env Shape" } } },
                               { {}, 26 },
                               { { { 26, "Attack" }, { 27, "Decay" }, { 28, "Sustain" }, { 29, "Release" } } } } },
              { "Output", { { { { 31, "Volume" }, { 30, "Pan" } } } } } },
        } },
        { "LFO | ENV", {
            { { "LFO 1", { { { { 40, "Freq" }, { 41, "Shape" }, { 42, "Amount" }, { 43, "Dest" } } } } },
              { "LFO 2", { { { { 44, "Freq" }, { 45, "Shape" }, { 46, "Amount" }, { 47, "Dest" } } } } },
              { "LFO 3", { { { { 104, "Freq" }, { 105, "Shape" }, { 106, "Amount" }, { 107, "Dest" } } } } },
              { "LFO 4", { { { { 108, "Freq" }, { 109, "Shape" }, { 110, "Amount" }, { 111, "Dest" } } } } } },
            { { "Envelope 3", { { { { 49, "Dest" }, { 48, "Amount" }, { 113, "Velocity" }, { 112, "Delay" } } },
                                { {}, 50 },
                                { { { 50, "Attack" }, { 51, "Decay" }, { 52, "Sustain" }, { 53, "Release" } } } } } },
        } },
        { "MATRIX", {
            { { "Modulation matrix", { { { { 85, "Source 1" }, { 86, "Amount" }, { 87, "Dest 1" } } },
                                       { { { 90, "Source 2" }, { 91, "Amount" }, { 92, "Dest 2" } } },
                                       { { { 93, "Source 3" }, { 94, "Amount" }, { 95, "Dest 3" } } },
                                       { { { 96, "Source 4" }, { 97, "Amount" }, { 98, "Dest 4" } } } } } },
            { { "Controllers", { { { { 118, "Velocity" }, { 119, "Vel Dest" }, { 120, "Mod Wheel" }, { 121, "Wheel Dest" } } },
                                 { { { 122, "Pressure" }, { 123, "Press Dest" }, { 124, "Breath" }, { 125, "Breath Dest" } } },
                                 { { { 126, "Foot" }, { 127, "Foot Dest" } } } } },
              { "External in as modulator", { { { { 114, "Peak Amt" }, { 115, "Peak Dest" }, { 116, "Follow Amt" }, { 117, "Follow Dest" } } } } } },
        } },
        { "FX", {
            { { "Feedback", { { { { 32, "Freq" }, { 33, "Amount" }, { 34, "Grunge" } } } } },
              { "Distortion / hack", { { { { 103, "Distortion" }, { 39, "Output Hack" } } } } } },
            { { "Delay", { { { { 35, "Time 1" }, { 99, "Time 2" }, { 101, "Time 3" }, { 37, "Fbk > Delay" } } },
                           { { { 36, "Level 1" }, { 100, "Level 2" }, { 102, "Level 3" }, { 38, "Fbk > Filter" } } } } } },
            { { "External input", { { { { 62, "Mode" }, { 61, "Volume" }, { 63, "Input Hack" } } } } } },
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
