// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

//
// The sequencer, checked on its own: that the clock puts every step on its sample whatever
// the block size, that shuffle holds back the off-beats and only on the machines that follow
// it, that a bass line's gates and slides reach the engine as the 303 would play them, that
// drums strike where they are written, that a pattern change waits for the loop, and that
// the rack's output stays sane. Links no JUCE.
//

#include "Core/Rack.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace debirth;

namespace
{

int failures = 0;

void check(bool ok, const char* what, const std::string& detail = {})
{
    std::printf("%s  %-62s %s\n", ok ? "ok  " : "FAIL", what, detail.c_str());
    if (! ok) ++failures;
}

template <class... A>
std::string fmt(const char* f, A... a)
{
    char buf[256];
    std::snprintf(buf, sizeof buf, f, a...);
    return buf;
}

struct Hit
{
    int64_t sample;
    int lane;
    Clock::Event event;
    int64_t step;
    bool operator==(const Hit& o) const { return sample == o.sample && lane == o.lane && event == o.event && step == o.step; }
};

// Every event of the first `total` samples, fed in blocks of the sizes `sizes` gives.
template <class Sizes>
std::vector<Hit> run(double rate, double bpm, double shuffle, int64_t total, Sizes sizes)
{
    Clock c;
    c.prepare(rate);
    c.setTempo(bpm);
    c.setShuffle(shuffle);
    c.start();

    std::vector<Hit> hits;
    int64_t done = 0;
    while (done < total)
    {
        int n = sizes();
        if (done + n > total) n = (int) (total - done);
        c.advance(n, [&] (int at, int lane, Clock::Event e, int64_t step)
        {
            hits.push_back({ done + at, lane, e, step });
        });
        done += n;
    }
    return hits;
}

void timing()
{
    std::printf("\nthe clock\n");

    // 120 bpm at 48 kHz: a sixteenth is 6000 samples exactly
    const auto hits = run(48000, 120, 0, 48000, [] { return 512; });

    bool onTime = true;
    int steps = 0, offs = 0;
    for (const auto& h : hits)
    {
        if (h.lane != 0) continue;
        const int64_t want = h.step * 6000 + (h.event == Clock::Event::GateOff ? 3000 : 0);
        onTime &= h.sample == want;
        (h.event == Clock::Event::Step ? steps : offs)++;
    }
    check(onTime, "every step and gate lands on its sample", fmt("%d steps, %d gates closed in a second", steps, offs));
    check(steps == 8 && offs == 8, "eight sixteenths a second at 120 bpm");

    // the same events, whatever the blocks
    std::mt19937 rng(101);
    std::uniform_int_distribution<int> size(1, 1500);
    const auto ragged = run(44100, 137, 0.6, 44100 * 4, [&] { return size(rng); });
    const auto whole  = run(44100, 137, 0.6, 44100 * 4, [] { return 44100 * 4; });
    check(ragged == whole, "ragged blocks put events where one block would", fmt("%zu events", whole.size()));

    // shuffle: lane 1's off-beats held back, lane 0 untouched
    const auto swung = run(48000, 120, 1.0, 48000, [] { return 256; });
    int64_t off1 = -1, off0 = -1, on1 = -1;
    for (const auto& h : swung)
    {
        if (h.event != Clock::Event::Step) continue;
        if (h.step == 1 && h.lane == 1) off1 = h.sample;
        if (h.step == 1 && h.lane == 0) off0 = h.sample;
        if (h.step == 2 && h.lane == 1) on1 = h.sample;
    }
    check(off1 == 8000, "full shuffle holds the off-beat back a third of a step", fmt("step 2 at %lld", (long long) off1));
    check(off0 == 6000 && on1 == 12000, "and leaves the beat, and the straight lane, alone");

    // a block ends exactly on a step: it belongs to the next block, not this one
    Clock c;
    c.prepare(48000);
    c.setTempo(120);
    c.start();
    int seen = 0;
    c.advance(6000, [&] (int, int, Clock::Event e, int64_t) { if (e == Clock::Event::Step) ++seen; });
    int firstAt = -1;
    c.advance(100, [&] (int at, int lane, Clock::Event e, int64_t) { if (e == Clock::Event::Step && lane == 0 && firstAt < 0) firstAt = at; });
    check(seen == 2 && firstAt == 0, "a step on the block's end opens the next block");
}

// Steps the rack in blocks, returning whatever f records at each block's end.
template <class F>
void play(Rack& r, int blocks, int n, F&& f)
{
    std::vector<float> l((size_t) n), rr((size_t) n);
    for (int b = 0; b < blocks; ++b)
    {
        r.render(l.data(), rr.data(), n);
        f(b, l, rr);
    }
}

void bassLine()
{
    std::printf("\nthe bass line\n");

    // 125 bpm at 48 kHz: 5760 samples a step. Watch the engine every 480 samples.
    Rack r;
    r.prepare(48000, 480);
    r.tempo = 125;
    r.mz303.track.shuffle = false;

    auto& p = r.mz303.track.patterns[0];
    BassStep a; a.gate = true; a.note = 0;                   // C, let go half way
    BassStep b; b.gate = true; b.note = 7; b.slide = true;   // G, slid into the next
    BassStep c; c.gate = true; c.note = 3; c.octave = 1; c.accent = true;
    p.set(0, a); p.set(1, b); p.set(2, c);
    p.length = 4;                                             // and a rest

    std::vector<int> key, gate, acc;
    r.playing = true;
    play(r, 60, 480, [&] (int, auto&, auto&)
    {
        key.push_back(r.mz303.view().currentKey());
        gate.push_back(r.mz303.view().gateIsOn() ? 1 : 0);
        acc.push_back(r.mz303.view().accented() ? 1 : 0);
    });

    // block k ends at sample 480 (k + 1); a step is 12 blocks
    auto at = [] (int step, double frac) { return (int) (step * 12 + frac * 12) - 1; };

    check(gate[at(0, 0.25)] && key[at(0, 0.25)] == 36, "a note is struck as its step begins", fmt("key %d", key[at(0, 0.25)]));
    check(! gate[at(0, 0.75)], "and let go half way through");
    check(gate[at(1, 0.75)] && key[at(1, 0.75)] == 43, "a slid note holds on past half way");
    check(gate[at(2, 0.25)] && key[at(2, 0.25)] == 51 && acc[at(2, 0.25)], "into the next, an octave up and accented",
          fmt("key %d", key[at(2, 0.25)]));
    check(! gate[at(3, 0.5)], "a rest is silent");
    check(gate[at(4, 0.25)] && key[at(4, 0.25)] == 36, "and four steps on, the pattern comes round");

    r.playing = false;
    play(r, 2, 480, [] (int, auto&, auto&) {});
    check(! r.mz303.view().gateIsOn() && r.mz303.track.position == -1, "stopping lets go of the note");
}

void drums()
{
    std::printf("\nthe drums\n");

    Rack r;
    r.prepare(48000, 480);
    r.tempo = 125;   // 5760 samples a step

    auto& p = r.mz808.track.patterns[0];
    for (int s = 0; s < 16; s += 4) p.set(tr808::Voice::BD, s, true);
    p.set(tr808::Voice::SD, 4, true);
    p.set(tr808::Voice::SD, 12, true);
    for (int s = 0; s < 16; ++s) p.set(tr808::Voice::CH, s, true);

    r.playing = true;
    uint32_t bd = 0, sd = 0, ch = 0;
    play(r, 12 * 16, 480, [&] (int, auto&, auto&)
    {
        bd = r.mz808.view().hitCount(tr808::Voice::BD);
        sd = r.mz808.view().hitCount(tr808::Voice::SD);
        ch = r.mz808.view().hitCount(tr808::Voice::CH);
    });
    check(bd == 4 && sd == 2 && ch == 16, "a bar strikes what is written in it", fmt("BD %u SD %u CH %u", bd, sd, ch));

    // a new pattern chosen mid-bar waits for the bar to end
    auto& q = r.mz808.track.patterns[1];
    q.set(tr808::Voice::CP, 0, true);
    q.length = 8;
    r.mz808.track.selected = 1;

    int switchedAt = -1;
    play(r, 12 * 16, 480, [&] (int k, auto&, auto&)
    {
        if (switchedAt < 0 && r.mz808.track.current == 1) switchedAt = k;
    });
    check(switchedAt == 0, "a pattern picked between bars starts on the next",
          fmt("on block %d", switchedAt));

    // three steps into the eight of pattern 2, pick pattern 1 again: it waits out the rest
    play(r, 12 * 3, 480, [] (int, auto&, auto&) {});
    r.mz808.track.selected = 0;
    play(r, 12 * 4, 480, [] (int, auto&, auto&) {});
    check(r.mz808.track.current == 1, "and one picked mid-pattern waits for its end");
    play(r, 12 + 1, 480, [] (int, auto&, auto&) {});   // the step that wraps opens the next block
    check(r.mz808.track.current == 0, "then takes over");
}

void output()
{
    std::printf("\nthe rack\n");

    Rack r;
    r.prepare(44100, 256);
    r.tempo = 140;
    r.shuffle = 0.5f;

    std::mt19937 rng(808);
    for (int s = 0; s < 16; ++s)
    {
        BassStep b;
        b.gate = rng() % 4 != 0;
        b.note = (int) (rng() % 13);
        b.octave = (int) (rng() % 3) - 1;
        b.accent = rng() % 3 == 0;
        b.slide = rng() % 3 == 0;
        r.mz303.track.patterns[0].set(s, b);
        for (int v = 0; v < DrumPattern::rows; ++v)
            r.mz808.track.patterns[0].set(v, s, rng() % 4 == 0);
    }

    r.playing = true;
    float peak = 0;
    bool finite = true;
    double energy = 0;
    play(r, 44100 * 6 / 256, 256, [&] (int, auto& l, auto& rr)
    {
        for (size_t i = 0; i < l.size(); ++i)
        {
            finite &= std::isfinite(l[i]) && std::isfinite(rr[i]);
            peak = std::fmax(peak, std::fmax(std::fabs(l[i]), std::fabs(rr[i])));
            energy += (double) l[i] * l[i];
        }
    });
    check(finite, "six seconds of a random pattern stay finite");
    check(peak > 0.05f && peak < 4.0f, "and at a sane level", fmt("peak %.2f", peak));

    // mute and pan
    r.channel[Rack::M303].mute = true;
    r.channel[Rack::M808].pan = -1.0f;
    float right = 0;
    play(r, 40, 256, [] (int, auto&, auto&) {});   // let the gains ramp
    play(r, 400, 256, [&] (int, auto&, auto& rr) { for (float x : rr) right = std::fmax(right, std::fabs(x)); });
    check(right < 1.0e-6f, "panned hard left and the bass muted, the right is silent", fmt("peak %.2e", right));
}

} // namespace

void others()
{
    std::printf("\nthe 101, the 909 and the 950\n");

    Rack r;
    r.prepare(48000, 480);
    r.tempo = 125;   // 5760 samples a step

    // the 101: a note, then one slid into - its gate stays up across the two
    BassStep a; a.gate = true; a.note = 0; a.slide = true;
    BassStep b; b.gate = true; b.note = 7;
    r.mz101.track.patterns[0].set(0, a);
    r.mz101.track.patterns[0].set(1, b);

    r.mz909.track.patterns[0].set(tr909::Voice::BD, 0, true);
    r.mz909.track.patterns[0].set(tr909::Voice::RC, 2, true);

    r.playing = true;
    std::vector<int> gate, key;
    play(r, 12 * 4, 480, [&] (int, auto&, auto&)
    {
        gate.push_back(r.mz101.view().gateIsOn() ? 1 : 0);
        key.push_back(r.mz101.view().currentKey());
    });

    // the 101's CPU comes round every few milliseconds, so look well inside each step
    check(gate[3] && key[3] == 48, "the 101 plays a step's note", fmt("key %d", key[3]));
    check(gate[11] && gate[15] && key[15] == 55, "and holds a slide on into the next");
    check(! gate[22], "then lets go half way through it");
    check(r.mz909.view().hitCount(tr909::Voice::BD) == 1 && r.mz909.view().hitCount(tr909::Voice::RC) == 1,
          "the 909 strikes what is written");

#ifdef MZ950_DISKS
    std::string error;
    const std::string disk = std::string(MZ950_DISKS) + "/DRUMS.hfe";
    if (! r.mz950.loadDisk(disk, error))
    {
        check(false, "the 950 loads a disk", error);
        return;
    }
    check(! r.mz950.programNames().empty() && r.mz950.program() >= 0, "the 950 loads a disk and picks a programme",
          fmt("%zu programmes, %zu key spans", r.mz950.programNames().size(), r.mz950.keySpans().size()));
    r.mz950.service();

    // a chord of the first two keygroups' first keys, on the step that comes next
    const auto spans = r.mz950.keySpans();
    auto& p = r.mz950.track.patterns[0];
    p.set(spans[0].low, 0, true);
    if (spans.size() > 1) p.set(spans[1].low, 0, true);

    r.playing = false;
    r.channel[Rack::M101].mute = true;   // the 950 on its own
    r.channel[Rack::M909].mute = true;
    play(r, 2, 480, [] (int, auto&, auto&) {});
    r.playing = true;
    float peak = 0;
    int voices = 0;
    play(r, 12, 480, [&] (int, auto& l, auto&)
    {
        voices = std::max(voices, r.mz950.activeVoices());
        for (float x : l) peak = std::fmax(peak, std::fabs(x));
    });
    check(voices >= (spans.size() > 1 ? 2 : 1), "the 950 sounds every note on a step", fmt("%d voices", voices));
    check(peak > 0.01f && peak < 4.0f, "and is heard", fmt("peak %.2f", peak));
#endif
}

int main()
{
    timing();
    bassLine();
    drums();
    others();
    output();

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : fmt("%d FAILED", failures).c_str());
    return failures == 0 ? 0 : 1;
}
