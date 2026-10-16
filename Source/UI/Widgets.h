// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Look.h"
#include "Core/Patterns.h"
#include "Core/Rack.h"

#include <functional>
#include <memory>
#include <vector>

//
// A rubber pad with an LED, which acts as it is pressed rather than let go, as the
// hardware's do.
//
//   lit      the LED is on
//   marked   a bright edge round it - the step playing, an instrument with hits written
//
class Pad final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Pad(juce::String text = {});

    std::function<void()> onPress;

    void setText(const juce::String&);
    void setLit(bool);
    void setMarked(bool);
    void setFace(juce::Colour face, juce::Colour text);
    void setLedColour(juce::Colour c) { led = c; repaint(); }
    void setShowsLed(bool b)          { showsLed = b; repaint(); }

    bool isLit() const { return lit; }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::String text;
    juce::Colour face { colours::pad }, ink { colours::light }, led { colours::ledRed };
    bool lit = false, marked = false, pressed = false, showsLed = true;
};

//
// A knob with its legend above it. It talks to the rack through get and set rather than
// holding the value, so a load needs only refresh().
//
class Knob final : public juce::Component, public juce::SettableTooltipClient
{
public:
    Knob(const juce::String& legend, float lo, float hi, float initial,
         std::function<float()> get, std::function<void(float)> set);

    void setColours(juce::Colour cap, juce::Colour pointer, juce::Colour legendInk);
    void setValueText(std::function<juce::String(double)> f) { slider.textFromValueFunction = std::move(f); }
    void refresh();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String legend;
    juce::Colour legendInk { colours::ink };
    juce::Slider slider;
    std::function<float()> get;
    std::function<void(float)> set;
};

//
// A switch, as a pad that steps through its positions, named on it.
//
class Switch final : public juce::Component
{
public:
    Switch(const juce::String& legend, juce::StringArray positions,
           std::function<int()> get, std::function<void(int)> set);

    void setLegendInk(juce::Colour c) { legendInk = c; repaint(); }
    void setPadWidth(int w)           { padWidth = w; resized(); }
    void refresh();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::String legend;
    juce::StringArray positions;
    juce::Colour legendInk { colours::ink };
    Pad pad;
    int padWidth = 46;
    std::function<int()> get;
    std::function<void(int)> set;
};

//
// A red seven-segment style readout, set by dragging up and down or the mouse wheel;
// double-click for its default.
//
class LedNumber final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LedNumber(int lo, int hi, int initial, int digits, std::function<int()> get, std::function<void(int)> set);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // off where it sits in something that scrolls, so the wheel scrolls that instead
    void setWheelEnabled(bool b) { wheelEnabled = b; }

private:
    void put(int v);

    int lo, hi, initial, digits;
    std::function<int()> get;
    std::function<void(int)> set;
    int dragFrom = 0;
    float wheel = 0;
    bool wheelEnabled = true;
};

//
// A machine's pattern section: eight pads for the patterns in a bank, the four banks, how
// many steps the pattern has, the machine's SHUFFLE switch, and copy, paste and clear.
//
// A pattern picked while playing waits for the one playing to come round: its pad blinks
// until then.
//
class PatternPanel final : public juce::Component
{
public:
    PatternPanel(debirth::TrackBase&, std::function<bool()> playing);

    void tick(bool blinkPhase);   // from the panel's timer

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void select(int index);

    debirth::TrackBase& track;
    std::function<bool()> playing;

    Pad numbers[debirth::patternsPerBank];
    Pad bankPads[debirth::banks];
    Pad shuffle { "SHUFFLE" };
    Pad copy { "COPY" }, paste { "PASTE" }, clear { "CLEAR" };
    std::unique_ptr<LedNumber> steps;

    int clipboard = -1;
};

//
// A channel of the mixer, beside the machine it carries: pan, fader and mute.
//
class MixerStrip final : public juce::Component
{
public:
    MixerStrip(debirth::Channel&, const juce::String& name);

    void refresh();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    debirth::Channel& channel;
    juce::String name;
    Knob pan, delay;
    Pad dist { "DIST" }, pcf { "PCF" }, comp { "COMP" };
    juce::Slider fader;
    Pad mute { "MUTE" };
};

// A panel's bevelled face, for the racks.
void drawFace(juce::Graphics&, juce::Rectangle<float>, juce::Colour);

//
// How a machine's panel looks: its face, the ink printed on it, its name, and its knobs.
//
struct Theme
{
    juce::String name, subtitle;
    juce::Colour face, ink, inkDim, title;
    juce::Colour knobCap, knobPointer;
    bool brushed = false;                    // a faint grain across the face

    void drawTitle(juce::Graphics&, juce::Rectangle<int> area) const;
    void drawFace(juce::Graphics&, juce::Rectangle<int> area) const;
};

//
// The knob or switch for one control on a machine's panel, talking straight to its value.
//
template <class S>
std::unique_ptr<juce::Component> makeControl(debirth::Knobs<S>& knobs, int i, const Theme& theme)
{
    const auto& c = knobs.control(i);

    if (c.sw != nullptr)
    {
        auto sw = std::make_unique<Switch>(c.legend, juce::StringArray::fromTokens(c.positions, "|", ""),
                                           [&knobs, i] { return (int) knobs.get(i); },
                                           [&knobs, i] (int v) { knobs.set(i, (float) v); });
        sw->setLegendInk(theme.ink);
        return sw;
    }

    auto k = std::make_unique<Knob>(c.legend, c.lo, c.hi, knobs.defaultOf(i),
                                    [&knobs, i] { return knobs.get(i); },
                                    [&knobs, i] (float v) { knobs.set(i, v); });
    k->setColours(theme.knobCap, theme.knobPointer, theme.ink);
    if (c.lo < 0.0f)
        k->setValueText([] (double v) { const int r = juce::roundToInt(v); return (r > 0 ? "+" : "") + juce::String(r); });
    return k;
}

// Reads a knob or switch made by makeControl back from its value, after a load.
void refreshControl(juce::Component&);
