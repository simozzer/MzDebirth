// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

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

} // namespace debirth
