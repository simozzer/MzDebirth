// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "ArrangeState.h"

namespace arrange
{

//
// The bars along the top of the lanes, with the left and right locators and the cycle
// between them. Click to move the cursor - and, while the song plays, the song with it; drag
// a locator's flag to move it; shift-drag to set both; double-click to turn the cycle on or
// off.
//
class Ruler final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Ruler(State&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    enum class Drag { None, Left, Right, Range, Cursor };

    void moveCursor(double tick);

    State& state;
    Drag drag = Drag::None;
    int rangeFrom = 0;
    debirth::Song before;   // the song as the drag found it, for its one undo
};

// The bar lines and beat lines, drawn under the lanes and in the ruler alike.
void drawGrid(juce::Graphics&, const State&, juce::Rectangle<int> area, bool beats);

} // namespace arrange
