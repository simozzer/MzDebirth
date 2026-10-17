// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Clock.h"
#include "Song.h"

#include <algorithm>
#include <cmath>

namespace debirth
{

//
// Plays a compiled song on the audio thread: says, to the sample, where in the next block
// each note starts and ends - as the clock says where each step lands, and with the same
// arithmetic, so a note on a sixteenth lands on the very sample that step would.
//
// It keeps no place in the song. Each block it looks up the notes due by when they start,
// so a new Playback - the song edited while it plays - can be swapped in between any two
// blocks. The notes sounding are its own: each one's end is remembered when it starts, so a
// note edited away while it sounds still ends, and none is struck twice.
//
class SongPlayer
{
public:
    enum class Kind { On, Off };

    struct Due
    {
        double time;     // in steps
        Kind kind;
        int machine, key, velocity;
        bool slide;
    };

    void setPlayback(const Playback* p) { playback = p; }

    // A tick as a time in steps, swung as the clock swings: everything in an off-beat
    // sixteenth of a swinging track held back by the shuffle.
    static double timeOf(int tick, bool swings, double shuffle)
    {
        const int step = tick / ticksPerStep;
        const double delay = swings && (step & 1) ? shuffle * Clock::maxSwing : 0.0;
        return (double) step + delay + (double) (tick % ticksPerStep) / (double) ticksPerStep;
    }

    //
    // Calls f(offset, due) for each note starting or ending in the n samples from `phase`
    // (the clock's position, in steps), in order of time - an end before a start at the same
    // moment. An event lands on the first sample at or after its time.
    //
    template <class F>
    void advance(double phase, int n, double perStep, double shuffle, F&& f)
    {
        auto sampleOf = [&] (double t) { return (int) std::ceil((t - phase) * perStep - 1.0e-6); };

        // the notes starting
        int starts = 0;
        if (playback != nullptr && ! playback->events.empty())
        {
            const auto& ev = playback->events;
            const double margin = 2.0 / perStep;
            const int fromTick = (int) std::floor((phase - Clock::maxSwing - margin) * ticksPerStep);
            const double untilTick = (phase + (double) n / perStep + margin) * ticksPerStep;

            auto it = std::lower_bound(ev.begin(), ev.end(), fromTick,
                                       [] (const SongEvent& e, int t) { return e.tick < t; });

            for (; it != ev.end() && (double) it->tick < untilTick && starts < maxDue; ++it)
            {
                const double t = timeOf(it->tick, it->shuffle, shuffle);
                const int at = sampleOf(t);
                if (at < 0 || at >= n) continue;
                due[starts++] = { t, Kind::On, it->machine, it->key, it->velocity, it->slide };
                endsOf[starts - 1] = timeOf(it->endTick, it->shuffle, shuffle);
            }
        }

        // In order of time; the sort keeps the song's order (by machine, then key) for
        // starts at the same moment.
        int order[maxDue];
        for (int i = 0; i < starts; ++i) order[i] = i;
        std::stable_sort(order, order + starts, [this] (int a, int b) { return due[a].time < due[b].time; });

        int next = 0;
        for (;;)
        {
            // the earliest end due in this block
            int end = -1;
            for (int s = 0; s < sounding; ++s)
                if (sampleOf(playing[s].offAt) < n && (end < 0 || playing[s].offAt < playing[end].offAt))
                    end = s;

            if (end >= 0 && (next == starts || playing[end].offAt <= due[order[next]].time))
            {
                const Sound s = playing[end];
                playing[end] = playing[--sounding];
                f(std::max(0, sampleOf(s.offAt)), Due { s.offAt, Kind::Off, s.machine, s.key, 0, false });
                continue;
            }

            if (next == starts) break;

            const int i = order[next++];
            const Due& d = due[i];

            // A mono line's new note takes over from the one sounding, whose end is now its.
            if (machineKind(d.machine) == MachineKind::Mono)
                for (int s = sounding - 1; s >= 0; --s)
                    if (playing[s].machine == d.machine) playing[s] = playing[--sounding];

            f(std::max(0, sampleOf(d.time)), d);

            // A drum just rings; anything else is let go when it ends.
            if (machineKind(d.machine) != MachineKind::Drums && sounding < maxSounding)
                playing[sounding++] = { endsOf[i], d.machine, d.key };
        }
    }

    // Every note sounding let go now - at STOP, or when the cycle jumps back.
    template <class F>
    void releaseAll(F&& f)
    {
        for (int s = 0; s < sounding; ++s)
            f(0, Due { 0.0, Kind::Off, playing[s].machine, playing[s].key, 0, false });
        sounding = 0;
    }

    void forget() { sounding = 0; }

    int notesSounding() const { return sounding; }

private:
    static constexpr int maxDue = 512, maxSounding = 256;

    struct Sound { double offAt; int machine, key; };

    const Playback* playback = nullptr;
    Due due[maxDue];
    double endsOf[maxDue];
    Sound playing[maxSounding];
    int sounding = 0;
};

} // namespace debirth
