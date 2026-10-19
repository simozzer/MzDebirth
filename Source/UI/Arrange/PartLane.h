// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "ArrangeState.h"

namespace arrange
{

//
// The parts, along each track's lane, played with Cubase's tools:
//
//   ARROW     click to pick (shift for more), drag to move - to another track of the same
//             kind, too - Alt-drag to copy, drag the right edge to resize, drag across empty
//             lane to pick by a box; double-click to edit the part
//   PENCIL    drag out a new, empty part - or click for a bar's
//   ERASER    click a part away
//   SCISSORS  cut a part in two where clicked
//   GLUE      join a part to the next on its track
//   MUTE      silence a part, or let it sound again
//
// Right-click for the toolbox, as on the ST, with what can be done to the part or lane
// clicked: insert a pattern from the rack as a part, repeat, rename, delete.
//
class PartLane final : public juce::Component
{
public:
    explicit PartLane(State&);

    std::function<void(PartRef)> onOpenPart;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // the edits the keyboard makes, too
    void deleteSelection();
    void copySelection();
    void paste();
    void repeatSelection(int times);

private:
    enum class Drag { None, Move, Copy, Resize, Draw, Box };

    int rowAt(float y) const;
    void paintAutomation(juce::Graphics&, int row, int owner, const std::vector<debirth::AutoLane>&) const;
    float rowY(int row) const { return (float) (row * trackHeight - state.scrollY); }
    juce::Rectangle<float> partBounds(int track, const debirth::Part&) const;
    bool partAt(juce::Point<float>, PartRef&) const;
    bool onRightEdge(juce::Point<float>, PartRef) const;

    void paintPart(juce::Graphics&, juce::Rectangle<float>, const debirth::Part&, juce::Colour, bool selected, int machine) const;
    void toolbox(const juce::MouseEvent&);
    void insertPattern(int track, int pattern, int at);

    // for a drag: how far, in ticks and tracks, the parts picked have moved
    int dragTicks(const juce::MouseEvent&) const;
    int dragTracks(const juce::MouseEvent&) const;
    int targetTrack(int from, int by) const;

    State& state;
    Drag drag = Drag::None;
    PartRef anchor { -1, -1 };
    juce::Point<float> downAt;
    int drawTrack = -1, drawFrom = 0, drawTo = 0;
    int resizeTo = 0;
    juce::Rectangle<float> box;

    // what COPY took, as parts with the track they came from
    static std::vector<std::pair<int, debirth::Part>> clipboard;
};

} // namespace arrange
