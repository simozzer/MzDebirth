// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <atomic>
#include <cstdint>

namespace debirth
{

constexpr int stepsPerPattern  = 16;
constexpr int banks            = 4;                        // A to D
constexpr int patternsPerBank  = 8;
constexpr int patternsPerTrack = banks * patternsPerBank;

//
// Everything a pattern holds is packed into atomic words, so the panel can edit a pattern
// while the audio thread plays it, with no lock between them. The panel is the only writer.
//

//
// A step of a bass line, as the 303 writes one: a note within an octave, an octave switch,
// and accent and slide. A slide holds the note over into the next step and glides to it.
//
struct BassStep
{
    int  note   = 0;        // 0 to 12, C up to the C above
    int  octave = 0;        // -1 DOWN, 0, +1 UP
    bool gate   = false;    // off is a rest
    bool accent = false;
    bool slide  = false;

    int midiNote(int root) const { return root + note + 12 * octave; }

    uint32_t pack() const
    {
        return (uint32_t) note | (uint32_t) (octave + 1) << 4
             | (uint32_t) gate << 6 | (uint32_t) accent << 7 | (uint32_t) slide << 8;
    }

    static BassStep unpack(uint32_t w)
    {
        BassStep s;
        s.note   = (int) (w & 15u) > 12 ? 12 : (int) (w & 15u);
        s.octave = (int) ((w >> 4) & 3u) - 1;
        if (s.octave > 1) s.octave = 1;
        s.gate   = (w >> 6) & 1u;
        s.accent = (w >> 7) & 1u;
        s.slide  = (w >> 8) & 1u;
        return s;
    }
};

struct PatternBase
{
    std::atomic<int> length { stepsPerPattern };   // 1 to 16

    int len() const
    {
        const int n = length.load(std::memory_order_relaxed);
        return n < 1 ? 1 : (n > stepsPerPattern ? stepsPerPattern : n);
    }
};

struct BassPattern : PatternBase
{
    std::atomic<uint32_t> steps[stepsPerPattern] {};

    BassStep get(int i) const      { return BassStep::unpack(steps[i].load(std::memory_order_relaxed)); }
    void set(int i, const BassStep& s) { steps[i].store(s.pack(), std::memory_order_relaxed); }

    void clear()
    {
        for (auto& s : steps) s.store(0, std::memory_order_relaxed);
        length = stepsPerPattern;
    }

    void copyFrom(const BassPattern& o)
    {
        for (int i = 0; i < stepsPerPattern; ++i) steps[i].store(o.steps[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
        length = o.len();
    }
};

//
// A grid: a row of sixteen bits per thing that can be struck, a bit per step.
//
template <int Rows>
struct GridPattern : PatternBase
{
    static constexpr int rows = Rows;

    std::atomic<uint32_t> hits[rows] {};

    bool get(int row, int step) const { return (hits[row].load(std::memory_order_relaxed) >> step) & 1u; }

    void set(int row, int step, bool on)
    {
        const uint32_t w = hits[row].load(std::memory_order_relaxed);
        hits[row].store(on ? (w | 1u << step) : (w & ~(1u << step)), std::memory_order_relaxed);
    }

    bool rowIsEmpty(int row) const { return hits[row].load(std::memory_order_relaxed) == 0; }

    void clear()
    {
        for (auto& h : hits) h.store(0, std::memory_order_relaxed);
        length = stepsPerPattern;
    }

    void copyFrom(const GridPattern& o)
    {
        for (int r = 0; r < rows; ++r) hits[r].store(o.hits[r].load(std::memory_order_relaxed), std::memory_order_relaxed);
        length = o.len();
    }
};

//
// A drum pattern: a row per instrument, and one more for the accents, which fall on every
// instrument struck on that step. Sixteen rows, the most any of the drum machines has.
//
struct DrumPattern : GridPattern<17>
{
    static constexpr int voices    = 16;
    static constexpr int accentRow = 16;
};

//
// A sampler's pattern: a row per MIDI note, so any number can sound on a step, then a row of
// accents, and a row of ties. A note on a tied step that was sounding on the step before
// carries on rather than being struck again - which is how a note is made longer than half
// a step.
//
struct NotePattern : GridPattern<130>
{
    static constexpr int notes     = 128;
    static constexpr int accentRow = 128;
    static constexpr int tieRow    = 129;
};

//
// A machine's patterns, and which of them is playing. The panel picks one (selected) and
// edits it; a playing machine moves on to it when the pattern it is on comes back round,
// as the hardware does, so a change always lands on the bar.
//
struct TrackBase
{
    std::atomic<int>  selected { 0 };     // the one on the panel; plays from the next loop
    std::atomic<int>  current  { 0 };     // the one playing
    std::atomic<int>  position { -1 };    // the step playing, for the lights; -1 stopped
    std::atomic<bool> shuffle  { true };  // whether this machine follows the shuffle

    virtual ~TrackBase() = default;
    virtual PatternBase& pattern(int index) = 0;
    virtual void clear(int index) = 0;
    virtual void copy(int from, int to) = 0;
};

template <class P>
struct Track final : TrackBase
{
    P patterns[patternsPerTrack];

    PatternBase& pattern(int i) override { return patterns[i]; }
    void clear(int i) override           { patterns[i].clear(); }
    void copy(int from, int to) override { if (from != to) patterns[to].copyFrom(patterns[from]); }

    P& edited() { return patterns[selected.load()]; }

    // audio thread: the pattern and step to play now, and on to the next
    const P& advance(int& step)
    {
        if (pos == 0) current = selected.load();
        const P& p = patterns[current.load()];
        if (pos >= p.len()) pos = 0;   // shortened under us
        step = pos;
        position = pos;
        if (++pos >= p.len()) pos = 0;
        return p;
    }

    void rewind()
    {
        pos = 0;
        current = selected.load();
        position = -1;
    }

private:
    int pos = 0;
};

} // namespace debirth
