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

void SfxScreen::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // dark screen like GRAIN FORM's displays
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 10.0f);
    g.setGradientFill (ColourGradient (Palette::accent.withAlpha (on ? 0.10f + 0.12f * activity : 0.0f), r.getCentreX(), r.getY() + 62,
                                       Colours::transparentBlack, r.getCentreX(), r.getY() + 62 + r.getWidth() * 0.55f, true));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    const Colour col = on ? Palette::glow : Palette::textDim.withAlpha (0.55f);
    const auto c = Point<float> (r.getCentreX(), r.getY() + 62);
    const float s = 30.0f;
    Path p;
    float stroke = 2.2f;
    switch (type)
    {
        case 0:   // WIDE: three overlapping rings
            for (int i = -1; i <= 1; ++i) p.addEllipse (Rectangle<float> (s * 1.25f, s * 1.25f).withCentre (c.translated (i * s * 0.62f, 0)));
            break;
        case 1:   // SHIMMER: light spreading in rings from a bright core
            p.addEllipse (Rectangle<float> (8, 8).withCentre (c));
            break;
        case 2:   // ECHO THROW: a dot and arcs that get weaker
        {
            const auto o = c.translated (-s * 0.95f, 0);
            p.addEllipse (Rectangle<float> (9, 9).withCentre (o));
            break;
        }
        default:  // BRUTAL: three heavy chevrons going down
            for (int i = 0; i < 3; ++i)
            {
                const float y = c.y - s * 0.62f + (float) i * s * 0.48f;
                p.startNewSubPath (c.x - s * 0.82f, y);
                p.lineTo (c.x, y + s * 0.52f);
                p.lineTo (c.x + s * 0.82f, y);
            }
            stroke = 3.4f;
            break;
    }
    auto drawGlow = [&] (const Path& path, float w, Colour cc)
    {
        if (on)
        {
            g.setColour (cc.withAlpha (0.10f + 0.10f * activity)); g.strokePath (path, PathStrokeType (w * 5.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (cc.withAlpha (0.22f + 0.15f * activity)); g.strokePath (path, PathStrokeType (w * 2.4f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (cc); g.strokePath (path, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    };
    drawGlow (p, stroke, col);
    if (type == 2)
    {
        const auto o = c.translated (-s * 0.95f, 0);
        for (int i = 1; i <= 4; ++i)
        {
            Path a; const float rr = s * 0.42f * (float) i;
            a.addCentredArc (o.x, o.y, rr, rr, 0.0f, MathConstants<float>::halfPi - 0.75f, MathConstants<float>::halfPi + 0.75f, true);
            drawGlow (a, stroke, col.withMultipliedAlpha (1.0f - 0.2f * (float) (i - 1)));
        }
    }
    if (type == 1)
    {
        for (int i = 1; i <= 3; ++i)
        {
            Path ring; const float rr = s * 0.36f * (float) i;
            ring.addEllipse (Rectangle<float> (rr * 2, rr * 2).withCentre (c));
            const Colour rc = col.withMultipliedAlpha (1.0f - 0.3f * (float) (i - 1));
            if (i == 1) drawGlow (ring, stroke, rc);
            else { g.setColour (rc); g.strokePath (ring, PathStrokeType (1.6f)); }
        }
    }
    if (type == 3)
    {
        // a solid floor line under the chevrons
        Path floor; floor.startNewSubPath (c.x - s * 1.0f, c.y + s * 1.12f); floor.lineTo (c.x + s * 1.0f, c.y + s * 1.12f);
        drawGlow (floor, 2.0f, col.withMultipliedAlpha (0.7f));
    }

    // name + hint + dots
    g.setColour (on ? Palette::text : Palette::textDim);
    g.setFont (uiFont (17.0f, true, 0.16f));
    g.drawText (lv::kSfx[type].name, Rectangle<float> (r.getX(), r.getY() + 112, r.getWidth(), 22).toNearestInt(), Justification::centred);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (11.5f));
    g.drawText (lv::kSfx[type].hint, Rectangle<float> (r.getX(), r.getY() + 134, r.getWidth(), 16).toNearestInt(), Justification::centred);
    for (int i = 0; i < 4; ++i)
    {
        auto d = Rectangle<float> (5, 5).withCentre ({ r.getCentreX() + (i - 1.5f) * 12.0f, r.getBottom() - 14.0f });
        g.setColour (i == type ? (on ? Palette::accent : Palette::text) : Colours::white.withAlpha (0.18f));
        g.fillEllipse (d);
    }
}

// ============================================================================
SaveOverlay::SaveOverlay (LiveVoiceProcessor& p) : proc (p)
{
    for (auto* t : { &name, &newFolder })
    {
        t->setFont (uiFont (15.0f));
        t->setColour (TextEditor::backgroundColourId, Palette::screen);
        t->setColour (TextEditor::textColourId, Palette::text);
        t->setColour (TextEditor::outlineColourId, Colours::white.withAlpha (0.08f));
        t->setColour (TextEditor::focusedOutlineColourId, Palette::accent);
        t->setColour (TextEditor::highlightColourId, Palette::accent.withAlpha (0.35f));
        t->setColour (CaretComponent::caretColourId, Palette::accent);
        t->setIndents (10, 7);
        t->setInputRestrictions (60, {});
        addAndMakeVisible (t);
    }
    name.setTextToShowWhenEmpty ("Preset name", Palette::textDim);
    newFolder.setTextToShowWhenEmpty ("New folder name", Palette::textDim);
    name.onTextChange = [this] { updateSaveText(); };
    name.onReturnKey = [this] { save.triggerClick(); };
    name.onEscapeKey = [this] { cancel.triggerClick(); };
    newFolder.onTextChange = [this] { updateSaveText(); };
    folder.setColour (ComboBox::backgroundColourId, Palette::screen);
    folder.setColour (ComboBox::textColourId, Palette::text);
    folder.setColour (ComboBox::outlineColourId, Colours::white.withAlpha (0.08f));
    folder.setColour (ComboBox::arrowColourId, Palette::textDim);
    folder.onChange = [this] { newFolder.setVisible (folder.getSelectedId() == 2); updateSaveText(); resized(); };
    addAndMakeVisible (folder);
    save.getProperties().set ("kind", "primary");
    cancel.onClick = [this] { setVisible (false); if (onDone) onDone(); };
    save.onClick = [this]
    {
        String f = folder.getSelectedId() == 2 ? newFolder.getText().trim() : folder.getSelectedId() == 1 ? String() : folder.getText();
        if (name.getText().trim().isEmpty()) { name.grabKeyboardFocus(); return; }
        if (proc.savePreset (name.getText(), f)) { setVisible (false); if (onDone) onDone(); }
    };
    addAndMakeVisible (cancel); addAndMakeVisible (save);
    setVisible (false);
}

void SaveOverlay::open()
{
    folder.clear (dontSendNotification);
    folder.addItem ("(no folder)", 1);
    folder.addItem ("+ New folder...", 2);
    folder.addSeparator();
    int id = 10, sel = 1;
    const String cur = proc.presetFile.existsAsFile() && proc.presetFile.getParentDirectory() != LiveVoiceProcessor::presetRoot()
                         ? proc.presetFile.getParentDirectory().getFileName() : String();
    for (auto& f : proc.listFolders()) { if (f == "Factory") { ++id; continue; } folder.addItem (f, id); if (f == cur) sel = id; ++id; }
    folder.setSelectedId (sel, dontSendNotification);
    newFolder.setVisible (false); newFolder.clear();
    name.setText (proc.presetName == "INIT" ? String() : proc.presetName, false);
    updateSaveText();
    setVisible (true); toFront (true); resized();
    name.grabKeyboardFocus(); name.selectAll();
}

void SaveOverlay::updateSaveText()
{
    const String f = folder.getSelectedId() == 2 ? newFolder.getText().trim() : folder.getSelectedId() == 1 ? String() : folder.getText();
    auto dir = f.isEmpty() ? LiveVoiceProcessor::presetRoot() : LiveVoiceProcessor::presetRoot().getChildFile (File::createLegalFileName (f));
    const bool exists = name.getText().trim().isNotEmpty() && dir.getChildFile (File::createLegalFileName (name.getText().trim()) + ".lvpreset").existsAsFile();
    save.setButtonText (exists ? "REPLACE" : "SAVE");
}

void SaveOverlay::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.55f));
    panelBox (g, box.toFloat());
    g.setColour (Palette::accent.withAlpha (0.6f));
    g.drawRoundedRectangle (box.toFloat().reduced (0.5f), 12.0f, 1.0f);
    caption (g, "SAVE PRESET", box.withHeight (54).withTrimmedLeft (24), Justification::centredLeft, Palette::text, 14.0f);
    g.setColour (Palette::textDim); g.setFont (uiFont (11.0f));
    g.drawText ("Documents / LIVE VOICE / Presets", box.withHeight (54).withTrimmedRight (24), Justification::centredRight);
    caption (g, "NAME", Rectangle<int> (box.getX() + 24, name.getY() - 20, 100, 16), Justification::centredLeft, Palette::textDim, 11.0f);
    caption (g, "FOLDER", Rectangle<int> (box.getX() + 24, folder.getY() - 20, 100, 16), Justification::centredLeft, Palette::textDim, 11.0f);
}

void SaveOverlay::resized()
{
    const int bh = newFolder.isVisible() ? 300 : 256;
    box = Rectangle<int> (420, bh).withCentre (getLocalBounds().getCentre());
    name.setBounds (box.getX() + 24, box.getY() + 74, box.getWidth() - 48, 34);
    folder.setBounds (box.getX() + 24, name.getBottom() + 30, box.getWidth() - 48, 32);
    newFolder.setBounds (box.getX() + 24, folder.getBottom() + 10, box.getWidth() - 48, 32);
    save.setBounds (box.getRight() - 24 - 110, box.getBottom() - 24 - 34, 110, 34);
    cancel.setBounds (save.getX() - 10 - 100, save.getY(), 100, 34);
}

// ============================================================================
LiveVoiceEditor::LiveVoiceEditor (LiveVoiceProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);
    auto& st = proc.apvts;

    auto setupKnob = [this] (Slider& s, bool big)
    {
        s.setSliderStyle (Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (big ? 420 : 300);
        s.setVelocityBasedMode (false);
        s.getProperties().set ("big", big);
        addAndMakeVisible (s);
    };

    // header
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);
    setupKnob (outKnob, false);
    aOut = std::make_unique<SA> (st, ParamID::output, outKnob);
    outKnob.setDoubleClickReturnValue (true, 0.0);
    muteBtn.getProperties().set ("kind", "mute"); muteBtn.setClickingTogglesState (true);
    addAndMakeVisible (muteBtn);
    aMute = std::make_unique<BA> (st, ParamID::mute, muteBtn);
    clipBtn.setClickingTogglesState (true);
    addAndMakeVisible (clipBtn);
    aClip = std::make_unique<BA> (st, ParamID::clipOn, clipBtn);
    audioBtn.onClick = [] { if (LiveVoiceEditor::onAudioSettings) LiveVoiceEditor::onAudioSettings(); };
    addAndMakeVisible (audioBtn);

    // presets + undo
    presetBox.getProperties().set ("kind", "preset");
    presetBox.onClick = [this] { showPresetMenu(); };
    for (auto* b : { &presetPrev, &presetNext }) b->getProperties().set ("kind", "arrow");
    presetPrev.onClick = [this] { stepPreset (-1); };
    presetNext.onClick = [this] { stepPreset (1); };
    saveBtn.onClick = [this] { saveOverlay.setBounds (getLocalBounds()); saveOverlay.open(); };
    undoBtn.onClick = [this] { proc.undo.undo(); };
    for (auto* b : { &presetPrev, &presetBox, &presetNext, &saveBtn, &undoBtn }) addAndMakeVisible (b);
    saveOverlay.onDone = [this] { grabKeyboardFocus(); };
    addChildComponent (saveOverlay);
    proc.addListener (this);

    // voice
    addAndMakeVisible (voiceLamp);
    aVoice = std::make_unique<BA> (st, ParamID::voiceOn, voiceLamp);
    for (int i = 0; i < 4; ++i)
    {
        auto& b = modeBtn[i];
        b.setButtonText (lv::kModes[i].name);
        b.getProperties().set ("kind", "mode");
        b.getProperties().set ("hint", lv::kModes[i].hint);
        b.onClick = [this, i] { setParam (ParamID::mode, (float) i); refreshButtons(); };
        addAndMakeVisible (b);
    }
    setupKnob (strengthKnob, true);
    strengthKnob.getProperties().set ("ticks", 21);
    aStrength = std::make_unique<SA> (st, ParamID::strength, strengthKnob);
    strengthKnob.setDoubleClickReturnValue (true, 0.7);

    // pitch
    setupKnob (pitchKnob, true);
    pitchKnob.getProperties().set ("ticks", 25);
    pitchKnob.getProperties().set ("majors", true);
    aPitch = std::make_unique<SA> (st, ParamID::formant, pitchKnob);
    pitchKnob.setDoubleClickReturnValue (true, 0.0);
    const float qv[4] = { -12.0f, -5.0f, 0.0f, 5.0f };
    const char* qt[4] = { "-12", "-5", "0", "+5" };
    for (int i = 0; i < 4; ++i)
    {
        quick[i].setButtonText (qt[i]);
        quick[i].getProperties().set ("kind", "quick");
        const float v = qv[i];
        quick[i].onClick = [this, v] { setParam (ParamID::formant, v); };
        addAndMakeVisible (quick[i]);
    }

    // sfx
    addAndMakeVisible (sfxLamp);
    aSfx = std::make_unique<BA> (st, ParamID::sfxOn, sfxLamp);
    addAndMakeVisible (sfxScreen);
    sfxScreen.setInterceptsMouseClicks (false, false);
    for (auto* b : { &prevBtn, &nextBtn })
    {
        b->getProperties().set ("kind", "arrow");
        addAndMakeVisible (b);
    }
    prevBtn.onClick = [this] { setParam (ParamID::sfxType, (float) (((int) getParam (ParamID::sfxType) + 3) % 4)); };
    nextBtn.onClick = [this] { setParam (ParamID::sfxType, (float) (((int) getParam (ParamID::sfxType) + 1) % 4)); };
    setupKnob (amountKnob, true);
    amountKnob.getProperties().set ("ticks", 21);
    aAmount = std::make_unique<SA> (st, ParamID::sfxAmount, amountKnob);
    amountKnob.setDoubleClickReturnValue (true, 0.5);

    // keys (SPACE, M, arrows) always go to the editor, never to a focused button/knob
    for (auto* c : getChildren())
        if (c != &saveOverlay) { c->setWantsKeyboardFocus (false); c->setMouseClickGrabsKeyboardFocus (false); }
    setWantsKeyboardFocus (true);
    setSize (1060, 586);
    Component::SafePointer<LiveVoiceEditor> safe (this);
    MessageManager::callAsync ([safe] { if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus(); });
    refreshButtons();
    startTimerHz (30);
}

LiveVoiceEditor::~LiveVoiceEditor()
{
    proc.removeListener (this);
    setLookAndFeel (nullptr);
}

void LiveVoiceEditor::audioProcessorParameterChangeGestureBegin (AudioProcessor*, int)
{
    // every knob turn / click is one UNDO step (may come from the audio or message thread - UndoManager is used on the message thread only)
    if (MessageManager::getInstance()->isThisTheMessageThread()) proc.undo.beginNewTransaction();
}

void LiveVoiceEditor::showPresetMenu()
{
    PopupMenu m;
    auto files = proc.listPresets();
    auto root = LiveVoiceProcessor::presetRoot();
    std::map<String, PopupMenu> folders;
    StringArray order;
    for (int i = 0; i < files.size(); ++i)
    {
        const auto& f = files.getReference (i);
        const bool ticked = f == proc.presetFile;
        if (f.getParentDirectory() == root) m.addItem (i + 1, f.getFileNameWithoutExtension(), true, ticked);
        else
        {
            const String fn = f.getParentDirectory().getFileName();
            if (! order.contains (fn)) order.add (fn);
            folders[fn].addItem (i + 1, f.getFileNameWithoutExtension(), true, ticked);
        }
    }
    if (order.contains ("Factory")) { order.removeString ("Factory"); order.insert (0, "Factory"); }
    for (auto& fn : order) m.addSubMenu (fn, folders[fn]);
    m.addSeparator();
    m.addItem (9001, "INIT (reset all)");
    m.addItem (9002, "Open presets folder");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (presetBox).withMinimumWidth (presetBox.getWidth()),
                     [this, files] (int r)
                     {
                         if (r == 9001) proc.initSound();
                         else if (r == 9002) { LiveVoiceProcessor::presetRoot().createDirectory(); LiveVoiceProcessor::presetRoot().startAsProcess(); }
                         else if (r > 0 && r <= files.size()) proc.loadPreset (files[r - 1]);
                         grabKeyboardFocus();
                     });
}

void LiveVoiceEditor::stepPreset (int dir)
{
    auto files = proc.listPresets();
    if (files.isEmpty()) return;
    int i = files.indexOf (proc.presetFile);
    i = i < 0 ? (dir > 0 ? 0 : files.size() - 1) : (i + dir + files.size()) % files.size();
    proc.loadPreset (files[i]);
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

void LiveVoiceEditor::refreshButtons()
{
    const int m = (int) getParam (ParamID::mode);
    for (int i = 0; i < 4; ++i) modeBtn[i].setToggleState (i == m, dontSendNotification);
    const float pv = getParam (ParamID::formant);
    const float qv[4] = { -12.0f, -5.0f, 0.0f, 5.0f };
    for (int i = 0; i < 4; ++i) quick[i].setToggleState (std::abs (pv - qv[i]) < 0.05f, dontSendNotification);
    sfxScreen.type = (int) getParam (ParamID::sfxType);
    sfxScreen.on = getParam (ParamID::sfxOn) > 0.5f;
    const bool von = getParam (ParamID::voiceOn) > 0.5f;
    for (auto& b : modeBtn) b.setAlpha (von ? 1.0f : 0.45f);
    strengthKnob.setAlpha (von ? 1.0f : 0.45f);
    amountKnob.setAlpha (sfxScreen.on ? 1.0f : 0.5f);
}

void LiveVoiceEditor::timerCallback()
{
    inMeter.setLevel (proc.engine.inLevel.load());
    outMeter.setLevel (proc.engine.outLevel.load());
    sfxScreen.activity = sfxScreen.activity * 0.7f + proc.engine.sfxActivity.load() * 0.3f;
    refreshButtons();
    if (presetBox.getButtonText() != proc.presetName) presetBox.setButtonText (proc.presetName);
    undoBtn.setEnabled (proc.undo.canUndo());
    undoBtn.setAlpha (proc.undo.canUndo() ? 1.0f : 0.45f);
    sfxScreen.repaint();
    repaint (pitchPanel.withHeight (60));
    repaint (voicePanel.withTop (voicePanel.getBottom() - 60));
    repaint (sfxPanel.withTop (sfxPanel.getBottom() - 60));
}

bool LiveVoiceEditor::keyPressed (const KeyPress& k)
{
    if (saveOverlay.isVisible()) return false;
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
    { if (k.getModifiers().isShiftDown()) proc.undo.redo(); else proc.undo.undo(); return true; }
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) { proc.undo.redo(); return true; }
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'S' || k.getKeyCode() == 's')) { saveBtn.triggerClick(); return true; }
    if (k == KeyPress::spaceKey) { setParam (ParamID::voiceOn, getParam (ParamID::voiceOn) > 0.5f ? 0.0f : 1.0f); return true; }
    if (k.getTextCharacter() == 'm' || k.getTextCharacter() == 'M' || k.getTextCharacter() == L'ь' || k.getTextCharacter() == L'Ь')
    { setParam (ParamID::mute, getParam (ParamID::mute) > 0.5f ? 0.0f : 1.0f); return true; }
    if (k.getTextCharacter() == 'c' || k.getTextCharacter() == 'C' || k.getTextCharacter() == L'с' || k.getTextCharacter() == L'С')
    { setParam (ParamID::clipOn, getParam (ParamID::clipOn) > 0.5f ? 0.0f : 1.0f); return true; }
    if (k == KeyPress::upKey)   { setParam (ParamID::formant, jmin (12.0f, std::round (getParam (ParamID::formant)) + 1.0f)); return true; }
    if (k == KeyPress::downKey) { setParam (ParamID::formant, jmax (-12.0f, std::round (getParam (ParamID::formant)) - 1.0f)); return true; }
    if (k.getTextCharacter() == '0') { setParam (ParamID::formant, 0.0f); return true; }
    return false;
}

void LiveVoiceEditor::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Palette::bgTop, 0, 0, Palette::bgBottom, 0, full.getHeight(), false));
    g.fillAll();

    // title + version plate (like GRAIN FORM)
    g.setColour (Palette::text);
    g.setFont (uiFont (36.0f, false, 0.05f));
    const String title = "LIVE VOICE";
    g.drawText (title, Rectangle<int> (26, 10, 400, 44), Justification::centredLeft);
    const int tw = GlyphArrangement::getStringWidthInt (uiFont (36.0f, false, 0.05f), title);
    auto plate = Rectangle<float> ((float) (26 + tw + 12), 22.0f, 46.0f, 20.0f);
    g.setColour (Palette::button);
    g.fillRoundedRectangle (plate, 4.0f);
    g.setColour (Palette::label);
    g.setFont (uiFont (11.5f, true, 0.06f));
    g.drawText ("v" + String (LIVEVOICE_LABEL), plate.toNearestInt(), Justification::centred);
    g.setColour (Palette::textDim);
    g.setFont (uiFont (11.5f, true, 0.30f));
    g.drawText ("LIVE VOCAL PROCESSOR", Rectangle<int> (28, 52, 260, 16), Justification::centredLeft);

    // header labels
    caption (g, "IN", inMeter.getBounds().withX (inMeter.getX() - 30).withWidth (26).expanded (0, 4), Justification::centredRight, Palette::textDim, 10.0f);
    caption (g, "OUT", outMeter.getBounds().withX (outMeter.getX() - 30).withWidth (26).expanded (0, 4), Justification::centredRight, Palette::textDim, 10.0f);
    caption (g, "LEVEL", Rectangle<int> (outKnob.getX() - 12, outKnob.getBottom() - 2, outKnob.getWidth() + 24, 14), Justification::centred, Palette::textDim, 10.0f);

    for (auto r : { voicePanel, pitchPanel, sfxPanel }) panelBox (g, r.toFloat());

    // ---- VOICE ----
    caption (g, "VOICE", Rectangle<int> (voicePanel.getX() + 22, voicePanel.getY() + 16, 120, 24), Justification::centredLeft, Palette::text, 14.0f);
    caption (g, "STRENGTH", Rectangle<int> (voicePanel.getX(), strengthKnob.getBottom() + 4, voicePanel.getWidth(), 18), Justification::centred, Palette::label, 12.0f);
    g.setColour (Palette::accent); g.setFont (uiFont (13.0f, true));
    g.drawText (String (roundToInt (getParam (ParamID::strength) * 100)) + " %", Rectangle<int> (voicePanel.getX(), strengthKnob.getBottom() + 22, voicePanel.getWidth(), 18), Justification::centred);

    // ---- PITCH ----
    caption (g, "PITCH", Rectangle<int> (pitchPanel.getX() + 22, pitchPanel.getY() + 16, 120, 24), Justification::centredLeft, Palette::text, 14.0f);
    const float pv = getParam (ParamID::formant);
    g.setColour (std::abs (pv) < 0.05f ? Palette::text : Palette::glow);
    g.setFont (uiFont (24.0f, true, 0.02f));
    g.drawText (semis (pv), Rectangle<int> (pitchPanel.getRight() - 182, pitchPanel.getY() + 12, 160, 32), Justification::centredRight);
    {
        // scale labels around the big knob
        const auto c = pitchKnob.getBounds().toFloat().getCentre();
        const float rr = pitchKnob.getWidth() * 0.5f + 14.0f;
        g.setFont (uiFont (11.5f, true));
        g.setColour (Palette::textDim);
        const float a0 = MathConstants<float>::pi * 1.25f, a1 = MathConstants<float>::pi * 2.75f;
        const char* lbl[5] = { "-12", "-6", "0", "+6", "+12" };
        for (int i = 0; i < 5; ++i)
        {
            const auto pt = c.getPointOnCircumference (rr, a0 + (a1 - a0) * (float) i / 4.0f);
            g.drawText (lbl[i], Rectangle<float> (36, 16).withCentre (pt).toNearestInt(), Justification::centred);
        }
        caption (g, "LOWER", Rectangle<int> ((int) (c.x - rr - 40), (int) (c.y + rr * 0.80f), 80, 16), Justification::centred, Palette::textDim, 10.5f);
        caption (g, "HIGHER", Rectangle<int> ((int) (c.x + rr - 40), (int) (c.y + rr * 0.80f), 80, 16), Justification::centred, Palette::textDim, 10.5f);
    }
    // hint under the quick buttons
    g.setColour (Palette::textDim); g.setFont (uiFont (11.5f));
    g.drawText ("double-click = normal voice      UP / DOWN = one step",
                Rectangle<int> (pitchPanel.getX(), quick[0].getBottom() + 26, pitchPanel.getWidth(), 18), Justification::centred);

    // ---- SFX ----
    caption (g, "SFX", Rectangle<int> (sfxPanel.getX(), sfxPanel.getY() + 16, sfxPanel.getWidth(), 24), Justification::centred,
             getParam (ParamID::sfxOn) > 0.5f ? Palette::glow : Palette::text, 14.0f);
    caption (g, "AMOUNT", Rectangle<int> (sfxPanel.getX(), amountKnob.getBottom() + 4, sfxPanel.getWidth(), 18), Justification::centred, Palette::label, 12.0f);
    g.setColour (Palette::accent); g.setFont (uiFont (13.0f, true));
    g.drawText (String (roundToInt (getParam (ParamID::sfxAmount) * 100)) + " %", Rectangle<int> (sfxPanel.getX(), amountKnob.getBottom() + 22, sfxPanel.getWidth(), 18), Justification::centred);

    // footer: shortcuts
    g.setColour (Palette::textDim.withAlpha (0.8f));
    g.setFont (uiFont (11.0f));
    g.drawText ("SPACE  voice on/off      M  mute      C  clip      UP / DOWN  pitch      0  normal voice      CTRL+Z  undo      CTRL+S  save preset",
                Rectangle<int> (0, getHeight() - 26, getWidth(), 18), Justification::centred);
}

void LiveVoiceEditor::resized()
{
    // header: title | preset bar | meters, level, audio, mute
    muteBtn.setBounds (getWidth() - 24 - 80, 20, 80, 34);
    audioBtn.setBounds (muteBtn.getX() - 8 - 70, 20, 70, 34);
    outKnob.setBounds (audioBtn.getX() - 16 - 40, 12, 40, 40);
    clipBtn.setBounds (outKnob.getX() - 14 - 56, 20, 56, 34);
    outMeter.setBounds (clipBtn.getX() - 12 - 80, 41, 80, 9);
    inMeter.setBounds (outMeter.getX(), 24, 80, 9);

    const int px = 272, py = 20, ph = 34;
    presetPrev.setBounds (px, py, 26, ph);
    presetBox.setBounds (presetPrev.getRight() + 4, py, 150, ph);
    presetNext.setBounds (presetBox.getRight() + 4, py, 26, ph);
    saveBtn.setBounds (presetNext.getRight() + 8, py, 58, ph);
    undoBtn.setBounds (saveBtn.getRight() + 6, py, 58, ph);

    const int top = 84, h = getHeight() - top - 34;
    voicePanel = { 24, top, 280, h };
    pitchPanel = { voicePanel.getRight() + 16, top, 404, h };
    sfxPanel   = { pitchPanel.getRight() + 16, top, getWidth() - 24 - (pitchPanel.getRight() + 16), h };

    // voice
    voiceLamp.setBounds (voicePanel.getRight() - 46, voicePanel.getY() + 12, 30, 30);
    for (int i = 0; i < 4; ++i)
        modeBtn[i].setBounds (voicePanel.getX() + 18, voicePanel.getY() + 56 + i * 60, voicePanel.getWidth() - 36, 52);
    strengthKnob.setBounds (Rectangle<int> (104, 104).withCentre ({ voicePanel.getCentreX(), voicePanel.getY() + 368 }));

    // pitch
    pitchKnob.setBounds (Rectangle<int> (236, 236).withCentre ({ pitchPanel.getCentreX(), pitchPanel.getY() + 196 }));
    const int qw = 62, qg = 10, qx = pitchPanel.getCentreX() - (4 * qw + 3 * qg) / 2;
    for (int i = 0; i < 4; ++i) quick[i].setBounds (qx + i * (qw + qg), pitchPanel.getY() + 368, qw, 30);

    // sfx
    sfxLamp.setBounds (sfxPanel.getX() + 14, sfxPanel.getY() + 12, 30, 30);
    sfxScreen.setBounds (sfxPanel.getX() + 16, sfxPanel.getY() + 52, sfxPanel.getWidth() - 32, 172);
    prevBtn.setBounds (sfxScreen.getX() + 4, sfxScreen.getY() + 42, 30, 40);
    nextBtn.setBounds (sfxScreen.getRight() - 34, sfxScreen.getY() + 42, 30, 40);
    amountKnob.setBounds (Rectangle<int> (128, 128).withCentre ({ sfxPanel.getCentreX(), sfxPanel.getY() + 318 }));

    saveOverlay.setBounds (getLocalBounds());
}
