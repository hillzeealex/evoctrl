#pragma once

#include "EvolverSysex.h"
#include <juce_core/juce_core.h>

// Local patch library: one Edit Buffer .syx per patch in ~/Documents/EvoCtrl/Library,
// tags + favourites in library.json next to them (files stay usable by any SysEx tool).
class PatchLibrary
{
public:
    struct Entry
    {
        juce::File file;
        juce::String name;
        juce::StringArray tags;
        bool favourite = false;
    };

    PatchLibrary();

    const juce::File& folder() const { return dir; }
    const std::vector<Entry>& entries() const { return list; }
    void refresh();

    juce::File save (const juce::String& name, const juce::StringArray& tags, const evo::Patch&);
    std::optional<evo::Patch> load (const Entry&) const;
    void setFavourite (const Entry&, bool);
    void setTags (const Entry&, const juce::StringArray&);
    void remove (const Entry&);

    // Adds every program / edit buffer found in a .syx file. Returns the patches added (in file order).
    std::vector<evo::Patch> import (const juce::File& syx);

    static std::vector<evo::Parsed> readSyx (const juce::File&);
    static bool writeSyx (const juce::File&, const evo::Bytes&);

private:
    void writeIndex() const;
    juce::var meta (const juce::File&) const;

    juce::File dir;
    juce::var index;  // { "<file name>": { "tags": [..], "favourite": bool } }
    std::vector<Entry> list;
};
