#pragma once

#include "EditorLayout.h"
#include "PluginProcessor.h"

class EvoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EvoLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
    int getTabButtonBestWidth (juce::TabBarButton&, int depth) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// ADSR graph that follows four consecutive host parameters.
class EnvelopeView : public juce::Component, private juce::Timer
{
public:
    EnvelopeView (EvoCtrlProcessor&, int attackIndex);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    EvoCtrlProcessor& proc;
    int first;
    std::array<int, 4> shown { -1, -1, -1, -1 };
};

// One framed section of a page: a knob, menu or switch per parameter, chosen from its display type.
class Panel : public juce::Component
{
public:
    Panel (EvoCtrlProcessor&, const layout::Panel&);
    int preferredWidth() const;
    int preferredHeight() const;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        std::unique_ptr<juce::Component> widget;
        juce::Label label;
        std::shared_ptr<void> attachment;  // Slider / ComboBox / ButtonParameterAttachment, declared last = destroyed first
        int width = 0;
    };
    struct Row
    {
        std::vector<std::unique_ptr<Control>> controls;
        std::unique_ptr<EnvelopeView> envelope;
        int width() const;
    };
    juce::String title;
    std::vector<Row> rows;
};

class SynthTab : public juce::Component
{
public:
    SynthTab (EvoCtrlProcessor&, const layout::Tab&);
    void resized() override;

private:
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::vector<std::unique_ptr<Panel>>> columns;
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
    Panel settings;
    std::array<juce::ComboBox, 4> dests;
    std::array<std::unique_ptr<juce::ComboBoxParameterAttachment>, 4> destAttachments;
    std::array<juce::Slider, evo::numSeqSteps> steps;
    juce::Label help;
    juce::Rectangle<int> grid;
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
    juce::Rectangle<int> statusDot, lcd;
    std::array<int, evo::numParams> seen {};   // last polled values: the LED shows whichever parameter moved last
    int touched = -1;
    juce::uint32 touchedAt = 0;
};
