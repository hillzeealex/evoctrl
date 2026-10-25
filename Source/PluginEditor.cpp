#include "PluginEditor.h"

// Art direction: the Evolver desktop itself - black metal box, blue Lexan overlay silkscreened in white
// (secondary legends in yellow), red LEDs and a red 3-digit LED readout.
namespace colours
{
    const juce::Colour chassis { 0xff0d0e10 }, blueTop { 0xff1f4596 }, blueBottom { 0xff11295f },
                       silk { 0xffeef2f8 }, yellow { 0xffffd447 }, led { 0xffff2b1c }, ledOff { 0xff3a0d0a },
                       recess { 0xff050608 }, key { 0xff26282d };
    const juce::Colour text = silk, accent = yellow, dim { 0xff9aabcf }, edge { 0xff5170b4 };
}

constexpr int knobW = 78, comboW = 132, rowH = 98, envH = 74, titleH = 28, pad = 8;

static juce::Font font (float height, int style = juce::Font::plain) { return juce::Font (juce::FontOptions (height, style)); }
static juce::Font monoFont (float height) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::bold)); }

static juce::Font spaced (float height)
{
    auto f = font (height, juce::Font::bold);
    f.setExtraKerningFactor (0.12f);
    return f;
}

static void showAlert (juce::Component* owner, const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::WarningIcon, title, message, {}, owner), nullptr);
}

// Cmd (macOS) / Ctrl (Windows) drag = fine adjustment: velocity mode with low sensitivity.
static void enableFineDrag (juce::Slider& s)
{
    s.setVelocityModeParameters (0.15, 1, 0.0, true, juce::ModifierKeys::commandModifier);
}

static void drawLed (juce::Graphics& g, juce::Rectangle<float> r, bool on)
{
    if (on)
    {
        g.setColour (colours::led.withAlpha (0.3f));
        g.fillEllipse (r.expanded (r.getWidth() * 0.55f));
    }
    g.setColour (on ? colours::led : colours::ledOff);
    g.fillEllipse (r);
}

// Rubber key, like the Evolver's panel buttons.
static void drawKey (juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f);
    g.setGradientFill (juce::ColourGradient::vertical (colours::key.brighter (over ? 0.3f : 0.15f), r.getY(),
                                                       colours::key.darker (down ? 0.1f : 0.45f), r.getBottom()));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

static float textWidth (const juce::Font& f, const juce::String& t)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, t, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// Silkscreened section: thin white frame, title set into a gap of the top rule, the part after "·" in yellow.
static void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    const auto dot = juce::String::fromUTF8 ("·");
    const auto main = title.upToFirstOccurrenceOf (dot, false, false).trim().toUpperCase();
    const auto sub = title.fromFirstOccurrenceOf (dot, false, false).trim().toUpperCase();
    const auto f = spaced (12.0f);
    const float mainW = textWidth (f, main), subW = sub.isEmpty() ? 0.0f : textWidth (f, sub) + 10.0f;

    const auto frame = r.reduced (1.0f).withTrimmedTop ((float) titleH / 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.13f));
    g.fillRoundedRectangle (frame, 6.0f);

    const float x0 = frame.getX(), y0 = frame.getY(), x1 = frame.getRight(), y1 = frame.getBottom(), cr = 6.0f;
    const float tx = x0 + 16.0f;
    juce::Path p;
    p.startNewSubPath (tx + mainW + subW + 6.0f, y0);
    p.lineTo (x1 - cr, y0);  p.quadraticTo (x1, y0, x1, y0 + cr);
    p.lineTo (x1, y1 - cr);  p.quadraticTo (x1, y1, x1 - cr, y1);
    p.lineTo (x0 + cr, y1);  p.quadraticTo (x0, y1, x0, y1 - cr);
    p.lineTo (x0, y0 + cr);  p.quadraticTo (x0, y0, x0 + cr, y0);
    p.lineTo (tx - 6.0f, y0);
    g.setColour (colours::silk.withAlpha (0.55f));
    g.strokePath (p, juce::PathStrokeType (1.2f));

    g.setFont (f);
    g.setColour (colours::silk);
    g.drawText (main, juce::Rectangle<float> (tx, r.getY(), mainW + 4.0f, (float) titleH), juce::Justification::centredLeft, false);
    if (sub.isNotEmpty())
    {
        g.setColour (colours::yellow);
        g.drawText (sub, juce::Rectangle<float> (tx + mainW + 10.0f, r.getY(), subW, (float) titleH), juce::Justification::centredLeft, false);
    }
}

// 7-segment encoding, bit 0..6 = segments a..g.
static int segmentsFor (juce::juce_wchar c)
{
    switch (juce::CharacterFunctions::toUpperCase (c))
    {
        case '0': case 'O': return 0x3f;
        case '1': return 0x06;
        case '2': return 0x5b;
        case '3': return 0x4f;
        case '4': return 0x66;
        case '5': return 0x6d;
        case '6': return 0x7d;
        case '7': return 0x07;
        case '8': return 0x7f;
        case '9': return 0x6f;
        case '-': return 0x40;
        case 'F': return 0x71;
        case 'P': return 0x73;
        default:  return 0;
    }
}

// The Evolver's red 3-character LED, unlit segments faintly visible like the real thing.
static void drawSevenSegment (juce::Graphics& g, juce::Rectangle<float> area, juce::String text)
{
    text = text.substring (juce::jmax (0, text.length() - 3)).paddedLeft (' ', 3);
    const float cell = area.getWidth() / 3.0f, w = cell * 0.74f, h = area.getHeight(), t = w * 0.19f;
    juce::Path lit, unlit;

    for (int d = 0; d < 3; ++d)
    {
        const float x = area.getX() + (float) d * cell + (cell - w) / 2.0f, y = area.getY();
        const juce::Rectangle<float> seg[7] {
            { x + t * 0.5f, y, w - t, t },                          // a
            { x + w - t, y + t * 0.5f, t, h / 2.0f - t * 0.5f },    // b
            { x + w - t, y + h / 2.0f, t, h / 2.0f - t * 0.5f },    // c
            { x + t * 0.5f, y + h - t, w - t, t },                  // d
            { x, y + h / 2.0f, t, h / 2.0f - t * 0.5f },            // e
            { x, y + t * 0.5f, t, h / 2.0f - t * 0.5f },            // f
            { x + t * 0.5f, y + (h - t) / 2.0f, w - t, t },         // g
        };
        const int bits = segmentsFor (text[d]);
        for (int s = 0; s < 7; ++s)
            ((bits >> s) & 1 ? lit : unlit).addRoundedRectangle (seg[s].reduced (0.8f), t * 0.4f);
    }

    const auto c = area.getCentre();
    const auto italic = juce::AffineTransform::translation (-c.x, -c.y).followedBy (juce::AffineTransform::shear (-0.1f, 0.0f)).translated (c.x, c.y);
    lit.applyTransform (italic);
    unlit.applyTransform (italic);
    g.setColour (colours::ledOff.withAlpha (0.55f));
    g.fillPath (unlit);
    g.setColour (colours::led.withAlpha (0.25f));
    g.strokePath (lit, juce::PathStrokeType (4.0f));
    g.setColour (colours::led);
    g.fillPath (lit);
}

static void drawScrew (juce::Graphics& g, juce::Point<float> c)
{
    const auto r = juce::Rectangle<float> (10.0f, 10.0f).withCentre (c);
    g.setGradientFill ({ juce::Colour (0xff9a9da3), r.getX(), r.getY(), juce::Colour (0xff2a2c30), r.getRight(), r.getBottom(), false });
    g.fillEllipse (r);
    g.setColour (juce::Colour (0xff15161a));
    g.drawLine ({ c.translated (-3.0f, -1.2f), c.translated (3.0f, 1.2f) }, 1.5f);
}

enum class Widget { Knob, Menu, Switch };

static Widget widgetFor (evo::Disp d)
{
    if (d == evo::Disp::OnOff)
        return Widget::Switch;
    return d >= evo::Disp::LfoShape ? Widget::Menu : Widget::Knob; // Disp lists every named-value type from LfoShape on
}

// One item per raw value: ComboBoxParameterAttachment maps item index linearly onto the parameter range.
static void fillMenu (juce::ComboBox& box, const evo::Param& p)
{
    for (int v = p.min; v <= p.max; ++v)
        box.addItem (evo::valueToText (p, v), v - p.min + 1);
}

static juce::String tooltipFor (const evo::Param& p)
{
    return juce::String (p.name) + "  -  Evolver param " + juce::String (p.index) + ", raw " + juce::String (p.min)
         + ".." + juce::String (p.max) + (p.cc >= 0 ? ", CC " + juce::String (p.cc) : juce::String());
}

// What the hardware LED would show: signed for bipolar params, the raw number otherwise.
static juce::String ledValue (const evo::Param& p, int v)
{
    if (p.disp == evo::Disp::Bipolar99) return juce::String (v - 99);
    if (p.disp == evo::Disp::Fine50)    return juce::String (v - 50);
    if (p.disp == evo::Disp::KeyXpose)  return v == 0 ? juce::String ("OFF") : juce::String (v - 37);
    return juce::String (v);
}

//==============================================================================
EvoLookAndFeel::EvoLookAndFeel()
{
    setColourScheme ({ colours::blueBottom, colours::key, colours::recess, colours::edge, colours::silk,
                       colours::led, colours::silk, colours::blueTop, colours::silk });
    setColour (juce::ResizableWindow::backgroundColourId, colours::chassis);
    setColour (juce::Label::textColourId, colours::silk);
    setColour (juce::Slider::rotarySliderFillColourId, colours::led);
    setColour (juce::Slider::rotarySliderOutlineColourId, colours::ledOff);
    setColour (juce::Slider::trackColourId, colours::led);
    setColour (juce::Slider::backgroundColourId, colours::recess);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, colours::yellow);
    setColour (juce::ComboBox::backgroundColourId, colours::recess);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::black);
    setColour (juce::ComboBox::textColourId, colours::led);
    setColour (juce::ComboBox::arrowColourId, colours::led);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0b0c0f));
    setColour (juce::PopupMenu::textColourId, colours::silk);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::blueTop);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::buttonColourId, colours::key);
    setColour (juce::TextButton::textColourOffId, colours::silk);
    setColour (juce::TextButton::textColourOnId, colours::silk);
    setColour (juce::TextEditor::backgroundColourId, colours::recess.withAlpha (0.85f));
    setColour (juce::TextEditor::textColourId, colours::silk);
    setColour (juce::TextEditor::outlineColourId, juce::Colours::black);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::yellow.withAlpha (0.6f));
    setColour (juce::ListBox::backgroundColourId, colours::recess.withAlpha (0.6f));
    setColour (juce::GroupComponent::outlineColourId, colours::silk.withAlpha (0.45f));
    setColour (juce::GroupComponent::textColourId, colours::yellow);
    setColour (juce::ToggleButton::textColourId, colours::silk);
    setColour (juce::ToggleButton::tickColourId, colours::yellow);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff0b0c0f));
    setColour (juce::TooltipWindow::textColourId, colours::silk);
    setColour (juce::TooltipWindow::outlineColourId, colours::edge);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff15181d));
    setColour (juce::AlertWindow::textColourId, colours::silk);
}

void EvoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float origin = s.getProperties()["bipolar"] ? (start + end) / 2.0f : start; // bipolar params light from 12 o'clock

    // Ring of red LEDs, lit from the origin to the value.
    constexpr int numLeds = 19;
    const float ledR = juce::jmax (1.4f, r * 0.075f), half = (end - start) / (float) (numLeds - 1) / 2.0f;
    for (int i = 0; i < numLeds; ++i)
    {
        const float a = start + (end - start) * (float) i / (float) (numLeds - 1);
        const bool lit = a >= juce::jmin (origin, angle) - half && a <= juce::jmax (origin, angle) + half;
        drawLed (g, juce::Rectangle<float> (ledR * 2.0f, ledR * 2.0f).withCentre (c.getPointOnCircumference (r - ledR, a)), lit);
    }

    // Black knurled knob, white pointer.
    const float kr = r - ledR * 2.0f - 4.0f;
    const auto body = juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre (c);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.0f));
    g.setColour (juce::Colour (0xff17181b));
    g.fillEllipse (body);
    juce::Path ridges;
    for (int i = 0; i < 28; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 28.0f;
        ridges.addLineSegment ({ c.getPointOnCircumference (kr * 0.8f, a), c.getPointOnCircumference (kr, a) }, 1.0f);
    }
    g.setColour (juce::Colour (0xff36383d));
    g.fillPath (ridges);
    const auto cap = body.reduced (kr * 0.22f);
    g.setGradientFill ({ juce::Colour (0xff42454c), cap.getX(), cap.getY(), juce::Colour (0xff121316), cap.getRight(), cap.getBottom(), false });
    g.fillEllipse (cap);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (cap, 1.0f);
    g.setColour (colours::silk);
    g.drawLine ({ c.getPointOnCircumference (kr * 0.25f, angle), c.getPointOnCircumference (kr * 0.96f, angle) }, 2.2f);
}

void EvoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    drawKey (g, r, over, down);
    if (b.getClickingTogglesState()) // parameter switch: key with a red LED
        drawLed (g, juce::Rectangle<float> (8.0f, 8.0f).withCentre (r.getCentre()), b.getToggleState());
}

void EvoLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    // Recessed LED window.
    const auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (0.5f);
    g.setColour (colours::recess);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (box.isMouseOver (true) ? 0.28f : 0.1f));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 13.0f, ay = (float) h / 2.0f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (colours::led.withAlpha (box.isEnabled() ? 0.9f : 0.3f));
    g.fillPath (arrow);
}

void EvoLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                                       juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearBarVertical)
        return LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);

    // LED bar graph (sequencer steps).
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    g.setColour (colours::recess);
    g.fillRoundedRectangle (r, 3.0f);
    const int n = juce::jmax (4, h / 6);
    const float segH = (r.getHeight() - 4.0f) / (float) n, level = (r.getBottom() - pos) / r.getHeight();
    const auto lit = s.findColour (juce::Slider::trackColourId);
    for (int i = 0; i < n; ++i)
    {
        const auto seg = juce::Rectangle<float> (r.getX() + 3.0f, r.getBottom() - 2.0f - (float) (i + 1) * segH, r.getWidth() - 6.0f, segH).reduced (0.0f, 1.0f);
        g.setColour (((float) i + 0.5f) / (float) n <= level ? lit : colours::ledOff.withAlpha (0.6f));
        g.fillRect (seg);
    }
}

void EvoLookAndFeel::drawTabButton (juce::TabBarButton& b, juce::Graphics& g, bool over, bool down)
{
    const auto r = b.getActiveArea().toFloat().reduced (3.0f, 5.0f);
    const bool front = b.isFrontTab();
    drawKey (g, r, over, down);
    drawLed (g, juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 13.0f, r.getCentreY() }), front);
    g.setColour (colours::silk.withAlpha (front ? 1.0f : over ? 0.85f : 0.55f));
    g.setFont (spaced (11.5f));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (20.0f), juce::Justification::centred);
}

int EvoLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& b, int)
{
    return juce::roundToInt (textWidth (spaced (11.5f), b.getButtonText())) + 52;
}

juce::Font EvoLookAndFeel::getComboBoxFont (juce::ComboBox&) { return monoFont (12.5f); }
juce::Font EvoLookAndFeel::getPopupMenuFont()                { return font (14.0f); }

//==============================================================================
EnvelopeView::EnvelopeView (EvoCtrlProcessor& p, int attackIndex) : proc (p), first (attackIndex)
{
    timerCallback();
    startTimerHz (20);
}

void EnvelopeView::timerCallback()
{
    std::array<int, 4> now {};
    for (size_t i = 0; i < 4; ++i)
        now[i] = proc.param (first + (int) i).get();
    if (now != shown)
    {
        shown = now;
        repaint();
    }
}

void EnvelopeView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.0f, 4.0f);
    g.setColour (colours::recess);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::blueTop.withAlpha (0.3f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine (juce::roundToInt (r.getX() + r.getWidth() * (float) i / 8.0f), r.getY(), r.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine (juce::roundToInt (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());

    const auto area = r.reduced (10.0f);
    const auto frac = [this] (size_t i)
    {
        const auto& p = evo::params[(size_t) first + i];
        return (float) (shown[i] - p.min) / (float) (p.max - p.min);
    };
    const float seg = area.getWidth() / 4.0f, top = area.getY(), bottom = area.getBottom();
    const float sustainY = bottom - frac (2) * area.getHeight();

    // Attack, decay and release take up to a quarter of the width each; the sustain plateau a fixed quarter.
    float x = area.getX();
    juce::Path env;
    env.startNewSubPath (x, bottom);
    x += frac (0) * seg; env.lineTo (x, top);
    x += frac (1) * seg; env.lineTo (x, sustainY);
    x += seg;            env.lineTo (x, sustainY);
    x += frac (3) * seg; env.lineTo (x, bottom);

    auto fill = env;
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient::vertical (colours::led.withAlpha (0.3f), top, colours::led.withAlpha (0.0f), bottom));
    g.fillPath (fill);
    g.setColour (colours::led.withAlpha (0.25f));
    g.strokePath (env, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::led);
    g.strokePath (env, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
int Panel::Row::width() const
{
    if (envelope != nullptr)
        return 4 * knobW;
    int w = 0;
    for (const auto& c : controls)
        w += c->width;
    return w;
}

Panel::Panel (EvoCtrlProcessor& proc, const layout::Panel& def) : title (juce::String::fromUTF8 (def.title))
{
    for (const auto& rowDef : def.rows)
    {
        Row row;
        if (rowDef.adsr >= 0)
            addAndMakeVisible (*(row.envelope = std::make_unique<EnvelopeView> (proc, rowDef.adsr)));

        for (const auto& c : rowDef.ctls)
        {
            const auto& p = evo::params[(size_t) c.index];
            auto& param = proc.param (c.index);
            auto ctl = std::make_unique<Control>();

            switch (widgetFor (p.disp))
            {
                case Widget::Menu:
                {
                    auto box = std::make_unique<juce::ComboBox>();
                    fillMenu (*box, p);
                    box->setTooltip (tooltipFor (p));
                    ctl->attachment = std::make_shared<juce::ComboBoxParameterAttachment> (param, *box);
                    ctl->width = comboW;
                    ctl->widget = std::move (box);
                    break;
                }
                case Widget::Switch:
                {
                    auto b = std::make_unique<juce::TextButton>();
                    b->setClickingTogglesState (true);
                    b->setTooltip (tooltipFor (p));
                    ctl->attachment = std::make_shared<juce::ButtonParameterAttachment> (param, *b);
                    ctl->width = knobW;
                    ctl->widget = std::move (b);
                    break;
                }
                case Widget::Knob:
                {
                    auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
                    s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW - 4, 18);
                    s->setTooltip (tooltipFor (p) + "\nDouble-click: default   Cmd/Ctrl-drag: fine");
                    s->getProperties().set ("bipolar", p.disp == evo::Disp::Bipolar99 || p.disp == evo::Disp::Fine50
                                                           || p.disp == evo::Disp::KeyXpose);
                    enableFineDrag (*s);
                    // Also sets the value text (names, cents...) and double-click = default from the host parameter.
                    ctl->attachment = std::make_shared<juce::SliderParameterAttachment> (param, *s);
                    ctl->width = knobW;
                    ctl->widget = std::move (s);
                    break;
                }
            }

            ctl->label.setText (juce::String (c.label).toUpperCase(), juce::dontSendNotification);
            ctl->label.setFont (spaced (10.5f));
            ctl->label.setJustificationType (juce::Justification::centred);
            ctl->label.setMinimumHorizontalScale (0.6f);
            ctl->label.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*ctl->widget);
            addAndMakeVisible (ctl->label);
            row.controls.push_back (std::move (ctl));
        }
        rows.push_back (std::move (row));
    }
}

int Panel::preferredWidth() const
{
    int w = 160;
    for (const auto& row : rows)
        w = juce::jmax (w, row.width());
    return w + 2 * pad;
}

int Panel::preferredHeight() const
{
    int h = titleH + pad;
    for (const auto& row : rows)
        h += row.envelope != nullptr ? envH : rowH;
    return h;
}

void Panel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), title);
}

void Panel::resized()
{
    // Extra height goes to the envelope graphs if any, otherwise it spaces the rows out evenly.
    const auto numEnv = (int) std::count_if (rows.begin(), rows.end(), [] (const Row& r) { return r.envelope != nullptr; });
    const int extra = juce::jmax (0, getHeight() - preferredHeight());
    const int gap = numEnv == 0 ? extra / ((int) rows.size() + 1) : 0;
    int y = titleH;

    for (auto& row : rows)
    {
        y += gap;
        if (row.envelope != nullptr)
        {
            const int h = envH + extra / numEnv;
            row.envelope->setBounds (pad * 2, y, getWidth() - pad * 4, h);
            y += h;
            continue;
        }

        int x = (getWidth() - row.width()) / 2;
        for (auto& c : row.controls)
        {
            auto slot = juce::Rectangle<int> (x, y, c->width, rowH);
            x += c->width;
            c->label.setBounds (slot.removeFromBottom (20).withTrimmedBottom (4));
            if (dynamic_cast<juce::Slider*> (c->widget.get()) != nullptr)
            {
                c->widget->setBounds (slot.reduced (2, 0));
                continue;
            }
            // Menus and switches sit at knob-centre height so a row reads as one line.
            const int w = dynamic_cast<juce::ComboBox*> (c->widget.get()) != nullptr ? c->width - 10 : 46;
            c->widget->setBounds (slot.getCentreX() - w / 2, slot.getY() + (slot.getHeight() - 18) / 2 - 13, w, 26);
        }
        y += rowH;
    }
}

//==============================================================================
SynthTab::SynthTab (EvoCtrlProcessor& proc, const layout::Tab& tab)
{
    for (const auto& columnDef : tab.columns)
    {
        auto& column = columns.emplace_back();
        for (const auto& panelDef : columnDef)
            content.addAndMakeVisible (*column.emplace_back (std::make_unique<Panel> (proc, panelDef)));
    }
    viewport.setViewedComponent (&content, false);
    addAndMakeVisible (viewport);
}

void SynthTab::resized()
{
    viewport.setBounds (getLocalBounds());

    // Natural size first, then columns and panels stretch to fill the page like a hardware front panel.
    std::vector<int> widths;
    int naturalW = pad, naturalH = 0;
    for (const auto& column : columns)
    {
        int w = 0, h = -pad;
        for (const auto& p : column)
        {
            w = juce::jmax (w, p->preferredWidth());
            h += p->preferredHeight() + pad;
        }
        widths.push_back (w);
        naturalW += w + pad;
        naturalH = juce::jmax (naturalH, h);
    }

    const int W = juce::jmax (naturalW, getWidth()), H = juce::jmax (naturalH + 2 * pad, getHeight());
    const int gaps = pad * ((int) columns.size() + 1);
    const double sx = (double) (W - gaps) / (double) (naturalW - gaps);

    int x = pad;
    for (size_t c = 0; c < columns.size(); ++c)
    {
        auto& column = columns[c];
        const int w = c + 1 == columns.size() ? W - pad - x : juce::roundToInt (widths[c] * sx);
        int prefSum = 0;
        for (const auto& p : column)
            prefSum += p->preferredHeight();
        const int available = H - 2 * pad - pad * ((int) column.size() - 1);

        int y = pad;
        for (size_t i = 0; i < column.size(); ++i)
        {
            const int h = i + 1 == column.size() ? H - pad - y
                                                 : juce::roundToInt ((double) column[i]->preferredHeight() * available / prefSum);
            column[i]->setBounds (x, y, w, h);
            y += h + pad;
        }
        x += w + pad;
    }
    content.setSize (W, H);
}

//==============================================================================
SequencerPage::SequencerPage (EvoCtrlProcessor& p) : proc (p), settings (p, layout::seqSettings())
{
    addAndMakeVisible (settings);

    for (size_t row = 0; row < dests.size(); ++row)
    {
        const auto& param = evo::params[(size_t) layout::seqDests[row]];
        fillMenu (dests[row], param);
        dests[row].setTooltip (tooltipFor (param));
        destAttachments[row] = std::make_unique<juce::ComboBoxParameterAttachment> (proc.param (param.index), dests[row]);
        addAndMakeVisible (dests[row]);
    }

    for (int i = 0; i < evo::numSeqSteps; ++i)
    {
        auto& s = steps[(size_t) i];
        s.setSliderStyle (juce::Slider::LinearBarVertical);
        s.setRange (0, evo::seqStepMax, 1);
        s.setDoubleClickReturnValue (true, 0);
        s.setColour (juce::Slider::textBoxTextColourId, colours::silk);
        enableFineDrag (s);
        s.onValueChange = [this, i] { proc.setSeqStep (i, (int) steps[(size_t) i].getValue()); };
        addAndMakeVisible (s);
    }

    help.setText ("Steps 0-100 (24 = one octave on an oscillator destination). Values above 100 (rst / off) "
                  "received from the Evolver are kept and shown in white; dragging such a step overwrites it.",
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
        s.setColour (juce::Slider::trackColourId, special ? colours::silk.withAlpha (0.6f) : colours::led);
        s.setTooltip ("Seq " + juce::String (i / 16 + 1) + " step " + juce::String (i % 16 + 1)
                      + (special ? "  -  raw " + juce::String (v) + " (rst / off)" : juce::String()));
    }
}

void SequencerPage::paint (juce::Graphics& g)
{
    drawPanel (g, grid.toFloat(), juce::String::fromUTF8 ("Steps  ·  4 x 16"));
    g.setFont (spaced (10.5f));
    g.setColour (colours::silk);
    for (size_t row = 0; row < dests.size(); ++row)
    {
        const auto b = dests[row].getBounds();
        g.drawText ("SEQ " + juce::String ((int) row + 1) + " DEST", b.getX(), b.getY() - 18, b.getWidth(), 16, juce::Justification::centredLeft);
    }
    g.setColour (colours::yellow);
    for (int i = 0; i < 16; ++i)
    {
        const auto b = steps[(size_t) i].getBounds();
        g.drawText (juce::String (i + 1), b.getX(), b.getY() - 16, b.getWidth(), 14, juce::Justification::centred);
    }
}

void SequencerPage::resized()
{
    auto r = getLocalBounds().reduced (pad);
    settings.setBounds (r.removeFromTop (settings.preferredHeight()));
    r.removeFromTop (pad);
    help.setBounds (r.removeFromBottom (32));
    grid = r;

    auto inner = r.reduced (pad + 4).withTrimmedTop (titleH + 12);
    const auto left = inner.removeFromLeft (180);
    const int rowHt = inner.getHeight() / 4, colW = inner.getWidth() / 16;
    for (int row = 0; row < 4; ++row)
        dests[(size_t) row].setBounds (left.getX(), inner.getY() + row * rowHt + rowHt / 2 - 4, left.getWidth() - 20, 26);
    for (int i = 0; i < evo::numSeqSteps; ++i)
        steps[(size_t) i].setBounds (juce::Rectangle<int> (inner.getX() + (i % 16) * colW, inner.getY() + (i / 16) * rowHt, colW, rowHt).reduced (2));
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
    nameEditor.onTextChange = [this] { proc.patchName = nameEditor.getText(); ++proc.changeCounter; }; // refreshes the LCD
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
    g.setColour (e.favourite ? colours::accent : colours::edge);
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

    intervalSlider.setRange (EvolverLink::minIntervalMs, 50, 1);
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

    // Transparent pages: the editor paints the chassis and the blue overlay behind them.
    const auto clear = juce::Colours::transparentBlack;
    tabs.setTabBarDepth (38);
    tabs.setOutline (0);
    for (const auto& t : layout::tabs())
        tabs.addTab (t.name, clear, new SynthTab (proc, t), true);
    tabs.addTab ("SEQUENCER", clear, seqPage = new SequencerPage (proc), true);
    tabs.addTab ("PATCHES", clear, patchesPage = new PatchesPage (proc), true);
    tabs.addTab ("SETUP", clear, new SetupPage (proc), true);
    tabs.addTab ("MONITOR", clear, monitorPage = new MonitorPage (proc), true);
    addAndMakeVisible (tabs);
    setLookAndFeel (&laf); // last: slider text boxes copy their colours when the look-and-feel reaches them

    setResizable (true, true);
    setResizeLimits (1040, 640, 2560, 1600);
    setSize (1240, 800);
    for (int i = 0; i < evo::numParams; ++i)
        seen[(size_t) i] = proc.param (i).get();
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

    // Like the hardware, the LED shows the parameter that just moved (mouse, Live automation or an Evolver knob).
    int changed = 0, last = -1;
    for (int i = 0; i < evo::numParams; ++i)
        if (const int v = proc.param (i).get(); v != seen[(size_t) i])
        {
            seen[(size_t) i] = v;
            ++changed;
            last = i;
        }
    const auto now = juce::Time::getMillisecondCounter();
    if (changed > 0 || (touched >= 0 && now - touchedAt > 2500))
    {
        touched = changed == 1 ? last : -1; // several at once = a whole patch arrived: back to the program number
        touchedAt = now;
        repaint (lcd);
    }

    const int c = proc.changeCounter.load();
    if (c == lastChange)
        return;
    lastChange = c;

    statusLabel.setText (proc.status.text, juce::dontSendNotification);
    statusLabel.setTooltip (proc.status.text);
    seqPage->refresh();
    patchesPage->refresh();
    repaint (statusDot);
    repaint (lcd);
}

void EvoCtrlEditor::paint (juce::Graphics& g)
{
    const int chassisH = tabs.getY() + tabs.getTabBarDepth();
    const float w = (float) getWidth(), h = (float) getHeight();

    // Blue Lexan overlay with a faint diagonal sheen.
    g.setGradientFill (juce::ColourGradient::vertical (colours::blueTop, (float) chassisH, colours::blueBottom, h));
    g.fillRect (getLocalBounds().withTrimmedTop (chassisH));
    juce::Path sheen;
    sheen.addQuadrilateral (w * 0.35f, (float) chassisH, w * 0.55f, (float) chassisH, w * 0.25f, h, w * 0.05f, h);
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    g.fillPath (sheen);

    // Black brushed-metal chassis.
    g.setColour (colours::chassis);
    g.fillRect (0, 0, getWidth(), chassisH);
    g.setColour (juce::Colours::white.withAlpha (0.018f));
    for (int y = 0; y < chassisH; y += 3)
        g.drawHorizontalLine (y, 0.0f, w);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (chassisH - 1, 0.0f, w);
    drawScrew (g, { 12.0f, 12.0f });
    drawScrew (g, { w - 12.0f, 12.0f });
    drawScrew (g, { 12.0f, (float) tabs.getY() - 10.0f });
    drawScrew (g, { w - 12.0f, (float) tabs.getY() - 10.0f });

    auto area = lcd;
    auto logo = area.removeFromLeft (170).reduced (0, 6);
    g.setColour (colours::silk);
    g.setFont (juce::Font (juce::FontOptions (32.0f, juce::Font::bold | juce::Font::italic)));
    g.drawText ("EvoCtrl", logo.removeFromTop (38), juce::Justification::bottomLeft);
    g.setColour (colours::yellow);
    g.setFont (spaced (9.5f));
    g.drawText ("EVOLVER EDITOR", logo.removeFromTop (16), juce::Justification::centredLeft);

    // Red 3-digit LED: program number, or the value of the parameter that just moved.
    area.removeFromLeft (pad);
    const auto ledBox = area.removeFromLeft (130).withSizeKeepingCentre (130, 62).toFloat();
    g.setColour (colours::recess);
    g.fillRoundedRectangle (ledBox, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (ledBox.reduced (0.5f), 5.0f, 1.0f);
    const auto* p = touched >= 0 ? &evo::params[(size_t) touched] : nullptr;
    drawSevenSegment (g, ledBox.reduced (14.0f, 11.0f), p != nullptr ? ledValue (*p, seen[(size_t) touched]) : juce::String (proc.program + 1));

    area.removeFromLeft (14);
    auto readout = area.withSizeKeepingCentre (area.getWidth(), 44);
    g.setColour (colours::silk);
    g.setFont (spaced (15.0f));
    g.drawText (p != nullptr ? juce::String (p->name).toUpperCase()
                             : proc.patchName.isEmpty() ? juce::String ("UNTITLED") : proc.patchName.toUpperCase(),
                readout.removeFromTop (24), juce::Justification::centredLeft, true);
    g.setColour (colours::yellow);
    g.setFont (spaced (11.5f));
    g.drawText (p != nullptr ? juce::String (evo::valueToText (*p, seen[(size_t) touched])).toUpperCase()
                             : "BANK " + juce::String (proc.bank + 1) + juce::String::fromUTF8 ("  ·  PROGRAM ")
                                   + juce::String (proc.program + 1).paddedLeft ('0', 3),
                readout, juce::Justification::centredLeft, true);

    using K = EvoCtrlProcessor::StatusKind;
    const auto k = proc.status.kind;
    const auto dotColour = k == K::Ok ? juce::Colour (0xff3ccf6e) : k == K::Warning ? juce::Colour (0xffffc23a)
                         : k == K::Error ? colours::led : colours::ledOff.brighter (0.4f);
    g.setColour (dotColour.withAlpha (0.3f));
    g.fillEllipse (statusDot.toFloat().expanded (4.0f));
    g.setColour (dotColour);
    g.fillEllipse (statusDot.toFloat());
}

void EvoCtrlEditor::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (96).reduced (pad * 3, 14);
    lcd = bar.removeFromLeft (560);
    bar.removeFromLeft (pad * 2);

    auto ports = bar.removeFromTop (28);
    const auto take = [] (juce::Rectangle<int>& area, juce::Component& c, int w) { c.setBounds (area.removeFromLeft (w).reduced (2, 0)); };
    const int boxW = juce::jlimit (110, 220, (ports.getWidth() - 230) / 2);
    take (ports, inLabel, 28);
    take (ports, inBox, boxW);
    take (ports, outLabel, 36);
    take (ports, outBox, boxW);
    take (ports, chLabel, 30);
    take (ports, channelBox, 60);
    take (ports, rescanBtn, 76);

    bar.removeFromTop (12);
    auto actions = bar.removeFromTop (28);
    sendBtn.setBounds (actions.removeFromRight (124).reduced (2, 0));
    getBtn.setBounds (actions.removeFromRight (132).reduced (2, 0));
    testBtn.setBounds (actions.removeFromRight (60).reduced (2, 0));
    statusDot = actions.removeFromLeft (22).withSizeKeepingCentre (10, 10);
    statusLabel.setBounds (actions);

    tabs.setBounds (r.withTrimmedLeft (pad).withTrimmedRight (pad));
}
