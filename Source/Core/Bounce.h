// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Rack.h"
#include "Song.h"

#include <functional>

namespace debirth
{

//
// EXPORT: a song rendered straight through, as fast as the machine can go, for writing to a
// file. The song is played on a rack of its own - a copy, set up as the one on screen is - so
// the rack being played is never touched. It plays from one tick to another in SONG mode,
// no cycle, then on for `tail` seconds so the delay and the reverb ring out. LIMIT's lookahead
// is taken off the front, so the file starts exactly on the song's first tick.
//
struct BounceRange
{
    int from = 0, to = 0;   // ticks
};

// the whole song, or between the locators when CYCLE is on
BounceRange bounceRange(const Song&);

// how many samples a range comes to, tail and all, at a rate and the rack's tempo
int64_t bounceLength(const Rack&, BounceRange, double rate, double tail);

//
// Plays the song on `rack` - prepared at `rate`, with every pattern, knob and disk in place -
// handing each block of the output to `sink`, which returns false to give up. It returns
// whether it got to the end. The song is published to the rack here.
//
bool bounce(Rack&, const Song&, double rate, double tail,
            const std::function<bool(const float* left, const float* right, int n, double done)>& sink);

} // namespace debirth
