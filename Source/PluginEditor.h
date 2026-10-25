#pragma once

#include "PluginProcessor.h"

class EvoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EvoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
};

// One section of the front panel: a titled group of knobs generated from evo::params.
class SectionPanel : public juce::Component
{
public:
    SectionPanel (EvoCtrlProcessor&, evo::Group);
    int preferredWidth (int maxCols) const;
    int preferredHeight (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    juce::String title;
    std::vector<std::unique_ptr<Knob>> knobs;
};

class SynthPage : public juce::Component
{
public:
    explicit SynthPage (EvoCtrlProcessor&);
    void resized() override;

private:
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<SectionPanel>> sections;
};

class SequencerPage : public juce::Component
{
public:
    explicit SequencerPage (EvoCtrlProcessor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EvoCtrlProcessor& proc;
    SectionPanel settings;   // seq destinations, tempo, clock, trigger: next to the grid they drive
    std::array<juce::Slider, evo::numSeqSteps> steps;
    juce::Label help;
};

class MonitorPage : public juce::Component
{
public:
    explicit MonitorPage (EvoCtrlProcessor&);
    void append (const juce::StringArray& lines);
    void resized() override;

private:
    juce::ToggleButton enable { "Log MIDI on the direct ports" };
    juce::TextButton clearBtn { "Clear" };
    juce::TextEditor log;
};

class PatchesPage : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PatchesPage (EvoCtrlProcessor&);
    void refresh();
    void resized() override;

private:
    int getNumRows() override { return (int) visible.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { loadSelected(); }
    void selectedRowsChanged (int row) override;
    void rebuildList();
    const PatchLibrary::Entry* selected() const;
    void loadSelected();

    EvoCtrlProcessor& proc;
    juce::GroupComponent deviceGroup { {}, "Evolver memory" }, fileGroup { {}, "Files" }, libGroup { {}, "Library" };
    juce::ComboBox bankBox, progBox;
    juce::TextButton readBtn { "Read" }, writeBtn { "Write..." };
    juce::Label nameLabel { {}, "Name" }, tagsLabel { {}, "Tags" };
    juce::TextEditor nameEditor, tagsEditor, searchEditor;
    juce::TextButton importBtn { "Import .syx..." }, exportBtn { "Export .syx..." };
    juce::ToggleButton favOnly { "Favourites only" };
    juce::ListBox list { "patches", this };
    juce::TextButton saveBtn { "Save current" }, loadBtn { "Load" }, favBtn { "Toggle fav" },
                     tagBtn { "Set tags" }, deleteBtn { "Delete..." }, revealBtn { "Open folder" };
    std::vector<size_t> visible;
    std::unique_ptr<juce::FileChooser> chooser;
};

class SetupPage : public juce::Component
{
public:
    explicit SetupPage (EvoCtrlProcessor&);
    void resized() override;

private:
    EvoCtrlProcessor& proc;
    juce::Label modeLabel { {}, "Parameter path" }, intervalLabel { {}, "Min. interval between messages (ms)" };
    juce::ComboBox modeBox;
    juce::Slider intervalSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton resendToggle { "Resend the patch to the Evolver when the Live set loads" };
    juce::TextEditor info;
};

class EvoCtrlEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EvoCtrlEditor (EvoCtrlProcessor&);
    ~EvoCtrlEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void fillPorts();
    void connectSelected();

    EvoCtrlProcessor& proc;
    EvoLookAndFeel laf;
    juce::TooltipWindow tooltips { this };
    juce::ComboBox inBox, outBox, channelBox;
    juce::Label inLabel { {}, "In" }, outLabel { {}, "Out" }, chLabel { {}, "Ch" }, statusLabel;
    juce::TextButton rescanBtn { "Rescan" }, testBtn { "Test" }, getBtn { "Get from Evolver" }, sendBtn { "Send to Evolver" };
    juce::Array<juce::MidiDeviceInfo> inputs, outputs;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    SequencerPage* seqPage = nullptr;
    PatchesPage* patchesPage = nullptr;
    MonitorPage* monitorPage = nullptr;
    int lastChange = -1;
    juce::Rectangle<int> statusDot;
};
