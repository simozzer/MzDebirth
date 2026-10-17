// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <string>
#include <vector>

namespace debirth
{

//
// A key, named as harmonic-mixing software names them - the Camelot wheel that Mixed In Key
// shows - so a DJ can set the rack to the key of the record it is to mix with.
//
// The wheel has twelve numbers, a fifth apart, each in an A (minor) and a B (major) that
// share their notes: 8A is A minor, 8B is C major, 9A E minor. Two keys mix when they are
// the same, a step round the wheel in the same letter, or the same number in the other
// letter.
//
// Stored as one number, 0 to 23: (number - 1) * 2, plus 1 for B.
//
struct Key
{
    int number = 5;        // 1 to 12
    bool major = false;    // B

    static Key fromIndex(int index);
    int index() const { return (number - 1) * 2 + (major ? 1 : 0); }

    int root() const;                     // its tonic, as a pitch class: C is 0
    const int* scale() const;             // the seven steps of its scale, from the tonic
    bool contains(int pitchClass) const;

    std::string code() const;             // "8A"
    std::string name() const;             // "A minor"

    // the keys it mixes with, itself first
    std::vector<Key> compatible() const;

    bool operator==(const Key& o) const { return number == o.number && major == o.major; }
};

//
// The key a set of notes is most likely in, by the Krumhansl-Schmuckler method: how well
// their pitch classes, each counted by how much it sounds, line up with the profile of each
// of the 24 keys. weights: twelve, a pitch class each.
//
Key likeliestKey(const double* weights);

} // namespace debirth
