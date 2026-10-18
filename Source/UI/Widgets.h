// MzDebirth - Created by Simon Moscrop
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
    std::function<void()> onRelease;   // for a pad that works while it is held

    void setText(const juce::String&);
    void setLit(bool);
    void setMarked(bool);
    void setFace(juce::Colour face, juce::Colour text);
    void setLedColour(juce::Colour c) { led = c; repaint(); }
    void setShowsLed(bool b)          { showsLed = b; repaint(); }
    void setArrow(int dir)            { arrow = dir; repaint(); }   // a triangle for the text: -1 left, +1 right
    void setLitFace(bool b)           { litFace = b; repaint(); }   // lit, the whole face takes the LED's colour

    bool isLit() const { return lit; }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    juce::String text;
    juce::Colour face { colours::pad }, ink { colours::light }, led { colours::ledRed };
    bool lit = false, marked = false, pressed = false, showsLed = true, litFace = false;
    int arrow = 0;
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
    void setLegend(const juce::String& l) { legend = l; repaint(); }
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

    //
    // Places after the point. The value is then counted in those - tenths, for one - and a
    // drag or the wheel moves it a whole unit at a time, or with Shift held the last place.
    // `digits` counts every digit, those after the point included.
    //
    void setDecimals(int d);

    // What to show, where that is not what get() says - a value bent for the moment.
    void setShown(std::function<int()> f) { shown = std::move(f); }

private:
    void put(int v);
    int step(const juce::ModifierKeys&) const;

    int lo, hi, initial, digits;
    std::function<int()> get, shown;
    std::function<void(int)> set;
    int decimals = 0, unit = 1;
    int dragFrom = 0, dragBase = 0;
    bool dragFine = false;
    float wheel = 0;
    bool wheelEnabled = true;
};

//
// The bar over a machine's step roll, with a triangle: a click folds the roll away, giving
// its room to the other machines' knobs, and another brings it back.
//
class FoldBar final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int height = 16;

    FoldBar();

    void setOpen(bool);
    bool isOpen() const { return open; }
    void setInk(juce::Colour c) { ink = c; repaint(); }

    std::function<void()> onToggle;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    bool open = true;
    juce::Colour ink { colours::light };
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
    PatternPanel(debirth::TrackBase&, debirth::Channel&, std::function<bool()> playing);

    void tick(bool blinkPhase);   // from the panel's timer

    //
    // Packed into five rows, for a machine folded down: the pads closer, their labels gone,
    // the banks on a row, the steps and the editing on the next, and SHUFFLE and STOP last.
    //
    static constexpr int height = 212, compactHeight = 124;
    void setCompact(bool);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void select(int index);

    debirth::TrackBase& track;
    debirth::Channel& channel;   // for the machine's STOP
    std::function<bool()> playing;
    bool compact = false;

    Pad numbers[debirth::patternsPerBank];
    Pad bankPads[debirth::banks];
    Pad shuffle { "SHUFFLE" }, stop { "STOP" };
    Pad copy { "COPY" }, paste { "PASTE" }, clear { "CLEAR" };
    std::unique_ptr<LedNumber> steps;

    int clipboard = -1;
};

//
// A channel's level, as two bars, left and right: -48 dB to +6, green to -12, yellow to 0
// and red over it. It falls 24 dB a second, as a peak meter does, and the highest it reached
// is held a second as a line.
//
class LevelMeter final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LevelMeter();

    void setVertical(bool v) { vertical = v; repaint(); }

    // the loudest samples since the last push - 30 a second, from the panel's timer
    void push(float peakL, float peakR);

    void paint(juce::Graphics&) override;

private:
    struct Bar { float level = -100.0f, held = -100.0f; int holdFor = 0; };
    Bar bars[2];
    bool vertical = true;
};

//
// A channel of the mixer, beside the machine it carries: pan, fader and mute.
//
class MixerStrip final : public juce::Component
{
public:
    MixerStrip(debirth::Channel&, const juce::String& name);

    void refresh();
    void tick();   // the meter, from the panel's timer

    //
    // Folded down beside a folded machine: PAN and DELAY side by side, the inserts and MUTE
    // as a row of lettered keys that glow when on, and the fader and meter on their sides.
    //
    static constexpr int compactHeight = 116;
    void setCompact(bool);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    debirth::Channel& channel;
    juce::String name;
    bool compact = false;
    Knob pan, delay, reverb;
    Pad dist { "DIST" }, pcf { "PCF" }, comp { "COMP" };
    juce::Slider fader;
    LevelMeter meter;
    Pad mute { "MUTE" };
};

// The least a machine stands folded: its pattern section and mixer strip, both compact.
constexpr int foldedMinimum = PatternPanel::compactHeight > MixerStrip::compactHeight
                            ? PatternPanel::compactHeight : MixerStrip::compactHeight;

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
