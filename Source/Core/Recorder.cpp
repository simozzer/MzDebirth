// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Recorder.h"

#include <algorithm>

namespace debirth
{

void Recorder::begin(int start, const bool arm[Rack::channels], bool over)
{
    startTick = start;
    overdub = over;
    for (int m = 0; m < Rack::channels; ++m)
    {
        armed[m] = arm[m];
        runs[m].clear();
    }
    lastStep = -1;
}

void Recorder::add(Rack& rack, const Rack::Played& p)
{
    lastStep = std::max(lastStep, p.step);

    const int m = p.machine;
    if (m < 0 || m >= Rack::channels || ! armed[m]) return;

    // on from the run before, or a new one
    auto& list = runs[m];
    if (list.empty() || list.back().pattern != p.pattern || p.step != list.back().last + 1)
        list.push_back({ p.pattern, p.step, p.step, {} });
    Run& run = list.back();
    run.last = p.step;

    // the notes of this one step of the pattern, as they are now
    const Part pattern = partFromPattern(rack, m, p.pattern, 1);
    const int from = p.index * ticksPerStep, to = from + ticksPerStep;
    const int offset = (int) (p.step - run.first) * ticksPerStep - from;
    for (Note n : pattern.notes)
    {
        if (n.tick < from || n.tick >= to) continue;
        n.tick += offset;
        run.notes.push_back(n);
    }
}

int Recorder::end() const
{
    const int64_t bars = (lastStep + stepsPerPattern) / stepsPerPattern;   // the bar it stopped in, whole
    return startTick + (int) (bars * ticksPerBar);
}

void Recorder::apply(Rack& rack, Song& song) const
{
    if (empty()) return;

    for (int m = 0; m < Rack::channels; ++m)
    {
        if (! armed[m] || (overdub && runs[m].empty())) continue;

        auto t = std::find_if(song.tracks.begin(), song.tracks.end(), [m] (const SongTrack& s) { return s.machine == m; });
        if (t == song.tracks.end()) continue;

        // A take is over the whole track, for its bars; an overdub only where the machine
        // played, and the track's SHUFFLE is the song's - the rest of it swings as it did.
        if (! overdub)
        {
            clearRange(*t, startTick, end());
            t->shuffle = rack.track(m).shuffle.load();
        }

        const auto& list = runs[m];
        for (size_t i = 0; i < list.size(); ++i)
        {
            const Run& run = list[i];

            Part part;
            part.name = std::string(1, (char) ('A' + run.pattern / patternsPerBank)) + std::to_string(run.pattern % patternsPerBank + 1);
            part.start = startTick + (int) run.first * ticksPerStep;
            part.length = (int) (run.last - run.first + 1) * ticksPerStep;
            part.notes = run.notes;

            // A note may carry on past the run - a slide, a tie. Into the next run, straight
            // on, it slides or holds as it did; into a gap, it lets go where the machine did.
            const bool joined = i + 1 < list.size() && list[i + 1].first == run.last + 1;
            if (! joined)
                for (auto& n : part.notes)
                    n.length = std::max(1, std::min(n.length, part.length - n.tick));

            part.sortNotes();
            if (overdub) clearRange(*t, part.start, part.end());   // round a cycle, the later pass wins
            t->parts.push_back(std::move(part));
        }

        std::stable_sort(t->parts.begin(), t->parts.end(), [] (const Part& a, const Part& b) { return a.start < b.start; });
    }
}

} // namespace debirth
