// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Bounce.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace debirth
{

BounceRange bounceRange(const Song& s)
{
    if (s.cycle && s.right > s.left) return { s.left, s.right };
    return { 0, std::max(s.end(), ticksPerBar) };
}

int64_t bounceLength(const Rack& rack, BounceRange r, double rate, double tail)
{
    const double seconds = (double) (r.to - r.from) / ticksPerQuarter * 60.0 / rack.tempo.load();
    return (int64_t) std::ceil((seconds + std::max(0.0, tail)) * rate);
}

bool bounce(Rack& rack, const Song& song, double rate, double tail,
            const std::function<bool(const float*, const float*, int, double)>& sink)
{
    const BounceRange range = bounceRange(song);

    Song s = song;
    s.cursor = range.from;
    s.cycle = false;
    rack.publish(std::make_unique<Playback>(compile(s, &rack)));
    rack.nudge = 0;
    rack.mode = Mode::Song;
    rack.playing = true;

    const int64_t total = bounceLength(rack, range, rate, tail);
    int64_t skip = rack.latency();   // LIMIT's lookahead, off the front
    constexpr int block = 512;
    std::vector<float> l(block), r(block);

    bool finished = true;
    for (int64_t done = 0; done < total;)
    {
        rack.render(l.data(), r.data(), block);
        rack.collect();

        int from = 0;
        if (skip > 0)
        {
            from = (int) std::min<int64_t>(skip, block);
            skip -= from;
        }
        const int n = (int) std::min<int64_t>(block - from, total - done);
        if (n <= 0) continue;
        done += n;
        if (! sink(l.data() + from, r.data() + from, n, (double) done / (double) total))
        {
            finished = false;
            break;
        }
    }

    rack.playing = false;
    rack.render(l.data(), r.data(), block);   // stopped, so a rack used again starts clean
    return finished;
}

} // namespace debirth
