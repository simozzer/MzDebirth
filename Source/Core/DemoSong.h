// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Song.h"

namespace debirth
{

class Rack;

//
// THE DEMO SONG, which a new rack opens with: an acid rave tune, sixty-four bars at 132 bpm,
// from bank A's factory grooves and a few parts written for it -
//
//     1   intro      the hats and claps alone, then the kick, then the tops
//     9   groove     the acid line comes in, sparse, then the full one
//    17   groove     the lead, and the 950's percussion
//    25   BUILD      the kick drops away; a snare roll speeding up, a riser climbing;
//                    a beat of silence
//    29   DROP       everything, and a crash
//    37   breakdown  no kick: the lead held long over the chords, the acid line thinned
//    45   BUILD      faster, higher, the acid line an octave up, a kick roll
//    49   DROP       twelve bars, two grooves turn about, a crash every eight
//    61   outro      the kick and the acid, thinning out
//
// It needs bank A's factory grooves in the rack - writeGrooves(), and writeSamplerGrooves()
// for the 950's. Without the 950's, its track is just left empty.
//
constexpr float demoTempo = 132.0f;

Song demoSong(Rack&);

} // namespace debirth
