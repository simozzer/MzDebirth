// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.
//
// A probe, not a check: the factory rack's kicks, alone, measured - how a hit's level falls
// over time, how much of it is low end, and what the channel's COMP does to both.

#include "Core/Bounce.h"
#include "Core/DemoSong.h"
#include "Core/Grooves.h"
#include "Core/Rack.h"

#include <cmath>
#include <functional>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace debirth;

namespace
{
    constexpr double rate = 48000;

    // a second of one machine's kick, four on the floor at 120, from the factory rack
    std::vector<float> render(int machine, bool comp, bool factoryComp)
    {
        Rack r;
        r.prepare(rate, 512);
        writeGrooves(r);
        r.tempo = 120;
        r.shuffle = 0;
        for (int m = 0; m < Rack::channels; ++m) r.channel[m].mute = m != machine;
        r.channel[machine].comp = comp;
        if (! factoryComp) r.channel[Rack::M808].comp = false;

        auto& t = r.track(machine);
        t.clear(0);
        t.selected = 0;
        t.current = 0;
        if (machine == Rack::M808)
            for (int s = 0; s < 16; s += 4) r.mz808.track.patterns[0].set(tr808::Voice::BD, s, true);
        else
            for (int s = 0; s < 16; s += 4) r.mz909.track.patterns[0].set(tr909::Voice::BD, s, true);

        r.playing = true;
        std::vector<float> out, l(512), rr(512);
        while (out.size() < (size_t) (rate * 2))
        {
            r.render(l.data(), rr.data(), 512);
            for (int i = 0; i < 512; ++i) out.push_back(0.5f * (l[(size_t) i] + rr[(size_t) i]));
        }
        return out;
    }

    // a two-pole low-pass, Butterworth
    std::vector<float> lowpass(const std::vector<float>& x, double hz)
    {
        const double w = std::tan(3.14159265358979 * hz / rate), n = 1 / (1 + std::sqrt(2.0) * w + w * w);
        const double b0 = w * w * n, b1 = 2 * b0, a1 = 2 * (w * w - 1) * n, a2 = (1 - std::sqrt(2.0) * w + w * w) * n;
        std::vector<float> y(x.size());
        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const double v = b0 * x[i] + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x[i]; y2 = y1; y1 = v;
            y[i] = (float) v;
        }
        return y;
    }

    double db(double v) { return 20 * std::log10(std::max(v, 1e-9)); }

    // RMS over [from, to) ms of the second beat - the first has the machine starting
    double rms(const std::vector<float>& x, double fromMs, double toMs)
    {
        const size_t beat = (size_t) (rate * 0.5);
        const size_t a = beat + (size_t) (fromMs * rate / 1000), b = beat + (size_t) (toMs * rate / 1000);
        double s = 0;
        for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
        return std::sqrt(s / (double) (b - a));
    }

    double peak(const std::vector<float>& x)
    {
        double p = 0;
        for (size_t i = (size_t) (rate * 0.5); i < (size_t) rate; ++i) p = std::max(p, (double) std::fabs(x[i]));
        return p;
    }

    void report(const char* what, const std::vector<float>& x)
    {
        const auto low = lowpass(x, 120);
        std::printf("%-26s peak %6.1f dB | whole hit, 0-10 / 10-50 / 50-150 / 150-400 ms: %6.1f %6.1f %6.1f %6.1f dB"
                    " | below 120 Hz %6.1f %6.1f %6.1f %6.1f dB | low share %4.0f%%\n",
                    what, db(peak(x)),
                    db(rms(x, 0, 10)), db(rms(x, 10, 50)), db(rms(x, 50, 150)), db(rms(x, 150, 400)),
                    db(rms(low, 0, 10)), db(rms(low, 10, 50)), db(rms(low, 50, 150)), db(rms(low, 150, 400)),
                    100 * std::pow(rms(low, 0, 400) / rms(x, 0, 400), 2));
    }
}

namespace
{
    // the factory rack playing bank A's pattern 1, with only `solo` heard (or all, if -1)
    std::vector<float> groove(int solo, int seconds = 8)
    {
        Rack r;
        r.prepare(rate, 512);
        writeGrooves(r);
        for (int m = 0; m < Rack::channels; ++m) r.channel[m].mute = solo >= 0 && m != solo;
        r.playing = true;
        std::vector<float> out, l(512), rr(512);
        while (out.size() < (size_t) (rate * seconds))
        {
            r.render(l.data(), rr.data(), 512);
            for (int i = 0; i < 512; ++i) out.push_back(0.5f * (l[(size_t) i] + rr[(size_t) i]));
        }
        return out;
    }

    double rmsAll(const std::vector<float>& x)
    {
        double s = 0;
        for (size_t i = (size_t) rate; i < x.size(); ++i) s += (double) x[i] * x[i];
        return std::sqrt(s / (double) (x.size() - (size_t) rate));
    }

    // the kick's pitch, from zero crossings of its low end between 40 and 200 ms
    double pitch(const std::vector<float>& x)
    {
        const auto low = lowpass(x, 200);
        const size_t a = (size_t) (rate * 0.54), b = (size_t) (rate * 0.70);
        int crossings = 0;
        size_t first = 0, last = 0;
        for (size_t i = a + 1; i < b; ++i)
            if (low[i - 1] < 0 && low[i] >= 0) { if (! crossings) first = i; last = i; ++crossings; }
        return crossings > 1 ? (crossings - 1) * rate / (double) (last - first) : 0;
    }
}

namespace
{
    struct Score { double rms, peak, low, kickEarly, kickLate; };

    // the factory groove under a change, all machines; and the 808 alone, for its body
    Score score(const std::function<void(Rack&)>& change)
    {
        auto run = [&] (bool soloKick)
        {
            Rack r;
            r.prepare(rate, 512);
            writeGrooves(r);
            change(r);
            if (soloKick)
            {
                for (int m = 0; m < Rack::channels; ++m) r.channel[m].mute = m != Rack::M808;
                auto& t = r.mz808.track;
                t.clear(0);
                for (int s = 0; s < 16; s += 4) t.patterns[0].set(tr808::Voice::BD, s, true);
                r.tempo = 120;
            }
            r.playing = true;
            std::vector<float> out, l(512), rr(512);
            while (out.size() < (size_t) (rate * 8))
            {
                r.render(l.data(), rr.data(), 512);
                for (int i = 0; i < 512; ++i) out.push_back(0.5f * (l[(size_t) i] + rr[(size_t) i]));
            }
            return out;
        };
        const auto mix = run(false), kick = run(true);
        double p = 0;
        for (float v : mix) p = std::max(p, (double) std::fabs(v));
        const auto low = lowpass(kick, 120);
        return { db(rmsAll(mix)), db(p), db(rmsAll(lowpass(mix, 120))), db(rms(low, 10, 150)), db(rms(low, 150, 400)) };
    }

    void line(const char* what, const Score& s)
    {
        std::printf("  %-44s mix rms %6.1f  peak %5.1f  low end %6.1f (%5.1f under the peak) | kick body 10-150 %6.1f, 150-400 %6.1f\n",
                    what, s.rms, s.peak, s.low, s.peak - s.low, s.kickEarly, s.kickLate);
    }
}

namespace
{
    // A programme chosen on a rack already playing reaches the 950's engine as the app's timer
    // would hand it over: a block rendered, so the engine takes the patch before, then service.
    void settle(Rack& r)
    {
        std::vector<float> l(512), rr(512);
        for (int i = 0; i < 2; ++i) { r.render(l.data(), rr.data(), 512); r.mz950.service(); }
    }

    // the demo song to a 16-bit WAV, the factory rack changed by `change` first - or the
    // ambient one
    void demoWav(const char* path, const std::function<void(Rack&)>& change, bool ambient = false)
    {
        Rack r;
#ifdef MZ950_DISKS
        std::string error;
        const bool kit = ! ambient && r.mz950.loadDisk(std::string(MZ950_DISKS) + "/DRUMS.hfe", error);
        const std::string whalesDisk = std::string(MZ950_DISKS) + "/WHALES.hfe";
#else
        const bool kit = false;
        const std::string whalesDisk;
#endif
        r.prepare(rate, 512);
        writeGrooves(r);
        if (kit) { r.mz950.selectProgram(0); writeSamplerGrooves(r.mz950); r.mz950.service(); }
        bool whales = false;
        if (ambient)
        {
            whales = voiceForAmbient(r, whalesDisk);
            settle(r);
            dressForAmbient(r);
        }
        r.tempo = ambient ? ambientTempo : demoTempo;
        change(r);
        const Song song = ambient ? ambientSong(r, whales) : demoSong(r);

        std::vector<int16_t> pcm;
        bounce(r, song, rate, 3.0, [&] (const float* l, const float* rr, int n, double)
        {
            for (int i = 0; i < n; ++i)
                for (float v : { l[i], rr[i] })
                    pcm.push_back((int16_t) std::lround(std::max(-1.0f, std::min(1.0f, v)) * 32767.0f));
            return true;
        });

        FILE* f = std::fopen(path, "wb");
        if (f == nullptr) return;
        auto u32 = [f] (uint32_t v) { std::fwrite(&v, 4, 1, f); };
        auto u16 = [f] (uint16_t v) { std::fwrite(&v, 2, 1, f); };
        const uint32_t bytes = (uint32_t) (pcm.size() * 2);
        std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
        u32(16); u16(1); u16(2); u32((uint32_t) rate); u32((uint32_t) rate * 4); u16(4); u16(16);
        std::fwrite("data", 1, 4, f); u32(bytes);
        std::fwrite(pcm.data(), 2, pcm.size(), f);
        std::fclose(f);
        std::printf("  wrote %s\n", path);
    }
}

namespace
{
    // each bank's first groove: every machine's level, crest and low end, and the 909's voices
    void everyBank()
    {
        const char* names[] = { "303", "101", "808", "909", "950" };
        for (int bank = 0; bank < 4; ++bank)
        {
            auto run = [&] (int solo, const char* only909)
            {
                Rack r;
                r.prepare(rate, 512);
                writeGrooves(r);
                for (int m = 0; m < Rack::channels; ++m)
                {
                    r.channel[m].mute = solo >= 0 && m != solo;
                    r.track(m).selected = bank * 8;
                    r.track(m).current = bank * 8;
                }
                if (only909 != nullptr)
                    for (int i = 0; i < r.mz909.knobs.size(); ++i)
                    {
                        const std::string id = r.mz909.knobs.control(i).id;
                        const bool level = id.find("Level") != std::string::npos;
                        if (level && id != only909) r.mz909.knobs.set(i, 0);
                    }
                r.playing = true;
                std::vector<float> out, l(512), rr(512);
                while (out.size() < (size_t) (rate * 8))
                {
                    r.render(l.data(), rr.data(), 512);
                    for (int i = 0; i < 512; ++i) out.push_back(0.5f * (l[(size_t) i] + rr[(size_t) i]));
                }
                return out;
            };
            auto peakOf = [] (const std::vector<float>& x) { double p = 0; for (float v : x) p = std::max(p, (double) std::fabs(v)); return p; };

            const auto mix = run(-1, nullptr);
            const double low = rmsAll(lowpass(mix, 120));
            std::printf("\n  bank %c: mix %6.1f dB rms, peak %5.1f\n", 'A' + bank, db(rmsAll(mix)), db(peakOf(mix)));
            for (int m = 0; m < Rack::channels; ++m)
            {
                const auto x = run(m, nullptr);
                if (rmsAll(x) < 1e-6) continue;
                std::printf("    %s  %6.1f dB rms (%5.1f under the mix)  crest %4.1f  low end %3.0f%%\n", names[m], db(rmsAll(x)),
                            db(rmsAll(mix)) - db(rmsAll(x)), db(peakOf(x)) - db(rmsAll(x)),
                            100 * std::pow(rmsAll(lowpass(x, 120)) / low, 2));
            }
            for (const char* v : { "bdLevel", "sdLevel", "cpLevel", "hhLevel", "rsLevel", "crLevel", "rdLevel", "ltLevel" })
            {
                const auto x = run(Rack::M909, v);
                if (rmsAll(x) > 1e-5)
                    std::printf("      909 %-8s %6.1f dB rms, peak %5.1f\n", v, db(rmsAll(x)), db(peakOf(x)));
            }
        }
    }
}

int main(int argc, char** argv)
{
    if (argc > 1 && std::string(argv[1]) == "--banks") { everyBank(); return 0; }
    if (argc > 2 && std::string(argv[1]) == "--ambient") { demoWav(argv[2], [] (Rack&) {}, true); return 0; }
    if (argc > 1)
    {
        const std::string folder = argv[1];
        demoWav((folder + "/demo-new.wav").c_str(), [] (Rack&) {});
        demoWav((folder + "/demo-old.wav").c_str(), [] (Rack& r)
        {
            // as the factory rack was before: the 808 through the COMP alone, lower faders
            r.channel[Rack::M808].dist = false;
            const float levels[] = { 0.7f, 0.55f, 0.7f, 0.55f, 0.7f };
            for (int m = 0; m < Rack::channels; ++m) r.channel[m].level = levels[m];
            r.master = 0.8f;
        });
        return 0;
    }

    std::printf("\nways to give the kick more weight, scored on the factory groove\n\n");
    auto fx = [] (Rack& r, const char* id, float v) { r.fx.set(r.fx.indexOf(id), v); };
    line("factory", score([] (Rack&) {}));
    line("808 COMP off", score([] (Rack& r) { r.channel[Rack::M808].comp = false; }));
    line("808 through DIST too (soft, factory drive)", score([] (Rack& r) { r.channel[Rack::M808].dist = true; }));
    line("808 DIST, COMP off", score([] (Rack& r) { r.channel[Rack::M808].dist = true; r.channel[Rack::M808].comp = false; }));
    line("MASTER up 6 dB into LIMIT", score([] (Rack& r) { r.master = r.master.load() * 1.4125f; }));
    line("MASTER up 9 dB into LIMIT", score([] (Rack& r) { r.master = r.master.load() * 1.679f; }));
    line("COMP on the master too", score([&] (Rack& r) { fx(r, "compMaster", 1); }));
    line("808 DIST, MASTER 1.0", score([] (Rack& r) { r.channel[Rack::M808].dist = true; r.master = 1.0f; }));
    line("808 DIST, MASTER 1.0, faders +3 dB", score([] (Rack& r)
    {
        r.channel[Rack::M808].dist = true; r.master = 1.0f;
        for (auto& ch : r.channel) ch.level = std::min(1.0f, ch.level.load() * 1.1885f);
    }));
    line("808 DIST, MASTER 1.0, faders +5 dB", score([] (Rack& r)
    {
        r.channel[Rack::M808].dist = true; r.master = 1.0f;
        for (auto& ch : r.channel) ch.level = std::min(1.0f, ch.level.load() * 1.3335f);
    }));
    {
        Rack r; writeGrooves(r);
        std::printf("\n  factory faders:");
        for (auto& ch : r.channel) std::printf(" %.2f", ch.level.load());
        std::printf("   MASTER %.2f   DIST amount %.2f shape %.2f\n", r.master.load(),
                    r.fx.get(r.fx.indexOf("distAmount")), r.fx.get(r.fx.indexOf("distShape")));
    }

    std::printf("\nbank A pattern 1, the factory mix: each machine's level, and its share below 120 Hz\n\n");
    const char* names[] = { "303", "101", "808", "909", "950" };
    const auto all = groove(-1);
    const double allLow = rmsAll(lowpass(all, 120));
    double mixPeak = 0;
    for (float v : all) mixPeak = std::max(mixPeak, (double) std::fabs(v));
    std::printf("  the mix       %6.1f dB   below 120 Hz %6.1f dB   peak %5.1f dB, so %4.1f dB of headroom unused\n",
                db(rmsAll(all)), db(allLow), db(mixPeak), -db(mixPeak));
    for (int m = 0; m < Rack::channels; ++m)
    {
        const auto x = groove(m);
        const double low = rmsAll(lowpass(x, 120));
        double p = 0;
        for (float v : x) p = std::max(p, (double) std::fabs(v));
        std::printf("  %s alone     %6.1f dB   below 120 Hz %6.1f dB   %3.0f%% of the mix's low end   peak %5.1f dB, crest %4.1f dB\n",
                    names[m], db(rmsAll(x)), db(low), 100 * std::pow(low / allLow, 2), db(p), db(p) - db(rmsAll(x)));
    }
    // the 808 in the groove, one voice at a time: which makes the peak
    {
        const char* voices[] = { "bdLevel", "sdLevel", "ltLevel", "mtLevel", "htLevel", "rsLevel",
                                 "cpLevel", "cbLevel", "cyLevel", "ohLevel", "chLevel" };
        std::printf("\n  the 808 in the groove, one voice at a time:\n");
        for (const char* only : voices)
        {
            Rack r;
            r.prepare(rate, 512);
            writeGrooves(r);
            for (int m = 0; m < Rack::channels; ++m) r.channel[m].mute = m != Rack::M808;
            const float kept = r.mz808.knobs.get(r.mz808.knobs.indexOf(only));
            for (const char* v : voices) r.mz808.knobs.set(r.mz808.knobs.indexOf(v), 0);
            r.mz808.knobs.set(r.mz808.knobs.indexOf(only), kept);
            r.playing = true;
            double p = 0, s = 0; size_t count = 0;
            std::vector<float> l(512), rr(512);
            for (int b = 0; b < (int) (rate * 8 / 512); ++b)
            {
                r.render(l.data(), rr.data(), 512);
                for (int i = 0; i < 512; ++i)
                {
                    const double v = 0.5 * (l[(size_t) i] + rr[(size_t) i]);
                    p = std::max(p, std::fabs(v)); s += v * v; ++count;
                }
            }
            if (p > 1e-6)
                std::printf("    %-8s level %4.1f   peak %6.1f dB   rms %6.1f dB\n", only, kept, db(p), db(std::sqrt(s / count)));
        }
    }

    std::printf("\n  808 kick pitch %.0f Hz, 909 kick pitch %.0f Hz\n",
                pitch(render(Rack::M808, false, false)), pitch(render(Rack::M909, false, false)));

    std::printf("\nthe factory rack's kicks, alone, at their factory levels\n\n");
    report("808, COMP on (factory)", render(Rack::M808, true, true));
    report("808, COMP off", render(Rack::M808, false, false));
    report("909, COMP off (factory)", render(Rack::M909, false, false));
    report("909, COMP on", render(Rack::M909, true, false));
    std::printf("\n");
    return 0;
}

