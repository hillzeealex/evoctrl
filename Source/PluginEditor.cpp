#include "PluginEditor.h"

namespace colours
{
    const juce::Colour bg { 0xff141619 }, panel { 0xff1e2126 }, outline { 0xff30353d },
                       text { 0xffd8dce2 }, dim { 0xff8a919b }, accent { 0xffff8c1a };
}

constexpr int knobW = 76, knobH = 96, titleH = 26, pad = 8;

static juce::Font font (float height, int style = juce::Font::plain) { return juce::Font (juce::FontOptions (height, style)); }

static void showAlert (juce::Component* owner, const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon, title, message, {}, owner), nullptr);
}

// Cmd (macOS) / Ctrl (Windows) drag = fine adjustment: velocity mode with low sensitivity.
static void enableFineDrag (juce::Slider& s)
{
    s.setVelocityModeParameters (0.15, 1, 0.0, true, juce::ModifierKeys::commandModifier);
}

//==============================================================================
EvoLookAndFeel::EvoLookAndFeel()
{
    setColourScheme ({ colours::bg, colours::panel, colours::panel, colours::outline, colours::text,
                       colours::accent, juce::Colours::black, colours::accent, colours::text });
    setColour (juce::ResizableWindow::backgroundColourId, colours::bg);
    setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, colours::outline);
    setColour (juce::Slider::trackColourId, colours::accent);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::TextEditor::backgroundColourId, colours::bg.darker (0.3f));
    setColour (juce::TextEditor::outlineColourId, colours::outline);
    setColour (juce::ListBox::backgroundColourId, colours::bg.darker (0.3f));
    setColour (juce::GroupComponent::outlineColourId, colours::outline);
    setColour (juce::GroupComponent::textColourId, colours::accent);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel.brighter (0.1f));
}

void EvoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float lw = 3.0f, r = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f - lw;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float origin = s.getProperties()["bipolar"] ? (start + end) / 2.0f : start; // bipolar params fill from 12 o'clock

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    value.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (origin, angle), juce::jmax (origin, angle), true);

    const juce::PathStrokeType stroke (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (s.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (track, stroke);
    g.setColour (s.findColour (juce::Slider::rotarySliderFillColourId));
    g.strokePath (value, stroke);

    const float kr = r - lw * 2.0f;
    g.setColour (colours::panel.brighter (0.18f));
    g.fillEllipse (juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c));
    g.setColour (colours::text);
    g.drawLine ({ c.getPointOnCircumference (kr * 0.35f, angle), c.getPointOnCircumference (kr, angle) }, 2.0f);
}

//==============================================================================
SectionPanel::SectionPanel (EvoCtrlProcessor& proc, evo::Group group) : title (evo::groupName (group))
{
    for (const auto& p : evo::params)
    {
        if (p.group != group)
            continue;

        auto k = std::make_unique<Knob>();
        k->label.setText (p.name, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (font (12.0f));
        k->label.setMinimumHorizontalScale (0.7f);
        k->label.setInterceptsMouseClicks (false, false);

        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW - 4, 18);
        // Also sets the value text (names, cents...) and double-click = default from the host parameter.
        k->attachment = std::make_unique<juce::SliderParameterAttachment> (proc.param (p.index), k->slider);
        enableFineDrag (k->slider);
        k->slider.getProperties().set ("bipolar", p.disp == evo::Disp::Bipolar99 || p.disp == evo::Disp::Fine50
                                                      || p.disp == evo::Disp::KeyXpose);
        k->slider.setTooltip (juce::String (p.name) + "  -  Evolver param " + juce::String (p.index)
                              + ", raw " + juce::String (p.min) + ".." + juce::String (p.max)
                              + (p.cc >= 0 ? ", CC " + juce::String (p.cc) : juce::String())
                              + "\nDouble-click: default   Cmd/Ctrl-drag: fine");

        addAndMakeVisible (k->label);
        addAndMakeVisible (k->slider);
        knobs.push_back (std::move (k));
    }
}

int SectionPanel::preferredWidth (int maxCols) const
{
    return juce::jmin (maxCols, (int) knobs.size()) * knobW + 2 * pad;
}

int SectionPanel::preferredHeight (int width) const
{
    const int cols = juce::jmax (1, (width - 2 * pad) / knobW);
    const int rows = ((int) knobs.size() + cols - 1) / cols;
    return titleH + rows * knobH + pad;
}

void SectionPanel::paint (juce::Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (colours::accent);
    g.setFont (font (13.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), getLocalBounds().reduced (pad, 0).removeFromTop (titleH), juce::Justification::centredLeft);
}

void SectionPanel::resized()
{
    const int cols = juce::jmax (1, (getWidth() - 2 * pad) / knobW);
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (pad + (int) (i % (size_t) cols) * knobW, titleH + (int) (i / (size_t) cols) * knobH, knobW, knobH);
        knobs[i]->label.setBounds (cell.removeFromTop (16));
        knobs[i]->slider.setBounds (cell.reduced (2, 0));
    }
}

//==============================================================================
SynthPage::SynthPage (EvoCtrlProcessor& proc)
{
    for (int g = 0; g < (int) evo::Group::NumGroups; ++g)
        if ((evo::Group) g != evo::Group::Seq) // lives on the Sequencer tab
            content.addAndMakeVisible (*sections.emplace_back (std::make_unique<SectionPanel> (proc, (evo::Group) g)));

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void SynthPage::resized()
{
    viewport.setBounds (getLocalBounds());
    const int width = viewport.getWidth() - viewport.getScrollBarThickness();

    // Flow layout: sections left to right, wrapping when the row is full.
    int x = pad, y = pad, rowH = 0;
    for (auto& s : sections)
    {
        const int w = juce::jmin (s->preferredWidth (6), width - 2 * pad);
        if (x > pad && x + w > width - pad)
        {
            x = pad;
            y += rowH + pad;
            rowH = 0;
        }
        const int h = s->preferredHeight (w);
        s->setBounds (x, y, w, h);
        x += w + pad;
        rowH = juce::jmax (rowH, h);
    }
    content.setSize (width, y + rowH + pad);
}

//==============================================================================
SequencerPage::SequencerPage (EvoCtrlProcessor& p) : proc (p), settings (p, evo::Group::Seq)
{
    addAndMakeVisible (settings);

    for (int i = 0; i < evo::numSeqSteps; ++i)
    {
        auto& s = steps[(size_t) i];
        s.setSliderStyle (juce::Slider::LinearBarVertical);
        s.setRange (0, evo::seqStepMax, 1);
        s.setDoubleClickReturnValue (true, 0);
        enableFineDrag (s);
        s.onValueChange = [this, i] { proc.setSeqStep (i, (int) steps[(size_t) i].getValue()); };
        addAndMakeVisible (s);
    }

    help.setText ("Steps 0-100 (24 = one octave on an oscillator destination). Values above 100 (rst / off) "
                  "received from the Evolver are kept and shown grey; dragging such a step overwrites it.",
                  juce::dontSendNotification);
    help.setColour (juce::Label::textColourId, colours::dim);
    addAndMakeVisible (help);
    refresh();
}

void SequencerPage::refresh()
{
    for (int i = 0; i < evo::numSeqSteps; ++i)
    {
        auto& s = steps[(size_t) i];
        const int v = proc.getSeqStep (i);
        const bool special = v > evo::seqStepMax;
        s.setValue (juce::jmin (v, evo::seqStepMax), juce::dontSendNotification); // no notification = no echo
        s.setColour (juce::Slider::trackColourId, special ? colours::dim : colours::accent);
        s.setTooltip ("Seq " + juce::String (i / 16 + 1) + " step " + juce::String (i % 16 + 1)
                      + (special ? "  -  raw " + juce::String (v) + " (rst / off)" : juce::String()));
    }
}

void SequencerPage::paint (juce::Graphics& g)
{
    g.setColour (colours::text);
    g.setFont (font (14.0f, juce::Font::bold));
    for (int row = 0; row < 4; ++row)
    {
        const auto b = steps[(size_t) row * 16].getBounds();
        g.drawText ("Seq " + juce::String (row + 1), pad, b.getY(), 56, b.getHeight(), juce::Justification::centredLeft);
    }
}

void SequencerPage::resized()
{
    auto r = getLocalBounds().reduced (pad);
    settings.setBounds (r.removeFromTop (settings.preferredHeight (r.getWidth())));
    help.setBounds (r.removeFromBottom (36));
    r.removeFromTop (pad);
    r.removeFromLeft (60);

    const int rowH = r.getHeight() / 4, colW = r.getWidth() / 16;
    for (int i = 0; i < evo::numSeqSteps; ++i)
        steps[(size_t) i].setBounds (juce::Rectangle<int> (r.getX() + (i % 16) * colW, r.getY() + (i / 16) * rowH, colW, rowH).reduced (2));
}

//==============================================================================
PatchesPage::PatchesPage (EvoCtrlProcessor& p) : proc (p)
{
    for (int b = 1; b <= 4; ++b)
        bankBox.addItem ("Bank " + juce::String (b), b);
    for (int i = 1; i <= 128; ++i)
        progBox.addItem ("Program " + juce::String (i), i);

    readBtn.setTooltip ("Load this program from the Evolver into the editor");
    readBtn.onClick = [this] { proc.readProgram (bankBox.getSelectedId() - 1, progBox.getSelectedId() - 1); };

    writeBtn.setTooltip ("Store the current patch (and its name) into this slot on the Evolver");
    writeBtn.onClick = [this]
    {
        const int b = bankBox.getSelectedId() - 1, prog = progBox.getSelectedId() - 1;
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "Write to the Evolver",
                "Overwrite bank " + juce::String (b + 1) + " program " + juce::String (prog + 1)
                    + " on the Evolver with the current patch? This cannot be undone.",
                "Write", "Cancel", this),
            [safe = SafePointer<PatchesPage> (this), b, prog] (int result)
            {
                if (safe != nullptr && result == 1)
                    safe->proc.writeProgram (b, prog);
            });
    };

    juce::String ascii; // Evolver names are 16 printable 7-bit ASCII chars
    for (char c = 32; c < 127; ++c)
        ascii << c;
    nameEditor.setInputRestrictions (evo::nameLength, ascii);
    nameEditor.onTextChange = [this] { proc.patchName = nameEditor.getText(); };
    tagsEditor.setTextToShowWhenEmpty ("comma separated, e.g. bass, dark", colours::dim);
    searchEditor.setTextToShowWhenEmpty ("Search name or tag", colours::dim);
    searchEditor.onTextChange = [this] { rebuildList(); };
    favOnly.onClick = [this] { rebuildList(); };

    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import .syx", juce::File(), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile() && proc.importSyx (fc.getResult()))
                                      rebuildList();
                              });
    };

    exportBtn.onClick = [this]
    {
        const auto name = juce::File::createLegalFileName (proc.patchName.isEmpty() ? "EvoCtrl patch" : proc.patchName);
        chooser = std::make_unique<juce::FileChooser> ("Export .syx", proc.library.folder().getParentDirectory().getChildFile (name + ".syx"), "*.syx");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult() == juce::File())
                                      return;
                                  const auto f = fc.getResult().withFileExtension ("syx");
                                  if (! proc.exportSyx (f))
                                      showAlert (this, "Export failed", "Could not write " + f.getFullPathName());
                              });
    };

    const auto tags = [this] { return juce::StringArray::fromTokens (tagsEditor.getText(), ",", {}); };

    saveBtn.setTooltip ("Save the current patch to the local library with the name and tags above");
    saveBtn.onClick = [this, tags]
    {
        proc.library.save (proc.patchName, tags(), proc.capturePatch());
        rebuildList();
    };
    loadBtn.setTooltip ("Load the selected patch into the editor and the Evolver edit buffer (double-click works too)");
    loadBtn.onClick = [this] { loadSelected(); };
    favBtn.onClick = [this]
    {
        if (auto* e = selected())
            proc.library.setFavourite (*e, ! e->favourite);
        rebuildList();
    };
    tagBtn.setTooltip ("Replace the selected patch's tags with the Tags field");
    tagBtn.onClick = [this, tags]
    {
        if (auto* e = selected())
            proc.library.setTags (*e, tags());
        rebuildList();
    };
    deleteBtn.onClick = [this]
    {
        const auto* e = selected();
        if (e == nullptr)
            return;
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "Delete patch",
                "Move \"" + e->name + "\" to the trash?", "Delete", "Cancel", this),
            [safe = SafePointer<PatchesPage> (this), file = e->file] (int result)
            {
                if (safe == nullptr || result != 1)
                    return;
                safe->proc.library.remove ({ file });
                safe->rebuildList();
            });
    };
    revealBtn.onClick = [this] { proc.library.folder().startAsProcess(); };

    list.setRowHeight (24);
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &deviceGroup, &fileGroup, &libGroup, &bankBox, &progBox, &readBtn, &writeBtn, &nameLabel, &tagsLabel,
             &nameEditor, &tagsEditor, &searchEditor, &importBtn, &exportBtn, &favOnly, &list,
             &saveBtn, &loadBtn, &favBtn, &tagBtn, &deleteBtn, &revealBtn })
        addAndMakeVisible (c);

    refresh();
    rebuildList();
}

void PatchesPage::refresh()
{
    if (! nameEditor.hasKeyboardFocus (true))
        nameEditor.setText (proc.patchName, false);
    bankBox.setSelectedId (proc.bank + 1, juce::dontSendNotification);
    progBox.setSelectedId (proc.program + 1, juce::dontSendNotification);
}

void PatchesPage::rebuildList()
{
    const auto* sel = selected();
    const auto keep = sel != nullptr ? sel->file : juce::File();
    const auto query = searchEditor.getText().trim();
    const auto& entries = proc.library.entries();

    visible.clear();
    int reselect = -1;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        if (favOnly.getToggleState() && ! e.favourite)
            continue;
        if (query.isNotEmpty() && ! e.name.containsIgnoreCase (query) && ! e.tags.joinIntoString (" ").containsIgnoreCase (query))
            continue;
        if (e.file == keep)
            reselect = (int) visible.size();
        visible.push_back (i);
    }

    list.updateContent();
    list.selectRow (reselect);
    list.repaint();
}

const PatchLibrary::Entry* PatchesPage::selected() const
{
    const int row = list.getSelectedRow();
    return row >= 0 && row < (int) visible.size() ? &proc.library.entries()[visible[(size_t) row]] : nullptr;
}

void PatchesPage::selectedRowsChanged (int)
{
    if (const auto* e = selected())
        tagsEditor.setText (e->tags.joinIntoString (", "), false);
}

void PatchesPage::loadSelected()
{
    const auto* e = selected();
    if (e == nullptr)
        return;
    if (auto patch = proc.library.load (*e))
    {
        proc.patchName = e->name.substring (0, evo::nameLength);
        proc.applyPatch (*patch, true);
    }
    else
    {
        showAlert (this, "Load failed", e->file.getFileName() + " does not contain an Evolver program.");
    }
}

void PatchesPage::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool isSelected)
{
    if (row < 0 || row >= (int) visible.size())
        return;
    const auto& e = proc.library.entries()[visible[(size_t) row]];
    if (isSelected)
        g.fillAll (colours::accent.withAlpha (0.25f));

    g.setFont (font (14.0f));
    g.setColour (e.favourite ? colours::accent : colours::outline);
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85")), 6, 0, 20, h, juce::Justification::centred);
    g.setColour (colours::text);
    g.drawText (e.name, 30, 0, w / 2, h, juce::Justification::centredLeft, true);
    g.setColour (colours::dim);
    g.drawText (e.tags.joinIntoString (", "), w / 2 + 36, 0, w / 2 - 42, h, juce::Justification::centredRight, true);
}

void PatchesPage::resized()
{
    const auto row = [] (juce::Rectangle<int>& area) { auto r = area.removeFromTop (26); area.removeFromTop (6); return r; };
    const auto split = [] (juce::Rectangle<int> r, juce::Component& a, juce::Component& b)
    {
        a.setBounds (r.removeFromLeft (r.getWidth() / 2).withTrimmedRight (3));
        b.setBounds (r.withTrimmedLeft (3));
    };

    auto r = getLocalBounds().reduced (pad);
    auto left = r.removeFromLeft (320);
    r.removeFromLeft (pad);

    auto nameRow = row (left);
    nameLabel.setBounds (nameRow.removeFromLeft (50));
    nameEditor.setBounds (nameRow);
    auto tagsRow = row (left);
    tagsLabel.setBounds (tagsRow.removeFromLeft (50));
    tagsEditor.setBounds (tagsRow);
    left.removeFromTop (pad);

    auto dev = left.removeFromTop (100);
    deviceGroup.setBounds (dev);
    dev = dev.reduced (12).withTrimmedTop (10);
    split (row (dev), bankBox, progBox);
    split (row (dev), readBtn, writeBtn);
    left.removeFromTop (pad);

    auto files = left.removeFromTop (68);
    fileGroup.setBounds (files);
    split (files.reduced (12).withTrimmedTop (10).removeFromTop (26), importBtn, exportBtn);

    libGroup.setBounds (r);
    auto lib = r.reduced (12).withTrimmedTop (10);
    auto top = row (lib);
    favOnly.setBounds (top.removeFromRight (140));
    searchEditor.setBounds (top.withTrimmedRight (6));

    auto buttons = lib.removeFromBottom (26);
    lib.removeFromBottom (6);
    const int bw = buttons.getWidth() / 6;
    for (auto* b : { &saveBtn, &loadBtn, &favBtn, &tagBtn, &deleteBtn, &revealBtn })
        b->setBounds (buttons.removeFromLeft (bw).reduced (3, 0));
    list.setBounds (lib);
}

//==============================================================================
SetupPage::SetupPage (EvoCtrlProcessor& p) : proc (p)
{
    modeBox.addItem ("Direct MIDI ports (SysEx, all 128 parameters)", 1);
    modeBox.addItem ("Via Ableton (CC, ~40 parameters, experimental)", 2);
    modeBox.setSelectedId (proc.viaAbleton.load() ? 2 : 1, juce::dontSendNotification);
    modeBox.onChange = [this] { proc.setViaAbleton (modeBox.getSelectedId() == 2); };

    intervalSlider.setRange (0, 50, 1);
    intervalSlider.setValue (proc.link.intervalMs.load(), juce::dontSendNotification);
    intervalSlider.onValueChange = [this] { proc.link.intervalMs = (int) intervalSlider.getValue(); };
    intervalSlider.setTooltip ("Raise it if the Evolver drops or lags parameter changes during dense automation");

    resendToggle.setToggleState (proc.resendOnLoad, juce::dontSendNotification);
    resendToggle.onClick = [this] { proc.resendOnLoad = resendToggle.getToggleState(); };

    info.setMultiLine (true);
    info.setReadOnly (true);
    info.setCaretVisible (false);
    info.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    info.setText (
        "EVOLVER GLOBAL SETTINGS (Main menu on the Evolver)\n"
        "  MIDI Rec     = ALL   receive dumps and SysEx parameter changes\n"
        "  MIDI Xmit    = ALL   send knob moves as SysEx (two-way sync)\n"
        "  MIDI Channel = ALL, or the channel chosen in the top bar (CC fallback only)\n"
        "  Poly Chain   = OFF   prevents MIDI echo loops\n\n"
        "PARAMETER PATH\n"
        "  Direct: EvoCtrl opens the Evolver MIDI ports itself and sends SysEx. Works for all\n"
        "  parameters, dumps and the sequencer. Recommended.\n"
        "  Via Ableton: knob moves go out as CC on this track's MIDI output; route it to the\n"
        "  Evolver from another track (MIDI From: this track > EvoCtrl). Only parameters with a\n"
        "  documented CC are sent, with a linear scaling not yet verified on hardware.\n\n"
        "ABLETON LIVE (Preferences > Link, Tempo & MIDI)\n"
        "  Turn Remote OFF for the Evolver ports so Live MIDI mappings don't double-route.\n"
        "  Windows: a MIDI port can only be open in one application. If EvoCtrl reports a busy\n"
        "  port, also turn Track OFF for it (input and output) and press Rescan.\n"
        "  See README.md for the full setup.\n");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &modeLabel, &intervalLabel, &modeBox, &intervalSlider, &resendToggle, &info })
        addAndMakeVisible (c);
}

void SetupPage::resized()
{
    auto r = getLocalBounds().reduced (pad * 2);
    const auto line = [&r] { auto l = r.removeFromTop (26); r.removeFromTop (8); return l; };

    auto l = line();
    modeLabel.setBounds (l.removeFromLeft (260));
    modeBox.setBounds (l.removeFromLeft (420));
    l = line();
    intervalLabel.setBounds (l.removeFromLeft (260));
    intervalSlider.setBounds (l.removeFromLeft (420));
    resendToggle.setBounds (line().removeFromLeft (680));
    r.removeFromTop (pad);
    info.setBounds (r);
}

//==============================================================================
MonitorPage::MonitorPage (EvoCtrlProcessor& proc)
{
    enable.setToggleState (proc.link.logEnabled.load(), juce::dontSendNotification);
    enable.setTooltip ("SysEx and notes on the ports opened by EvoCtrl. MIDI clock / active sensing are hidden; "
                       "CCs sent through Ableton (Via Ableton mode) are not shown.");
    enable.onClick = [this, &proc] { proc.link.logEnabled = enable.getToggleState(); };
    clearBtn.onClick = [this] { log.clear(); };

    log.setMultiLine (true);
    log.setReadOnly (true);
    log.setCaretVisible (false);
    log.setScrollbarsShown (true);
    log.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));

    addAndMakeVisible (enable);
    addAndMakeVisible (clearBtn);
    addAndMakeVisible (log);
}

void MonitorPage::append (const juce::StringArray& lines)
{
    if (lines.isEmpty())
        return;
    log.moveCaretToEnd();
    log.insertTextAtCaret (lines.joinIntoString ("\n") + "\n");
    if (log.getTotalNumChars() > 200000) // keep the editor responsive during long sessions
    {
        log.setText (log.getText().getLastCharacters (100000), false);
        log.moveCaretToEnd();
    }
}

void MonitorPage::resized()
{
    auto r = getLocalBounds().reduced (pad);
    auto top = r.removeFromTop (26);
    clearBtn.setBounds (top.removeFromRight (80));
    enable.setBounds (top);
    r.removeFromTop (pad);
    log.setBounds (r);
}

//==============================================================================
EvoCtrlEditor::EvoCtrlEditor (EvoCtrlProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    for (auto* l : { &inLabel, &outLabel, &chLabel })
        l->setJustificationType (juce::Justification::centredRight);
    statusLabel.setMinimumHorizontalScale (0.8f);

    for (int c = 1; c <= 16; ++c)
        channelBox.addItem (juce::String (c), c);
    channelBox.setSelectedId (proc.midiChannel.load(), juce::dontSendNotification);
    channelBox.onChange = [this] { proc.midiChannel = channelBox.getSelectedId(); };
    channelBox.setTooltip ("MIDI channel for the Via Ableton CC fallback (Evolver SysEx has no channel)");

    inBox.setTooltip ("MIDI input from the Evolver (knob moves, dumps)");
    outBox.setTooltip ("MIDI output to the Evolver");
    inBox.onChange = [this] { connectSelected(); };
    outBox.onChange = [this] { connectSelected(); };
    rescanBtn.setTooltip ("Refresh the port lists and reopen the selected ports");
    rescanBtn.onClick = [this] { fillPorts(); connectSelected(); };
    testBtn.setTooltip ("Ask the Evolver for its edit buffer and check it answers (nothing is changed)");
    testBtn.onClick = [this] { proc.testConnection(); };
    getBtn.setTooltip ("Replace the editor's patch with the Evolver's current edit buffer");
    getBtn.onClick = [this] { proc.getFromEvolver(); };
    sendBtn.setTooltip ("Send the whole editor patch to the Evolver edit buffer");
    sendBtn.onClick = [this] { proc.sendToEvolver(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &inLabel, &inBox, &outLabel, &outBox, &chLabel, &channelBox, &rescanBtn, &testBtn, &getBtn, &sendBtn, &statusLabel })
        addAndMakeVisible (c);
    fillPorts();

    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    tabs.addTab ("Synth", colours::bg, new SynthPage (proc), true);
    tabs.addTab ("Sequencer", colours::bg, seqPage = new SequencerPage (proc), true);
    tabs.addTab ("Patches", colours::bg, patchesPage = new PatchesPage (proc), true);
    tabs.addTab ("Setup", colours::bg, new SetupPage (proc), true);
    tabs.addTab ("MIDI monitor", colours::bg, monitorPage = new MonitorPage (proc), true);
    addAndMakeVisible (tabs);
    setLookAndFeel (&laf); // last: slider text boxes copy their colours when the look-and-feel reaches them

    setResizable (true, true);
    setResizeLimits (900, 560, 2560, 1600);
    setSize (1180, 780);
    timerCallback();
    startTimerHz (15);
}

EvoCtrlEditor::~EvoCtrlEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EvoCtrlEditor::fillPorts()
{
    inputs = juce::MidiInput::getAvailableDevices();
    outputs = juce::MidiOutput::getAvailableDevices();

    const auto fill = [] (juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& current)
    {
        box.clear (juce::dontSendNotification);
        box.addItem ("None", 1);
        int selectedId = 1;
        for (int i = 0; i < devices.size(); ++i)
        {
            box.addItem (devices[i].name, i + 2);
            if (devices[i].identifier == current)
                selectedId = i + 2;
        }
        box.setSelectedId (selectedId, juce::dontSendNotification);
    };
    fill (inBox, inputs, proc.inputId);
    fill (outBox, outputs, proc.outputId);
}

void EvoCtrlEditor::connectSelected()
{
    // Out-of-range Array access returns an empty MidiDeviceInfo, i.e. "None" -> empty id -> port closed.
    const auto error = proc.connect (inputs[inBox.getSelectedId() - 2].identifier, outputs[outBox.getSelectedId() - 2].identifier);
    if (error.isNotEmpty())
        showAlert (this, "MIDI port unavailable", error);
}

void EvoCtrlEditor::timerCallback()
{
    monitorPage->append (proc.link.popLog());

    const int c = proc.changeCounter.load();
    if (c == lastChange)
        return;
    lastChange = c;

    statusLabel.setText (proc.status.text, juce::dontSendNotification);
    statusLabel.setTooltip (proc.status.text);
    seqPage->refresh();
    patchesPage->refresh();
    repaint (statusDot);
}

void EvoCtrlEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    using K = EvoCtrlProcessor::StatusKind;
    const auto k = proc.status.kind;
    g.setColour (k == K::Ok ? juce::Colour (0xff3ccf6e) : k == K::Warning ? juce::Colour (0xffffc23a)
               : k == K::Error ? juce::Colour (0xffff4d4d) : colours::dim);
    g.fillEllipse (statusDot.toFloat());
}

void EvoCtrlEditor::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (70).reduced (pad, 6);
    auto ports = bar.removeFromTop (30);
    const auto take = [&ports] (juce::Component& c, int w) { c.setBounds (ports.removeFromLeft (w).reduced (2, 0)); };

    take (inLabel, 28);
    take (inBox, 190);
    take (outLabel, 36);
    take (outBox, 190);
    take (chLabel, 30);
    take (channelBox, 60);
    take (rescanBtn, 72);
    ports.removeFromLeft (pad);
    sendBtn.setBounds (ports.removeFromRight (130).reduced (2, 0));
    getBtn.setBounds (ports.removeFromRight (140).reduced (2, 0));
    testBtn.setBounds (ports.removeFromRight (64).reduced (2, 0));

    bar.removeFromTop (6);
    statusDot = bar.removeFromLeft (28).withSizeKeepingCentre (12, 12);
    statusLabel.setBounds (bar);

    tabs.setBounds (r);
}
