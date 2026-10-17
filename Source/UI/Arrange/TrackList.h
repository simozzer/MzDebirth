// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "ArrangeState.h"

#include <array>

namespace arrange
{

//
// The tracks down the left of the Arrange window, as Cubase listed them: a colour, an
// activity light, the name, the machine it plays, and its M (mute), S (solo) and SH
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

    // the track menu, from here or the Arrange window's ADD TRACK
    void addTrack();
    void rename(int track);

private:
    enum class Hit { None, Colour, Name, Machine, Mute, Solo, Shuffle };

    int rowAt(int y) const;
    juce::Rectangle<int> rowBounds(int row) const;
    juce::Rectangle<int> switchBounds(int row, int which) const;   // 0 M, 1 S, 2 SH
    Hit hitAt(juce::Point<int>, int& row) const;
    void menu(int row);

    State& state;
    std::array<bool, debirth::Rack::channels> active {};
};

} // namespace arrange
