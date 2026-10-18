// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Rack.h"
#include "Song.h"

#include <vector>

namespace debirth
{

//
// A TAKE: the rack played into the song, as a tape machine would record it. The rack logs
// every step each armed machine plays - its pattern and the step of it - and the panel hands
// them here as they come. Each stretch of one pattern played without a break becomes a part,
// named after it, holding the notes of exactly the steps that played; a change of pattern, or
// the machine's STOP, starts another.
//
// The take goes into the song at the bar it began from. Over the bars it ran - the bar STOP
// fell in, too - each armed machine's track is wiped first, so whatever was there is
// recorded over, a stopped machine's included.
//
class Recorder
{
public:
    // a take from this tick of the song - a bar - for the machines armed
    void begin(int startTick, const bool armed[Rack::channels]);

    // a step the rack logged; the notes are read from its pattern now
    void add(Rack&, const Rack::Played&);

    // whether the take ran at all - a step played, if only the clock's
    bool empty() const { return lastStep < 0; }

    // the ticks the take covers: from its start, to the end of the bar it stopped in
    int start() const { return startTick; }
    int end() const;

    // how far it has got, in the song's ticks: the end of the last step logged
    int reached() const { return startTick + (int) ((lastStep + 1) * ticksPerStep); }

    //
    // The take into the song. Each armed machine's track is wiped over the take and its
    // parts put in; its SHUFFLE is set as the machine's was, so they swing as they did.
    //
    void apply(Rack&, Song&) const;

private:
    struct Run
    {
        int pattern;
        int64_t first, last;       // the clock's steps
        std::vector<Note> notes;   // from the run's first step
    };

    int startTick = 0;
    bool armed[Rack::channels] {};
    std::vector<Run> runs[Rack::channels];
    int64_t lastStep = -1;
};

} // namespace debirth
