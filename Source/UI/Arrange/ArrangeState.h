// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Processor.h"
#include "UI/Widgets.h"

#include <array>
#include <functional>
#include <set>
#include <string>
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
constexpr int trackHeight = 72;

// The rows down the window: a track for each machine, then FX - the effects units' automation.
inline int fxRow(const debirth::Song& s) { return (int) s.tracks.size(); }
inline int rowCount(const debirth::Song& s) { return fxRow(s) + 1; }

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

    // which automation each row draws, by owner (a machine, or Rack::FX): "" every lane,
    // "-" none, or one control's id - see TrackList's selector
    std::array<std::string, debirth::Rack::owners> shownAutomation;
    bool shows(int owner, const std::string& param) const;

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

    //
    // The rows shown: a track for each machine in the rack, then FX. A row keeps its track's
    // index - fxRow() past them all - and its slot is where it is drawn, counting only the
    // rows shown, so a machine left out of the rack leaves no gap.
    //
    bool shown(int row) const
    {
        const auto& tracks = song().tracks;
        if (row == (int) tracks.size()) return true;   // FX
        if (row < 0 || row > (int) tracks.size()) return false;
        const int m = tracks[(size_t) row].machine;
        return m < 0 || m >= debirth::Rack::channels || processor.rack.channel[m].inRack.load();
    }
    // where a row comes down the window, as the rack has its machines: each machine's second
    // under its first, FX last
    int orderOf(int row) const
    {
        const auto& tracks = song().tracks;
        if (row < 0 || row >= (int) tracks.size()) return 1 << 20;   // FX
        const int m = tracks[(size_t) row].machine;
        return debirth::Rack::typeOf(m) * debirth::Rack::copies + debirth::Rack::copyOf(m);
    }
    int slotOf(int row) const
    {
        int slot = 0;
        for (int r = 0; r <= (int) song().tracks.size(); ++r)
            slot += r != row && shown(r) && orderOf(r) < orderOf(row) ? 1 : 0;
        return slot;
    }
    int rowOfSlot(int slot) const   // -1 past the last
    {
        if (slot < 0) return -1;
        for (int r = 0; r <= (int) song().tracks.size(); ++r)
            if (shown(r) && slotOf(r) == slot) return r;
        return -1;
    }
    int slots() const { return slotOf((int) song().tracks.size()) + 1; }

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
