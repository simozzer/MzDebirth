// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

//
// The sequencer, checked on its own: that the clock puts every step on its sample whatever
// the block size, that shuffle holds back the off-beats and only on the machines that follow
// it, that a bass line's gates and slides reach the engine as the 303 would play them, that
// drums strike where they are written, that a pattern change waits for the loop, and that
// the rack's output stays sane. Links no JUCE.
//

#include "Core/DemoSong.h"
#include "Core/Grooves.h"
#include "Core/Rack.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
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

// A check that cannot run here, said so rather than passed over in silence.
void skip(const char* what)
{
    std::printf("skip  %s\n", what);
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
    if (! std::ifstream(disk).good())
    {
        skip("the 950's checks: no drum disk - make Mz950's with AkaiS950Synth\\build.ps1 -To disks");
        return;
    }
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

void effects()
{
    std::printf("\nthe effects\n");

    static constexpr double rate = 48000;
    auto sine = [] (std::vector<float>& v, float hz, float amp)
    {
        for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin(2.0 * 3.14159265358979 * hz * (double) i / rate);
    };
    auto peakOf = [] (const std::vector<float>& v, size_t from = 0)
    {
        float p = 0;
        for (size_t i = from; i < v.size(); ++i) p = std::fmax(p, std::fabs(v[i]));
        return p;
    };
    auto rmsOf = [] (const std::vector<float>& v, size_t from = 0)
    {
        double s = 0;
        for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
        return (float) std::sqrt(s / (double) (v.size() - from));
    };

    // DIST: full drive squares a sine off - its peak and its loudness close up - and keeps
    // the level about where it was
    {
        std::vector<float> x(4800);
        sine(x, 220.0f, 0.3f);
        const float crestIn = peakOf(x) / rmsOf(x);
        Distortion d;
        d.prepare(rate);
        d.process(x.data(), (int) x.size(), 1.0f, 0.5f);
        const float crestOut = peakOf(x, 960) / rmsOf(x, 960);
        check(crestOut < crestIn * 0.85f, "DIST squares a sine off", fmt("crest %.2f to %.2f", crestIn, crestOut));
        check(peakOf(x) < 0.6f && peakOf(x) > 0.15f, "and keeps its level", fmt("peak %.2f from 0.30", peakOf(x)));
    }

    // PCF: shut, a 2 kHz tone is gone; the envelope full open, it comes through
    {
        EffectSettings s;
        s.pcfFreq = 0.1f;
        s.pcfAmount = 1.0f;
        s.pcfQ = 0.0f;

        std::vector<float> shut(4800), open(4800), env(4800);
        sine(shut, 2000.0f, 0.5f);
        sine(open, 2000.0f, 0.5f);

        pcf::Filter f;
        f.prepare(rate);
        std::fill(env.begin(), env.end(), 0.0f);
        f.process(shut.data(), nullptr, env.data(), 4800, pcfSettings(s));
        f.reset();
        std::fill(env.begin(), env.end(), 1.0f);
        f.process(open.data(), nullptr, env.data(), 4800, pcfSettings(s));

        check(rmsOf(shut, 960) < 0.01f && rmsOf(open, 960) > 0.25f, "the PCF opens as its envelope is struck",
              fmt("rms %.4f shut, %.3f open", rmsOf(shut, 960), rmsOf(open, 960)));
    }

    // DELAY: a click comes back the time later, and again, quieter
    {
        StepDelay d;
        d.prepare(rate, 1.0);
        std::vector<float> in(9000, 0.0f), l(9000, 0.0f), r(9000, 0.0f);
        in[0] = 1.0f;
        d.process(in.data(), l.data(), r.data(), 9000, 3000.0, 0.5f, 0.0f);

        int first = -1;
        for (int i = 0; i < 9000 && first < 0; ++i) if (std::fabs(l[(size_t) i]) > 0.05f) first = i;
        float second = 0;
        for (int i = 5900; i < 6100; ++i) second = std::fmax(second, std::fabs(l[(size_t) i]));
        check(first >= 3000 && first <= 3002, "the DELAY's echo comes back on time", fmt("at sample %d of 3000", first));
        check(second > 0.02f && second < peakOf(l), "and again, quieter, with feedback", fmt("%.3f", second));
    }

    // COMP: a loud tone pulled down; a quiet one let be (and lifted by the make-up)
    {
        Compressor c;
        c.prepare(rate);
        std::vector<float> loud(9600), quiet(9600);
        sine(loud, 300.0f, 1.0f);
        sine(quiet, 300.0f, 0.02f);
        c.process(loud.data(), nullptr, 9600, -20.0f, 20.0f);
        c.prepare(rate);
        c.process(quiet.data(), nullptr, 9600, -20.0f, 20.0f);
        check(peakOf(loud, 4800) < 0.5f, "the COMP pulls a loud tone down", fmt("peak %.2f from 1.00", peakOf(loud, 4800)));
        check(peakOf(quiet, 4800) > 0.02f, "and leaves a quiet one standing", fmt("peak %.3f from 0.020", peakOf(quiet, 4800)));
    }

    // In the rack: the 808 through every insert and the delay, stopped, and its echo still
    // rings on after
    {
        Rack r;
        r.prepare(48000, 480);
        r.tempo = 125;
        r.mz808.track.patterns[0].set(tr808::Voice::BD, 0, true);
        auto& ch = r.channel[Rack::M808];
        ch.dist = true;
        ch.pcf = true;
        ch.comp = true;
        ch.delay = 1.0f;
        r.fx.set(r.fx.indexOf("compMaster"), 1.0f);
        r.fx.set(r.fx.indexOf("delaySteps"), 8.0f);
        r.fx.set(r.fx.indexOf("delayFeedback"), 0.6f);

        r.playing = true;
        bool finite = true;
        float peak = 0;
        play(r, 12 * 4, 480, [&] (int, auto& l, auto& rr)
        {
            for (size_t i = 0; i < l.size(); ++i)
            {
                finite &= std::isfinite(l[i]) && std::isfinite(rr[i]);
                peak = std::fmax(peak, std::fabs(l[i]));
            }
        });
        check(r.pcfStep() == 3, "the PCF follows the steps", fmt("on step %d", r.pcfStep() + 1));

        r.playing = false;
        float echo = 0;
        // the kick was at 0; eight steps on (96 blocks) it comes back, with the rack stopped
        play(r, 12 * 4 + 8, 480, [] (int, auto&, auto&) {});
        play(r, 12, 480, [&] (int, auto& l, auto&) { for (float x : l) echo = std::fmax(echo, std::fabs(x)); });
        check(finite && peak > 0.05f && peak < 4.0f, "through every effect, the 808 stays sane", fmt("peak %.2f", peak));
        check(echo > 0.01f, "and its echo rings on after STOP", fmt("peak %.3f", echo));
    }
}

void grooves()
{
    std::printf("\nthe factory grooves\n");

    Rack r;
    r.prepare(48000, 512);

    const auto problems = writeGrooves(r);
    check(problems.empty(), "every factory groove reads", problems.empty() ? std::string() : problems.front());
    for (size_t i = 1; i < problems.size() && i < 8; ++i) std::printf("        %s\n", problems[i].c_str());

    // every slot has a part for the bass line and for one drum machine at least
    int empty = 0;
    for (int i = 0; i < patternsPerTrack; ++i)
    {
        bool bass = false, beat = false;
        for (int s = 0; s < stepsPerPattern; ++s) bass |= r.mz303.track.patterns[i].get(s).gate;
        for (int v = 0; v < DrumPattern::voices; ++v)
            beat |= ! r.mz808.track.patterns[i].rowIsEmpty(v) || ! r.mz909.track.patterns[i].rowIsEmpty(v);
        empty += bass && beat ? 0 : 1;
    }
    check(empty == 0, "all 32 have a bass line and a beat", fmt("%d without", empty));

#ifdef MZ950_DISKS
    std::string error;
    if (r.mz950.loadDisk(std::string(MZ950_DISKS) + "/DRUMS.hfe", error))
    {
        const auto sampler = writeSamplerGrooves(r.mz950);
        check(sampler.empty(), "and the 950's find their sounds on the drum disk", sampler.empty() ? std::string() : sampler.front());
    }
    else
    {
        skip("the 950's grooves: no drum disk");
    }
#endif

    // a bar of each, everything playing at once, stays sane
    r.mz950.service();
    float peak = 0;
    bool finite = true;
    std::vector<float> l(512), rr(512);
    for (int i = 0; i < patternsPerTrack; ++i)
    {
        for (int m = 0; m < Rack::channels; ++m) r.track(m).selected = i;
        r.playing = false;
        r.render(l.data(), rr.data(), 512);
        r.playing = true;
        for (int b = 0; b < 48000 * 60 / 128 * 4 / 512; ++b)
        {
            r.render(l.data(), rr.data(), 512);
            for (size_t k = 0; k < l.size(); ++k)
            {
                finite &= std::isfinite(l[k]) && std::isfinite(rr[k]);
                peak = std::fmax(peak, std::fmax(std::fabs(l[k]), std::fabs(rr[k])));
            }
        }
    }
    check(finite && peak > 0.1f && peak < 4.0f, "a bar of each, all five machines playing, stays sane", fmt("peak %.2f", peak));
}

void keys()
{
    std::printf("\nkeys\n");

    // the Camelot wheel, as Mixed In Key names it
    const Key a8 { 8, false }, b8 { 8, true }, a5 { 5, false }, b1 { 1, true }, a12 { 12, false };
    check(a8.name() == "A minor" && b8.name() == "C major" && a5.name() == "C minor"
          && b1.name() == "B major" && a12.name() == "Db minor",
          "the wheel's codes name the right keys", "8A " + a8.name() + ", 8B " + b8.name() + ", 12A " + a12.name());

    const auto mixes = Key { 12, false }.compatible();
    check(mixes.size() == 4 && mixes[1].code() == "11A" && mixes[2].code() == "1A" && mixes[3].code() == "12B",
          "12A mixes with 11A, 1A and 12B", mixes[1].code() + " " + mixes[2].code() + " " + mixes[3].code());

    // the factory banks' keys, found from their notes
    Rack r;
    writeGrooves(r);
    std::string found;
    int right = 0;
    for (int b = 0; b < banks; ++b)
    {
        const Key k = detectKey(r, b);
        found += std::string(1, (char) ('A' + b)) + " " + k.code() + "  ";
        right += k == factoryKey(b) ? 1 : 0;
    }
    check(right == banks, "every factory bank's key is found from its notes", found);

    // C minor into C major, degree for degree: E flat becomes E, C stays C
    auto pitchesOf = [] (Track<BassPattern>& t, int root, int index)
    {
        std::vector<int> v;
        const auto& p = t.patterns[index];
        for (int s = 0; s < stepsPerPattern; ++s)
            if (const BassStep st = p.get(s); st.gate) v.push_back(root + st.note + 12 * st.octave);
        return v;
    };
    const auto before = pitchesOf(r.mz303.track, r.mz303.rootNote, 0);
    transposeBank(r, 0, a5, Key { 8, true });
    const auto after = pitchesOf(r.mz303.track, r.mz303.rootNote, 0);
    bool degreeForDegree = before.size() == after.size();
    for (size_t i = 0; i < before.size() && degreeForDegree; ++i)
    {
        const int pc = before[i] % 12, moved = after[i] - before[i];
        degreeForDegree &= (pc == 3 || pc == 8 || pc == 10) ? moved == 1 : moved == 0;   // Eb, Ab, Bb up a semitone
    }
    check(degreeForDegree && r.bankKey[0] == Key { 8, true }.index(), "5A into 8B keeps the tonic and raises the third");

    // and into A minor, every note down three semitones
    Rack r2;
    writeGrooves(r2);
    transposeBank(r2, 0, a5, a8);
    const auto down = pitchesOf(r2.mz303.track, r2.mz303.rootNote, 0);
    bool allDown = down.size() == before.size();
    for (size_t i = 0; i < down.size() && allDown; ++i) allDown &= before[i] - down[i] == 3 || before[i] - down[i] == -9;
    check(allDown, "5A into 8A moves every note down a minor third");

    // GENERATE, in every key and every style: every note of the bass and lead in the key,
    // something in every groove, and the bank marked with its key
    int outside = 0, silent = 0, wrongKey = 0;
    for (int i = 0; i < 24; ++i)
        for (int style = 0; style < Style::count; ++style)
        {
            const Key k = Key::fromIndex(i);
            generateBank(r, 2, k, style, (unsigned) (i * 7 + style));
            wrongKey += r.bankKey[2] == k.index() ? 0 : 1;
            for (int n = 0; n < patternsPerBank; ++n)
            {
                const int index = 2 * patternsPerBank + n;
                int notes = 0;
                auto look = [&] (const std::vector<int>& pitches)
                {
                    for (int p : pitches) { ++notes; outside += k.contains(p % 12) ? 0 : 1; }
                };
                look(pitchesOf(r.mz303.track, r.mz303.rootNote, index));
                look(pitchesOf(r.mz101.track, r.mz101.rootNote, index));
                silent += notes == 0 ? 1 : 0;
            }
        }
    check(outside == 0, "GENERATE writes only the key's notes, in all 24 keys and 4 styles", fmt("%d outside", outside));
    check(silent == 0 && wrongKey == 0, "and every pattern has a part, the bank its key", fmt("%d silent", silent));

    // the same seed, the same bank
    Rack x, y;
    generateBank(x, 1, a8, Style::Acid, 1234);
    generateBank(y, 1, a8, Style::Acid, 1234);
    bool same = true;
    for (int s = 0; s < stepsPerPattern; ++s)
        same &= x.mz303.track.patterns[8].steps[s].load() == y.mz303.track.patterns[8].steps[s].load();
    check(same, "the same seed writes the same bank");
}

//============================================================================== the song

namespace
{
    void publish(Rack& r, const Song& s) { r.publish(std::make_unique<Playback>(compile(s))); }

    Note note(int tick, int length, int pitch, int velocity = 100)
    {
        Note n;
        n.tick = tick; n.length = length; n.pitch = pitch; n.velocity = velocity;
        return n;
    }

    Part part(int start, int length, std::vector<Note> notes)
    {
        Part p;
        p.start = start; p.length = length; p.notes = std::move(notes);
        return p;
    }

    const int bd808 = tr808::voiceInfo(tr808::Voice::BD).note;
    const int ch808 = tr808::voiceInfo(tr808::Voice::CH).note;
}

void song()
{
    std::printf("\nthe song\n");

    // 125 bpm at 48 kHz: 5760 samples a step, 92160 a bar; blocks of 480, twelve a step
    auto rack = [] (Rack& r)
    {
        r.prepare(48000, 480);
        r.tempo = 125;
        r.mode = Mode::Song;
    };

    // a note lands on its sample
    {
        Rack r;
        rack(r);
        Song s = defaultSong();
        s.tracks[Rack::M808].parts.push_back(part(0, ticksPerBar, { note(2 * ticksPerStep, 24, bd808) }));
        publish(r, s);
        r.playing = true;

        uint32_t before = 0, after = 0;
        play(r, 25, 480, [&] (int k, auto&, auto&)
        {
            if (k == 23) before = r.mz808.view().hitCount(tr808::Voice::BD);
            if (k == 24) after = r.mz808.view().hitCount(tr808::Voice::BD);
        });
        check(before == 0 && after == 1, "a song's note lands on its sample", fmt("before %u, after %u", before, after));
    }

    // PLAY starts from the cursor
    {
        Rack r;
        rack(r);
        Song s = defaultSong();
        s.cursor = ticksPerBar;
        s.tracks[Rack::M808].parts.push_back(part(0, 2 * ticksPerBar, { note(0, 24, bd808), note(ticksPerBar, 24, ch808) }));
        publish(r, s);
        r.playing = true;
        play(r, 1, 480, [] (int, auto&, auto&) {});
        check(r.mz808.view().hitCount(tr808::Voice::CH) == 1 && r.mz808.view().hitCount(tr808::Voice::BD) == 0,
              "PLAY starts from the cursor", fmt("at tick %.0f", r.songPosition()));
    }

    // Every factory groove, made into parts and played as a song, sounds as its patterns do:
    // the same notes on the same samples, slides, accents and shuffle and all.
    {
        double worst = 0;
        int worstGroove = -1;
        constexpr int steps = 32;

        for (int g = 0; g < patternsPerTrack; ++g)
        {
            Rack a, b;
            for (Rack* r : { &a, &b })
            {
                r->prepare(48000, 480);
                writeGrooves(*r);
                r->shuffle = 0.6f;
                for (int m = 0; m < Rack::channels; ++m) r->track(m).selected = g;
            }
            a.mode = Mode::Pattern;
            b.mode = Mode::Song;

            Song s = defaultSong();
            for (int m : { Rack::M303, Rack::M101, Rack::M808, Rack::M909 })
            {
                const int len = b.track(m).pattern(g).len();
                s.tracks[(size_t) m].parts.push_back(partFromPattern(b, m, g, steps / len + 2));
                s.tracks[(size_t) m].shuffle = b.track(m).shuffle.load();
            }
            publish(b, s);

            a.playing = true;
            b.playing = true;
            const int blocks = (int) std::lround(steps * 48000.0 * 15.0 / a.tempo.load() / 480.0);
            std::vector<float> al(480), ar(480), bl(480), br(480);
            double diff = 0;
            for (int k = 0; k < blocks; ++k)
            {
                a.render(al.data(), ar.data(), 480);
                b.render(bl.data(), br.data(), 480);
                for (int i = 0; i < 480; ++i)
                    diff = std::max(diff, (double) std::max(std::fabs(al[(size_t) i] - bl[(size_t) i]),
                                                            std::fabs(ar[(size_t) i] - br[(size_t) i])));
            }
            if (diff > worst) { worst = diff; worstGroove = g; }
        }
        check(worst < 1.0e-5, "every factory groove as a song sounds as its patterns",
              worstGroove < 0 ? "identical" : fmt("worst %.2g, on %c%d", worst, 'A' + worstGroove / 8, worstGroove % 8 + 1));
    }

    // and the 950's, its ties and chords and all, on the drum disk
#ifdef MZ950_DISKS
    {
        double worst = 0;
        bool loaded = true;
        constexpr int steps = 32;

        for (int g = 0; g < patternsPerBank && loaded; ++g)
        {
            Rack a, b;
            for (Rack* r : { &a, &b })
            {
                std::string error;
                loaded &= r->mz950.loadDisk(std::string(MZ950_DISKS) + "/DRUMS.hfe", error);
                r->prepare(48000, 480);
                writeGrooves(*r);
                writeSamplerGrooves(r->mz950);
                r->mz950.service();
                r->shuffle = 0.6f;
                for (int m = 0; m < Rack::channels; ++m)
                {
                    r->track(m).selected = g;
                    r->channel[m].mute = m != Rack::M950;
                }
            }
            a.mode = Mode::Pattern;
            b.mode = Mode::Song;

            Song s = defaultSong();
            const int len = b.track(Rack::M950).pattern(g).len();
            s.tracks[Rack::M950].parts.push_back(partFromPattern(b, Rack::M950, g, steps / len + 2));
            s.tracks[Rack::M950].shuffle = b.track(Rack::M950).shuffle.load();
            publish(b, s);

            a.playing = b.playing = true;
            std::vector<float> al(480), ar(480), bl(480), br(480);
            const int blocks = (int) std::lround(steps * 48000.0 * 15.0 / a.tempo.load() / 480.0);
            for (int k = 0; k < blocks; ++k)
            {
                a.render(al.data(), ar.data(), 480);
                b.render(bl.data(), br.data(), 480);
                for (int i = 0; i < 480; ++i)
                    worst = std::max(worst, (double) std::max(std::fabs(al[(size_t) i] - bl[(size_t) i]),
                                                              std::fabs(ar[(size_t) i] - br[(size_t) i])));
            }
        }
        if (loaded) check(worst < 1.0e-5, "and the 950's, ties and all", fmt("worst %.2g", worst));
        else        skip("the 950's grooves as a song: no drum disk");
    }
#endif

    // the cycle goes round, and lets go of what was sounding
    {
        Rack r;
        rack(r);
        Song s = defaultSong();
        s.left = 0;
        s.right = ticksPerBar;
        s.cycle = true;
        s.tracks[Rack::M808].parts.push_back(part(0, ticksPerBar, { note(0, 24, bd808) }));
        s.tracks[Rack::M303].parts.push_back(part(0, 2 * ticksPerBar, { note(ticksPerBar / 2, ticksPerBar, 36) }));
        publish(r, s);
        r.playing = true;

        bool gateAfterJump = true, gateMidBar = false;
        const int bar = 16 * 12;   // blocks
        play(r, bar * 3 + bar / 2, 480, [&] (int k, auto&, auto&)
        {
            if (k == bar + 1) gateAfterJump = r.mz303.view().gateIsOn();
            if (k == bar + bar * 3 / 4) gateMidBar = r.mz303.view().gateIsOn();
        });
        check(r.mz808.view().hitCount(tr808::Voice::BD) == 4, "the cycle goes round, three and a half times",
              fmt("%u kicks", r.mz808.view().hitCount(tr808::Voice::BD)));
        check(! gateAfterJump && gateMidBar, "and lets go of a note held over its end");
        check(r.songPosition() >= 0 && r.songPosition() < ticksPerBar, "and stays inside the locators",
              fmt("at tick %.0f", r.songPosition()));
    }

    // a new song swapped in while it plays drops no note and strikes none twice; a note
    // edited away while it sounds still ends
    {
        Rack r;
        rack(r);
        Song s = defaultSong();
        std::vector<Note> hats;
        for (int i = 0; i < 32; ++i) hats.push_back(note(i * ticksPerStep, 24, ch808));
        s.tracks[Rack::M808].parts.push_back(part(0, 2 * ticksPerBar, hats));
        s.tracks[Rack::M303].parts.push_back(part(0, 2 * ticksPerBar, { note(0, ticksPerBar, 40) }));
        publish(r, s);
        r.playing = true;

        Song edited = s;
        edited.tracks[Rack::M303].parts.clear();
        bool heldOn = false, letGo = false;
        play(r, 2 * 16 * 12, 480, [&] (int k, auto&, auto&)
        {
            r.collect();
            if (k % 7 == 3) publish(r, k > 48 ? edited : s);
            if (k == 100) heldOn = r.mz303.view().gateIsOn();
            if (k == 16 * 12 + 2) letGo = ! r.mz303.view().gateIsOn();
        });
        check(r.mz808.view().hitCount(tr808::Voice::CH) == 32, "a song swapped in while playing drops and doubles nothing",
              fmt("%u of 32 hats", r.mz808.view().hitCount(tr808::Voice::CH)));
        check(heldOn && letGo, "a note edited away while it sounds still ends");

        r.playing = false;
        play(r, 1, 480, [] (int, auto&, auto&) {});
        Song held = defaultSong();
        held.tracks[Rack::M303].parts.push_back(part(0, 4 * ticksPerBar, { note(0, 4 * ticksPerBar, 40) }));
        r.collect();
        publish(r, held);
        r.playing = true;
        play(r, 10, 480, [&] (int, auto&, auto&) { r.collect(); });
        const bool sounding = r.mz303.view().gateIsOn();
        r.playing = false;
        play(r, 1, 480, [] (int, auto&, auto&) {});
        check(sounding && ! r.mz303.view().gateIsOn(), "STOP lets go of the song's notes");
    }

    // a change of mode waits for the bar
    {
        Rack r;
        rack(r);
        r.mode = Mode::Pattern;
        publish(r, defaultSong());
        r.playing = true;
        play(r, 12 * 5, 480, [] (int, auto&, auto&) {});
        r.mode = Mode::Song;
        int switchedAt = -1;
        play(r, 12 * 16, 480, [&] (int k, auto&, auto&) { if (switchedAt < 0 && r.playingNow() == Mode::Song) switchedAt = k; });
        // five steps were played; the bar is eleven steps (132 blocks) on: the block that starts it
        check(switchedAt == 132, "SONG mode, picked mid-bar, starts on the next", fmt("on block %d", switchedAt));
    }

    // undo and redo
    {
        SongDocument d;
        int changes = 0;
        d.onChange = [&] (const Song&) { ++changes; };
        d.edit([] (Song& s) { s.tracks[0].parts.push_back(Part {}); });
        d.edit([] (Song& s) { s.tracks[0].parts[0].start = ticksPerBar; });
        d.adjust([] (Song& s) { s.cursor = 96; });
        const bool undone = d.undo() && d.song().tracks[0].parts[0].start == 0;
        const bool again = d.undo() && d.song().tracks[0].parts.empty() && ! d.canUndo();
        const bool redone = d.redo() && d.redo() && d.song().tracks[0].parts[0].start == ticksPerBar && ! d.canRedo();
        check(undone && again && redone && changes == 7, "undo and redo walk the edits back and forth", fmt("%d changes", changes));
    }
}

//
// The demo song a new rack opens with, played through: it must stay sane, and have the shape
// of a rave tune - the drops louder than the intro and the breakdown, and a beat of near
// silence before each drop lands.
//
void demo()
{
    std::printf("\nthe demo song\n");

    Rack r;
#ifdef MZ950_DISKS
    std::string error;
    const bool kit = r.mz950.loadDisk(std::string(MZ950_DISKS) + "/DRUMS.hfe", error);
#else
    const bool kit = false;
#endif
    r.prepare(48000, 512);
    writeGrooves(r);
    if (kit) { r.mz950.selectProgram(0); writeSamplerGrooves(r.mz950); r.mz950.service(); }
    r.tempo = demoTempo;
    r.mode = Mode::Song;

    const Song s = demoSong(r);
    int empty = 0;
    for (const auto& t : s.tracks) empty += t.parts.empty() && (t.machine != Rack::M950 || kit) ? 1 : 0;
    check(empty == 0 && s.end() == 64 * ticksPerBar, "sixty-four bars, every track playing",
          fmt("%zu tracks, %d empty, %d bars", s.tracks.size(), empty, s.end() / ticksPerBar));

    publish(r, s);
    r.playing = true;

    // the level of every beat, as RMS
    const double perBeat = 48000.0 * 60.0 / demoTempo;
    const int beats = 64 * 4;
    std::vector<double> energy((size_t) beats, 0.0), count((size_t) beats, 0.0);
    std::vector<float> l(512), rr(512);
    bool finite = true;
    float peak = 0;
    for (int64_t done = 0; done < (int64_t) (perBeat * beats); done += 512)
    {
        r.render(l.data(), rr.data(), 512);
        for (int i = 0; i < 512; ++i)
        {
            const int b = (int) ((double) (done + i) / perBeat);
            if (b >= beats) break;
            finite &= std::isfinite(l[(size_t) i]) && std::isfinite(rr[(size_t) i]);
            peak = std::fmax(peak, std::fmax(std::fabs(l[(size_t) i]), std::fabs(rr[(size_t) i])));
            energy[(size_t) b] += (double) l[(size_t) i] * l[(size_t) i] + (double) rr[(size_t) i] * rr[(size_t) i];
            count[(size_t) b] += 2.0;
        }
    }
    for (size_t b = 0; b < energy.size(); ++b) energy[b] = std::sqrt(energy[b] / std::max(1.0, count[b]));

    // the mean level over bars [from, to)
    auto level = [&] (int from, int to)
    {
        double sum = 0;
        for (int b = from * 4; b < to * 4; ++b) sum += energy[(size_t) b];
        return sum / (double) ((to - from) * 4);
    };

    std::printf("      bars:");
    for (int b = 0; b < 64; b += 4) std::printf(" %5d", b + 1);
    std::printf("\n     level:");
    for (int b = 0; b < 64; b += 4) std::printf(" %5.3f", level(b, b + 4));
    std::printf("\n");

    const double intro = level(0, 4), drop1 = level(28, 36), breakdown = level(36, 40), drop2 = level(48, 60);
    check(finite && peak < 4.0f, "two minutes of it stay finite and sane", fmt("peak %.2f", peak));
    check(drop1 > intro * 1.5 && drop2 > intro * 1.5, "the drops are louder than the intro",
          fmt("%.3f and %.3f against %.3f", drop1, drop2, intro));
    check(drop1 > breakdown * 1.5 && drop2 > breakdown * 1.5, "and than the breakdown",
          fmt("%.3f and %.3f against %.3f", drop1, drop2, breakdown));

    // the beat before each drop: the builds cut out, and only the echoes ring on
    const double gap1 = energy[28 * 4 - 1], gap2 = energy[48 * 4 - 1];
    const double before1 = energy[28 * 4 - 2], before2 = energy[48 * 4 - 2];
    check(gap1 < before1 * 0.5 && gap2 < before2 * 0.5, "a beat of near silence before each drop",
          fmt("%.3f after %.3f, %.3f after %.3f", gap1, before1, gap2, before2));
}

int main()
{
    demo();
    song();
    keys();
    grooves();
    effects();
    timing();
    bassLine();
    drums();
    others();
    output();

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : fmt("%d FAILED", failures).c_str());
    return failures == 0 ? 0 : 1;
}
