// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "ArrangeState.h"

#include <cmath>

namespace arrange
{

const char* toolName(int tool)
{
    static const char* const names[Tool::count] = { "ARROW", "PENCIL", "ERASER", "SCISSORS", "GLUE", "MUTE" };
    return names[juce::jlimit(0, Tool::count - 1, tool)];
}

const char* snapName(int snap)
{
    static const char* const names[Snap::count] = { "BAR", "BEAT", "1/16", "OFF" };
    return names[juce::jlimit(0, Snap::count - 1, snap)];
}

juce::Colour trackColour(int index)
{
    // the 303's orange, the 101's amber, the 808's red, the 909's yellow, the 950's teal,
    // then green, blue and violet
    static const juce::Colour colours[trackColours] = {
        juce::Colour(0xffef7d22), juce::Colour(0xffe0a73a), juce::Colour(0xffd8321f), juce::Colour(0xffe8c22a),
        juce::Colour(0xff2fa6a0), juce::Colour(0xff5fae47), juce::Colour(0xff4a7fd0), juce::Colour(0xff9a62c9),
    };
    return colours[((index % trackColours) + trackColours) % trackColours];
}

juce::String position(double tick)
{
    const int t = juce::jmax(0, (int) std::floor(tick));
    const int bar = t / ticksPerBar + 1;
    const int beat = t % ticksPerBar / ticksPerQuarter + 1;
    const int rest = t % ticksPerQuarter;
    return juce::String(bar).paddedLeft('0', 4) + "." + juce::String(beat).paddedLeft('0', 2) + "."
         + juce::String(rest).paddedLeft('0', 3);
}

int State::grid() const
{
    switch (snap)
    {
        case Snap::Bar:  return ticksPerBar;
        case Snap::Beat: return ticksPerQuarter;
        case Snap::Step: return ticksPerStep;
        default:         return 1;
    }
}

int State::snapped(double tick) const
{
    const int g = grid();
    return juce::jmax(0, (int) std::lround(tick / g) * g);
}

int State::snappedDown(double tick) const
{
    const int g = grid();
    return juce::jmax(0, (int) std::floor(tick / g) * g);
}

double State::now() const
{
    const double playing = processor.rack.songPosition();
    return playing >= 0.0 ? playing : (double) song().cursor;
}

//============================================================================== LedText

void LedText::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::display);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);
    // as large as fits: a monospaced digit is about 0.6 of its height wide
    const float fits = (r.getWidth() - 12.0f) / (0.62f * (float) juce::jmax(1, text.length()));
    g.setFont(fonts::digits(juce::jmin(r.getHeight() * 0.62f, fits)));
    g.setColour(colours::ledRed);
    g.drawText(text, r.reduced(6.0f, 0.0f), juce::Justification::centred, false);
}

void LedText::mouseDrag(const juce::MouseEvent& e)
{
    if (! onDrag) return;
    const int steps = -e.getDistanceFromDragStartY() / 6;
    if (steps != dragged)
    {
        onDrag(steps - dragged);
        dragged = steps;
    }
}

} // namespace arrange
