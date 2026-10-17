// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "ArrangeState.h"

namespace arrange
{

//
// The transport along the bottom of the Arrange window, as Cubase kept its: back to the
// start, play and stop, the position, the left and right locators and CYCLE, SONG mode, the
// tempo, and whether the window follows the song as it plays.
//
// The keys are the ST's numeric keypad's: Enter plays from the cursor, 0 stops, . goes back
// to the start, / turns the cycle on and off. The space bar plays and stops anywhere.
//
class ArrangeTransport final : public juce::Component
{
public:
    explicit ArrangeTransport(State&);

    void tick();
    void paint(juce::Graphics&) override;
    void resized() override;

    void toStart();
    void playFromCursor();
    void toggleCycle();

private:
    void moveLocator(bool right, int bars);

    State& state;
    Pad start { "|<" }, play { "PLAY" }, stop { "STOP" }, cycle { "CYCLE" }, songMode { "SONG" }, follow { "FOLLOW" };
    LedText pos, left, right;
    LedNumber tempo;
};

} // namespace arrange
