// MzDebirth - Copyright (C) 2026 Simon Moscrop
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
    pressed = false;
    repaint();
}

void Pad::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced(1.0f);
    if (pressed) r = r.translated(0.0f, 1.0f);

    g.setGradientFill(juce::ColourGradient(face.brighter(pressed ? 0.0f : 0.15f), r.getX(), r.getY(),
                                           face.darker(0.3f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 3.0f);
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

    if (text.isNotEmpty())
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

void LedNumber::mouseDown(const juce::MouseEvent&)            { dragFrom = get(); }
void LedNumber::mouseDrag(const juce::MouseEvent& e)          { put(dragFrom - e.getDistanceFromDragStartY() / 4); }
void LedNumber::mouseDoubleClick(const juce::MouseEvent&)     { put(initial); }

void LedNumber::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (! wheelEnabled) { Component::mouseWheelMove(e, w); return; }

    wheel += w.deltaY * 8.0f;
    const int whole = (int) wheel;
    if (whole != 0) { wheel -= (float) whole; put(get() + whole); }
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
    g.setColour(colours::ledOff.withAlpha(0.4f));   // the unlit segments, there but not read
    g.drawText(juce::String::repeatedString("8", digits), r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
    g.setColour(colours::ledRed);
    g.drawText(juce::String(get()), r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
}

//============================================================================== PatternPanel

PatternPanel::PatternPanel(debirth::TrackBase& t, std::function<bool()> p)
    : track(t), playing(std::move(p))
{
    using namespace debirth;

    for (int i = 0; i < patternsPerBank; ++i)
    {
        numbers[i].setText(juce::String(i + 1));
        numbers[i].onPress = [this, i] { select(track.selected.load() / patternsPerBank * patternsPerBank + i); };
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

    copy.setTooltip("Copy this pattern, to paste over another.");
    copy.onPress = [this] { clipboard = track.selected.load(); };
    paste.setTooltip("Paste the copied pattern over this one.");
    paste.onPress = [this] { if (clipboard >= 0) track.copy(clipboard, track.selected.load()); };
    clear.setTooltip("Clear this pattern.");
    clear.onPress = [this] { track.clear(track.selected.load()); };
    for (auto* b : { &copy, &paste, &clear })
    {
        b->setShowsLed(false);
        addAndMakeVisible(*b);
    }

    steps = std::make_unique<LedNumber>(1, stepsPerPattern, stepsPerPattern, 2,
        [this] { return track.pattern(track.selected.load()).len(); },
        [this] (int v) { track.pattern(track.selected.load()).length = v; });
    steps->setTooltip("How many steps this pattern has. Drag up or down; double-click for 16.");
    steps->setWheelEnabled(false);
    addAndMakeVisible(*steps);
}

void PatternPanel::select(int index)
{
    track.selected = juce::jlimit(0, debirth::patternsPerTrack - 1, index);
    if (! playing()) track.current = track.selected.load();
}

void PatternPanel::tick(bool blinkPhase)
{
    using namespace debirth;

    const int sel = track.selected.load(), cur = track.current.load();
    const int bank = sel / patternsPerBank;
    const bool waiting = playing() && cur != sel;

    for (int i = 0; i < patternsPerBank; ++i)
    {
        const int index = bank * patternsPerBank + i;
        numbers[i].setLit(index == sel && (! waiting || blinkPhase));
        numbers[i].setMarked(waiting && index == cur);
    }
    for (int b = 0; b < banks; ++b)
        bankPads[b].setLit(b == bank);

    shuffle.setLit(track.shuffle.load());
    paste.setEnabled(clipboard >= 0);
    paste.setAlpha(clipboard >= 0 ? 1.0f : 0.5f);
    steps->repaint();
}

void PatternPanel::resized()
{
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

//============================================================================== MixerStrip

MixerStrip::MixerStrip(debirth::Channel& c, const juce::String& n)
    : channel(c), name(n),
      pan("PAN", -1.0f, 1.0f, 0.0f, [&c] { return c.pan.load(); }, [&c] (float v) { c.pan = v; }),
      delay("DELAY", 0.0f, 1.0f, 0.0f, [&c] { return c.delay.load(); }, [&c] (float v) { c.delay = v; })
{
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

    // the inserts, in the order the sound goes through them
    struct Insert { Pad& pad; std::atomic<bool>& on; const char* tip; };
    for (auto& in : { Insert { dist, channel.dist, "Through the DIST unit." },
                      Insert { pcf,  channel.pcf,  "Through the PCF: the filter the PCF pattern plays." },
                      Insert { comp, channel.comp, "Through the COMP unit." } })
    {
        auto* on = &in.on;
        auto* pad = &in.pad;
        pad->setTooltip(in.tip);
        pad->setLedColour(colours::ledGreen);
        pad->onPress = [on, pad] { *on = ! on->load(); pad->setLit(on->load()); };
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
    fader.onValueChange = [this] { channel.level = (float) fader.getValue(); };
    addAndMakeVisible(fader);

    mute.setLedColour(colours::stepYellow);
    mute.onPress = [this] { channel.mute = ! channel.mute.load(); mute.setLit(channel.mute.load()); };
    addAndMakeVisible(mute);

    refresh();
}

void MixerStrip::refresh()
{
    pan.refresh();
    delay.refresh();
    fader.setValue(channel.level.load(), juce::dontSendNotification);
    mute.setLit(channel.mute.load());
    dist.setLit(channel.dist.load());
    pcf.setLit(channel.pcf.load());
    comp.setLit(channel.comp.load());
}

void MixerStrip::resized()
{
    auto r = getLocalBounds().reduced(6);
    r.removeFromTop(18);

    auto knobs = r.removeFromTop(46);
    pan.setBounds(knobs.removeFromLeft(knobs.getWidth() / 2));
    delay.setBounds(knobs);

    r.removeFromTop(4);
    for (auto* p : { &dist, &pcf, &comp })
    {
        p->setBounds(r.removeFromTop(18));
        r.removeFromTop(2);
    }

    mute.setBounds(r.removeFromBottom(24));
    r.removeFromBottom(4);
    fader.setBounds(r.withSizeKeepingCentre(36, r.getHeight()));
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
