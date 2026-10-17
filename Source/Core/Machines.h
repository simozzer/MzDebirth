// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Knobs.h"
#include "Patterns.h"

#include "SH101/Engine.h"
#include "TB303/Engine.h"
#include "TR808/Engine.h"
#include "TR909/Engine.h"

#include <cstdint>

namespace debirth
{

//============================================================================== the mono synths

//
// What a mono line needs of a synth, for each one: strike, let go, render. The 303 takes an
// accent as a velocity; the 101 has no velocity at all, and keeps its own time for its
// synced modulator, so it is told where the rack's clock is.
//
struct Tb303Voice
{
    using Settings = tb303::Settings;
    tb303::Engine engine;

    void prepare(double rate)               { engine.prepare(rate); }
    void setSettings(const Settings& s)     { engine.setSettings(s); }
    void setClock(bool, double, double)     {}
    void noteOn(int note, bool accent)      { engine.noteOn(note, accent ? 127 : 90); }
    void noteOff(int note)                  { engine.noteOff(note); }
    void allNotesOff()                      { engine.allNotesOff(); }
    void render(float* out, int n)          { engine.render(out, n); }
};

struct Sh101Voice
{
    using Settings = sh101::Settings;
    sh101::Engine engine;

    void prepare(double rate)               { engine.prepare(rate); }
    void setSettings(const Settings& s)     { engine.setSettings(s); }
    void setClock(bool playing, double quarters, double bpm) { engine.setHostClock(true, playing, quarters, bpm); }
    void noteOn(int note, bool)             { engine.noteOn(note); }
    void noteOff(int note)                  { engine.noteOff(note); }
    void allNotesOff()                      { engine.allNotesOff(); }
    void render(float* out, int n)          { engine.render(out, n); }
};

//
// A mono synth played from a pattern. A step's note is struck as it begins and let go half
// way through; a slide holds it over instead, and the next note is struck before it is let
// go - which the 303 takes for a slide, and the 101 for a legato, gliding when its
// PORTAMENTO is on AUTO.
//
template <class Voice>
class MonoLine
{
public:
    using Settings = typename Voice::Settings;

    MonoLine(const std::vector<Control<Settings>>& controls, int root)
        : knobs(controls), rootNote(root) {}

    Knobs<Settings> knobs;
    Track<BassPattern> track;
    const int rootNote;                      // a step's note 0 at octave 0

    void prepare(double rate) { voice.prepare(rate); }

    void update(bool playing, double quarters, double bpm)
    {
        voice.setSettings(knobs.read());
        voice.setClock(playing, quarters, bpm);
    }

    void render(float* out, int n) { voice.render(out, n); }

    void start() { track.rewind(); release(); }
    void stop()  { release(); voice.allNotesOff(); track.position = -1; }

    void step()
    {
        int i = 0;
        const BassPattern& p = track.advance(i);
        const BassStep s = p.get(i);

        // A rest - and a note slid into a rest ends here.
        if (! s.gate) { release(); return; }

        const int note = s.midiNote(rootNote);

        if (held >= 0 && tied)
        {
            // Struck over the one held, then that let go. Slid on to the same note, it
            // just holds on.
            if (note != held)
            {
                voice.noteOn(note, s.accent);
                voice.noteOff(held);
            }
        }
        else
        {
            release();
            voice.noteOn(note, s.accent);
        }

        held = note;
        tied = s.slide;
    }

    void gateOff()
    {
        if (! tied) release();
    }

    //
    // Played from the song, a note at a time rather than a step. A note that starts before
    // the one sounding has ended slides into it, as a slid step does: struck over it, then
    // the old one let go - or, on the same note, just held on. Velocity 110 and up is an
    // accent.
    //
    static constexpr int accentVelocity = 110;

    void songNoteOn(int note, int velocity, bool slide)
    {
        const bool accent = velocity >= accentVelocity;

        if (slide && held >= 0)
        {
            if (note != held)
            {
                voice.noteOn(note, accent);
                voice.noteOff(held);
            }
        }
        else
        {
            release();
            voice.noteOn(note, accent);
        }

        held = note;
        tied = false;
    }

    // a note ending lets go only if it is still the one sounding
    void songNoteOff(int note)
    {
        if (note == held) release();
    }

    const decltype(Voice::engine)& view() const { return voice.engine; }

private:
    void release()
    {
        if (held >= 0) voice.noteOff(held);
        held = -1;
        tied = false;
    }

    Voice voice;
    int  held = -1;        // the note sounding, if any
    bool tied = false;     // and it slides on into the next step
};

//============================================================================== the drum machines

//
// A drum machine played from a pattern. Every hit rings out on its own, so stopping lets
// them; there are no gates to close.
//
template <class Engine, class S, int Voices>
class DrumMachine
{
public:
    using Settings = S;
    static constexpr int voices = Voices;
    static_assert(Voices <= DrumPattern::voices, "a drum pattern has sixteen rows");

    explicit DrumMachine(const std::vector<Control<Settings>>& controls) : knobs(controls) {}

    Knobs<Settings> knobs;
    Track<DrumPattern> track;

    void prepare(double rate)      { engine.prepare(rate); }
    void update()                  { engine.setSettings(knobs.read()); }
    void render(float* out, int n) { engine.render(out, n); }

    void start() { track.rewind(); }
    void stop()  { track.position = -1; }

    void step()
    {
        int i = 0;
        const DrumPattern& p = track.advance(i);
        const bool accented = p.get(DrumPattern::accentRow, i);

        for (int v = 0; v < voices; ++v)
            if (p.get(v, i)) engine.trigger(v, accented);
    }

    // Played from the song: one instrument, accented at velocity 110 and up.
    static constexpr int accentVelocity = 110;

    void hit(int voice, int velocity)
    {
        if (voice >= 0 && voice < voices) engine.trigger(voice, velocity >= accentVelocity);
    }

    const Engine& view() const { return engine; }

private:
    Engine engine;
};

const std::vector<Control<tb303::Settings>>& controls303();
const std::vector<Control<sh101::Settings>>& controls101();
const std::vector<Control<tr808::Settings>>& controls808();
const std::vector<Control<tr909::Settings>>& controls909();

using Mz303 = MonoLine<Tb303Voice>;
using Mz101 = MonoLine<Sh101Voice>;
using Mz808 = DrumMachine<tr808::Engine, tr808::Settings, tr808::Voice::count>;
using Mz909 = DrumMachine<tr909::Engine, tr909::Settings, tr909::Voice::count>;

} // namespace debirth
