// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Widgets.h"

#include <juce_audio_basics/juce_audio_basics.h>

void drawFace(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    g.setGradientFill(juce::ColourGradient(c.brighter(0.08f), r.getX(), r.getY(),
                                           c.darker(0.12f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(c.brighter(0.35f).withAlpha(0.6f));
    g.drawLine(r.getX() + 3.0f, r.getY() + 0.5f, r.getRight() - 3.0f, r.getY() + 0.5f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
}

//============================================================================== Pad

Pad::Pad(juce::String t) : text(std::move(t)) {}

void Pad::setText(const juce::String& t) { if (t != text) { text = t; repaint(); } }
void Pad::setLit(bool b)                 { if (b != lit) { lit = b; repaint(); } }
void Pad::setMarked(bool b)              { if (b != marked) { marked = b; repaint(); } }

void Pad::setFace(juce::Colour f, juce::Colour t)
{
    face = f;
    ink = t;
    repaint();
}

void Pad::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    pressed = true;
    repaint();
    if (onPress) onPress();
}

void Pad::mouseUp(const juce::MouseEvent&)
{
    const bool was = pressed;
    pressed = false;
    repaint();
    if (was && onRelease) onRelease();
}

void Pad::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.0f);
    if (pressed) r = r.translated(0.0f, 1.0f);

    g.setGradientFill(juce::ColourGradient(face.brighter(pressed ? 0.0f : 0.15f), r.getX(), r.getY(),
                                           face.darker(0.3f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.0f);
    if (litFace && lit)   // too small a pad for an LED: lit, it glows instead
    {
        g.setColour(led.withAlpha(0.65f));
        g.fillRoundedRectangle(r, 3.0f);
    }
    g.setColour(marked ? colours::light : juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(r, 3.0f, marked ? 1.8f : 1.0f);

    auto inside = r.reduced(2.0f);
    const bool tall = r.getHeight() >= 26.0f;

    if (showsLed)
    {
        const float d = tall ? 6.0f : 5.0f;
        const auto ledBox = tall ? juce::Rectangle<float>(d, d).withCentre({ inside.getCentreX(), inside.getY() + 5.0f })
                                 : juce::Rectangle<float>(d, d).withCentre({ inside.getX() + 5.0f, inside.getCentreY() });
        if (lit)
        {
            g.setColour(led.withAlpha(0.35f));
            g.fillEllipse(ledBox.expanded(3.0f));
        }
        g.setColour(lit ? led.brighter(0.3f) : led.darker(2.2f).withAlpha(0.9f));
        g.fillEllipse(ledBox);

        if (tall) inside.removeFromTop(9.0f);
        else      inside.removeFromLeft(9.0f);
    }

    if (arrow != 0)
    {
        // drawn, not a glyph: the fonts that have the triangles do not agree on their size
        const auto c = inside.getCentre();
        const float w = 5.0f, h = 6.0f;
        juce::Path p;
        p.addTriangle(c.x - w * arrow, c.y - h, c.x - w * arrow, c.y + h, c.x + w * arrow, c.y);
        g.setColour(ink);
        g.fillPath(p);
    }
    else if (text.isNotEmpty())
    {
        g.setColour(ink);
        g.setFont(fonts::bold(juce::jmin(11.0f, inside.getHeight() - 1.0f)));
        g.drawFittedText(text, inside.toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
}

//============================================================================== Knob

Knob::Knob(const juce::String& l, float lo, float hi, float initial,
           std::function<float()> g, std::function<void(float)> s)
    : legend(l), get(std::move(g)), set(std::move(s))
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setRange(lo, hi, 0.0);
    slider.setMouseDragSensitivity(180);
    slider.setScrollWheelEnabled(false);   // the wheel scrolls the rack, not whatever it passes over
    slider.setDoubleClickReturnValue(true, initial);
    slider.setPopupDisplayEnabled(true, true, nullptr);
    slider.textFromValueFunction = [] (double v) { return juce::String(v, 1); };
    slider.onValueChange = [this] { set((float) slider.getValue()); };
    slider.onDragStart = [this] { if (onHeld) onHeld(true); };
    slider.onDragEnd = [this] { if (onHeld) onHeld(false); };
    addAndMakeVisible(slider);

    setColours(juce::Colour(0xff1d1d1f), colours::light, colours::ink);
    refresh();
}

void Knob::setColours(juce::Colour cap, juce::Colour pointer, juce::Colour ink)
{
    slider.setColour(juce::Slider::rotarySliderFillColourId, cap);
    slider.setColour(juce::Slider::thumbColourId, pointer);
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, ink);
    legendInk = ink;
    repaint();
}

void Knob::refresh()
{
    slider.setValue(get(), juce::dontSendNotification);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(12);
    const int side = juce::jmin(r.getWidth(), r.getHeight());
    slider.setBounds(r.withSizeKeepingCentre(side, side).withY(r.getY()));
}

void Knob::paint(juce::Graphics& g)
{
    g.setColour(legendInk);
    g.setFont(fonts::bold(9.5f));
    g.drawText(legend, getLocalBounds().removeFromTop(12), juce::Justification::centred);
}

//============================================================================== Switch

Switch::Switch(const juce::String& l, juce::StringArray p, std::function<int()> g, std::function<void(int)> s)
    : legend(l), positions(std::move(p)), get(std::move(g)), set(std::move(s))
{
    pad.setShowsLed(false);
    pad.onPress = [this]
    {
        set((get() + 1) % juce::jmax(1, positions.size()));
        refresh();
    };
    addAndMakeVisible(pad);
    refresh();
}

void Switch::refresh()
{
    pad.setText(positions[juce::jlimit(0, positions.size() - 1, get())]);
}

void Switch::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(14);
    pad.setBounds(r.withSizeKeepingCentre(juce::jmin(r.getWidth(), padWidth), 24));
}

void Switch::paint(juce::Graphics& g)
{
    g.setColour(legendInk);
    g.setFont(fonts::bold(9.5f));
    g.drawText(legend, getLocalBounds().removeFromTop(12), juce::Justification::centred);
}

//============================================================================== LedNumber

LedNumber::LedNumber(int l, int h, int i, int d, std::function<int()> g, std::function<void(int)> s)
    : lo(l), hi(h), initial(i), digits(d), get(std::move(g)), set(std::move(s))
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
}

void LedNumber::put(int v)
{
    set(juce::jlimit(lo, hi, v));
    repaint();
}

void LedNumber::setDecimals(int d)
{
    decimals = d;
    unit = 1;
    for (int i = 0; i < d; ++i) unit *= 10;
    repaint();
}

// a whole unit, or with Shift held the last place
int LedNumber::step(const juce::ModifierKeys& m) const { return m.isShiftDown() ? 1 : unit; }

void LedNumber::mouseDown(const juce::MouseEvent& e)
{
    dragFrom = get();
    dragBase = 0;
    dragFine = e.mods.isShiftDown();
}

void LedNumber::mouseDrag(const juce::MouseEvent& e)
{
    // Shift pressed or let go mid-drag carries on from where the value is, not from the start
    const int dy = e.getDistanceFromDragStartY();
    if (e.mods.isShiftDown() != dragFine)
    {
        dragFine = e.mods.isShiftDown();
        dragFrom = get();
        dragBase = dy;
    }
    put(dragFrom - (dy - dragBase) / 4 * step(e.mods));
}

void LedNumber::mouseDoubleClick(const juce::MouseEvent&)     { put(initial); }

void LedNumber::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! wheelEnabled) { Component::mouseWheelMove(e, w); return; }

    wheel += w.deltaY * 8.0f;
    const int whole = (int) wheel;
    if (whole != 0) { wheel -= (float) whole; put(get() + whole * step(e.mods)); }
}

void LedNumber::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::display);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);

    const auto font = fonts::digits(r.getHeight() * 0.78f);
    g.setFont(font);
    // with a point, the value in units and its last places: 1283 tenths is 128.3
    auto text = [this] (int v)
    {
        if (decimals == 0) return juce::String(v);
        return juce::String(v / unit) + "." + juce::String(std::abs(v % unit)).paddedLeft('0', decimals);
    };
    juce::String unlit = juce::String::repeatedString("8", digits - decimals);
    if (decimals > 0) unlit << "." << juce::String::repeatedString("8", decimals);

    g.setColour(colours::ledOff.withAlpha(0.4f));   // the unlit segments, there but not read
    g.drawText(unlit, r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
    g.setColour(colours::ledRed);
    g.drawText(text(shown ? shown() : get()), r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
}

//============================================================================== FoldBar

FoldBar::FoldBar()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setOpen(true);
}

void FoldBar::setOpen(bool b)
{
    open = b;
    setTooltip(open ? "Fold the " + label.toLowerCase() + " away, to see more of the rack at once."
                    : "Unfold the " + label.toLowerCase() + ".");
    repaint();
}

void FoldBar::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    setOpen(! open);
    if (onToggle) onToggle();
}

void FoldBar::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float cy = r.getCentreY();

    // the triangle: down when open, right when folded
    juce::Path p;
    if (open) p.addTriangle(4.0f, cy - 3.0f, 12.0f, cy - 3.0f, 8.0f, cy + 3.0f);
    else      p.addTriangle(5.0f, cy - 4.0f, 5.0f, cy + 4.0f, 11.0f, cy);
    g.setColour(ink);
    g.fillPath(p);

    g.setFont(fonts::bold(9.0f));
    const juce::String text = open ? label : label + "  (FOLDED)";
    g.drawText(text, r.withTrimmedLeft(18.0f), juce::Justification::centredLeft);

    // a rule on from the text, to the end
    const float from = 18.0f + juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), text) + 8.0f;
    g.setColour(ink.withAlpha(0.35f));
    g.fillRect(from, cy, r.getRight() - from, 1.0f);
}

//============================================================================== PatternPanel

PatternPanel::PatternPanel(debirth::TrackBase& t, debirth::Channel& c, std::function<bool()> p)
    : track(t), channel(c), playing(std::move(p))
{
    using namespace debirth;

    for (int i = 0; i < patternsPerBank; ++i)
    {
        numbers[i].setText(juce::String(i + 1));
        numbers[i].onPress = [this, i]
        {
            const int index = track.selected.load() / patternsPerBank * patternsPerBank + i;

            // the song playing: the pattern takes the machine over, on the bar - and picked
            // again, gives it back to the song
            if (songPlaying && songPlaying())
            {
                if (channel.liveWanted.load() && index == track.selected.load())
                {
                    channel.liveWanted = false;
                    return;
                }
                channel.liveWanted = true;
            }

            select(index);
            channel.stopWanted = false;   // a pattern picked brings a STOPped machine back, on the bar
        };
        addAndMakeVisible(numbers[i]);
    }

    for (int b = 0; b < banks; ++b)
    {
        bankPads[b].setText(juce::String::charToString((juce::juce_wchar) ('A' + b)));
        bankPads[b].onPress = [this, b] { select(b * patternsPerBank + track.selected.load() % patternsPerBank); };
        addAndMakeVisible(bankPads[b]);
    }

    shuffle.setTooltip("Whether this machine swings with the SHUFFLE knob in the transport.");
    shuffle.onPress = [this] { track.shuffle = ! track.shuffle.load(); };
    addAndMakeVisible(shuffle);

    // Not a switch: STOP only stops. Picking a pattern is what brings the machine back.
    stop.setTooltip("STOP this machine at the start of the next bar: it lets go of its notes and plays "
                    "nothing more. Pick a pattern to bring it back in, at the start of a bar.");
    stop.setLedColour(colours::ledRed);
    stop.onPress = [this] { channel.stopWanted = true; };
    addAndMakeVisible(stop);

    copy.setTooltip("Copy this pattern, to paste over another - on this machine or another. A mono synth's "
                    "line pastes onto the other mono synth, or onto the Mz950, the Mz7 or the Mz106 as notes; "
                    "their notes paste onto each other; a drum pattern onto its own machine.");
    copy.onPress = [this] { copyOut(); };
    paste.onPress = [this] { pasteIn(); if (onChange) onChange(); };
    clear.setTooltip("Clear this pattern.");
    clear.onPress = [this] { track.clear(track.selected.load()); if (onChange) onChange(); };
    for (auto* b : { &copy, &paste, &clear })
    {
        b->setShowsLed(false);
        addAndMakeVisible(*b);
    }

    steps = std::make_unique<LedNumber>(1, stepsPerPattern, stepsPerPattern, 2,
        [this] { return track.pattern(track.selected.load()).len(); },
        [this] (int v) { track.pattern(track.selected.load()).length = v; if (onChange) onChange(); });
    steps->setTooltip("How many steps this pattern has. Drag up or down; double-click for 16.");
    steps->setWheelEnabled(false);
    addAndMakeVisible(*steps);
}

//
// THE CLIPBOARD, one for the whole rack: what was copied, by kind, with its notes. A mono
// synth's line is kept as its steps and the pitch its note 0 plays, so it can go onto the
// other mono synth or into a roll of notes at the pitches it had; a roll's notes go onto
// either roll; a drum pattern only back onto its own machine, whose rows are its instruments.
// Nothing goes from a roll onto a mono synth - chords have nowhere to go on one.
//
namespace
{
    struct Clip
    {
        enum Kind { None, Mono, Notes, Drums } kind = None;
        const debirth::TrackBase* from = nullptr;
        int length = debirth::stepsPerPattern;
        int root = 36;
        uint32_t steps[debirth::stepsPerPattern] {};
        uint32_t hits[debirth::NotePattern::rows] {};
    };
    Clip clip;

    Clip::Kind kindOf(const debirth::TrackBase& t)
    {
        using namespace debirth;
        if (dynamic_cast<const Track<BassPattern>*>(&t) != nullptr) return Clip::Mono;
        if (dynamic_cast<const Track<NotePattern>*>(&t) != nullptr) return Clip::Notes;
        if (dynamic_cast<const Track<DrumPattern>*>(&t) != nullptr) return Clip::Drums;
        return Clip::None;
    }
}

void PatternPanel::copyOut()
{
    using namespace debirth;
    const int index = track.selected.load();

    clip = {};
    clip.kind = kindOf(track);
    clip.from = &track;
    clip.root = monoRoot;
    clip.length = track.pattern(index).len();

    if (auto* mono = dynamic_cast<Track<BassPattern>*>(&track))
        for (int i = 0; i < stepsPerPattern; ++i) clip.steps[i] = mono->patterns[index].steps[i].load();
    else if (auto* notes = dynamic_cast<Track<NotePattern>*>(&track))
        for (int r = 0; r < NotePattern::rows; ++r) clip.hits[r] = notes->patterns[index].hits[r].load();
    else if (auto* drums = dynamic_cast<Track<DrumPattern>*>(&track))
        for (int r = 0; r < DrumPattern::rows; ++r) clip.hits[r] = drums->patterns[index].hits[r].load();
}

bool PatternPanel::canPaste() const
{
    const auto here = kindOf(track);
    switch (clip.kind)
    {
        case Clip::Mono:  return here == Clip::Mono || here == Clip::Notes;
        case Clip::Notes: return here == Clip::Notes;
        case Clip::Drums: return clip.from == &track;
        default:          return false;
    }
}

void PatternPanel::pasteIn()
{
    using namespace debirth;
    if (! canPaste()) return;
    const int index = track.selected.load();

    if (auto* mono = dynamic_cast<Track<BassPattern>*>(&track))
    {
        // the same pitches from this machine's root, an octave nearer where they will not reach
        auto& p = mono->patterns[index];
        for (int i = 0; i < stepsPerPattern; ++i)
        {
            BassStep s = BassStep::unpack(clip.steps[i]);
            int rel = s.midiNote(clip.root) - monoRoot;
            while (rel < -12) rel += 12;
            while (rel > 24) rel -= 12;
            s.octave = rel < 0 ? -1 : rel > 12 ? 1 : 0;
            s.note = rel - 12 * s.octave;
            p.set(i, s);
        }
        p.length = clip.length;
        return;
    }

    if (auto* notes = dynamic_cast<Track<NotePattern>*>(&track))
    {
        auto& p = notes->patterns[index];
        if (clip.kind == Clip::Notes)
        {
            for (int r = 0; r < NotePattern::rows; ++r) p.hits[r].store(clip.hits[r]);
            p.length = clip.length;
            return;
        }

        // a line as notes: each gate a note at its pitch, accents as accents, and a slide onto
        // the same pitch a tie - the one slide a roll of notes can hold
        p.clear();
        for (int i = 0; i < clip.length; ++i)
        {
            const BassStep s = BassStep::unpack(clip.steps[i]);
            if (! s.gate) continue;
            const int pitch = juce::jlimit(0, NotePattern::notes - 1, s.midiNote(clip.root));
            p.set(pitch, i, true);
            if (s.accent) p.set(NotePattern::accentRow, i, true);

            const BassStep before = BassStep::unpack(clip.steps[(i + clip.length - 1) % clip.length]);
            if (i > 0 && before.gate && before.slide && before.midiNote(clip.root) == s.midiNote(clip.root))
                p.set(NotePattern::tieRow, i, true);
        }
        p.length = clip.length;
        return;
    }

    if (auto* drums = dynamic_cast<Track<DrumPattern>*>(&track))
    {
        auto& p = drums->patterns[index];
        for (int r = 0; r < DrumPattern::rows; ++r) p.hits[r].store(clip.hits[r]);
        p.length = clip.length;
    }
}

void PatternPanel::select(int index)
{
    track.selected = juce::jlimit(0, debirth::patternsPerTrack - 1, index);
    if (! playing()) track.current = track.selected.load();
    if (onChange) onChange();
}

void PatternPanel::tick(bool blinkPhase)
{
    using namespace debirth;

    const int sel = track.selected.load(), cur = track.current.load();
    const int bank = sel / patternsPerBank;
    const bool waiting = playing() && cur != sel;

    // The song playing, the pads show a pattern only while it has the machine: lit, or
    // blinking while it waits for the bar to take over or to hand back.
    if (songPlaying && songPlaying())
    {
        const bool wanted = channel.liveWanted.load(), live = channel.live.load();
        const bool lit = wanted == live ? live : blinkPhase;
        for (int i = 0; i < patternsPerBank; ++i)
        {
            const int index = bank * patternsPerBank + i;
            numbers[i].setLit(index == sel && lit && (! (live && waiting) || blinkPhase));
            numbers[i].setMarked(live && waiting && index == cur);
        }
    }
    else
    {
        for (int i = 0; i < patternsPerBank; ++i)
        {
            const int index = bank * patternsPerBank + i;
            numbers[i].setLit(index == sel && (! waiting || blinkPhase));
            numbers[i].setMarked(waiting && index == cur);
        }
    }
    for (int b = 0; b < banks; ++b)
        bankPads[b].setLit(b == bank);

    shuffle.setLit(track.shuffle.load());

    // STOP: lit while stopped, blinking while it waits for the bar to stop or to start again
    const bool stopWaiting = playing() && channel.stopWanted.load() != channel.stopped.load();
    stop.setLit(stopWaiting ? blinkPhase : channel.stopped.load());
    const bool can = canPaste();
    paste.setEnabled(can);
    paste.setAlpha(can ? 1.0f : 0.5f);
    paste.setTooltip(can ? "Paste the copied pattern over this one."
                         : "Nothing copied that this machine can take: copy a pattern first. Chords don't "
                           "paste onto a mono synth, nor one drum machine's pattern onto the other.");
    steps->repaint();
}

void PatternPanel::setCompact(bool c)
{
    if (c == compact) return;
    compact = c;
    resized();
    repaint();
}

void PatternPanel::resized()
{
    if (compact)
    {
        auto r = getLocalBounds().reduced(6);
        const int w = r.getWidth() / 4;
        for (int row = 0; row < 2; ++row)
        {
            auto line = r.removeFromTop(20);
            for (int i = 0; i < 4; ++i)
                numbers[row * 4 + i].setBounds(line.getX() + i * w, line.getY(), w, 20);
            r.removeFromTop(2);
        }

        r.removeFromTop(2);
        auto line = r.removeFromTop(20);
        for (int b = 0; b < debirth::banks; ++b)
            bankPads[b].setBounds(line.getX() + b * w, line.getY(), w, 20);

        r.removeFromTop(4);
        line = r.removeFromTop(20);
        steps->setBounds(line.removeFromLeft(40));
        line.removeFromLeft(4);
        const int e = line.getWidth() / 3;
        copy.setBounds(line.removeFromLeft(e));
        paste.setBounds(line.removeFromLeft(e));
        clear.setBounds(line);

        r.removeFromTop(2);
        line = r.removeFromTop(20);
        shuffle.setBounds(line.removeFromLeft(line.getWidth() / 2 + 8));
        line.removeFromLeft(2);
        stop.setBounds(line);
        return;
    }

    auto r = getLocalBounds().reduced(8);
    r.removeFromTop(16);

    auto grid = r.removeFromTop(64);
    const int w = grid.getWidth() / 4;
    for (int i = 0; i < debirth::patternsPerBank; ++i)
        numbers[i].setBounds(grid.getX() + (i % 4) * w, grid.getY() + (i / 4) * 32, w, 32);

    r.removeFromTop(14);
    auto bankRow = r.removeFromTop(24);
    for (int b = 0; b < debirth::banks; ++b)
        bankPads[b].setBounds(bankRow.getX() + b * w, bankRow.getY(), w, 24);

    r.removeFromTop(14);
    auto row = r.removeFromTop(24);
    steps->setBounds(row.removeFromLeft(40));
    row.removeFromLeft(6);
    stop.setBounds(row.removeFromRight(46));
    row.removeFromRight(4);
    shuffle.setBounds(row);

    r.removeFromTop(8);
    auto edit = r.removeFromTop(20);
    const int e = edit.getWidth() / 3;
    copy.setBounds(edit.removeFromLeft(e));
    paste.setBounds(edit.removeFromLeft(e));
    clear.setBounds(edit);
}

void PatternPanel::paint(juce::Graphics& g)
{
    drawFace(g, getLocalBounds().toFloat(), colours::metal);
    if (compact) return;   // the pads say what they are; there is no room to say it twice

    auto r = getLocalBounds().reduced(8);
    g.setColour(colours::light);
    g.setFont(fonts::bold(11.0f));
    g.drawText("PATTERN", r.removeFromTop(14), juce::Justification::centredLeft);

    g.setFont(fonts::bold(9.0f));
    g.setColour(colours::light.withAlpha(0.7f));
    r.removeFromTop(66);
    g.drawText("BANK", r.removeFromTop(12), juce::Justification::bottomLeft);
    r.removeFromTop(26);
    g.drawText("STEPS", r.removeFromTop(12), juce::Justification::bottomLeft);
}

//============================================================================== LevelMeter

namespace
{
    constexpr float meterFloor = -48.0f, meterTop = 6.0f;
    constexpr float fallPerTick = 24.0f / 30.0f;   // dB a second, at the panel's 30 a second
    constexpr int holdTicks = 30;                  // a second

    float meterFraction(float db) { return juce::jlimit(0.0f, 1.0f, (db - meterFloor) / (meterTop - meterFloor)); }
}

LevelMeter::LevelMeter()
{
    setInterceptsMouseClicks(false, false);
}

void LevelMeter::push(float peakL, float peakR)
{
    bool changed = false;
    const float peaks[2] = { peakL, peakR };
    for (int i = 0; i < 2; ++i)
    {
        auto& b = bars[i];
        const float db = juce::Decibels::gainToDecibels(peaks[i], -100.0f);
        const float level = std::max(db, b.level - fallPerTick);

        if (db >= b.held)            { b.held = db; b.holdFor = holdTicks; }
        else if (--b.holdFor <= 0)   { b.held = level; }

        changed |= std::abs(level - b.level) > 0.05f || b.holdFor == holdTicks;
        b.level = level;
    }
    if (changed) repaint();
}

void LevelMeter::paint(juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff0d0e10));
    g.fillRoundedRectangle(area, 1.5f);

    // the two bars, along the meter: side by side standing up, one over the other lying down
    for (int i = 0; i < 2; ++i)
    {
        auto bar = vertical ? area.reduced(1.0f).withWidth((area.getWidth() - 3.0f) / 2.0f).translated(i * (area.getWidth() - 1.0f) / 2.0f, 0.0f)
                            : area.reduced(1.0f).withHeight((area.getHeight() - 3.0f) / 2.0f).translated(0.0f, i * (area.getHeight() - 1.0f) / 2.0f);
        const float length = vertical ? bar.getHeight() : bar.getWidth();

        // the part of the bar between two levels, from the quiet end
        auto span = [&] (float from, float to)
        {
            const float a = meterFraction(from) * length, b = meterFraction(to) * length;
            return vertical ? juce::Rectangle<float>(bar.getX(), bar.getBottom() - b, bar.getWidth(), b - a)
                            : juce::Rectangle<float>(bar.getX() + a, bar.getY(), b - a, bar.getHeight());
        };

        const struct { float from, to; juce::Colour colour; } zones[] = {
            { meterFloor, -12.0f, colours::ledGreen },
            { -12.0f, 0.0f, colours::stepYellow },
            { 0.0f, meterTop, colours::ledRed },
        };
        const float level = bars[i].level;
        for (const auto& z : zones)
        {
            g.setColour(z.colour.withAlpha(0.12f));   // unlit, there to read the scale by
            g.fillRect(span(z.from, z.to));
            if (level > z.from)
            {
                g.setColour(z.colour);
                g.fillRect(span(z.from, std::min(level, z.to)));
            }
        }

        // the peak, held
        if (bars[i].held > meterFloor)
        {
            const float held = std::min(bars[i].held, meterTop);
            g.setColour(held >= 0.0f ? colours::ledRed : colours::light);
            const auto line = span(held - 0.8f, held);
            g.fillRect(line);
        }
    }
}

//============================================================================== MixerStrip

MixerStrip::MixerStrip(debirth::Channel& c, const juce::String& n)
    : channel(c), name(n),
      pan("PAN", -1.0f, 1.0f, 0.0f, [&c] { return c.pan.load(); }, [&c] (float v) { c.pan = v; ControlHooks::move(&c, 1, v); }),
      delay("DELAY", 0.0f, 1.0f, 0.0f, [&c] { return c.delay.load(); }, [&c] (float v) { c.delay = v; ControlHooks::move(&c, 2, v); }),
      reverb("REVERB", 0.0f, 1.0f, 0.0f, [&c] { return c.reverb.load(); }, [&c] (float v) { c.reverb = v; ControlHooks::move(&c, 3, v); })
{
    // A channel's controls, for automation, in the order the rack lists them after a
    // machine's own: level, pan, the sends, MUTE, and the inserts.
    pan.onHeld    = [&c] (bool h) { ControlHooks::hold(&c, 1, h); };
    delay.onHeld  = [&c] (bool h) { ControlHooks::hold(&c, 2, h); };
    reverb.onHeld = [&c] (bool h) { ControlHooks::hold(&c, 3, h); };

    pan.setColours(juce::Colour(0xff1a1d22), colours::light, colours::light);
    pan.setValueText([] (double v)
    {
        const int p = juce::roundToInt(v * 100.0);
        return p == 0 ? juce::String("C") : (p < 0 ? "L" + juce::String(-p) : "R" + juce::String(p));
    });
    addAndMakeVisible(pan);

    delay.setColours(juce::Colour(0xff1a1d22), colours::stepOrange, colours::light);
    delay.setValueText([] (double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
    delay.setTooltip("How much of this channel goes to the DELAY, after its fader.");
    addAndMakeVisible(delay);

    reverb.setColours(juce::Colour(0xff1a1d22), colours::stepYellow, colours::light);
    reverb.setValueText([] (double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
    reverb.setTooltip("How much of this channel goes to the REVERB, after its fader.");
    addAndMakeVisible(reverb);

    // the inserts, in the order the sound goes through them
    struct Insert { Pad& pad; std::atomic<bool>& on; const char* tip; int param; };
    for (auto& in : { Insert { dist, channel.dist, "Through the DIST unit.", 5 },
                      Insert { pcf,  channel.pcf,  "Through the PCF: the filter the PCF pattern plays.", 6 },
                      Insert { comp, channel.comp, "Through the COMP unit.", 7 } })
    {
        auto* on = &in.on;
        auto* pad = &in.pad;
        auto* ch = &channel;
        const int param = in.param;
        pad->setTooltip(in.tip);
        pad->setLedColour(colours::ledGreen);
        pad->onPress = [on, pad, ch, param]
        {
            *on = ! on->load();
            pad->setLit(on->load());
            ControlHooks::move(ch, param, on->load() ? 1.0f : 0.0f);
        };
        addAndMakeVisible(*pad);
    }

    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange(0.0, 1.0, 0.0);
    fader.setScrollWheelEnabled(false);
    fader.setDoubleClickReturnValue(true, 0.7);
    fader.setPopupDisplayEnabled(true, true, nullptr);
    fader.textFromValueFunction = [] (double v)
    {
        return v <= 0.0 ? juce::String("-inf dB") : juce::String(juce::Decibels::gainToDecibels(v * v), 1) + " dB";
    };
    fader.setColour(juce::Slider::rotarySliderOutlineColourId, colours::light.withAlpha(0.5f));
    fader.setColour(juce::Slider::thumbColourId, colours::light);
    fader.onValueChange = [this] { channel.level = (float) fader.getValue(); ControlHooks::move(&channel, 0, channel.level.load()); };
    fader.onDragStart = [this] { ControlHooks::hold(&channel, 0, true); };
    fader.onDragEnd = [this] { ControlHooks::hold(&channel, 0, false); };
    addAndMakeVisible(fader);
    meter.setTooltip("What this channel puts into the output, left and right - after its fader, pan and "
                     "the MASTER. Red: this machine alone would take the output over full scale.");
    addAndMakeVisible(meter);

    mute.setLedColour(colours::stepYellow);
    mute.onPress = [this]
    {
        channel.mute = ! channel.mute.load();
        mute.setLit(channel.mute.load());
        ControlHooks::move(&channel, 4, channel.mute.load() ? 1.0f : 0.0f);
    };
    addAndMakeVisible(mute);

    refresh();
}

void MixerStrip::refresh()
{
    pan.refresh();
    delay.refresh();
    reverb.refresh();
    fader.setValue(channel.level.load(), juce::dontSendNotification);
    mute.setLit(channel.mute.load());
    dist.setLit(channel.dist.load());
    pcf.setLit(channel.pcf.load());
    comp.setLit(channel.comp.load());
}

void MixerStrip::setCompact(bool c)
{
    if (c == compact) return;
    compact = c;

    // too narrow for the words and an LED: a letter each, and the face glows when on
    const std::pair<Pad*, const char*> pads[] = { { &dist, "DIST" }, { &pcf, "PCF" }, { &comp, "COMP" }, { &mute, "MUTE" } };
    for (auto [pad, word] : pads)
    {
        pad->setText(c ? juce::String::charToString(word[0]) : juce::String(word));
        pad->setShowsLed(! c);
        pad->setLitFace(c);
    }
    fader.setSliderStyle(c ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical);
    meter.setVertical(! c);
    delay.setLegend(c ? "DLY" : "DELAY");    // three knobs to a row, folded
    reverb.setLegend(c ? "REV" : "REVERB");

    resized();
    repaint();
}

void MixerStrip::resized()
{
    auto r = getLocalBounds().reduced(6);
    r.removeFromTop(18);

    if (compact)
    {
        auto knobs = r.removeFromTop(40);
        const int third = knobs.getWidth() / 3;
        pan.setBounds(knobs.removeFromLeft(third));
        delay.setBounds(knobs.removeFromLeft(third));
        reverb.setBounds(knobs);

        r.removeFromTop(3);
        auto keys = r.removeFromTop(16);
        const int w = keys.getWidth() / 4;
        for (auto* p : { &dist, &pcf, &comp })
            p->setBounds(keys.removeFromLeft(w));
        mute.setBounds(keys);

        // under the keys, wherever the strip ends - beside a tall folded machine, a fader
        // floating in the middle of nothing reads as something else - and the meter under it
        r.removeFromTop(3);
        fader.setBounds(r.removeFromTop(16).reduced(4, 0));
        r.removeFromTop(3);
        meter.setBounds(r.removeFromTop(5).reduced(4, 0));
        return;
    }

    auto knobs = r.removeFromTop(46);
    pan.setBounds(knobs.removeFromLeft(knobs.getWidth() / 2));
    delay.setBounds(knobs);

    // the second send under the first, on a row of its own
    knobs = r.removeFromTop(44);
    reverb.setBounds(knobs.withSizeKeepingCentre(knobs.getWidth() / 2, knobs.getHeight()));

    r.removeFromTop(4);
    for (auto* p : { &dist, &pcf, &comp })
    {
        p->setBounds(r.removeFromTop(18));
        r.removeFromTop(2);
    }

    mute.setBounds(r.removeFromBottom(24));
    r.removeFromBottom(4);

    // the meter down the right, the fader in what is left: its marks reach 15 either side
    meter.setBounds(r.removeFromRight(8));
    r.removeFromRight(2);
    fader.setBounds(r.withSizeKeepingCentre(36, r.getHeight()));
}

void MixerStrip::tick()
{
    meter.push(channel.peakL.exchange(0.0f), channel.peakR.exchange(0.0f));
}

void MixerStrip::paint(juce::Graphics& g)
{
    drawFace(g, getLocalBounds().toFloat(), colours::mixer);

    auto top = getLocalBounds().reduced(6).removeFromTop(16);
    g.setColour(colours::mixerEdge);
    g.fillRoundedRectangle(top.toFloat(), 2.0f);
    g.setColour(colours::light);
    g.setFont(fonts::bold(10.0f));
    g.drawText(name, top, juce::Justification::centred);
}

//============================================================================== Theme

void Theme::drawFace(juce::Graphics& g, juce::Rectangle<int> area) const
{
    ::drawFace(g, area.toFloat(), face);
    if (! brushed) return;

    juce::Random grain(name.hashCode());
    for (int y = area.getY() + 2; y < area.getBottom() - 2; y += 2)
    {
        g.setColour(juce::Colours::white.withAlpha(0.02f + 0.04f * grain.nextFloat()));
        g.fillRect(area.getX() + 2, y, area.getWidth() - 4, 1);
    }
}

void Theme::drawTitle(juce::Graphics& g, juce::Rectangle<int> area) const
{
    g.setColour(title);
    g.setFont(fonts::bold(24.0f));
    const int w = juce::roundToInt(juce::GlyphArrangement::getStringWidth(fonts::bold(24.0f), name)) + 10;
    g.drawText(name, area.removeFromLeft(w), juce::Justification::centredLeft);
    g.setColour(inkDim);
    g.setFont(fonts::bold(10.5f));
    g.drawText(subtitle, area, juce::Justification::centredLeft);
}

void refreshControl(juce::Component& c)
{
    if (auto* k = dynamic_cast<Knob*>(&c)) k->refresh();
    if (auto* s = dynamic_cast<Switch*>(&c)) s->refresh();
}
