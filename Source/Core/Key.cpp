// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Key.h"

#include <algorithm>
#include <cmath>

namespace debirth
{

namespace
{
    const int minorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
    const int majorScale[7] = { 0, 2, 4, 5, 7, 9, 11 };

    const char* const pitchNames[12] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
}

Key Key::fromIndex(int index)
{
    index = std::clamp(index, 0, 23);
    return { index / 2 + 1, index % 2 == 1 };
}

int Key::root() const
{
    // 1A is G# minor, and each number on is a fifth up; a B is its A's relative major,
    // a minor third above
    const int minorRoot = (8 + 7 * (number - 1)) % 12;
    return major ? (minorRoot + 3) % 12 : minorRoot;
}

const int* Key::scale() const { return major ? majorScale : minorScale; }

bool Key::contains(int pitchClass) const
{
    const int rel = ((pitchClass - root()) % 12 + 12) % 12;
    return std::find(scale(), scale() + 7, rel) != scale() + 7;
}

std::string Key::code() const { return std::to_string(number) + (major ? "B" : "A"); }

std::string Key::name() const { return std::string(pitchNames[root()]) + (major ? " major" : " minor"); }

std::vector<Key> Key::compatible() const
{
    auto wrap = [] (int n) { return (n + 11) % 12 + 1; };
    return { *this, { wrap(number - 1), major }, { wrap(number + 1), major }, { number, ! major } };
}

Key likeliestKey(const double* weights)
{
    static const double majorProfile[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double minorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    auto correlation = [weights] (const double* profile, int root)
    {
        double mw = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mw += weights[i]; mp += profile[i]; }
        mw /= 12;
        mp /= 12;

        double num = 0, dw = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double w = weights[(i + root) % 12] - mw, p = profile[i] - mp;
            num += w * p;
            dw += w * w;
            dp += p * p;
        }
        return dw > 0 && dp > 0 ? num / std::sqrt(dw * dp) : -2.0;
    };

    Key best;
    double bestScore = -3;
    for (int i = 0; i < 24; ++i)
    {
        const Key k = Key::fromIndex(i);
        const double score = correlation(k.major ? majorProfile : minorProfile, k.root());
        if (score > bestScore) { bestScore = score; best = k; }
    }
    return best;
}

} // namespace debirth
