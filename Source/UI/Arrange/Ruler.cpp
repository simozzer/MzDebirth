// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Ruler.h"

#include <cmath>

using namespace debirth;

namespace arrange
{

void drawGrid(juce::Graphics& g, const State& state, juce::Rectangle<int> area, bool beats)
{
    const double barPx = ticksPerBar * state.pxPerTick;
    const bool showBeats = beats && barPx >= 48.0;
    const int every = barPx >= 24.0 ? 1 : barPx >= 8.0 ? 4 : 16;   // bars between lines when zoomed out

    const int first = juce::jmax(0, (int) std::floor(state.scrollTick / ticksPerBar));
    for (int bar = first; ; ++bar)
    {
        const float x = (float) area.getX() + (float) state.xOf((double) bar * ticksPerBar);
        if (x > (float) area.getRight()) break;

        if (bar % every == 0)
        {
            g.setColour(juce::Colours::black.withAlpha(bar % (every * 4) == 0 ? 0.55f : 0.35f));
            g.drawVerticalLine(juce::roundToInt(x), (float) area.getY(), (float) area.getBottom());
        }

        if (showBeats)
        {
            g.setColour(juce::Colours::black.withAlpha(0.16f));
            for (int b = 1; b < 4; ++b)
            {
                const float bx = x + (float) (b * ticksPerQuarter * state.pxPerTick);
                g.drawVerticalLine(juce::roundToInt(bx), (float) area.getY(), (float) area.getBottom());
            }
        }
    }
}

Ruler::Ruler(State& s) : state(s)
{
    setTooltip("Click to move the cursor (and the song, while it plays). Drag the L and R flags to "
               "move the locators, or shift-drag to set both. Double-click to turn the cycle on or off.");
}

void Ruler::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds();
    g.setColour(juce::Colour(0xff2a2e36));
    g.fillRect(r);

    const auto& song = state.song();

    // the cycle, between the locators
    const float lx = (float) state.xOf(song.left), rx = (float) state.xOf(song.right);
    g.setColour(song.cycle ? colours::ledGreen.withAlpha(0.35f) : colours::light.withAlpha(0.12f));
    g.fillRect(juce::Rectangle<float>(lx, 0.0f, rx - lx, (float) r.getHeight()).getIntersection(r.toFloat()));

    drawGrid(g, state, r.withTrimmedTop(r.getHeight() / 2), true);

    // the bar numbers
    const double barPx = ticksPerBar * state.pxPerTick;
    const int every = barPx >= 30.0 ? 1 : barPx >= 12.0 ? 4 : 16;
    g.setFont(fonts::bold(10.0f));
    g.setColour(colours::light.withAlpha(0.85f));
    for (int bar = juce::jmax(0, (int) std::floor(state.scrollTick / ticksPerBar)); ; ++bar)
    {
        const int x = juce::roundToInt(state.xOf((double) bar * ticksPerBar));
        if (x > r.getRight()) break;
        if (bar % every == 0) g.drawText(juce::String(bar + 1), x + 3, 1, 40, 13, juce::Justification::centredLeft);
    }

    // the locators' flags
    auto flag = [&] (float x, const char* label)
    {
        juce::Path p;
        p.addTriangle(x - 6.0f, 0.0f, x + 6.0f, 0.0f, x, 9.0f);
        g.setColour(song.cycle ? colours::ledGreen : colours::light);
        g.fillPath(p);
        g.setFont(fonts::bold(9.0f));
        g.drawText(label, juce::roundToInt(x) - 8, 10, 16, 12, juce::Justification::centred);
    };
    flag(lx, "L");
    flag(rx, "R");

    // where the song is
    const float px = (float) state.xOf(state.now());
    g.setColour(state.songPlaying() ? colours::ledRed : colours::light);
    juce::Path marker;
    marker.addTriangle(px - 5.0f, (float) r.getBottom() - 8.0f, px + 5.0f, (float) r.getBottom() - 8.0f, px, (float) r.getBottom());
    g.fillPath(marker);

    g.setColour(juce::Colours::black);
    g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float) r.getWidth());
}

void Ruler::moveCursor(double tick)
{
    const int t = state.snapped(tick);
    state.doc().adjust([t] (Song& s) { s.cursor = t; });
    if (state.songPlaying()) state.processor.rack.locate(t);
    state.repaintAll();
}

void Ruler::mouseDown(const juce::MouseEvent& e)
{
    const auto& song = state.song();
    before = song;
    const double tick = state.tickAt(e.position.x);

    if (e.mods.isShiftDown())
    {
        drag = Drag::Range;
        rangeFrom = state.snapped(tick);
        return;
    }

    const float lx = (float) state.xOf(song.left), rx = (float) state.xOf(song.right);
    if (e.position.y < 22.0f && std::abs(e.position.x - rx) <= 7.0f) { drag = Drag::Right; return; }
    if (e.position.y < 22.0f && std::abs(e.position.x - lx) <= 7.0f) { drag = Drag::Left; return; }

    drag = Drag::Cursor;
    moveCursor(tick);
}

void Ruler::mouseDrag(const juce::MouseEvent& e)
{
    const int t = state.snapped(state.tickAt(e.position.x));

    switch (drag)
    {
        case Drag::Left:   state.doc().adjust([t] (Song& s) { s.left = juce::jmin(t, s.right - 1); }); break;
        case Drag::Right:  state.doc().adjust([t] (Song& s) { s.right = juce::jmax(t, s.left + 1); }); break;
        case Drag::Range:
        {
            const int a = juce::jmin(rangeFrom, t), b = juce::jmax(rangeFrom, t);
            if (b > a) state.doc().adjust([a, b] (Song& s) { s.left = a; s.right = b; });
            break;
        }
        case Drag::Cursor: moveCursor(state.tickAt(e.position.x)); return;
        default: return;
    }
    state.repaintAll();
}

void Ruler::mouseUp(const juce::MouseEvent&)
{
    // the locators' move is one undo
    if (drag == Drag::Left || drag == Drag::Right || drag == Drag::Range)
    {
        const Song now = state.song();
        if (now.left != before.left || now.right != before.right)
        {
            state.doc().adjust([this] (Song& s) { s = before; });
            state.doc().edit([now] (Song& s) { s.left = now.left; s.right = now.right; });
        }
    }
    drag = Drag::None;
}

void Ruler::mouseDoubleClick(const juce::MouseEvent&)
{
    state.doc().edit([] (Song& s) { s.cycle = ! s.cycle; });
    state.repaintAll();
}

} // namespace arrange
