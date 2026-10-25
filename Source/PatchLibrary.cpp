#include "PatchLibrary.h"

PatchLibrary::PatchLibrary()
    : dir (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("EvoCtrl/Library"))
{
    dir.createDirectory();
    refresh();
}

juce::var PatchLibrary::meta (const juce::File& f) const
{
    return index.getProperty (f.getFileName(), {});
}

void PatchLibrary::refresh()
{
    index = juce::JSON::parse (dir.getChildFile ("library.json"));
    if (! index.isObject())
        index = new juce::DynamicObject();

    list.clear();
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.syx"))
    {
        Entry e { f, f.getFileNameWithoutExtension() };
        const auto m = meta (f);
        if (const auto* tags = m["tags"].getArray())
            for (const auto& t : *tags)
                e.tags.add (t.toString());
        e.favourite = (bool) m["favourite"];
        list.push_back (std::move (e));
    }
    std::sort (list.begin(), list.end(), [] (const Entry& a, const Entry& b) { return a.name.compareNatural (b.name) < 0; });
}

void PatchLibrary::writeIndex() const
{
    dir.getChildFile ("library.json").replaceWithText (juce::JSON::toString (index));
}

juce::File PatchLibrary::save (const juce::String& name, const juce::StringArray& tags, const evo::Patch& patch)
{
    const auto safe = juce::File::createLegalFileName (name.trim().isEmpty() ? "Untitled" : name.trim());
    const auto f = dir.getNonexistentChildFile (safe, ".syx", false);
    writeSyx (f, evo::editBufferDump (patch));
    Entry e { f };
    setTags (e, tags);
    return f;
}

std::optional<evo::Patch> PatchLibrary::load (const Entry& e) const
{
    for (const auto& m : readSyx (e.file))
        if (m.kind == evo::Parsed::Kind::EditBuffer || m.kind == evo::Parsed::Kind::Program)
            return m.patch;
    return std::nullopt;
}

static juce::DynamicObject* metaObject (juce::var& index, const juce::File& f)
{
    auto* root = index.getDynamicObject();
    if (! root->getProperty (f.getFileName()).isObject())
        root->setProperty (f.getFileName(), new juce::DynamicObject());
    return root->getProperty (f.getFileName()).getDynamicObject();
}

void PatchLibrary::setFavourite (const Entry& e, bool fav)
{
    metaObject (index, e.file)->setProperty ("favourite", fav);
    writeIndex();
    refresh();
}

void PatchLibrary::setTags (const Entry& e, const juce::StringArray& tags)
{
    juce::Array<juce::var> arr;
    for (const auto& t : tags)
        if (t.trim().isNotEmpty())
            arr.add (t.trim());
    metaObject (index, e.file)->setProperty ("tags", arr);
    writeIndex();
    refresh();
}

void PatchLibrary::remove (const Entry& e)
{
    e.file.moveToTrash();
    index.getDynamicObject()->removeProperty (e.file.getFileName());
    writeIndex();
    refresh();
}

std::vector<evo::Patch> PatchLibrary::import (const juce::File& syx)
{
    std::vector<evo::Patch> added;
    const auto messages = readSyx (syx);
    for (const auto& m : messages)
    {
        if (m.kind != evo::Parsed::Kind::EditBuffer && m.kind != evo::Parsed::Kind::Program)
            continue;
        auto name = syx.getFileNameWithoutExtension();
        if (m.kind == evo::Parsed::Kind::Program && messages.size() > 1)
            name << " B" << (m.index + 1) << " P" << juce::String (m.value + 1).paddedLeft ('0', 3);
        save (name, { "imported" }, m.patch);
        added.push_back (m.patch);
    }
    return added;
}

std::vector<evo::Parsed> PatchLibrary::readSyx (const juce::File& f)
{
    juce::MemoryBlock data;
    std::vector<evo::Parsed> out;
    if (! f.loadFileAsData (data))
        return out;
    for (const auto& body : evo::splitSysex (static_cast<const uint8_t*> (data.getData()), data.getSize()))
        if (auto p = evo::parse (body.data(), body.size()))
            out.push_back (std::move (*p));
    return out;
}

bool PatchLibrary::writeSyx (const juce::File& f, const evo::Bytes& bytes)
{
    return f.replaceWithData (bytes.data(), bytes.size());
}
