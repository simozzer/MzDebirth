// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Processor.h"
#include "UI/Widgets.h"

#include <functional>
#include <set>
#include <utility>

//
// What the pieces of the Arrange window share: the song, how it is laid out on screen - its
// zoom and scroll - and what is picked: the tool, the snap, the track and the parts.
//
namespace arrange
{

using debirth::ticksPerBar;
using debirth::ticksPerQuarter;
using debirth::ticksPerStep;

namespace Tool { enum { Arrow, Pencil, Eraser, Scissors, Glue, Mute, count }; }
namespace Snap { enum { Bar, Beat, Step, Off, count }; }

const char* toolName(int tool);
const char* snapName(int snap);

constexpr int rulerHeight = 26;
constexpr int trackHeight = 46;

// The track colours: the machines' own first, then more to choose from.
juce::Colour trackColour(int index);
constexpr int trackColours = 8;

// A position as Cubase showed one: bar.beat.tick, from 0001.01.000.
juce::String position(double tick);

// A part on a track, by index - good until the next edit.
using PartRef = std::pair<int, int>;

struct State
{
    explicit State(DebirthProcessor& p) : processor(p) {}

    DebirthProcessor& processor;
    debirth::SongDocument& doc() { return processor.song; }
    const debirth::Song& song() const { return processor.song.song(); }

    // the time axis: pixels a tick, and the tick at the left edge of the lanes
    double pxPerTick = 64.0 / ticksPerBar;
    double scrollTick = 0.0;
    int scrollY = 0;   // pixels down the tracks

    int tool = Tool::Arrow;
    int snap = Snap::Bar;
    int track = 0;                 // the track picked
    std::set<PartRef> selection;   // the parts picked
    bool follow = true;            // keep the playing position in view

    double xOf(double tick) const  { return (tick - scrollTick) * pxPerTick; }
    double tickAt(double x) const  { return x / pxPerTick + scrollTick; }

    int grid() const;                       // the snap, in ticks; 1 when off
    int snapped(double tick) const;         // to the nearest grid line
    int snappedDown(double tick) const;     // to the one at or before

    // where the song is: playing in SONG mode, the position; otherwise the cursor
    double now() const;
    bool songPlaying() const { return processor.rack.songPosition() >= 0.0; }

    // Everything listening hears of a change: the song, or the view of it.
    std::function<void()> changed;
    void repaintAll() { if (changed) changed(); }

    // Over to the rack, at a machine, to turn its knobs - track n's is machine n.
    std::function<void(int machine)> showMachine;
};

//
// A small LED readout of text - the position, the locators - in the rack's red.
//
class LedText final : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setText(const juce::String& t) { if (t != text) { text = t; repaint(); } }
    void paint(juce::Graphics&) override;

    std::function<void(int)> onDrag;   // up and down, in steps of the mouse
    std::function<void()> onDoubleClick;

    void mouseDown(const juce::MouseEvent&) override    { dragged = 0; }
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }

private:
    juce::String text;
    int dragged = 0;
};

} // namespace arrange
