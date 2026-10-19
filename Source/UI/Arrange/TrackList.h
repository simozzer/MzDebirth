// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "ArrangeState.h"

#include <array>

namespace arrange
{

//
// The tracks down the left of the Arrange window, as Cubase listed them - one for each
// machine in the rack, in its order: a colour, an activity light, the name, the machine it
// plays, RACK to go and turn that machine's knobs, and its M (mute), S (solo) and SH
// (shuffle) switches. Click to pick a track; double-click the name to rename it; right-click
// for the rest.
//
class TrackList final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TrackList(State&);

    // which machines are sounding, for the activity lights
    void setActivity(const std::array<bool, debirth::Rack::channels>&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void rename(int track);

private:
    enum class Hit { None, Colour, Name, Machine, Mute, Solo, Shuffle, Rack, Fx, Shown };

    int rowAt(int y) const;
    juce::Rectangle<int> rowBounds(int row) const;
    juce::Rectangle<int> switchBounds(int row, int which) const;   // 0 M, 1 S, 2 SH, 3 RACK
    Hit hitAt(juce::Point<int>, int& row) const;
    void menu(int row);
    void fxMenu();
    void paintBadge(juce::Graphics&, juce::Rectangle<int> row) const;
    void paintSelector(juce::Graphics&, int row, int owner, const std::vector<debirth::AutoLane>&) const;
    void automationMenu(int row);
    juce::String controlName(int owner, const std::string& param) const;
    juce::Rectangle<int> nameBounds(juce::Rectangle<int> row) const;
    juce::Rectangle<int> selectorBounds(int row) const;

    State& state;
    std::array<bool, debirth::Rack::channels> active {};
};

} // namespace arrange
