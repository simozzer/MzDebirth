// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "ArrangeTransport.h"

#include <cmath>

using namespace debirth;

namespace arrange
{

ArrangeTransport::ArrangeTransport(State& s)
    : state(s),
      tempo((int) Clock::minTempo * 10, (int) Clock::maxTempo * 10, 1200, 4,
            [&s] { return juce::roundToInt(s.processor.rack.tempo.load() * 10.0f); },
            [&s] (int v) { s.processor.rack.tempo = (float) v / 10.0f; })
{
    tempo.setDecimals(1);
    tempo.setShown([&s]
    {
        auto& rack = s.processor.rack;
        return juce::roundToInt((rack.nudge.load() != 0 ? rack.tempoNow() : rack.tempo.load()) * 10.0f);
    });

    start.setShowsLed(false);
    start.setTooltip("Back to the start of the song (the keypad's . key).");
    start.onPress = [this] { toStart(); };

    play.setLedColour(colours::ledGreen);
    play.setTooltip("Play the song from the cursor (the keypad's Enter). The space bar plays and stops.");
    play.onPress = [this] { playFromCursor(); };

    stop.setTooltip("Stop (the keypad's 0). The cursor stays where the song stopped.");
    stop.onPress = [this]
    {
        auto& rack = state.processor.rack;
        if (state.songPlaying())
        {
            const int at = state.snappedDown(rack.songPosition());
            state.doc().adjust([at] (Song& song) { song.cursor = at; });
        }
        rack.playing = false;
        state.repaintAll();
    };

    cycle.setLedColour(colours::ledGreen);
    cycle.setTooltip("Play round between the L and R locators (the keypad's / key).");
    cycle.onPress = [this] { toggleCycle(); };

    songMode.setLedColour(colours::ledGreen);
    songMode.setTooltip("SONG: the arrangement plays the machines. Off, they play their patterns, as in the rack.");
    songMode.onPress = [this]
    {
        auto& mode = state.processor.rack.mode;
        mode = mode.load() == Mode::Song ? Mode::Pattern : Mode::Song;
    };

    follow.setLedColour(colours::ledGreen);
    follow.setTooltip("Scroll the Arrange window along with the song as it plays.");
    follow.onPress = [this] { state.follow = ! state.follow; };

    pos.setTooltip("Where the song is: bar, beat and tick, 192 ticks to the beat.");
    left.setTooltip("The left locator. Drag up or down to move it a bar; double-click to set it to the cursor.");
    right.setTooltip("The right locator. Drag up or down to move it a bar; double-click to set it to the cursor.");
    left.onDrag = [this] (int bars) { moveLocator(false, bars); };
    right.onDrag = [this] (int bars) { moveLocator(true, bars); };
    left.onDoubleClick = [this]
    {
        const int c = state.song().cursor;
        state.doc().edit([c] (Song& song) { song.left = juce::jmin(c, song.right - 1); });
        state.repaintAll();
    };
    right.onDoubleClick = [this]
    {
        const int c = state.song().cursor;
        state.doc().edit([c] (Song& song) { song.right = juce::jmax(c, song.left + 1); });
        state.repaintAll();
    };
    tempo.setTooltip("Tempo, in beats a minute: the rack's. Drag, or use the wheel - hold Shift for tenths. "
                     "The left and right arrow keys DRAG and PUSH while held.");

    for (juce::Component* c : std::initializer_list<juce::Component*> { &start, &play, &stop, &cycle, &songMode, &follow, &pos, &left, &right, &tempo })
        addAndMakeVisible(c);
}

void ArrangeTransport::toStart()
{
    state.doc().adjust([] (Song& song) { song.cursor = 0; });
    if (state.songPlaying()) state.processor.rack.locate(0);
    state.scrollTick = 0.0;
    state.repaintAll();
}

void ArrangeTransport::playFromCursor()
{
    auto& rack = state.processor.rack;
    if (! rack.playing.load()) rack.mode = Mode::Song;   // from here, PLAY plays the song
    rack.playing = true;
}

void ArrangeTransport::toggleCycle()
{
    state.doc().edit([] (Song& song) { song.cycle = ! song.cycle; });
    state.repaintAll();
}

void ArrangeTransport::moveLocator(bool isRight, int bars)
{
    state.doc().adjust([isRight, bars] (Song& song)
    {
        if (isRight) song.right = juce::jmax(song.left + ticksPerBar, song.right + bars * ticksPerBar);
        else         song.left = juce::jlimit(0, song.right - ticksPerBar, song.left + bars * ticksPerBar);
    });
    state.repaintAll();
}

void ArrangeTransport::tick()
{
    auto& rack = state.processor.rack;
    const bool playing = rack.playing.load();
    play.setLit(playing);
    stop.setLit(! playing);
    cycle.setLit(state.song().cycle);
    follow.setLit(state.follow);

    const bool wanted = rack.mode.load() == Mode::Song;
    const bool waiting = playing && wanted != (rack.playingNow() == Mode::Song);
    songMode.setLit(waiting ? (juce::Time::getMillisecondCounter() / 250) % 2 == 0 : wanted);

    pos.setText(position(state.now()));
    left.setText(position(state.song().left).substring(0, 7));
    right.setText(position(state.song().right).substring(0, 7));
    tempo.repaint();
}

void ArrangeTransport::resized()
{
    auto r = getLocalBounds().reduced(10, 7);

    start.setBounds(r.removeFromLeft(40));
    r.removeFromLeft(6);
    play.setBounds(r.removeFromLeft(64));
    r.removeFromLeft(6);
    stop.setBounds(r.removeFromLeft(64));
    r.removeFromLeft(18);

    r.removeFromLeft(34);   // "POS"
    pos.setBounds(r.removeFromLeft(140));
    r.removeFromLeft(18);
    r.removeFromLeft(14);   // "L"
    left.setBounds(r.removeFromLeft(92));
    r.removeFromLeft(10);
    r.removeFromLeft(14);   // "R"
    right.setBounds(r.removeFromLeft(92));
    r.removeFromLeft(8);
    cycle.setBounds(r.removeFromLeft(70));
    r.removeFromLeft(18);

    follow.setBounds(r.removeFromRight(78));
    r.removeFromRight(18);
    tempo.setBounds(r.removeFromRight(90));
    r.removeFromRight(52);   // "TEMPO"
    songMode.setBounds(r.removeFromRight(64));
}

void ArrangeTransport::paint(juce::Graphics& g)
{
    drawFace(g, getLocalBounds().toFloat(), colours::metal);

    g.setColour(colours::light.withAlpha(0.75f));
    g.setFont(fonts::bold(10.0f));
    auto label = [&] (const char* text, const juce::Component& c, int width)
    {
        g.drawText(text, c.getX() - width - 4, c.getY(), width, c.getHeight(), juce::Justification::centredRight);
    };
    label("POS", pos, 30);
    label("L", left, 12);
    label("R", right, 12);
    label("TEMPO", tempo, 46);
}

} // namespace arrange
