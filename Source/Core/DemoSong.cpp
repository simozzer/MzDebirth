// MzDebirth - Copyright (C) 2026 Simon Moscrop
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

    // the tracks, top to bottom
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
    song.tracks[Fx].shuffle = false;      // rolls and risers dead straight
    song.tracks[Riser].shuffle = false;

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

    // 45-48: BUILD - faster from the start, the acid line climbing an octave, a kick roll
    w.part(Fx, "ROLL", 44, 4 * bar - beat, snareRoll(8));
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

    for (auto& t : song.tracks)
        std::sort(t.parts.begin(), t.parts.end(), [] (const Part& a, const Part& b) { return a.start < b.start; });

    song.left = 0;
    song.right = 64 * bar;
    song.cycle = false;
    song.cursor = 0;
    return song;
}

} // namespace debirth
