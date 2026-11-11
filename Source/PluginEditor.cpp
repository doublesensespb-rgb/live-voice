#include <map>
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;

namespace
{
    // Inter (OFL) is embedded, so every computer shows the same font (same as GRAIN FORM)
    Typeface::Ptr interFace (bool bold)
    {
        static Typeface::Ptr reg  = Typeface::createSystemTypefaceFor (BinaryData::InterRegular_otf,  (size_t) BinaryData::InterRegular_otfSize);
        static Typeface::Ptr semi = Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_otf, (size_t) BinaryData::InterSemiBold_otfSize);
        return bold ? semi : reg;
    }
    Font uiFont (float size, bool bold = false, float kerning = 0.0f)
    {
        return Font { FontOptions (interFace (bold)).withHeight (size * 0.96f).withKerningFactor (kerning + 0.02f) };
    }
    Path arc (Point<float> c, float r, float a0, float a1)
    {
        Path p; p.addCentredArc (c.x, c.y, r, r, 0.0f, jmin (a0, a1), jmax (a0, a1), true); return p;
    }
    void panelBox (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (r.translated (0, 2), 12.0f);
        g.setColour (Palette::panel);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (Colours::white.withAlpha (0.045f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
    }
    void caption (Graphics& g, const String& s, Rectangle<int> r, Justification j, Colour c = Palette::label, float size = 13.0f)
    {
        g.setColour (c); g.setFont (uiFont (size, true, 0.12f)); g.drawText (s, r, j);
    }
    String semis (float v)
    {
        if (std::abs (v) < 0.05f) return "0.0 st";
        return String (v > 0 ? "+" : "-") + String (std::abs (v), 1) + " st";
    }
}

// ============================================================================
LiveVoiceLook::LiveVoiceLook()
{
    setColour (ResizableWindow::backgroundColourId, Palette::bgTop);
    setColour (PopupMenu::backgroundColourId, Palette::panel);
    setColour (PopupMenu::textColourId, Palette::text);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::accent);
    setDefaultSansSerifTypeface (interFace (false));
}

void LiveVoiceLook::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle st, Slider& s)
{
    if (st != Slider::LinearBar) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, st, s); return; }
    auto r = Rectangle<int> (x, y, w, h).toFloat();
    g.setColour (Palette::screen); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    auto f = r.withRight (jmax (r.getX() + r.getHeight(), pos));
    g.setColour (Palette::accent); g.fillRoundedRectangle (f, r.getHeight() * 0.5f);
    g.setColour (Palette::track); g.fillEllipse (Rectangle<float> (r.getHeight() + 4, r.getHeight() + 4).withCentre ({ f.getRight() - r.getHeight() * 0.5f, r.getCentreY() }));
}

Font LiveVoiceLook::getComboBoxFont (ComboBox&) { return uiFont (14.5f); }
Font LiveVoiceLook::getPopupMenuFont() { return uiFont (15.0f); }

void LiveVoiceLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    auto b = Rectangle<int> (x, y, w, h).toFloat();
    const auto c = b.getCentre();
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    const bool big = (bool) s.getProperties()["big"];
    const bool hot = s.isMouseOverOrDragging();

    if (big)
    {
        // big knob: dark well with tick marks, copper value arc, flat disc with one pointer (GRAPHITE)
        const float well = r - 2.0f;
        g.setColour (Palette::deep);
        g.fillEllipse (Rectangle<float> (well * 2, well * 2).withCentre (c));
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawEllipse (Rectangle<float> (well * 2, well * 2).withCentre (c).reduced (0.5f), 1.0f);

        const int ticks = (int) s.getProperties().getWithDefault ("ticks", 25);
        for (int i = 0; i < ticks; ++i)
        {
            const float t = (float) i / (float) (ticks - 1);
            const float a = start + t * (end - start);
            const bool major = (bool) s.getProperties()["majors"] ? (i % 6 == 0 || i == ticks / 2) : (i % 5 == 0);
            const float r0 = well * (major ? 0.80f : 0.84f), r1 = well * 0.90f;
            const bool lit = (a >= jmin (from, angle) - 0.001f && a <= jmax (from, angle) + 0.001f);
            g.setColour (lit ? Palette::accent.withAlpha (0.95f) : Colours::white.withAlpha (major ? 0.32f : 0.14f));
            g.drawLine ({ c.getPointOnCircumference (r0, a), c.getPointOnCircumference (r1, a) }, major ? 2.0f : 1.3f);
        }
        // value arc
        const float ar = well * 0.955f;
        g.setColour (Palette::track.withAlpha (0.07f));
        g.strokePath (arc (c, ar, start, end), PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        if (std::abs (angle - from) > 0.002f)
        {
            g.setColour (Palette::accent.withAlpha (0.16f));
            g.strokePath (arc (c, ar, from, angle), PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (Palette::accent);
            g.strokePath (arc (c, ar, from, angle), PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        // disc
        const float br = well * 0.70f;
        auto disc = Rectangle<float> (br * 2, br * 2).withCentre (c);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillEllipse (disc.translated (0, br * 0.05f).expanded (2.0f));
        g.setColour (hot ? Palette::knob.brighter (0.10f) : Palette::knob);
        g.fillEllipse (disc);
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawEllipse (disc.reduced (0.6f), 1.0f);
        // pointer: warm white line + copper tip dot
        g.setColour (Palette::track);
        g.drawLine ({ c.getPointOnCircumference (br * 0.30f, angle), c.getPointOnCircumference (br * 0.80f, angle) }, jmax (2.5f, br * 0.045f));
        g.setColour (Palette::accent);
        g.fillEllipse (Rectangle<float> (7, 7).withCentre (c.getPointOnCircumference (br * 0.88f, angle)));
        return;
    }

    // small flat knob (GRAIN FORM style) + a thin copper value ring
    const float ar = r - 2.5f;
    g.setColour (Palette::track.withAlpha (0.09f));
    g.strokePath (arc (c, ar, start, end), PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));
    if (std::abs (angle - from) > 0.002f)
    {
        g.setColour (Palette::accent);
        g.strokePath (arc (c, ar, from, angle), PathStrokeType (2.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    const float br = r * 0.72f;
    g.setColour (hot ? Palette::knob.brighter (0.12f) : Palette::knob);
    g.fillEllipse (Rectangle<float> (br * 2, br * 2).withCentre (c));
    g.setColour (Palette::track);
    g.drawLine ({ c.getPointOnCircumference (br * 0.15f, angle), c.getPointOnCircumference (br * 0.8f, angle) }, jmax (1.8f, br * 0.09f));
}

void LiveVoiceLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const String kind = b.getProperties()["kind"].toString();
    const bool on = b.getToggleState();
    const float rad = 7.0f;

    if (kind == "tab")
    {
        g.setColour (over ? Palette::buttonHi : Palette::panel); g.fillRoundedRectangle (r, 12.0f);
        g.setColour (over ? Palette::accent.withAlpha (0.7f) : Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        return;
    }
    if (kind == "preset")
    {
        g.setColour (Palette::screen); g.fillRoundedRectangle (r, rad);
        g.setColour (over ? Palette::accent.withAlpha (0.6f) : Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
        return;
    }
    if (kind == "arrow")
    {
        if (over) { g.setColour (Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (r, rad); }
        return;
    }
    if (kind == "mute" && on)
    {
        g.setColour (Palette::clip.withAlpha (0.9f)); g.fillRoundedRectangle (r, rad); return;
    }
    if (kind == "mode")
    {
        g.setColour (on ? Palette::accent.withAlpha (0.13f) : (over ? Palette::buttonHi : Palette::button).withAlpha (0.55f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (on ? Palette::accent : Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, on ? 1.4f : 1.0f);
        // LED
        auto led = Rectangle<float> (8, 8).withCentre ({ r.getX() + 18.0f, r.getCentreY() });
        if (on) { g.setColour (Palette::accent.withAlpha (0.25f)); g.fillEllipse (led.expanded (4)); g.setColour (Palette::glow); g.fillEllipse (led); }
        else { g.setColour (Palette::textDim.withAlpha (0.7f)); g.drawEllipse (led.reduced (0.6f), 1.2f); }
        return;
    }
    Colour base = (on || kind == "primary") ? Palette::accent : (over ? Palette::buttonHi : Palette::button);
    if (kind == "primary" && over) base = base.brighter (0.08f);
    if (down) base = base.darker (0.15f);
    g.setColour (base);
    g.fillRoundedRectangle (r, rad);
    if (on)
    {
        // copper with a light glint, like GRAIN FORM's SOUNDS / CHORD buttons
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.20f), 0, r.getY(), Colours::transparentWhite, 0, r.getCentreY(), false));
        g.fillRoundedRectangle (r, rad);
    }
    g.setColour (Colours::white.withAlpha (0.06f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

void LiveVoiceLook::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const String kind = b.getProperties()["kind"].toString();
    const bool on = b.getToggleState();
    auto r = b.getLocalBounds();
    if (kind == "mode")
    {
        auto t = r.withTrimmedLeft (34).reduced (0, 9);
        g.setColour (on ? Palette::glow : Palette::text);
        g.setFont (uiFont (15.5f, true, 0.10f));
        g.drawText (b.getButtonText(), t.removeFromTop (t.getHeight() / 2 + 1), Justification::bottomLeft);
        g.setColour (on ? Palette::accent.withAlpha (0.85f) : Palette::textDim);
        g.setFont (uiFont (11.5f, false, 0.02f));
        g.drawText (b.getProperties()["hint"].toString(), t.translated (0, 2), Justification::topLeft);
        return;
    }
    if (kind == "hide")
    {
        // "HIDE >" : clearly the switch for this module
        g.setColour (Palette::text); g.setFont (uiFont (12.5f, true, 0.10f));
        g.drawText (b.getButtonText(), r.withTrimmedRight (18), Justification::centred);
        Path p; const float cx = (float) r.getRight() - 18.0f, cy = (float) r.getCentreY();
        p.startNewSubPath (cx - 3, cy - 6); p.lineTo (cx + 3, cy); p.lineTo (cx - 3, cy + 6);
        g.setColour (Palette::accent); g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        return;
    }
    if (kind == "tab")
    {
        // slim vertical tab: "<" + "PITCH & EFFECTS" written upwards
        auto c = r.toFloat().getCentre();
        Path p; const float cy = (float) r.getY() + 34.0f;
        p.startNewSubPath (c.x + 4, cy - 8); p.lineTo (c.x - 4, cy); p.lineTo (c.x + 4, cy + 8);
        g.setColour (Palette::accent); g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, c.x, c.y));
        g.setColour (Palette::text); g.setFont (uiFont (13.0f, true, 0.18f));
        g.drawText ("SHOW  PITCH & EFFECTS", Rectangle<float> ((float) r.getHeight() - 40.0f, 30.0f).withCentre (c).toNearestInt(), Justification::centred);
        return;
    }
    if (kind == "arrow")
    {
        auto c = r.toFloat().getCentre();
        Path p;
        const float d = b.getButtonText() == "<" ? -1.0f : 1.0f;
        p.startNewSubPath (c.x - 4 * d, c.y - 9); p.lineTo (c.x + 4 * d, c.y); p.lineTo (c.x - 4 * d, c.y + 9);
        g.setColour (b.isOver() ? Palette::text : Palette::textDim);
        g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        return;
    }
    g.setColour ((on || kind == "primary") ? Colour (0xff1b1410) : Palette::text);
    if (kind == "preset")
    {
        auto t = r.reduced (12, 0);
        g.setColour (Palette::text); g.setFont (uiFont (14.0f, true, 0.04f));
        g.drawText (b.getButtonText(), t.withTrimmedRight (18), Justification::centredLeft, true);
        Path tri; const float cx = (float) t.getRight() - 5.0f, cy = (float) r.getCentreY();
        tri.addTriangle (cx - 5, cy - 2.5f, cx + 5, cy - 2.5f, cx, cy + 3.5f);
        g.setColour (Palette::textDim); g.fillPath (tri);
        return;
    }
    if (kind == "mute" && on) g.setColour (Colours::white);
    g.setFont (uiFont (kind == "quick" ? 13.0f : 13.5f, true, 0.10f));
    g.drawText (b.getButtonText(), r, Justification::centred);
}

void Lamp::paintButton (Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = r.getCentre();
    const float rr = jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const bool on = getToggleState();
    if (on)
    {
        g.setColour (Palette::accent.withAlpha (0.18f));
        g.fillEllipse (Rectangle<float> (rr * 2, rr * 2).withCentre (c));
    }
    // power symbol
    const Colour col = on ? Palette::glow : (over ? Palette::text : Palette::textDim);
    g.setColour (col);
    const float pr = rr * 0.52f;
    g.strokePath (arc (c, pr, 0.55f, MathConstants<float>::twoPi - 0.55f), PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
    g.drawLine (c.x, c.y - pr - 1.5f, c.x, c.y - pr * 0.1f, 1.8f);
}

void LevelMeter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 2.5f);
    const int segs = 22;
    const float gap = 2.0f, sw = (r.getWidth() - 4 - gap * (segs - 1)) / segs;
    const float db = jlimit (-60.0f, 0.0f, 20.0f * std::log10 (jmax (1e-5f, level)));
    const int lit = roundToInt ((db + 60.0f) / 60.0f * segs);
    for (int i = 0; i < segs; ++i)
    {
        auto s = Rectangle<float> (r.getX() + 2 + i * (sw + gap), r.getY() + 2, sw, r.getHeight() - 4);
        const bool red = i >= segs - 2, warm = i >= segs - 6;
        Colour c = red ? Palette::clip : warm ? Palette::accent : Palette::label;
        g.setColour (i < lit ? c : c.withAlpha (0.10f));
        g.fillRoundedRectangle (s, 1.0f);
    }
}

// copper icons (strict and geometric), shared by the SFX buttons
static void drawSfxIcon (Graphics& g, int type, Point<float> c, float s, bool on, float activity)
{
    const Colour col = on ? Palette::glow : Palette::textDim.withAlpha (0.75f);
    auto drawGlow = [&] (const Path& path, float w, Colour cc)
    {
        if (on)
        {
            g.setColour (cc.withAlpha (0.10f + 0.10f * activity)); g.strokePath (path, PathStrokeType (w * 5.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (cc.withAlpha (0.22f + 0.15f * activity)); g.strokePath (path, PathStrokeType (w * 2.4f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (cc); g.strokePath (path, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    };
    const float stroke = jmax (1.4f, s * 0.07f);
    if (type == 0)
    {
        Path p; for (int i = -1; i <= 1; ++i) p.addEllipse (Rectangle<float> (s * 1.25f, s * 1.25f).withCentre (c.translated (i * s * 0.62f, 0)));
        drawGlow (p, stroke, col);
    }
    else if (type == 1)
    {
        Path core; core.addEllipse (Rectangle<float> (s * 0.25f, s * 0.25f).withCentre (c));
        drawGlow (core, stroke, col);
        for (int i = 1; i <= 3; ++i)
        {
            Path ring; const float rr = s * 0.36f * (float) i;
            ring.addEllipse (Rectangle<float> (rr * 2, rr * 2).withCentre (c));
            const Colour rc = col.withMultipliedAlpha (1.0f - 0.3f * (float) (i - 1));
            if (i == 1) drawGlow (ring, stroke, rc); else { g.setColour (rc); g.strokePath (ring, PathStrokeType (stroke * 0.7f)); }
        }
    }
    else if (type == 2)
    {
        const auto o = c.translated (-s * 0.85f, 0);
        Path dot; dot.addEllipse (Rectangle<float> (s * 0.28f, s * 0.28f).withCentre (o));
        drawGlow (dot, stroke, col);
        for (int i = 1; i <= 4; ++i)
        {
            Path a; const float rr = s * 0.42f * (float) i;
            a.addCentredArc (o.x, o.y, rr, rr, 0.0f, MathConstants<float>::halfPi - 0.75f, MathConstants<float>::halfPi + 0.75f, true);
            drawGlow (a, stroke, col.withMultipliedAlpha (1.0f - 0.2f * (float) (i - 1)));
        }
    }
    else
    {
        Path p;
        for (int i = 0; i < 3; ++i)
        {
            const float y = c.y - s * 0.62f + (float) i * s * 0.44f;
            p.startNewSubPath (c.x - s * 0.8f, y); p.lineTo (c.x, y + s * 0.5f); p.lineTo (c.x + s * 0.8f, y);
        }
        drawGlow (p, stroke * 1.5f, col);
        Path floor; floor.startNewSubPath (c.x - s * 0.95f, c.y + s * 0.98f); floor.lineTo (c.x + s * 0.95f, c.y + s * 0.98f);
        drawGlow (floor, stroke, col.withMultipliedAlpha (0.7f));
    }
}

void SfxButton::paintButton (Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    auto box = r.withHeight (r.getHeight() - 22.0f);
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (box, 10.0f);
    if (on)
    {
        g.setGradientFill (ColourGradient (Palette::accent.withAlpha (0.18f + 0.12f * activity), box.getCentreX(), box.getCentreY(),
                                           Colours::transparentBlack, box.getCentreX(), box.getBottom() + 10, true));
        g.fillRoundedRectangle (box, 10.0f);
    }
    g.setColour (on ? Palette::accent : Colours::white.withAlpha (over ? 0.14f : 0.06f));
    g.drawRoundedRectangle (box.reduced (0.5f), 10.0f, on ? 1.4f : 1.0f);
    drawSfxIcon (g, type, box.getCentre(), box.getHeight() * 0.27f, on, activity);
    g.setColour (on ? Palette::glow : Palette::label);
    g.setFont (uiFont (11.5f, true, 0.10f));
    static const char* shortName[4] = { "WIDE", "SHIMMER", "ECHO", "BRUTAL" };
    g.drawText (shortName[jlimit (0, 3, type)], r.withTop (box.getBottom() + 4).toNearestInt(), Justification::centredTop);
}

void VoiceTile::paintButton (Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const auto& v = kVoices[index];
    g.setColour (on ? Palette::accent.withAlpha (0.14f) : (over ? Palette::buttonHi : Palette::button).withAlpha (0.55f));
    g.fillRoundedRectangle (r, 9.0f);
    g.setColour (on ? Palette::accent : Colours::white.withAlpha (over ? 0.12f : 0.05f));
    g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, on ? 1.5f : 1.0f);
    if (r.getHeight() > 110.0f)
    {
        // big tile (controls hidden): LED + number on top, name + hint centred, depth bar at the bottom
        auto led = Rectangle<float> (9, 9).withCentre ({ r.getX() + 22.0f, r.getY() + 24.0f });
        if (on) { g.setColour (Palette::accent.withAlpha (0.25f)); g.fillEllipse (led.expanded (4)); g.setColour (Palette::glow); g.fillEllipse (led); }
        else { g.setColour (Palette::textDim.withAlpha (0.7f)); g.drawEllipse (led.reduced (0.6f), 1.2f); }
        g.setColour (on ? Palette::glow : Palette::text);
        g.setFont (uiFont (21.0f, true, 0.10f));
        g.drawText (v.name, r.toNearestInt().withSizeKeepingCentre ((int) r.getWidth() - 12, 30).translated (0, -10), Justification::centred);
        g.setColour (on ? Palette::accent.withAlpha (0.85f) : Palette::textDim);
        g.setFont (uiFont (12.5f));
        g.drawText (v.hint, r.toNearestInt().withSizeKeepingCentre ((int) r.getWidth() - 12, 20).translated (0, 18), Justification::centred);
        const float depth = jlimit (0.0f, 1.0f, -v.pitch / 6.0f);
        auto bar = Rectangle<float> (r.getX() + 20.0f, r.getBottom() - 26.0f, r.getWidth() - 40.0f, 4.0f);
        g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (bar, 2.0f);
        if (depth > 0) { g.setColour (on ? Palette::glow : Palette::accent.withAlpha (0.8f)); g.fillRoundedRectangle (bar.withWidth (jmax (4.0f, bar.getWidth() * depth)), 2.0f); }
        return;
    }
    // LED
    auto led = Rectangle<float> (8, 8).withCentre ({ r.getX() + 19.0f, r.getCentreY() });
    if (on) { g.setColour (Palette::accent.withAlpha (0.25f)); g.fillEllipse (led.expanded (4)); g.setColour (Palette::glow); g.fillEllipse (led); }
    else { g.setColour (Palette::textDim.withAlpha (0.7f)); g.drawEllipse (led.reduced (0.6f), 1.2f); }
    // name + hint
    auto t = r.toNearestInt().withTrimmedLeft (36).withTrimmedRight (62).reduced (0, 10);
    g.setColour (on ? Palette::glow : Palette::text);
    g.setFont (uiFont (16.0f, true, 0.10f));
    g.drawText (v.name, t.removeFromTop (t.getHeight() / 2 + 1), Justification::bottomLeft);
    g.setColour (on ? Palette::accent.withAlpha (0.85f) : Palette::textDim);
    g.setFont (uiFont (11.5f, false, 0.02f));
    g.drawText (v.hint, t.translated (0, 2), Justification::topLeft);
    // how much lower: a small depth bar + number
    auto tag = r.toNearestInt().removeFromRight (60).reduced (10, 0);
    const float depth = jlimit (0.0f, 1.0f, -v.pitch / 6.0f);
    auto bar = Rectangle<float> ((float) tag.getX(), r.getCentreY() - 1.5f, (float) tag.getWidth(), 3.0f);
    g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (bar, 1.5f);
    if (depth > 0) { g.setColour (on ? Palette::glow : Palette::accent.withAlpha (0.8f)); g.fillRoundedRectangle (bar.withWidth (jmax (3.0f, bar.getWidth() * depth)), 1.5f); }
}

// ============================================================================
LiveVoiceEditor::LiveVoiceEditor (LiveVoiceProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    auto& st = proc.apvts;

    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);
    addAndMakeVisible (powerBtn);
    aPower = std::make_unique<BA> (st, ParamID::voiceOn, powerBtn);
    muteBtn.getProperties().set ("kind", "mute"); muteBtn.setClickingTogglesState (true);
    addAndMakeVisible (muteBtn);
    aMute = std::make_unique<BA> (st, ParamID::mute, muteBtn);
    audioBtn.onClick = [] { if (LiveVoiceEditor::onAudioSettings) LiveVoiceEditor::onAudioSettings(); };
    addAndMakeVisible (audioBtn);
    clipBtn.setClickingTogglesState (true);
    addAndMakeVisible (clipBtn);
    aClip = std::make_unique<BA> (st, ParamID::clipOn, clipBtn);

    for (int i = 0; i < kNumVoices; ++i)
    {
        auto* t = tiles.add (new VoiceTile (i));
        t->onClick = [this, i] { selectVoice (i); };
        addAndMakeVisible (t);
    }

    pitchKnob.setSliderStyle (Slider::RotaryVerticalDrag);
    pitchKnob.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    pitchKnob.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    pitchKnob.setMouseDragSensitivity (420);
    pitchKnob.getProperties().set ("big", true);
    pitchKnob.getProperties().set ("ticks", 25);
    pitchKnob.getProperties().set ("majors", true);
    addAndMakeVisible (pitchKnob);
    aPitch = std::make_unique<SA> (st, ParamID::formant, pitchKnob);

    for (int t = 0; t < 4; ++t)
    {
        auto* b = sfxBtns.add (new SfxButton (t));
        b->setClickingTogglesState (true);
        addAndMakeVisible (b);
        aFxOn.add (new BA (st, kFxOn[t], *b));
        auto* bar = fxBars.add (new Slider());
        bar->setSliderStyle (Slider::LinearBar);
        bar->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (bar);
        aFxAmt.add (new SA (st, kFxAmt[t], *bar));
    }
    hideBtn.onClick = [this] { setShowControls (! showControls()); };
    addAndMakeVisible (hideBtn);

    // keys always go to the editor
    for (auto* c : getChildren()) { c->setWantsKeyboardFocus (false); c->setMouseClickGrabsKeyboardFocus (false); }
    setWantsKeyboardFocus (true);
    setSize (980, 560);
    refresh();
    startTimerHz (30);
    Component::SafePointer<LiveVoiceEditor> safe (this);
    MessageManager::callAsync ([safe] { if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus(); });
}

LiveVoiceEditor::~LiveVoiceEditor() { setLookAndFeel (nullptr); }

bool LiveVoiceEditor::showControls() const { return (bool) proc.apvts.state.getProperty ("showControls", false); }   // false = first start shows presets only
void LiveVoiceEditor::setShowControls (bool b)
{
    proc.apvts.state.setProperty ("showControls", b, nullptr);
    resized(); repaint();
}

float LiveVoiceEditor::getParam (const char* id) const
{
    auto* p = proc.apvts.getParameter (id);
    return p->convertFrom0to1 (p->getValue());
}

void LiveVoiceEditor::setParam (const char* id, float v)
{
    auto* p = proc.apvts.getParameter (id);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (v));
    p->endChangeGesture();
}

void LiveVoiceEditor::selectVoice (int i)
{
    const auto& v = kVoices[i];
    setParam (ParamID::voice, (float) i);
    setParam (ParamID::voiceOn, 1.0f);
    setParam (ParamID::mode, (float) v.mode);
    setParam (ParamID::strength, v.strength);
    setParam (ParamID::formant, v.pitch);
    setParam (ParamID::clipOn, v.clip ? 1.0f : 0.0f);
    for (int k = 0; k < 4; ++k)
    {
        if (v.fx[k] > 0.0f) setParam (kFxAmt[k], v.fx[k]);
        setParam (kFxOn[k], v.fx[k] > 0.0f ? 1.0f : 0.0f);
    }
    refresh();
}

void LiveVoiceEditor::refresh()
{
    const int v = jlimit (0, kNumVoices - 1, (int) getParam (ParamID::voice));
    const bool on = getParam (ParamID::voiceOn) > 0.5f;
    for (auto* t : tiles) { t->setToggleState (t->index == v, dontSendNotification); t->setAlpha (on ? 1.0f : 0.45f); }
    pitchKnob.setDoubleClickReturnValue (true, kVoices[v].pitch);
    for (int k = 0; k < 4; ++k) fxBars[k]->setAlpha (getParam (kFxOn[k]) > 0.5f ? 1.0f : 0.35f);
}

void LiveVoiceEditor::timerCallback()
{
    inMeter.setLevel (proc.engine.inLevel.load());
    outMeter.setLevel (proc.engine.outLevel.load());
    const float act = proc.engine.sfxActivity.load();
    for (auto* b : sfxBtns) { b->activity = b->activity * 0.7f + act * 0.3f; if (b->getToggleState()) b->repaint(); }
    if (controlsPanel.isEmpty() == showControls()) resized();
    refresh();
    repaint (pitchPanel.withHeight (60));
    repaint (sfxPanel.withHeight (50));
}

bool LiveVoiceEditor::keyPressed (const KeyPress& k)
{
    const auto ch = k.getTextCharacter();
    if (k == KeyPress::spaceKey) { setParam (ParamID::voiceOn, getParam (ParamID::voiceOn) > 0.5f ? 0.0f : 1.0f); return true; }
    if (ch == 'm' || ch == 'M' || ch == L'ь' || ch == L'Ь') { setParam (ParamID::mute, getParam (ParamID::mute) > 0.5f ? 0.0f : 1.0f); return true; }
    if (k == KeyPress::upKey)   { setParam (ParamID::formant, jmin (6.0f, std::round (getParam (ParamID::formant)) + 1.0f)); return true; }
    if (k == KeyPress::downKey) { setParam (ParamID::formant, jmax (-6.0f, std::round (getParam (ParamID::formant)) - 1.0f)); return true; }
    if (ch == 'c' || ch == 'C' || ch == L'с' || ch == L'С') { setParam (ParamID::clipOn, getParam (ParamID::clipOn) > 0.5f ? 0.0f : 1.0f); return true; }
    if (ch == 'h' || ch == 'H' || ch == L'р' || ch == L'Р') { setShowControls (! showControls()); return true; }
    if (ch >= '1' && ch <= '9') { selectVoice ((int) (ch - '1')); return true; }
    if (ch == '0') { selectVoice (9); return true; }
    return false;
}

void LiveVoiceEditor::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Palette::bgTop, 0, 0, Palette::bgBottom, 0, full.getHeight(), false));
    g.fillAll();

    // title + version plate
    g.setColour (Palette::text);
    g.setFont (uiFont (36.0f, false, 0.05f));
    g.drawText ("LIVE VOICE", Rectangle<int> (26, 10, 400, 44), Justification::centredLeft);
    const int tw = GlyphArrangement::getStringWidthInt (uiFont (36.0f, false, 0.05f), "LIVE VOICE");
    auto plate = Rectangle<float> ((float) (26 + tw + 12), 22.0f, 46.0f, 20.0f);
    g.setColour (Palette::button); g.fillRoundedRectangle (plate, 4.0f);
    g.setColour (Palette::label); g.setFont (uiFont (11.5f, true, 0.06f));
    g.drawText ("v" + String (LIVEVOICE_LABEL), plate.toNearestInt(), Justification::centred);
    g.setColour (Palette::textDim); g.setFont (uiFont (11.5f, true, 0.30f));
    g.drawText ("LIVE VOCAL PROCESSOR", Rectangle<int> (28, 52, 260, 16), Justification::centredLeft);

    caption (g, "IN", inMeter.getBounds().withX (inMeter.getX() - 30).withWidth (24).expanded (0, 4), Justification::centredRight, Palette::textDim, 10.0f);
    caption (g, "OUT", outMeter.getBounds().withX (outMeter.getX() - 34).withWidth (28).expanded (0, 4), Justification::centredRight, Palette::textDim, 10.0f);
    const bool on = getParam (ParamID::voiceOn) > 0.5f;
    caption (g, on ? "ON" : "OFF", Rectangle<int> (powerBtn.getX() - 8, powerBtn.getBottom() - 1, powerBtn.getWidth() + 16, 14), Justification::centred,
             on ? Palette::accent : Palette::textDim, 10.0f);

    for (auto r : { voicePanel, controlsPanel }) if (! r.isEmpty()) panelBox (g, r.toFloat());

    // VOICE PRESETS
    caption (g, "VOICE PRESETS", Rectangle<int> (voicePanel.getX() + 22, voicePanel.getY() + 14, 200, 24), Justification::centredLeft, Palette::text, 14.0f);

    if (showControls())
    {
    // PITCH (top part of the one CONTROLS module; the HIDE button sits in its top-right corner)
    caption (g, "PITCH", Rectangle<int> (pitchPanel.getX() + 22, pitchPanel.getY() + 14, 70, 24), Justification::centredLeft, Palette::text, 14.0f);
    const float pv = getParam (ParamID::formant);
    g.setColour (std::abs (pv) < 0.05f ? Palette::text : Palette::glow);
    g.setFont (uiFont (20.0f, true, 0.02f));
    g.drawText (semis (pv), Rectangle<int> (pitchPanel.getX() + 86, pitchPanel.getY() + 11, 110, 30), Justification::centredLeft);
    {
        const auto c = pitchKnob.getBounds().toFloat().getCentre();
        const float rr = pitchKnob.getWidth() * 0.5f + 13.0f;
        g.setFont (uiFont (11.0f, true)); g.setColour (Palette::textDim);
        const float a0 = MathConstants<float>::pi * 1.25f, a1 = MathConstants<float>::pi * 2.75f;
        const char* lbl[5] = { "-6", "-3", "0", "+3", "+6" };
        for (int i = 0; i < 5; ++i)
            g.drawText (lbl[i], Rectangle<float> (36, 16).withCentre (c.getPointOnCircumference (rr, a0 + (a1 - a0) * (float) i / 4.0f)).toNearestInt(), Justification::centred);
        caption (g, "LOWER", Rectangle<int> ((int) (c.x - rr - 36), (int) (c.y + rr * 0.78f), 72, 16), Justification::centred, Palette::textDim, 10.0f);
        caption (g, "HIGHER", Rectangle<int> ((int) (c.x + rr - 36), (int) (c.y + rr * 0.78f), 72, 16), Justification::centred, Palette::textDim, 10.0f);
    }
    g.setColour (Palette::textDim); g.setFont (uiFont (11.0f));
    g.drawText ("double-click = back to the preset", Rectangle<int> (pitchPanel.getX(), pitchPanel.getBottom() - 26, pitchPanel.getWidth(), 18), Justification::centred);
    // divider between PITCH and EFFECTS inside the same module
    g.setColour (Colours::white.withAlpha (0.06f));
    g.fillRect (Rectangle<int> (controlsPanel.getX() + 20, sfxPanel.getY(), controlsPanel.getWidth() - 40, 1));

    // EFFECT
    caption (g, "EFFECTS", Rectangle<int> (sfxPanel.getX() + 22, sfxPanel.getY() + 12, 160, 22), Justification::centredLeft, Palette::text, 14.0f);
    g.setColour (Palette::textDim); g.setFont (uiFont (11.0f));
    g.drawText ("any together, bar = amount", Rectangle<int> (sfxPanel.getRight() - 222, sfxPanel.getY() + 12, 200, 22), Justification::centredRight);
    }

    g.setColour (Palette::textDim.withAlpha (0.8f)); g.setFont (uiFont (11.0f));
    g.drawText ("1 - 0  presets      SPACE  on/off      C  clip      M  mute      UP / DOWN  pitch      H  hide / show",
                Rectangle<int> (0, getHeight() - 26, getWidth(), 18), Justification::centred);
}

void LiveVoiceEditor::resized()
{
    muteBtn.setBounds (getWidth() - 24 - 80, 20, 80, 34);
    audioBtn.setBounds (muteBtn.getX() - 8 - 74, 20, 74, 34);
    powerBtn.setBounds (audioBtn.getX() - 18 - 34, 14, 34, 34);
    clipBtn.setBounds (powerBtn.getX() - 18 - 62, 20, 62, 34);
    outMeter.setBounds (clipBtn.getX() - 20 - 110, 41, 110, 9);
    inMeter.setBounds (outMeter.getX(), 24, 110, 9);

    const int top = 84, h = getHeight() - top - 34;
    const bool show = showControls();
    pitchKnob.setVisible (show);
    for (auto* b : sfxBtns) b->setVisible (show);
    for (auto* b : fxBars) b->setVisible (show);
    hideBtn.getProperties().set ("kind", show ? "hide" : "tab");
    hideBtn.setButtonText (show ? "HIDE" : "PITCH & EFFECTS");
    if (! show)
    {
        // presets only: 5 x 2 big tiles; a slim tab on the right brings PITCH & EFFECTS back
        const int tabW = 44;
        hideBtn.setBounds (getWidth() - 24 - tabW, top, tabW, h);
        voicePanel = { 24, top, hideBtn.getX() - 12 - 24, h };
        controlsPanel = pitchPanel = sfxPanel = {};
        const int cols = 5, tw = (voicePanel.getWidth() - 36 - (cols - 1) * 12) / cols, th = (voicePanel.getHeight() - 50 - 18 - 12) / 2;
        for (int i = 0; i < tiles.size(); ++i)
            tiles[i]->setBounds (voicePanel.getX() + 18 + (i % cols) * (tw + 12), voicePanel.getY() + 50 + (i / cols) * (th + 12), tw, th);
        return;
    }
    voicePanel = { 24, top, 560, h };
    const int rx = voicePanel.getRight() + 16, rw = getWidth() - 24 - rx;
    controlsPanel = { rx, top, rw, h };
    pitchPanel = { rx, top, rw, 270 };
    sfxPanel   = { rx, pitchPanel.getBottom(), rw, h - 270 };
    hideBtn.setBounds (controlsPanel.getRight() - 18 - 82, controlsPanel.getY() + 12, 82, 30);

    const int tw = (voicePanel.getWidth() - 36 - 12) / 2, th = 68, tg = 10;
    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (voicePanel.getX() + 18 + (i % 2) * (tw + 12), voicePanel.getY() + 50 + (i / 2) * (th + tg), tw, th);

    pitchKnob.setBounds (Rectangle<int> (160, 160).withCentre ({ pitchPanel.getCentreX(), pitchPanel.getY() + 140 }));

    const int bw = (sfxPanel.getWidth() - 36 - 3 * 10) / 4;
    for (int i = 0; i < 4; ++i)
    {
        sfxBtns[i]->setBounds (sfxPanel.getX() + 18 + i * (bw + 10), sfxPanel.getY() + 44, bw, bw + 4);
        fxBars[i]->setBounds (sfxBtns[i]->getX() + 4, sfxBtns[i]->getBottom() + 10, bw - 8, 8);
    }
}
