// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Song.h"

#include <string>

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
//    37   breakdown  no kick: the lead held long over the chords, the acid line thinned;
//                    from 41 a long snare roll, starting almost silent
//    45   BUILD      the roll louder and faster, the acid line an octave up, a kick roll
//    49   DROP       twelve bars, two grooves turn about, a crash every eight
//    61   outro      the kick and the acid, thinning out
//
// with the knobs turned as it goes: the 303's filter opening as the acid comes in and through
// each build, the 909's snare level bringing the rolls up, the 101's filter opening with each
// riser, and its reverb sent up for the breakdown - each put back after.
//
// It needs bank A's factory grooves in the rack - writeGrooves(), and writeSamplerGrooves()
// for the 950's. Without the 950's, its track is just left empty.
//
constexpr float demoTempo = 132.0f;

Song demoSong(Rack&);

//
// THE AMBIENT DEMO, the other song a new rack can open with: something to fall asleep to,
// sixty-four bars at 72 bpm in E flat, three and a half minutes -
//
//     1   the sea alone, and the first whales far off
//     9   the Mz7's pad comes in: E flat, C minor, A flat, B flat, four bars each
//    17   the 101's slow melody, echoing, over the 303's soft low pulse
//    33   the melody and the pulse rest: the pad, the sea and the whales
//    41   the melody again, the other way about, the 303's filter breathing
//    57   the last chord held, the whales going, the sea fading away
//
// No drums. The whales and the sea come from the Mz950's WHALES disk, its WHALE SEA
// programme - the sea held on the keys above the whales the whole way through.
//
// voiceForAmbient() puts the voices in: the WHALES disk into the 950 from `whalesDisk`, unless
// it is in already, on WHALE SEA, its patterns cleared - the factory ones are the drum disk's -
// and the Mz7 on WARM PAD. It says whether the whales are in; with no disk, they aren't.
// dressForAmbient() then sets the knobs - the 101 and 303 voiced soft, the sends and the
// effects long - and the song's automation is written against those settings. Without the
// whales, the 950's track is left empty.
//
constexpr float ambientTempo = 72.0f;

bool voiceForAmbient(Rack&, const std::string& whalesDisk);
void dressForAmbient(Rack&);
Song ambientSong(Rack&, bool whales);

} // namespace debirth
