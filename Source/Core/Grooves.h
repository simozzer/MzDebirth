// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Key.h"
#include "Rack.h"

#include <string>
#include <vector>

namespace debirth
{

//
// The factory patterns: all four banks of every machine, eight grooves a bank, each bank
// a style of its own -
//
//   A   acid house, in C minor
//   B   techno, in F minor
//   C   electro, in A minor
//   D   deep and dub house, in G minor
//
// - and pattern n of every machine in a bank written to play with pattern n of the others.
// In each bank 1 to 3, 6 and 7 are grooves, 4 and 8 fills, and 5 a breakdown.
//
// writeGrooves() writes the four machines with patterns of their own, and the knobs and
// effects the grooves were written against; writeSamplerGrooves() the 950's, which strike
// the sounds of a kit by their sample names - KICK 1, HAT PEDAL - and so are only written
// once a programme with those names is loaded. Both return what they could not read: empty,
// unless a pattern below has a typing error in it, which the check makes sure it has not.
//
std::vector<std::string> writeGrooves(Rack&);
std::vector<std::string> writeSamplerGrooves(Mz950&);

// the keys the factory banks are written in: 5A, 4A, 8A and 6A
Key factoryKey(int bank);

//==============================================================================
//
// GROOVES IN A KEY
//
// The styles GENERATE writes, one for each factory bank, whose drums it starts from.
//
namespace Style { enum { Acid, Techno, Electro, Deep, count }; }
const char* styleName(int style);

//
// The key a bank's bass line and lead are in, from their notes. A guess - a bank of one
// repeated note could be in any key with that note in it - but the right one for anything
// with a tune.
//
Key detectKey(Rack&, int bank);

//
// Moves a bank's bass line and lead, and the 950's notes if it is playing a tuned
// programme rather than a kit, from one key to another, degree by degree: the tonic stays
// the tonic and the third the third, so C minor into C major turns every E flat to E. The
// tonic moves the shorter way, so a line never goes up more than a fifth or down more than
// a fourth. The drums have no key and are left alone.
//
void transposeBank(Rack&, int bank, Key from, Key to);

//
// Fills a bank, on all five machines, with eight new patterns in a key and a style - laid
// out as the factory banks are: 1 to 3, 6 and 7 grooves, 4 and 8 fills, 5 a breakdown -
// over a progression the key's chords make. The bass lines and leads are made from the
// scale; the drums from the style's factory patterns, each moved about a little. The same
// seed writes the same bank.
//
void generateBank(Rack&, int bank, Key, int style, unsigned seed);

} // namespace debirth
