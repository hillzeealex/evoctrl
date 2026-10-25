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
        juce::String tag, tagSub;
        juce::Rectangle<int> tagArea;
        int width() const;
    };
    juce::String title;
    std::vector<Row> rows;
};

// A page laid out once at its natural size, then scaled as a whole to the window (like a hardware panel).
class SynthTab : public juce::Component
{
public:
    // scaleLike: another page whose scale this one copies, so knobs keep one size across pages.
    SynthTab (EvoCtrlProcessor&, const layout::Tab&, const SynthTab* scaleLike = nullptr);
    void resized() override;

private:
    const SynthTab* scaleLike;
    juce::Component content;
    std::vector<std::vector<std::unique_ptr<Panel>>> rows;
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
    juce::TextButton startBtn { "START / STOP" }, resetBtn { "RESET" };
    juce::Label help;
    juce::Rectangle<int> grid;
};

// Draws a waveshape. The big editable one takes mouse drawing; the small ones are clickable library tiles.
class WaveView : public juce::Component
{
public:
    explicit WaveView (bool editable);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    evo::Waveshape wave {};
    bool known = false, selected = false;
    int number = 0;
    juce::String label;
    std::function<void()> onClick, onEditStart, onEditEnd;

private:
    juce::Rectangle<float> plotArea() const;
    void drawTo (juce::Point<float>);
    const bool editable;
    juce::Point<float> last;
};

class WavesPage : public juce::Component, private juce::Timer
{
public:
    explicit WavesPage (EvoCtrlProcessor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void timerCallback() override;      // follows the current program's Osc 3/4 wave
    void follow (bool force);
    void showBank (bool user);
    void beginEdit() { undo = editor.wave; dirty = true; }
    void endEdit();
    void setWave (const evo::Waveshape&);
    void fromHarmonics();
    void layoutTiles();

    EvoCtrlProcessor& proc;
    WaveView editor { true };
    evo::Waveshape undo {};
    std::array<juce::Slider, 16> harmonics;
    bool settingHarmonics = false;
    std::vector<std::unique_ptr<juce::TextButton>> presetButtons, toolButtons;
    juce::TextButton importBtn { "IMPORT WAV..." }, romBtn { "ROM 1-96" }, userBtn { "USER 97-128" },
                     readBtn { "READ ALL" }, sendBtn, restoreBtn { "RESTORE ORIGINAL" }, factoryBtn { "FACTORY DEFAULT" },
                     saveBtn { "SAVE TO LIBRARY" },
                     auditionBtn { "PLAY ON" };
    juce::ComboBox oscBox;
    juce::ToggleButton liveToggle { "Live: send at the end of each stroke" };
    juce::Label progress;
    juce::Viewport tilesView;
    juce::Component tilesContent;
    std::array<std::unique_ptr<WaveView>, evo::numWaves> tiles;
    bool showUser = false;
    int selectedSlot = evo::firstUserWave, loadedFrom = -1;
    int followed = -1, lastOsc3 = -1, lastOsc4 = -1;
    bool dirty = false;   // the editor holds the user's own edit: don't replace it when the program changes
    juce::Rectangle<int> editorPanel, harmPanel, libPanel, audPanel;
    std::unique_ptr<juce::FileChooser> chooser;
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

// The Evolver's own memory: 4 banks x 128 programs, with names read from it.
class BankBrowser : public juce::Component, private juce::ListBoxModel
{
public:
    explicit BankBrowser (EvoCtrlProcessor&);
    void refresh();
    void shown() { showBank (proc.bank); }   // the page became visible: open on the current program's bank
    void resized() override;

private:
    int getNumRows() override { return 128; }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { load (row); }
    void showBank (int b);
    void load (int row);

    EvoCtrlProcessor& proc;
    std::array<juce::TextButton, 4> bankButtons;
    juce::ListBox list { "programs", this };
    juce::TextButton readNamesBtn { "READ NAMES" }, loadBtn { "LOAD ON EVOLVER" }, writeBtn { "WRITE HERE..." };
    int bank = 0;
};

class PatchesPage : public juce::Component, private juce::ListBoxModel
{
public:
    explicit PatchesPage (EvoCtrlProcessor&);
    void refresh();
    void resized() override;
    void visibilityChanged() override { if (isShowing()) banks.shown(); }  // children get no callback on tab switches

private:
    int getNumRows() override { return (int) visible.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { loadSelected(); }
    void selectedRowsChanged (int row) override;
    void rebuildList();
    const PatchLibrary::Entry* selected() const;
    void loadSelected();

    EvoCtrlProcessor& proc;
    juce::GroupComponent deviceGroup { {}, "Evolver memory  -  4 banks" }, fileGroup { {}, "Files" }, libGroup { {}, "Library" };
    BankBrowser banks;
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
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void changed (int index, int value);

    EvoCtrlProcessor& proc;
    // Evolver globals (Main menu), index = main parameter number; only the ones shown here are set.
    std::array<std::unique_ptr<juce::Slider>, evo::numMainParams> globalKnobs;
    std::array<std::unique_ptr<juce::ComboBox>, evo::numMainParams> globalMenus;
    std::array<juce::Label, evo::numMainParams> globalLabels;
    juce::TextButton readGlobalsBtn { "READ FROM EVOLVER" };
    juce::Label globalsStatus;
    juce::Rectangle<int> globalsPanel, appPanel;
    bool updating = false;
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
    WavesPage* wavesPage = nullptr;
    SetupPage* setupPage = nullptr;
    int lastChange = -1;
    juce::Rectangle<int> statusDot, lcd;
    std::array<int, evo::numParams> seen {};   // last polled values: the LED shows whichever parameter moved last
    int touched = -1;
    juce::uint32 touchedAt = 0;
};
