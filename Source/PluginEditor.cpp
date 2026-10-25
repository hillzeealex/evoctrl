#include "PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>

// Art direction: the Evolver desktop itself - black metal box, blue Lexan overlay silkscreened in white
// (secondary legends in yellow), red LEDs and a red 3-digit LED readout.
namespace colours
{
    const juce::Colour chassis { 0xff0d0e10 }, blueTop { 0xff1f4596 }, blueBottom { 0xff11295f },
                       silk { 0xffeef2f8 }, yellow { 0xffffd447 }, led { 0xffff2b1c }, ledOff { 0xff3a0d0a },
                       recess { 0xff050608 }, key { 0xff26282d };
    const juce::Colour text = silk, accent = yellow, dim { 0xff9aabcf }, edge { 0xff5170b4 };
}

constexpr int woodW = 22; // walnut cheeks, like the Evolver keyboard
constexpr int knobW = 66, comboW = 116, rowH = 86, envH = 62, titleH = 28, tagW = 54, pad = 8;

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

// Wordmark in the spirit of the Evolver panel logo: mixed-size letters on one baseline, slightly
// overlapping, each filled light blue with a white outline (later letters sit over earlier ones).
static void drawWordmark (juce::Graphics& g, juce::Point<float> centre, float H, float fillAlpha, float strokeAlpha)
{
    struct Letter { const char* ch; float size; };
    const Letter spec[] { { "e", 0.72f }, { "V", 0.95f }, { "O", 1.1f }, { "L", 1.6f }, { "v", 0.58f }, { "e", 0.82f }, { "r", 1.6f } };
    std::vector<juce::Path> letters;
    juce::Rectangle<float> all;
    float x = 0.0f;
    for (const auto& l : spec)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (juce::Font (juce::FontOptions ("Futura", H * l.size, juce::Font::bold)), l.ch, 0.0f, 0.0f);
        juce::Path p;
        ga.createPath (p);
        const auto b = p.getBounds();
        p.applyTransform (juce::AffineTransform::translation (x - b.getX(), 0.0f)); // baseline stays at y = 0
        x += b.getWidth() - H * 0.1f;
        all = all.isEmpty() ? p.getBounds() : all.getUnion (p.getBounds());
        letters.push_back (std::move (p));
    }
    const auto shift = juce::AffineTransform::translation (centre.x - all.getCentreX(), centre.y - all.getCentreY());
    for (auto& p : letters)
    {
        p.applyTransform (shift);
        g.setColour (juce::Colour (0xff3d7fe0).withAlpha (fillAlpha));
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (strokeAlpha));
        g.strokePath (p, juce::PathStrokeType (H * 0.035f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
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
    if (p.disp == evo::Disp::Wave)      return juce::String (v + 1);   // waves are numbered 1-128 on the Evolver
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
    int w = tag.isNotEmpty() ? tagW : 0;
    for (const auto& c : controls)
        w += c->width;
    return w;
}

Panel::Panel (EvoCtrlProcessor& proc, const layout::Panel& def) : title (juce::String::fromUTF8 (def.title))
{
    for (const auto& rowDef : def.rows)
    {
        Row row;
        row.tag = juce::String::fromUTF8 (rowDef.tag != nullptr ? rowDef.tag : "");
        row.tagSub = juce::String::fromUTF8 (rowDef.tagSub != nullptr ? rowDef.tagSub : "");
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
    for (const auto& row : rows)
    {
        if (row.tag.isEmpty())
            continue;
        auto r = row.tagArea;
        g.setColour (colours::silk);
        g.setFont (spaced (13.0f));
        g.drawText (row.tag, r.removeFromTop (r.getHeight() / 2 + 2), juce::Justification::bottomLeft);
        g.setColour (colours::yellow);
        g.setFont (spaced (10.0f));
        g.drawText (row.tagSub, r, juce::Justification::topLeft);
    }
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

        // Named rows (OSC 1, LFO 2...) line up on the left like silkscreened rows; the others are centred.
        int x = row.tag.isNotEmpty() ? pad * 2 : (getWidth() - row.width()) / 2;
        if (row.tag.isNotEmpty())
        {
            row.tagArea = { x, y, tagW, rowH - 20 };
            x += tagW;
        }
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
SynthTab::SynthTab (EvoCtrlProcessor& proc, const layout::Tab& tab, const SynthTab* like) : scaleLike (like)
{
    for (const auto& rowDef : tab.rows)
    {
        auto& row = rows.emplace_back();
        for (const auto& panelDef : rowDef)
            content.addAndMakeVisible (*row.emplace_back (std::make_unique<Panel> (proc, panelDef)));
    }

    // Natural layout: each row of panels is stretched to the widest row, so the panel edges line up.
    std::vector<int> rowWidths;
    int naturalW = 0;
    for (const auto& row : rows)
    {
        int w = -pad;
        for (const auto& p : row)
            w += p->preferredWidth() + pad;
        rowWidths.push_back (w);
        naturalW = juce::jmax (naturalW, w);
    }

    int y = pad * 2;
    for (size_t r = 0; r < rows.size(); ++r)
    {
        auto& row = rows[r];
        int h = 0;
        for (const auto& p : row)
            h = juce::jmax (h, p->preferredHeight());
        const int gaps = pad * ((int) row.size() - 1);
        const double stretch = (double) (naturalW - gaps) / (double) (rowWidths[r] - gaps);

        int x = pad;
        for (size_t i = 0; i < row.size(); ++i)
        {
            const int w = i + 1 == row.size() ? naturalW + pad - x : juce::roundToInt (row[i]->preferredWidth() * stretch);
            row[i]->setBounds (x, y, w, h);
            x += w + pad;
        }
        y += h + pad * 2;
    }
    content.setSize (naturalW + pad * 2, y);
    addAndMakeVisible (content);
}

void SynthTab::resized()
{
    const auto& ref = (scaleLike != nullptr ? scaleLike->content : content).getLocalBounds();
    const float s = juce::jmin ((float) getWidth() / (float) ref.getWidth(), (float) getHeight() / (float) ref.getHeight());
    content.setTransform (juce::AffineTransform::scale (s).translated (((float) getWidth() - (float) content.getWidth() * s) / 2.0f,
                                                                       scaleLike != nullptr ? 0.0f // smaller pages sit at the top
                                                                                            : ((float) getHeight() - (float) content.getHeight() * s) / 2.0f));
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

    startBtn.setTooltip ("Start or stop the Evolver's own sequencer (its Start/Stop button)");
    startBtn.onClick = [this] { proc.startStopSequencer(); };
    resetBtn.setTooltip ("Back to step 1 (its Reset button)");
    resetBtn.onClick = [this] { proc.resetSequencer(); };
    addAndMakeVisible (startBtn);
    addAndMakeVisible (resetBtn);

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
    auto transport = settings.getBounds().reduced (pad * 2).withTrimmedTop (titleH - pad).removeFromRight (160);
    startBtn.setBounds (transport.removeFromTop (30));
    transport.removeFromTop (8);
    resetBtn.setBounds (transport.removeFromTop (30));
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
WaveView::WaveView (bool canEdit) : editable (canEdit)
{
    setMouseCursor (canEdit ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> WaveView::plotArea() const
{
    auto r = getLocalBounds().toFloat().reduced (editable ? 14.0f : 4.0f);
    if (! editable)
        r.removeFromBottom (12.0f);
    return r;
}

void WaveView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (colours::recess);
    g.fillRoundedRectangle (r, editable ? 6.0f : 3.0f);
    if (selected)
    {
        g.setColour (colours::yellow);
        g.drawRoundedRectangle (r.reduced (0.75f), 3.0f, 1.5f);
    }

    const auto a = plotArea();
    g.setColour (colours::blueTop.withAlpha (0.45f));
    if (editable)
        for (int i = 0; i <= 4; ++i)
            g.drawVerticalLine (juce::roundToInt (a.getX() + a.getWidth() * (float) i / 4.0f), a.getY(), a.getBottom());
    g.drawHorizontalLine (juce::roundToInt (a.getCentreY()), a.getX(), a.getRight());

    if (known)
    {
        juce::Path p;
        for (size_t i = 0; i < wave.size(); ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / 127.0f;
            const float y = a.getCentreY() - (float) wave[i] / 32768.0f * a.getHeight() / 2.0f;
            i == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
        }
        if (editable)
        {
            auto fill = p;
            fill.lineTo (a.getRight(), a.getCentreY());
            fill.lineTo (a.getX(), a.getCentreY());
            fill.closeSubPath();
            g.setColour (colours::led.withAlpha (0.14f));
            g.fillPath (fill);
            g.setColour (colours::led.withAlpha (0.3f));
            g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (colours::led);
        g.strokePath (p, juce::PathStrokeType (editable ? 2.5f : 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (colours::silk.withAlpha (0.3f));
        g.setFont (monoFont (editable ? 16.0f : 11.0f));
        g.drawText (editable ? "DRAW A WAVE HERE, OR LOAD ONE" : "?", a, juce::Justification::centred);
    }

    if (editable && label.isNotEmpty())
    {
        g.setColour (colours::yellow);
        g.setFont (spaced (12.0f));
        g.drawText (label, r.reduced (18.0f, 8.0f).removeFromTop (16.0f), juce::Justification::centredLeft);
    }
    if (! editable)
    {
        const auto row = r.reduced (5.0f, 2.0f).removeFromBottom (12.0f);
        g.setFont (monoFont (10.0f));
        g.setColour (colours::yellow);
        g.drawText (juce::String (number + 1), row, juce::Justification::centredLeft);
        g.setColour (colours::led.withAlpha (known ? 0.9f : 0.4f));
        g.drawText (label, row, juce::Justification::centredRight);
    }
}

void WaveView::drawTo (juce::Point<float> p)
{
    const auto a = plotArea();
    const auto index = [&a] (float x) { return juce::jlimit (0.0f, 127.0f, (x - a.getX()) / a.getWidth() * 127.0f); };
    const auto value = [&a] (float y) { return juce::jlimit (-32767.0f, 32767.0f, (a.getCentreY() - y) / (a.getHeight() / 2.0f) * 32767.0f); };
    const float i0 = index (last.x), i1 = index (p.x), v0 = value (last.y), v1 = value (p.y);
    const int lo = juce::roundToInt (juce::jmin (i0, i1)), hi = juce::roundToInt (juce::jmax (i0, i1));
    for (int i = lo; i <= hi; ++i) // fill every point the stroke crossed, so fast drags leave no gaps
    {
        const float t = hi == lo ? 1.0f : ((float) i - i0) / (i1 - i0);
        wave[(size_t) i] = (int16_t) juce::roundToInt (v0 + juce::jlimit (0.0f, 1.0f, t) * (v1 - v0));
    }
    known = true;
    last = p;
    repaint();
}

void WaveView::mouseDown (const juce::MouseEvent& e)
{
    if (! editable)
        return;
    if (onEditStart)
        onEditStart();
    last = e.position;
    drawTo (e.position);
}

void WaveView::mouseDrag (const juce::MouseEvent& e)
{
    if (editable)
        drawTo (e.position);
}

void WaveView::mouseUp (const juce::MouseEvent& e)
{
    if (editable ? onEditEnd != nullptr : onClick != nullptr && getLocalBounds().contains (e.getPosition()))
        editable ? onEditEnd() : onClick();
}

//==============================================================================
static evo::Waveshape normalised (std::array<float, 128> v)
{
    float peak = 0.0f;
    for (auto x : v)
        peak = juce::jmax (peak, std::abs (x));
    evo::Waveshape w {};
    for (size_t i = 0; i < v.size(); ++i)
        w[i] = (int16_t) juce::roundToInt (peak > 0.0f ? v[i] / peak * 32000.0f : 0.0f);
    return w;
}

WavesPage::WavesPage (EvoCtrlProcessor& p) : proc (p)
{
    addAndMakeVisible (editor);
    editor.onEditStart = [this] { beginEdit(); };
    editor.onEditEnd = [this] { endEdit(); };

    for (size_t k = 0; k < harmonics.size(); ++k)
    {
        auto& s = harmonics[k];
        s.setSliderStyle (juce::Slider::LinearBarVertical);
        s.setRange (0, 100, 1);
        s.setColour (juce::Slider::textBoxTextColourId, colours::silk);
        s.setTooltip ("Harmonic " + juce::String ((int) k + 1));
        s.onDragStart = [this] { beginEdit(); };
        s.onDragEnd = [this] { endEdit(); };
        s.onValueChange = [this] { if (! settingHarmonics) fromHarmonics(); };
        addAndMakeVisible (s);
    }

    // Starting points, as harmonic recipes (amplitude per harmonic 1-16, sign = phase).
    const std::vector<std::pair<juce::String, std::function<float (int)>>> presets {
        { "SINE",     [] (int k) { return k == 1 ? 1.0f : 0.0f; } },
        { "SAW",      [] (int k) { return 1.0f / (float) k; } },
        { "SQUARE",   [] (int k) { return k % 2 ? 1.0f / (float) k : 0.0f; } },
        { "TRIANGLE", [] (int k) { return k % 2 ? (((k - 1) / 2) % 2 ? -1.0f : 1.0f) / (float) (k * k) : 0.0f; } },
        { "ORGAN",    [] (int k) { const float v[] { 1, .8f, 0, .6f, 0, .45f, 0, .3f, 0, 0, 0, .2f, 0, 0, 0, .15f }; return v[k - 1]; } },
        { "BELL",     [] (int k) { const float v[] { 1, 0, .7f, 0, 0, .55f, 0, 0, .4f, 0, .3f, 0, 0, .25f, 0, .2f }; return v[k - 1]; } },
    };
    for (const auto& preset : presets)
    {
        auto& b = *presetButtons.emplace_back (std::make_unique<juce::TextButton> (preset.first));
        b.onClick = [this, recipe = preset.second]
        {
            beginEdit();
            settingHarmonics = true;
            for (size_t k = 0; k < harmonics.size(); ++k)
                harmonics[k].setValue (std::abs (recipe ((int) k + 1)) * 100.0, juce::dontSendNotification);
            settingHarmonics = false;
            std::array<float, 128> v {};
            for (size_t n = 0; n < v.size(); ++n)
                for (int k = 1; k <= 16; ++k)
                    v[n] += recipe (k) * std::sin (juce::MathConstants<float>::twoPi * (float) (k * (int) n) / 128.0f);
            setWave (normalised (v));
            endEdit();
        };
        addAndMakeVisible (b);
    }

    const auto tool = [this] (const juce::String& name, std::function<void (evo::Waveshape&)> fn)
    {
        auto& b = *toolButtons.emplace_back (std::make_unique<juce::TextButton> (name));
        b.onClick = [this, fn]
        {
            beginEdit();
            auto w = editor.wave;
            fn (w);
            setWave (w);
            endEdit();
        };
        addAndMakeVisible (b);
    };
    tool ("SMOOTH", [] (evo::Waveshape& w)
    {
        const auto src = w;
        for (size_t i = 0; i < w.size(); ++i) // circular: the wave loops
            w[i] = (int16_t) ((src[(i + 127) % 128] + 2 * src[i] + src[(i + 1) % 128]) / 4);
    });
    tool ("NORMALIZE", [] (evo::Waveshape& w)
    {
        std::array<float, 128> v {};
        for (size_t i = 0; i < w.size(); ++i) v[i] = w[i];
        w = normalised (v);
    });
    tool ("INVERT", [] (evo::Waveshape& w) { for (auto& x : w) x = (int16_t) juce::jlimit (-32767, 32767, -(int) x); });
    tool ("REVERSE", [] (evo::Waveshape& w) { std::reverse (w.begin(), w.end()); });
    {
        auto& b = *toolButtons.emplace_back (std::make_unique<juce::TextButton> ("UNDO"));
        b.onClick = [this] { const auto w = editor.wave; setWave (undo); undo = w; endEdit(); };
        addAndMakeVisible (b);
    }

    importBtn.setTooltip ("Load a single-cycle .wav (any length, resampled to 128 points)");
    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import a single-cycle wave", juce::File(), "*.wav;*.aif;*.aiff");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (fc.getResult()));
            if (reader == nullptr || reader->lengthInSamples < 2)
                return;
            const int n = (int) juce::jmin<juce::int64> (reader->lengthInSamples, 1 << 16);
            juce::AudioBuffer<float> buffer (1, n);
            reader->read (&buffer, 0, n, 0, true, false);
            std::array<float, 128> v {};
            for (size_t i = 0; i < v.size(); ++i) // linear resample of the whole file to one 128-point cycle
            {
                const float pos = (float) i * (float) (n - 1) / 127.0f;
                const int i0 = (int) pos, i1 = juce::jmin (i0 + 1, n - 1);
                v[i] = juce::jmap (pos - (float) i0, buffer.getSample (0, i0), buffer.getSample (0, i1));
            }
            beginEdit();
            setWave (normalised (v));
            endEdit();
        });
    };

    for (auto* b : { &romBtn, &userBtn })
        b->setColour (juce::TextButton::textColourOnId, colours::yellow); // active bank = yellow text (no LED over the label)
    romBtn.setToggleState (true, juce::dontSendNotification);
    romBtn.onClick = [this] { showBank (false); };
    userBtn.onClick = [this] { showBank (true); };

    readBtn.setTooltip ("Read all 128 waveshapes from the Evolver (about 15 s; kept for next time)");
    readBtn.onClick = [this] { proc.readWaves (false); };

    sendBtn.setTooltip ("Write the wave on screen into this user wave of the Evolver (97-128), replacing what is there. "
                        "Any program can then use it by setting an Osc 3/4 Wave to this number.");
    sendBtn.onClick = [this]
    {
        const auto& current = proc.waves[(size_t) selectedSlot];
        if (! current || *current == editor.wave)
            return proc.sendWave (selectedSlot, editor.wave);
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "Replace a user wave",
                "Wave " + juce::String (selectedSlot + 1) + " on the Evolver already holds another shape. Replace it?",
                "Replace", "Cancel", this),
            [safe = SafePointer<WavesPage> (this), slot = selectedSlot, wave = editor.wave] (int result)
            {
                if (safe != nullptr && result == 1)
                    safe->proc.sendWave (slot, wave);
            });
    };

    restoreBtn.setTooltip ("Put back the wave this slot held before EvoCtrl first changed it (kept in Documents/EvoCtrl/Waves/originals.syx)");
    restoreBtn.onClick = [this]
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "Restore the original wave",
                "Put the original wave " + juce::String (selectedSlot + 1) + " back on the Evolver? The current version of that slot is replaced "
                "(use Save to library first to keep it).", "Restore", "Cancel", this),
            [safe = SafePointer<WavesPage> (this), slot = selectedSlot] (int result)
            {
                if (safe == nullptr || result != 1)
                    return;
                safe->proc.restoreOriginal (slot);
                if (const auto& w = safe->proc.waves[(size_t) slot])
                    safe->setWave (*w);
            });
    };
    // Manual p.24: "The Evolver ships with waves 97 - 128 the same as 1 - 32".
    factoryBtn.setTooltip ("Put back the factory content of this user wave: a copy of ROM wave (number - 96)");
    factoryBtn.onClick = [this]
    {
        const int rom = selectedSlot - evo::firstUserWave;
        if (! proc.waves[(size_t) rom])
            return showAlert (this, "Factory default", "ROM wave " + juce::String (rom + 1) + " has not been read yet: press READ ALL first.");
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::QuestionIcon, "Factory default",
                "Reset wave " + juce::String (selectedSlot + 1) + " to its factory content (a copy of ROM wave " + juce::String (rom + 1)
                + ")? The current version of that slot is replaced.", "Reset", "Cancel", this),
            [safe = SafePointer<WavesPage> (this), slot = selectedSlot, rom] (int result)
            {
                if (safe == nullptr || result != 1 || ! safe->proc.waves[(size_t) rom])
                    return;
                const auto factory = *safe->proc.waves[(size_t) rom];
                safe->proc.sendWave (slot, factory);
                safe->setWave (factory);
            });
    };
    saveBtn.setTooltip ("Save the edited wave as a .syx file in Documents/EvoCtrl/Waves");
    saveBtn.onClick = [this]
    {
        const auto dir = EvoCtrlProcessor::wavesFolder();
        dir.createDirectory();
        PatchLibrary::writeSyx (dir.getNonexistentChildFile ("Wave " + juce::String (selectedSlot + 1), ".syx", false),
                                evo::waveshapeDump (selectedSlot, editor.wave));
    };

    oscBox.addItem ("OSC 3", 1);
    oscBox.addItem ("OSC 4", 2);
    oscBox.setSelectedId (1, juce::dontSendNotification);
    oscBox.setTooltip ("The oscillator whose wave this page shows and plays");
    oscBox.onChange = [this] { dirty = false; follow (true); };
    auditionBtn.setTooltip ("Send the wave to its slot and select that slot on the chosen digital oscillator");
    auditionBtn.onClick = [this]
    {
        proc.sendWave (selectedSlot, editor.wave);
        auto& shape = proc.param (oscBox.getSelectedId() == 1 ? 10 : 14);   // Osc 3 / Osc 4 Shape: 0-127 = waves 1-128
        shape.beginChangeGesture();
        shape.setValueNotifyingHost (shape.convertTo0to1 ((float) selectedSlot));
        shape.endChangeGesture();
    };
    liveToggle.setTooltip ("Each stroke, preset or tool is stored in the Evolver's user slot when you release it");

    progress.setColour (juce::Label::textColourId, colours::dim);
    progress.setFont (font (13.0f));

    for (int i = 0; i < evo::numWaves; ++i)
    {
        auto& t = tiles[(size_t) i];
        t = std::make_unique<WaveView> (false);
        t->number = i;
        t->onClick = [this, i]
        {
            if (i >= evo::firstUserWave)
                selectedSlot = i;
            if (proc.waves[(size_t) i])
            {
                beginEdit();
                setWave (*proc.waves[(size_t) i]);
                loadedFrom = i;
                editor.label = "WAVE " + juce::String (i + 1) + (i < evo::firstUserWave ? "  ·  ROM: edit a copy, then send it to a user slot" : "  ·  USER");
            }
            refresh();
        };
        tilesContent.addChildComponent (*t);
    }
    startTimerHz (5);
    tilesView.setViewedComponent (&tilesContent, false);
    tilesView.setScrollBarsShown (true, false);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &importBtn, &romBtn, &userBtn, &readBtn, &sendBtn, &restoreBtn, &factoryBtn, &saveBtn, &auditionBtn, &oscBox, &liveToggle, &progress, &tilesView })
        addAndMakeVisible (c);
    refresh();
}

void WavesPage::setWave (const evo::Waveshape& w)
{
    editor.wave = w;
    editor.known = true;
    editor.repaint();
    refresh();
}

void WavesPage::endEdit()
{
    if (liveToggle.getToggleState())
        proc.sendWave (selectedSlot, editor.wave);
    refresh();
}

void WavesPage::fromHarmonics()
{
    std::array<float, 128> v {};
    for (size_t n = 0; n < v.size(); ++n)
        for (size_t k = 0; k < harmonics.size(); ++k)
            v[n] += (float) harmonics[k].getValue() / 100.0f
                  * std::sin (juce::MathConstants<float>::twoPi * (float) ((k + 1) * n) / 128.0f);
    editor.wave = normalised (v);
    editor.known = true;
    editor.repaint();
}

void WavesPage::visibilityChanged()
{
    if (! isShowing())
        return;
    // Fetch whatever is not cached yet (all 128 on the first visit), then show the program's wave.
    if (proc.wavesKnown() < evo::numWaves && ! proc.readingWaves() && proc.link.hasInput() && proc.link.hasOutput())
        proc.readWaves (true);
    follow (false);
}

void WavesPage::timerCallback()
{
    const int osc3 = proc.param (10).get(), osc4 = proc.param (14).get();
    if (osc3 == lastOsc3 && osc4 == lastOsc4)
        return;
    lastOsc3 = osc3;
    lastOsc4 = osc4;
    follow (false);
    refresh();
}

void WavesPage::showBank (bool user)
{
    showUser = user;
    romBtn.setToggleState (! user, juce::dontSendNotification);
    userBtn.setToggleState (user, juce::dontSendNotification);
    layoutTiles();
    refresh();
}

// Shows the wave the current program plays on the chosen oscillator (unless the user is editing their own).
void WavesPage::follow (bool force)
{
    const int w = proc.param (oscBox.getSelectedId() == 1 ? 10 : 14).get();   // Osc 3/4 Shape: 0-127 = waves 1-128
    if (! force && (dirty || w == followed))
        return;
    followed = loadedFrom = w;
    if (w >= evo::firstUserWave)
        selectedSlot = w;   // a user wave: SEND writes straight back to the slot the sound uses
    editor.known = proc.waves[(size_t) w].has_value();
    if (editor.known)
        editor.wave = *proc.waves[(size_t) w];
    editor.label = oscBox.getText() + "  ·  WAVE " + juce::String (w + 1)
                 + (w < evo::firstUserWave ? "  ·  ROM: edit a copy, then send it to a user slot" : "  ·  USER, editable");
    editor.repaint();
    showBank (w >= evo::firstUserWave);
    tilesView.setViewPosition (0, juce::jmax (0, tiles[(size_t) w]->getY() - 8));
}

void WavesPage::refresh()
{
    // The followed wave arrived after the page showed it as unknown.
    if (! dirty && followed >= 0 && ! editor.known && proc.waves[(size_t) followed])
    {
        editor.wave = *proc.waves[(size_t) followed];
        editor.known = true;
        editor.repaint();
    }
    const int osc3 = proc.param (10).get(), osc4 = proc.param (14).get();
    for (int i = 0; i < evo::numWaves; ++i)
    {
        auto& t = *tiles[(size_t) i];
        const auto& w = proc.waves[(size_t) i];
        t.known = w.has_value();
        if (w)
            t.wave = *w;
        const bool isSlot = i == selectedSlot;
        t.selected = isSlot || i == loadedFrom;
        t.label = i == osc3 ? juce::String ("OSC 3") : i == osc4 ? juce::String ("OSC 4")
                : i < evo::firstUserWave ? juce::String ("ROM") : isSlot ? juce::String ("EDIT")
                : proc.isModified (i) ? juce::String ("MODIFIED") : w ? juce::String ("USER") : juce::String ("EMPTY");
        t.repaint();
    }
    sendBtn.setButtonText ("STORE AS WAVE " + juce::String (selectedSlot + 1));
    restoreBtn.setEnabled (proc.isModified (selectedSlot));
    const auto& factory = proc.waves[(size_t) (selectedSlot - evo::firstUserWave)];
    factoryBtn.setEnabled (! factory || ! proc.waves[(size_t) selectedSlot] || *factory != *proc.waves[(size_t) selectedSlot]);
    progress.setText (proc.readingWaves() ? proc.status.text
                                          : juce::String (proc.wavesKnown()) + " / 128 read  ·  one list for all 4 banks: wave 97 is the same in every program",
                      juce::dontSendNotification);
}

void WavesPage::layoutTiles()
{
    const int first = showUser ? evo::firstUserWave : 0, last = showUser ? evo::numWaves : evo::firstUserWave;
    const int w = tilesView.getWidth() - tilesView.getScrollBarThickness() - 2, cols = 4, gap = 6, h = 62;
    const int tw = (w - gap * (cols - 1)) / cols;
    for (int i = 0; i < evo::numWaves; ++i)
    {
        auto& t = *tiles[(size_t) i];
        const bool shown = i >= first && i < last;
        t.setVisible (shown);
        if (shown)
        {
            const int k = i - first;
            t.setBounds ((k % cols) * (tw + gap), (k / cols) * (h + gap), tw, h);
        }
    }
    tilesContent.setSize (w, ((last - first + cols - 1) / cols) * (h + gap));
}

void WavesPage::paint (juce::Graphics& g)
{
    drawPanel (g, editorPanel.toFloat(), juce::String::fromUTF8 ("Waveshape  ·  draw with the mouse, or use the harmonics"));
    drawPanel (g, harmPanel.toFloat(), juce::String::fromUTF8 ("Harmonics  ·  16 drawbars"));
    drawPanel (g, libPanel.toFloat(), juce::String::fromUTF8 ("Waves  ·  1–128, click to load"));
    drawPanel (g, audPanel.toFloat(), juce::String::fromUTF8 ("Audition  ·  hear it on the Evolver"));
}

void WavesPage::resized()
{
    auto r = getLocalBounds().reduced (pad * 2);
    auto right = r.removeFromRight (juce::jlimit (340, 480, r.getWidth() / 3));
    r.removeFromRight (pad * 2);

    editorPanel = r.removeFromTop (juce::roundToInt ((float) r.getHeight() * 0.6f));
    r.removeFromTop (pad * 2);
    harmPanel = r;

    auto e = editorPanel.reduced (pad * 2).withTrimmedTop (titleH - pad);
    auto tools = e.removeFromBottom (30);
    e.removeFromBottom (pad);
    editor.setBounds (e);
    importBtn.setBounds (tools.removeFromRight (130));
    for (auto& b : toolButtons)
        b->setBounds (tools.removeFromLeft (96).withTrimmedRight (6));

    auto h = harmPanel.reduced (pad * 2).withTrimmedTop (titleH - pad);
    auto presetCol = h.removeFromLeft (110);
    h.removeFromLeft (pad * 2);
    const int bh = juce::jmin (28, (presetCol.getHeight() - 5 * 4) / 6);
    for (auto& b : presetButtons)
    {
        b->setBounds (presetCol.removeFromTop (bh));
        presetCol.removeFromTop (4);
    }
    const int sw = h.getWidth() / (int) harmonics.size();
    for (auto& s : harmonics)
        s.setBounds (h.removeFromLeft (sw).reduced (3, 0));

    audPanel = right.removeFromBottom (110);
    right.removeFromBottom (pad * 2);
    libPanel = right;

    auto l = libPanel.reduced (pad * 2).withTrimmedTop (titleH - pad);
    auto banks = l.removeFromTop (28);
    romBtn.setBounds (banks.removeFromLeft (banks.getWidth() / 2).withTrimmedRight (3));
    userBtn.setBounds (banks.withTrimmedLeft (3));
    l.removeFromTop (pad);
    auto bottom = l.removeFromBottom (34);
    saveBtn.setBounds (bottom.removeFromRight (140));
    sendBtn.setBounds (bottom.withTrimmedRight (6));
    l.removeFromBottom (6);
    auto resets = l.removeFromBottom (28);
    restoreBtn.setBounds (resets.removeFromLeft (resets.getWidth() / 2).withTrimmedRight (3));
    factoryBtn.setBounds (resets.withTrimmedLeft (3));
    l.removeFromBottom (pad);
    auto readRow = l.removeFromBottom (26);
    readBtn.setBounds (readRow.removeFromLeft (100));
    progress.setBounds (readRow.withTrimmedLeft (6));
    l.removeFromBottom (pad);
    tilesView.setBounds (l);
    layoutTiles();

    auto a = audPanel.reduced (pad * 2).withTrimmedTop (titleH - pad);
    auto row = a.removeFromTop (28);
    auditionBtn.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (6);
    oscBox.setBounds (row.removeFromLeft (100));
    a.removeFromTop (6);
    liveToggle.setBounds (a.removeFromTop (24));
}

//==============================================================================
BankBrowser::BankBrowser (EvoCtrlProcessor& p) : proc (p), bank (p.bank)
{
    for (int b = 0; b < 4; ++b)
    {
        auto& btn = bankButtons[(size_t) b];
        btn.setButtonText ("BANK " + juce::String (b + 1));
        btn.setColour (juce::TextButton::textColourOnId, colours::yellow);
        btn.setToggleState (b == bank, juce::dontSendNotification);
        btn.onClick = [this, b] { showBank (b); };
        addAndMakeVisible (btn);
    }
    list.setRowHeight (22);
    readNamesBtn.setTooltip ("Read the 128 program names of this bank from the Evolver (a few seconds; kept for next time)");
    readNamesBtn.onClick = [this] { proc.readNames (bank); };
    loadBtn.setTooltip ("Switch the Evolver to the selected program and load it here (double-click works too). "
                        "Unsaved edits on the Evolver are lost, as with its own program knob.");
    loadBtn.onClick = [this] { load (list.getSelectedRow()); };
    writeBtn.setTooltip ("Store the current patch and its name into the selected program on the Evolver");
    writeBtn.onClick = [this]
    {
        const int row = list.getSelectedRow();
        if (row < 0)
            return;
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "Write to the Evolver",
                "Overwrite bank " + juce::String (bank + 1) + " program " + juce::String (row + 1) + " on the Evolver with the current patch? "
                "This cannot be undone.", "Write", "Cancel", this),
            [safe = SafePointer<BankBrowser> (this), b = bank, row] (int result)
            {
                if (safe != nullptr && result == 1)
                    safe->proc.writeProgram (b, row);
            });
    };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &list, &readNamesBtn, &loadBtn, &writeBtn })
        addAndMakeVisible (c);
}

void BankBrowser::showBank (int b)
{
    bank = b;
    for (int i = 0; i < 4; ++i)
        bankButtons[(size_t) i].setToggleState (i == b, juce::dontSendNotification);
    list.updateContent();
    list.repaint();
    const bool missing = std::any_of (proc.nameKnown.begin() + b * 128, proc.nameKnown.begin() + b * 128 + 128, [] (bool k) { return ! k; });
    if (missing && isShowing() && ! proc.readingNames() && proc.link.hasInput() && proc.link.hasOutput())
        proc.readNames (b);
}

void BankBrowser::refresh()
{
    list.repaint();
}

void BankBrowser::load (int row)
{
    if (row >= 0)
        proc.selectProgram (bank, row);
}

void BankBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool isSelected)
{
    const auto i = (size_t) (bank * 128 + row);
    const bool current = proc.bank == bank && proc.program == row;
    if (isSelected)
        g.fillAll (colours::accent.withAlpha (0.25f));
    else if (row % 2)
        g.fillAll (juce::Colours::white.withAlpha (0.03f));

    g.setFont (monoFont (12.0f));
    g.setColour (current ? colours::yellow : colours::dim);
    g.drawText (juce::String (row + 1).paddedLeft ('0', 3), 8, 0, 40, h, juce::Justification::centredLeft);
    g.setFont (font (14.0f));
    g.setColour (proc.nameKnown[i] ? colours::silk : colours::dim.withAlpha (0.6f));
    g.drawText (! proc.nameKnown[i] ? juce::String ("...") : proc.programNames[i].isEmpty() ? juce::String ("(no name)") : proc.programNames[i],
                52, 0, w - 90, h, juce::Justification::centredLeft, true);
    if (current)
        drawLed (g, juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) w - 18.0f, (float) h / 2.0f }), true);
}

void BankBrowser::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (28);
    const int bw = top.getWidth() / 4;
    for (auto& b : bankButtons)
        b.setBounds (top.removeFromLeft (bw).reduced (2, 0));
    r.removeFromTop (6);
    auto bottom = r.removeFromBottom (28);
    r.removeFromBottom (6);
    readNamesBtn.setBounds (bottom.removeFromLeft (bottom.getWidth() / 3).withTrimmedRight (4));
    loadBtn.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).withTrimmedRight (4));
    writeBtn.setBounds (bottom);
    list.setBounds (r);
}

//==============================================================================
PatchesPage::PatchesPage (EvoCtrlProcessor& p) : proc (p), banks (p)
{
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
             &deviceGroup, &fileGroup, &libGroup, &banks, &nameLabel, &tagsLabel,
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
    banks.refresh();
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
    auto left = r.removeFromLeft (420);
    r.removeFromLeft (pad);

    auto nameRow = row (left);
    nameLabel.setBounds (nameRow.removeFromLeft (50));
    nameEditor.setBounds (nameRow);
    auto tagsRow = row (left);
    tagsLabel.setBounds (tagsRow.removeFromLeft (50));
    tagsEditor.setBounds (tagsRow);
    left.removeFromTop (pad);

    auto files = left.removeFromBottom (68);
    fileGroup.setBounds (files);
    split (files.reduced (12).withTrimmedTop (10).removeFromTop (26), importBtn, exportBtn);
    left.removeFromBottom (pad);
    deviceGroup.setBounds (left);
    banks.setBounds (left.reduced (12).withTrimmedTop (10));

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
        "PARAMETER PATH\n"
        "  Direct: EvoCtrl opens the Evolver MIDI ports itself and sends SysEx: all parameters, dumps,\n"
        "  sequencer, waves. Recommended.\n"
        "  Via Ableton: knob moves go out as CC on this track's MIDI output (route it to the Evolver from\n"
        "  another track). Only parameters with a documented CC, linear scaling not verified on hardware.\n\n"
        "ABLETON LIVE (Preferences > Link, Tempo & MIDI)\n"
        "  Turn Remote OFF for the Evolver ports. To sync the Evolver's sequencer, delays and LFOs to Live:\n"
        "  turn Sync ON for the UltraLite MIDI Out in Live, and set MIDI Clock to 'MIDI clock in' above.\n"
        "  Windows: a MIDI port can only be open in one application (see README).\n");

    // Evolver globals (manual p.53-54). Program/bank (0, 1) are chosen in PATCHES.
    const auto knob = [this] (int index, const char* name, int lo, int hi, int offset, const char* unit)
    {
        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        s->setRange (lo, hi, 1);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobW, 18);
        s->textFromValueFunction = [offset, unit] (double v)
        {
            const int x = (int) v + offset;
            return juce::String (offset != 0 && x > 0 ? "+" : "") + juce::String (x) + unit;
        };
        s->getProperties().set ("bipolar", offset != 0);
        s->onValueChange = [this, index, raw = s.get()] { changed (index, (int) raw->getValue()); };
        enableFineDrag (*s);
        addAndMakeVisible (*s);
        globalKnobs[(size_t) index] = std::move (s);
        globalLabels[(size_t) index].setText (name, juce::dontSendNotification);
        globalLabels[(size_t) index].setJustificationType (juce::Justification::centred);
    };
    const auto menu = [this] (int index, const char* name, juce::StringArray items)
    {
        auto c = std::make_unique<juce::ComboBox>();
        c->addItemList (items, 1);
        c->onChange = [this, index, raw = c.get()] { changed (index, raw->getSelectedId() - 1); };
        addAndMakeVisible (*c);
        globalMenus[(size_t) index] = std::move (c);
        globalLabels[(size_t) index].setText (name, juce::dontSendNotification);
        globalLabels[(size_t) index].setJustificationType (juce::Justification::centredRight);
    };
    knob (2, "MASTER VOLUME", 0, 100, 0, "");
    knob (3, "TRANSPOSE", 0, 72, -36, " st");
    knob (11, "FINE TUNE", 0, 100, -50, " ct");
    knob (4, "BPM", 30, 250, 0, "");
    menu (12, "MIDI REC", { "Off", "All", "Program changes", "Parameters" });
    menu (13, "MIDI XMIT", { "Off", "All", "Program changes", "Parameters" });
    juce::StringArray channels { "All" };
    for (int c = 1; c <= 16; ++c)
        channels.add (juce::String (c));
    menu (14, "MIDI CHANNEL", channels);
    menu (9, "POLY CHAIN", { "Off", "Echo all MIDI", "Echo notes only" });
    menu (7, "MIDI CLOCK", { "Internal", "Internal + send clock", "MIDI clock in", "MIDI clock in + thru", "No change", "No change",
                             "MIDI clock in, ignore start/stop" });
    menu (6, "USE PROGRAM TEMPO", { "Off", "On" });
    juce::StringArray divs;
    for (auto* d : evo::clockDivs)
        divs.add (d);
    menu (5, "CLOCK DIVIDE", divs);
    menu (8, "LOCK SEQUENCE", { "Off", "On" });
    menu (10, "INPUT GAIN", { "0 dB", "+3 dB", "+6 dB", "+9 dB", "+12 dB", "+15 dB", "+18 dB", "+21 dB", "+24 dB" });
    for (auto& l : globalLabels)
    {
        l.setFont (spaced (10.5f));
        addChildComponent (l);
    }
    for (int i = 2; i < evo::numMainParams; ++i)
        globalLabels[(size_t) i].setVisible (true);

    readGlobalsBtn.setTooltip ("Read the Evolver's global settings (also done each time this page opens)");
    readGlobalsBtn.onClick = [this] { proc.readGlobals(); };
    globalsStatus.setFont (spaced (12.0f));
    addAndMakeVisible (readGlobalsBtn);
    addAndMakeVisible (globalsStatus);
    refresh();

    for (juce::Component* c : std::initializer_list<juce::Component*> { &modeLabel, &intervalLabel, &modeBox, &intervalSlider, &resendToggle, &info })
        addAndMakeVisible (c);
}

void SetupPage::changed (int index, int value)
{
    if (updating)
        return;
    if ((index != 12 && index != 13) || value == 1)
        return proc.setGlobal (index, value);
    // Anything but ALL on MIDI Rec / Xmit takes away dumps or two-way sync.
    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions::makeOptionsOkCancel (juce::MessageBoxIconType::WarningIcon, "This may disconnect EvoCtrl",
            juce::String (index == 12 ? "MIDI Rec" : "MIDI Xmit") + " other than ALL stops part of what EvoCtrl needs "
            "(dumps, knob sync). You may have to set it back from the Evolver's Main menu. Continue?", "Continue", "Cancel", this),
        [safe = SafePointer<SetupPage> (this), index, value] (int result)
        {
            if (safe == nullptr)
                return;
            if (result == 1)
                safe->proc.setGlobal (index, value);
            else
                safe->refresh();
        });
}

void SetupPage::visibilityChanged()
{
    if (isShowing())
        proc.readGlobals();
}

void SetupPage::refresh()
{
    updating = true;
    for (int i = 0; i < evo::numMainParams; ++i)
    {
        const int v = proc.globals[(size_t) i];
        if (auto& k = globalKnobs[(size_t) i])
        {
            k->setValue (v, juce::dontSendNotification);
            k->setEnabled (proc.globalsKnown);
        }
        if (auto& m = globalMenus[(size_t) i])
        {
            m->setSelectedId (v + 1, juce::dontSendNotification);
            m->setEnabled (proc.globalsKnown);
        }
    }
    updating = false;

    const auto& g = proc.globals;
    juce::StringArray issues;
    if (proc.globalsKnown)
    {
        if (g[9] != 0)  issues.add ("Poly Chain should be Off");
        if (g[12] != 1) issues.add ("MIDI Rec should be All");
        if (g[13] != 1) issues.add ("MIDI Xmit should be All");
    }
    globalsStatus.setColour (juce::Label::textColourId, issues.isEmpty() ? juce::Colour (0xff3ccf6e) : colours::led);
    globalsStatus.setText (! proc.globalsKnown ? juce::String ("Not read yet: connect the Evolver, then READ")
                           : issues.isEmpty() ? juce::String ("Read from the Evolver  -  all good for EvoCtrl")
                                              : issues.joinIntoString ("  -  "),
                           juce::dontSendNotification);
}

void SetupPage::paint (juce::Graphics& g)
{
    drawPanel (g, globalsPanel.toFloat(), juce::String::fromUTF8 ("Evolver globals  ·  Main menu, kept by the Evolver"));
    drawPanel (g, appPanel.toFloat(), juce::String::fromUTF8 ("EvoCtrl  ·  MIDI path and options"));
}

void SetupPage::resized()
{
    auto all = getLocalBounds().reduced (pad * 2);
    globalsPanel = all.removeFromTop (juce::jmin (340, all.getHeight() * 55 / 100));
    all.removeFromTop (pad * 2);
    appPanel = all;

    auto gp = globalsPanel.reduced (pad * 2).withTrimmedTop (titleH - pad);
    auto top = gp.removeFromTop (rowH);
    for (int i : { 2, 3, 11, 4 })
    {
        auto slot = top.removeFromLeft (knobW + 24);
        globalLabels[(size_t) i].setBounds (slot.removeFromBottom (18));
        globalKnobs[(size_t) i]->setBounds (slot);
    }
    top.removeFromLeft (pad * 3);
    auto side = top.withSizeKeepingCentre (top.getWidth(), 64);
    readGlobalsBtn.setBounds (side.removeFromTop (28).removeFromLeft (190));
    side.removeFromTop (8);
    globalsStatus.setBounds (side);
    gp.removeFromTop (pad);

    const int order[] { 12, 13, 14, 9, 7, 6, 5, 8, 10 };
    const int colW = gp.getWidth() / 3;
    for (int k = 0; k < 9; ++k)
    {
        auto cell = juce::Rectangle<int> (gp.getX() + (k % 3) * colW, gp.getY() + (k / 3) * 34, colW - 14, 26);
        globalLabels[(size_t) order[k]].setBounds (cell.removeFromLeft (140));
        globalMenus[(size_t) order[k]]->setBounds (cell.withTrimmedLeft (8));
    }

    auto r = appPanel.reduced (pad * 2).withTrimmedTop (titleH - pad);
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
    statusLabel.setFont (font (13.5f));

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
    auto* synth = new SynthTab (proc, layout::tabs()[0]);
    tabs.addTab (layout::tabs()[0].name, clear, synth, true);
    tabs.addTab ("SEQUENCER", clear, seqPage = new SequencerPage (proc), true);
    tabs.addTab ("WAVES", clear, wavesPage = new WavesPage (proc), true);
    tabs.addTab (layout::tabs()[1].name, clear, new SynthTab (proc, layout::tabs()[1], synth), true);
    tabs.addTab ("PATCHES", clear, patchesPage = new PatchesPage (proc), true);
    tabs.addTab ("SETUP", clear, setupPage = new SetupPage (proc), true);
    tabs.addTab ("MONITOR", clear, monitorPage = new MonitorPage (proc), true);
    addAndMakeVisible (tabs);
    setLookAndFeel (&laf); // last: slider text boxes copy their colours when the look-and-feel reaches them

    setResizable (true, true);
    setResizeLimits (1000, 640, 2880, 1800);
    setSize (1480, 900);
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

    statusLabel.setText (proc.status.text.upToFirstOccurrenceOf ("\n", false, false), juce::dontSendNotification);
    statusLabel.setTooltip (proc.status.text);
    seqPage->refresh();
    patchesPage->refresh();
    wavesPage->refresh();
    setupPage->refresh();
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

    // Watermark in the spirit of the Evolver panel: the big swoosh arc and an outlined lowercase wordmark.
    {
        const float top = (float) chassisH, ph = h - top;
        juce::Path arc;
        arc.addCentredArc (w * 0.17f, top + ph * 0.58f, ph * 0.62f, ph * 0.56f, 0.0f, -2.4f, 2.0f, true);
        g.setColour (colours::blueTop.brighter (0.5f).withAlpha (0.16f));
        g.strokePath (arc, juce::PathStrokeType (ph * 0.022f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        drawWordmark (g, { w * 0.5f, top + ph * 0.55f }, ph * 0.3f, 0.22f, 0.3f);
    }

    // Black brushed-metal chassis.
    g.setColour (colours::chassis);
    g.fillRect (0, 0, getWidth(), chassisH);
    g.setColour (juce::Colours::white.withAlpha (0.018f));
    for (int y = 0; y < chassisH; y += 3)
        g.drawHorizontalLine (y, 0.0f, w);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawHorizontalLine (chassisH - 1, 0.0f, w);
    const float sx = (float) woodW + 12.0f;
    drawScrew (g, { sx, 12.0f });
    drawScrew (g, { w - sx, 12.0f });
    drawScrew (g, { sx, (float) tabs.getY() - 10.0f });
    drawScrew (g, { w - sx, (float) tabs.getY() - 10.0f });

    for (const bool left : { true, false })
    {
        const auto cheek = (left ? getLocalBounds().removeFromLeft (woodW) : getLocalBounds().removeFromRight (woodW)).toFloat();
        juce::ColourGradient wood (juce::Colour (0xff2e170b), cheek.getX(), 0.0f, juce::Colour (0xff2a1409), cheek.getRight(), 0.0f, false);
        wood.addColour (left ? 0.4 : 0.6, juce::Colour (0xff7a4524));
        wood.addColour (0.5, juce::Colour (0xff9a5a30));
        g.setGradientFill (wood);
        g.fillRect (cheek);
        g.setColour (juce::Colours::black.withAlpha (0.12f)); // grain
        for (float gy = 0.0f; gy < h; gy += 7.0f)
            g.drawLine (cheek.getX(), gy, cheek.getRight(), gy + 3.0f, 0.6f);
    }

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
    auto r = getLocalBounds().reduced (woodW, 0);
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
    statusLabel.setBounds (actions.withSizeKeepingCentre (actions.getWidth(), 18)); // one line; the full text is in the tooltip

    tabs.setBounds (r.withTrimmedLeft (pad).withTrimmedRight (pad));
}
