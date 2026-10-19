// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "DemoSong.h"

#include "Rack.h"

#include <algorithm>

namespace debirth
{

namespace
{
    constexpr int bar = ticksPerBar, beat = ticksPerQuarter, step = ticksPerStep;

    // bank A's patterns, by their number on the pads
    constexpr int A1 = 0, A2 = 1, A3 = 2, A4 = 3, A5 = 4, A6 = 5, A7 = 6, A8 = 7;

    // the lines it is written in - merged at the end into a track a machine
    enum { Beat, Tops, Fx, Acid, Lead, Riser, Perc, trackCount };

    // C minor, for the riser to climb
    const int scale[] = { 0, 2, 3, 5, 7, 8, 10 };

    Note note(int tick, int length, int pitch, int velocity)
    {
        Note n;
        n.tick = tick;
        n.length = length;
        n.pitch = pitch;
        n.velocity = velocity;
        return n;
    }

    struct Writer
    {
        Rack& rack;
        Song& song;

        //
        // A pattern from the rack as a part, from bar `from` (counting from 0) for `bars`
        // bars, cut short by `trim` ticks - the beat of silence before a drop - and moved by
        // `transpose` semitones.
        //
        void pattern(int track, int p, int from, int bars, int trim = 0, int transpose = 0)
        {
            const int machine = song.tracks[(size_t) track].machine;
            const int len = rack.track(machine).pattern(p).len();
            Part part = partFromPattern(rack, machine, p, (bars * 16 + len - 1) / len);
            if (part.notes.empty()) return;   // the 950's, with no kit to play them on

            part.start = from * bar;
            part.length = bars * bar - trim;
            for (auto& n : part.notes) n.pitch = std::clamp(n.pitch + transpose, 0, 127);
            if (transpose != 0) part.name += transpose > 0 ? "+" : "-";
            song.tracks[(size_t) track].parts.push_back(part);
        }

        void part(int track, const char* name, int from, int length, std::vector<Note> notes)
        {
            Part p;
            p.name = name;
            p.start = from * bar;
            p.length = length;
            p.notes = std::move(notes);
            p.sortNotes();
            song.tracks[(size_t) track].parts.push_back(p);
        }
    };

    int drum909(int voice) { return tr909::voiceInfo(voice).note; }
    int drum808(int voice) { return tr808::voiceInfo(voice).note; }

    //
    // A snare roll over four bars, each bar twice as fast as the last - `from` hits a bar
    // to start with - getting louder: accented from the third bar. The claps join it on
    // the last.
    //
    std::vector<Note> snareRoll(int from)
    {
        std::vector<Note> notes;
        for (int b = 0, hits = from; b < 4; ++b, hits = std::min(hits * 2, 32))
        {
            const int every = bar / hits;
            for (int i = 0; i < hits; ++i)
            {
                const int t = b * bar + i * every;
                notes.push_back(note(t, step / 2, drum909(tr909::Voice::SD), b >= 2 ? 127 : 100));
                if (b == 3 && i % 2 == 0) notes.push_back(note(t, step / 2, drum909(tr909::Voice::CP), 127));
            }
        }
        return notes;
    }

    //
    // The long roll, eight bars less a beat: steady sixteenths for six bars, then
    // thirty-seconds, accented on the beat, the claps joining for the last bar. It builds by
    // its volume - the 909's SD LEVEL, turned up the whole way by the song's automation.
    //
    std::vector<Note> longRoll()
    {
        std::vector<Note> notes;
        for (int b = 0; b < 8; ++b)
        {
            const int hits = b < 6 ? 16 : 32, every = bar / hits;
            for (int i = 0; i < hits; ++i)
            {
                const int t = b * bar + i * every;
                if (t >= 8 * bar - beat) break;
                notes.push_back(note(t, step / 2, drum909(tr909::Voice::SD), i % (hits / 4) == 0 ? 127 : 100));
                if (b == 7 && i % 2 == 0) notes.push_back(note(t, step / 2, drum909(tr909::Voice::CP), 127));
            }
        }
        return notes;
    }

    //
    // The song's automation, as a hand on the knobs would have it: a control turned through
    // runs of points, held at its setting in the rack before each and put back after.
    //
    struct Automator
    {
        Rack& rack;
        Song& song;

        float setting(int owner, const char* id) const
        {
            const int i = rack.paramIndex(owner, id);
            return i < 0 ? 0.0f : rack.paramGet(owner, i);
        }

        void turn(int owner, const char* id, const std::vector<std::vector<AutoPoint>>& runs)
        {
            if (rack.paramIndex(owner, id) < 0) return;
            const float normal = setting(owner, id);
            std::vector<AutoPoint> points { { 0, normal } };
            for (const auto& run : runs)
            {
                if (run.empty()) continue;
                points.push_back({ run.front().tick - 1, normal });
                points.insert(points.end(), run.begin(), run.end());
                points.push_back({ run.back().tick + step, normal });
            }
            auto& lanes = owner == Rack::FX ? song.fx : song.tracks[(size_t) owner].automation;
            lanes.push_back({ id, points });
        }
    };

    // A riser: sixteenths climbing C minor a note a beat, from `root`, over four bars.
    std::vector<Note> riser(int root)
    {
        std::vector<Note> notes;
        for (int s = 0; s < 64; ++s)
        {
            const int degree = s / 4;
            const int pitch = root + 12 * (degree / 7) + scale[degree % 7];
            notes.push_back(note(s * step, step / 2, std::min(pitch, 108), s % 4 == 0 ? 127 : 90));
        }
        return notes;
    }

    // a crash at the top of the drop
    std::vector<Note> crash() { return { note(0, step, drum909(tr909::Voice::CC), 127) }; }
}

Song demoSong(Rack& rack)
{
    Song song;

    struct Spec { const char* name; int machine; int colour; };
    const Spec specs[trackCount] = {
        { "Beat",        Rack::M808, 2 },
        { "Tops",        Rack::M909, 3 },
        { "Rolls & FX",  Rack::M909, 7 },
        { "Acid",        Rack::M303, 0 },
        { "Lead",        Rack::M101, 1 },
        { "Riser & Pad", Rack::M101, 6 },
        { "Percussion",  Rack::M950, 4 },
    };
    for (const auto& s : specs)
    {
        SongTrack t;
        t.name = s.name;
        t.machine = s.machine;
        t.colour = s.colour;
        t.shuffle = rack.track(s.machine).shuffle.load();
        song.tracks.push_back(t);
    }

    Writer w { rack, song };

    // 1-8: intro - the hats and claps alone, then the kick, then the tops
    w.pattern(Beat, A5, 0, 4);
    w.pattern(Beat, A1, 4, 4);
    w.pattern(Tops, A5, 4, 4);

    // 9-16: the acid line, sparse, then the full one; the 950's percussion
    w.pattern(Beat, A1, 8, 8);
    w.pattern(Tops, A1, 8, 8);
    w.pattern(Acid, A5, 8, 4);
    w.pattern(Acid, A1, 12, 4);
    w.pattern(Perc, A1, 8, 8);

    // 17-24: the lead, with a fill at the end
    w.pattern(Beat, A2, 16, 7);
    w.pattern(Beat, A4, 23, 1);
    w.pattern(Tops, A2, 16, 8);
    w.pattern(Acid, A2, 16, 8);
    w.pattern(Lead, A1, 16, 8);
    w.pattern(Perc, A2, 16, 8);

    // 25-28: BUILD - the kick on the beat for two bars, then gone; the roll speeding up and
    // the riser climbing; and a beat of silence before the drop
    {
        std::vector<Note> kick;
        for (int i = 0; i < 8; ++i) kick.push_back(note(i * beat, step / 2, drum808(tr808::Voice::BD), 127));
        w.part(Beat, "KICK", 24, 2 * bar, kick);
    }
    w.part(Fx, "ROLL", 24, 4 * bar - beat, snareRoll(4));
    w.pattern(Acid, A1, 24, 4, beat);
    w.part(Riser, "RISER", 24, 4 * bar - beat, riser(48));

    // 29-36: DROP
    w.part(Fx, "CRASH", 28, bar, crash());
    w.pattern(Beat, A3, 28, 7);
    w.pattern(Beat, A8, 35, 1);
    w.pattern(Tops, A3, 28, 8);
    w.pattern(Acid, A3, 28, 8);
    w.pattern(Lead, A2, 28, 8);
    w.pattern(Perc, A3, 28, 8);

    // 37-44: breakdown - no kick; the lead held long over the chords, gliding between them;
    // the acid line thinned; the hats and claps back for the second half
    {
        const int roots[] = { 60, 56, 58, 55 };   // C, A flat, B flat, G
        std::vector<Note> pad;
        for (int i = 0; i < 4; ++i)
            pad.push_back(note(i * 2 * bar, 2 * bar + step, roots[i], 100));   // overlapping: they slide
        w.part(Riser, "PAD", 36, 8 * bar, pad);
    }
    w.pattern(Tops, A5, 36, 8);
    w.pattern(Acid, A5, 36, 8);
    w.pattern(Beat, A5, 40, 4);
    w.pattern(Perc, A5, 40, 4);

    // 41-48: the long roll, under the breakdown's second half and on through the build
    w.part(Fx, "LONG ROLL", 40, 8 * bar - beat, longRoll());

    // 45-48: BUILD - the acid line climbing an octave, a kick roll
    w.pattern(Acid, A4, 44, 2);
    w.pattern(Acid, A4, 46, 2, beat, 12);
    w.part(Riser, "RISER", 44, 4 * bar - beat, riser(60));
    {
        std::vector<Note> kicks;
        for (int i = 0; i < 4; ++i) kicks.push_back(note(i * beat, step / 2, drum808(tr808::Voice::BD), 127));
        for (int i = 0; i < 8; ++i) kicks.push_back(note(bar * 2 + i * beat / 2, step / 2, drum808(tr808::Voice::BD), 127));
        for (int i = 0; i < 12; ++i) kicks.push_back(note(bar * 3 + i * step, step / 2, drum808(tr808::Voice::BD), 127));
        w.part(Beat, "KICK ROLL", 44, 4 * bar - beat, kicks);
    }

    // 49-60: DROP - two grooves turning about, a crash every eight bars, a fill at the end
    for (int b : { 48, 56 }) w.part(Fx, "CRASH", b, bar, crash());
    w.pattern(Beat, A6, 48, 4);
    w.pattern(Beat, A7, 52, 4);
    w.pattern(Beat, A6, 56, 3);
    w.pattern(Beat, A8, 59, 1);
    w.pattern(Tops, A6, 48, 4);
    w.pattern(Tops, A7, 52, 4);
    w.pattern(Tops, A6, 56, 3);
    w.pattern(Tops, A8, 59, 1);
    w.pattern(Acid, A6, 48, 4);
    w.pattern(Acid, A7, 52, 4);
    w.pattern(Acid, A3, 56, 4);
    w.pattern(Lead, A6, 48, 4);
    w.pattern(Lead, A7, 52, 4);
    w.pattern(Lead, A3, 56, 4);
    w.pattern(Perc, A6, 48, 4);
    w.pattern(Perc, A7, 52, 8);

    // 61-64: outro - the kick and the acid, thinning out
    w.part(Fx, "CRASH", 60, bar, crash());
    w.pattern(Beat, A1, 60, 4);
    w.pattern(Tops, A5, 60, 2);
    w.pattern(Acid, A5, 60, 2);

    //
    // Written as seven lines, played as five machines: one track each, as the rack has. The
    // 909's tops and its rolls go on one track, and the 101's lead and its risers and pad on
    // another - they never sound together, so the one synth plays both.
    //
    song = oneTrackPerMachine(std::move(song));
    song.tracks[Rack::M909].name = "Tops & Rolls";
    song.tracks[Rack::M101].name = "Lead & Pad";

    //
    // The knobs turned as it goes, each back where the rack had it after:
    //
    //   the 303's CUT OFF  opening as the acid line comes in; pushed up through each build
    //   the 909's SD LEVEL the rolls getting louder - the long one from almost nothing
    //   the 101's FREQ     opening with each riser
    //   the 101's REVERB   sent up for the breakdown's long notes
    //
    Automator a { rack, song };
    const float cutoff = a.setting(Rack::M303, "cutoff");
    const float freq = a.setting(Rack::M101, "cutoff");
    const float reverb = a.setting(Rack::M101, "mixReverb");
    auto up = [] (float v, float by) { return std::min(10.0f, v + by); };
    auto down = [] (float v, float by) { return std::max(0.0f, v - by); };

    a.turn(Rack::M303, "cutoff", {
        { { 8 * bar, down(cutoff, 4.0f) }, { 16 * bar - step, cutoff } },
        { { 24 * bar, cutoff }, { 28 * bar - beat, up(cutoff, 3.0f) } },
        { { 44 * bar, cutoff }, { 48 * bar - beat, 10.0f } },
    });
    a.turn(Rack::M909, "sdLevel", {
        { { 24 * bar, 3.0f }, { 28 * bar - beat, 10.0f } },
        { { 40 * bar, 0.5f }, { 48 * bar - beat, 10.0f } },
    });
    a.turn(Rack::M101, "cutoff", {
        { { 24 * bar, down(freq, 3.0f) }, { 28 * bar - beat, up(freq, 2.5f) } },
        { { 44 * bar, down(freq, 2.0f) }, { 48 * bar - beat, up(freq, 3.5f) } },
    });
    a.turn(Rack::M101, "mixReverb", {
        { { 36 * bar, reverb }, { 37 * bar, 0.5f }, { 44 * bar - step, 0.5f } },
    });

    song.left = 0;
    song.right = 64 * bar;
    song.cycle = false;
    song.cursor = 0;
    return song;
}

} // namespace debirth
