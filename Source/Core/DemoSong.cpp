// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "DemoSong.h"

#include "Rack.h"
#include "DX7/Banks.h"

#include <algorithm>
#include <string>

namespace debirth
{

namespace
{
    constexpr int bar = ticksPerBar, beat = ticksPerQuarter, step = ticksPerStep;

    // bank A's patterns, by their number on the pads
    constexpr int A1 = 0, A2 = 1, A3 = 2, A4 = 3, A5 = 4, A6 = 5, A7 = 6, A8 = 7;

    // the lines it is written in - merged at the end into a track a machine
    enum { Beat, Tops, Fx, Acid, Lead, Riser, Perc, Keys, trackCount };

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
        { "Keys",        Rack::M7,   5 },
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
        // the last bar's sixteenths stop a beat short of the gap, so the kicks' tails - long,
        // through the 808's DIST - have died away before the beat of silence
        for (int i = 0; i < 8; ++i) kicks.push_back(note(bar * 3 + i * step, step / 2, drum808(tr808::Voice::BD), 127));
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

    // the Mz7's electric piano: offbeat chords through both drops, and the breakdown's
    // chords held under the lead - C minor, A flat, B flat, G
    {
        const int chords[4][3] = { { 60, 63, 67 }, { 56, 60, 63 }, { 58, 62, 65 }, { 55, 59, 62 } };
        auto stabs = [&] (int bars)
        {
            std::vector<Note> n;
            for (int b = 0; b < bars; ++b)
                for (int s : { 2, 6, 10, 14 })
                    for (int k : chords[(b / 2) % 4]) n.push_back(note(b * bar + s * step, step + step / 2, k, s == 2 ? 110 : 90));
            return n;
        };
        w.part(Keys, "STABS", 28, 8 * bar, stabs(8));
        std::vector<Note> held;
        for (int i = 0; i < 4; ++i)
            for (int k : chords[i]) held.push_back(note(i * 2 * bar, 2 * bar - step, k, 96));
        w.part(Keys, "CHORDS", 36, 8 * bar, held);
        w.part(Keys, "STABS", 48, 12 * bar, stabs(12));
    }

    // 61-64: outro - the kick and the acid, thinning out
    w.part(Fx, "CRASH", 60, bar, crash());
    w.pattern(Beat, A1, 60, 4);
    w.pattern(Tops, A5, 60, 2);
    w.pattern(Acid, A5, 60, 2);

    //
    // Written as eight lines, played as six machines: one track each, as the rack has. The
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

//============================================================================== the ambient demo

namespace
{
    void setKnob(Rack& rack, int owner, const char* id, float v)
    {
        if (const int i = rack.paramIndex(owner, id); i >= 0) rack.paramSet(owner, i, v);
    }

    // E flat major: E flat, C minor, A flat, B flat with its fourth - a pad's voicing of each
    const std::vector<int> padChords[4] = {
        { 51, 55, 58, 62, 65 },   // E flat, with its seventh and ninth
        { 48, 55, 58, 62, 63 },   // C minor, the same
        { 44, 55, 60, 63 },       // A flat, with its seventh
        { 46, 53, 58, 60, 63 },   // B flat, its second and fourth over it
    };
    const int padRoots[4] = { 39, 36, 32, 34 };   // the 303's, an octave and more below

    //
    // The 101's melody, in E flat's pentatonic: two eight-bar phrases, as (beat, beats, note) -
    // long notes, slow to come, for the delay to fill the gaps.
    //
    struct Sung { int beat, beats, pitch; };
    const std::vector<Sung> phraseA = {
        { 0, 3, 70 }, { 3, 1, 72 }, { 4, 7, 75 }, { 12, 2, 72 }, { 14, 4, 70 }, { 20, 10, 67 },
    };
    const std::vector<Sung> phraseB = {
        { 0, 2, 75 }, { 2, 2, 77 }, { 4, 8, 79 }, { 14, 2, 77 }, { 16, 4, 75 }, { 20, 10, 72 },
    };

    std::vector<Note> sung(const std::vector<Sung>& phrase, int from)
    {
        std::vector<Note> notes;
        for (const auto& s : phrase)
            notes.push_back(note(from + s.beat * beat, s.beats * beat - step, s.pitch, 80));
        return notes;
    }
}

namespace
{
    // the index of the program of that name, padded out or not, or -1
    int programNamed(const std::vector<std::string>& names, const std::string& name)
    {
        for (size_t i = 0; i < names.size(); ++i)
        {
            auto n = names[i];
            while (! n.empty() && n.back() == ' ') n.pop_back();
            if (n == name) return (int) i;
        }
        return -1;
    }
}

bool voiceForAmbient(Rack& rack, const std::string& whalesDisk)
{
    bool whales = false;
    std::string error;
    if (! whalesDisk.empty() && (rack.mz950.diskPath() == whalesDisk || rack.mz950.loadDisk(whalesDisk, error)))
    {
        const int p = programNamed(rack.mz950.programNames(), "WHALE SEA");
        whales = p >= 0 && rack.mz950.selectProgram(p);
        auto& t = rack.track(Rack::M950);
        for (int i = 0; i < patternsPerTrack; ++i) t.clear(i);
    }

    for (int b = 0; b < ::banks::count(); ++b)
    {
        rack.mz7.loadBank(b);
        if (const int p = programNamed(rack.mz7.programNames(), "WARM PAD"); p >= 0)
        {
            rack.mz7.selectProgram(p);
            break;
        }
    }
    return whales;
}

//
// The rack's knobs as the ambient demo wants them, before the song is written.
//
void dressForAmbient(Rack& rack)
{
    // the 101 soft: a little saw and the sub, the filter low, slow in and slower out, a
    // touch of vibrato, gliding where the notes run together
    setKnob(rack, Rack::M101, "saw", 6.0f);
    setKnob(rack, Rack::M101, "pulse", 0.0f);
    setKnob(rack, Rack::M101, "sub", 3.0f);
    setKnob(rack, Rack::M101, "noise", 0.0f);
    setKnob(rack, Rack::M101, "cutoff", 3.5f);
    setKnob(rack, Rack::M101, "res", 1.5f);
    setKnob(rack, Rack::M101, "vcfEnv", 1.5f);
    setKnob(rack, Rack::M101, "vcfKybd", 5.0f);
    setKnob(rack, Rack::M101, "attack", 4.0f);
    setKnob(rack, Rack::M101, "decay", 6.0f);
    setKnob(rack, Rack::M101, "sustain", 7.0f);
    setKnob(rack, Rack::M101, "release", 6.5f);
    setKnob(rack, Rack::M101, "lfoRate", 4.0f);
    setKnob(rack, Rack::M101, "vcoMod", 0.8f);
    setKnob(rack, Rack::M101, "portaMode", 1.0f);   // AUTO
    setKnob(rack, Rack::M101, "portaTime", 3.0f);

    // the 303 as a soft low pulse, not a squelch: the filter nearly shut, little resonance
    setKnob(rack, Rack::M303, "cutoff", 1.5f);
    setKnob(rack, Rack::M303, "res", 2.0f);
    setKnob(rack, Rack::M303, "envMod", 1.0f);
    setKnob(rack, Rack::M303, "decay", 8.0f);
    setKnob(rack, Rack::M303, "accent", 0.0f);

    // the Mz7's pad slower in and out, a little darker
    setKnob(rack, Rack::M7, "bright", -8.0f);
    setKnob(rack, Rack::M7, "attack", -20.0f);
    setKnob(rack, Rack::M7, "release", -25.0f);

    // a long echo - six sixteenths - and a long dark hall
    setKnob(rack, Rack::FX, "delaySteps", 6.0f);
    setKnob(rack, Rack::FX, "delayFeedback", 0.55f);
    setKnob(rack, Rack::FX, "delayPan", 0.4f);
    setKnob(rack, Rack::FX, "reverbDecay", 0.9f);
    setKnob(rack, Rack::FX, "reverbTone", 0.35f);
    setKnob(rack, Rack::FX, "reverbPre", 0.3f);

    // the mix: the sea and the whales up front, everything sent to the hall
    struct Mix { int machine; float level, delay, reverb; };
    for (const Mix& m : { Mix { Rack::M950, 0.8f,  0.0f,  0.35f },
                          Mix { Rack::M7,   0.5f,  0.1f,  0.5f },
                          Mix { Rack::M101, 0.45f, 0.45f, 0.6f },
                          Mix { Rack::M303, 0.55f, 0.0f,  0.2f } })
    {
        rack.channel[m.machine].level = m.level;
        rack.channel[m.machine].delay = m.delay;
        rack.channel[m.machine].reverb = m.reverb;
    }
}

Song ambientSong(Rack& rack, bool whales)
{
    Song song;

    enum { Sea, Pad, Melody, Bass, lines };
    struct Spec { const char* name; int machine; int colour; };
    const Spec specs[lines] = {
        { "Sea & Whales", Rack::M950, 4 },
        { "Pad",          Rack::M7,   5 },
        { "Melody",       Rack::M101, 1 },
        { "Pulse",        Rack::M303, 0 },
    };
    for (const auto& s : specs)
    {
        SongTrack t;
        t.name = s.name;
        t.machine = s.machine;
        t.colour = s.colour;
        t.shuffle = false;
        song.tracks.push_back(t);
    }

    Writer w { rack, song };

    //
    // The sea and the whales, on WHALE SEA: each whale an octave of the keyboard from C0, its
    // own pitch on the top key - HUMPBACK, HUMPMOO, BLUE, FIN, MINKE, ORCA, SPERM, BLOW - and
    // the sea on the keys above them, held a little under its own pitch the whole way.
    //
    if (whales)
    {
        enum { Humpback, Moo, Blue, Fin, Minke, Orca, Sperm, Blow };
        auto key = [] (int whale, int below) { return 35 + 12 * whale - below; };

        // (bar, beat, whale, semitones below its own pitch, velocity), counting bars from 0
        struct Call { int bar, beat, whale, below, velocity; };
        const Call calls[] = {
            { 2, 1, Moo, 2, 70 },      { 6, 0, Humpback, 3, 80 },  { 11, 2, Blue, 5, 75 },
            { 14, 0, Minke, 4, 60 },   { 18, 1, Moo, 0, 85 },      { 21, 3, Orca, 5, 60 },
            { 25, 0, Humpback, 0, 90 },{ 28, 2, Sperm, 3, 50 },    { 30, 0, Blow, 4, 60 },
            { 32, 1, Blue, 7, 85 },    { 34, 2, Moo, 5, 80 },      { 36, 0, Humpback, 5, 85 },
            { 37, 3, Orca, 2, 65 },    { 38, 2, Fin, 0, 70 },      { 42, 0, Moo, 3, 80 },
            { 45, 2, Minke, 7, 60 },   { 49, 0, Humpback, 2, 85 }, { 52, 1, Blue, 3, 75 },
            { 55, 3, Orca, 7, 55 },    { 57, 0, Moo, 5, 75 },      { 60, 2, Humpback, 7, 65 },
        };
        // one part: parts on a track that overlap play only the later
        std::vector<Note> notes { note(0, 64 * bar - step, 125, 100) };
        for (const auto& c : calls)
            notes.push_back(note(c.bar * bar + c.beat * beat, 2 * bar, key(c.whale, c.below), c.velocity));
        w.part(Sea, "SEA & WHALES", 0, 64 * bar, notes);
    }

    // The pad, from bar 9: the four chords, four bars each, three times round; then E flat
    // held for the last eight
    {
        std::vector<Note> notes;
        for (int b = 8, i = 0; b < 56; b += 4, ++i)
            for (int k : padChords[i % 4]) notes.push_back(note((b - 8) * bar, 4 * bar - step, k, 70));
        w.part(Pad, "CHORDS", 8, 48 * bar, notes);

        std::vector<Note> last;
        for (int k : padChords[0]) last.push_back(note(0, 8 * bar - beat, k, 70));
        w.part(Pad, "LAST", 56, 8 * bar, last);
    }

    // The melody, bars 17-32 and 41-56: the two phrases, then the other way about
    w.part(Melody, "MELODY", 16, 16 * bar, [] { auto n = sung(phraseA, 0); auto b = sung(phraseB, 8 * bar); n.insert(n.end(), b.begin(), b.end()); return n; }());
    w.part(Melody, "MELODY", 40, 16 * bar, [] { auto n = sung(phraseB, 0); auto a = sung(phraseA, 8 * bar); n.insert(n.end(), a.begin(), a.end()); return n; }());

    // The 303's pulse under the melody: the chord's root on the first and third beats, soft
    auto pulse = [] (int fromChord)
    {
        std::vector<Note> notes;
        for (int b = 0; b < 16; ++b)
        {
            const int root = padRoots[(fromChord + b / 4) % 4];
            for (int q : { 0, 2 }) notes.push_back(note(b * bar + q * beat, beat + beat / 2, root, 90));
        }
        return notes;
    };
    w.part(Bass, "PULSE", 16, 16 * bar, pulse(2));   // bar 17 is the progression's third block
    w.part(Bass, "PULSE", 40, 16 * bar, pulse(0));

    song = oneTrackPerMachine(std::move(song));

    //
    // The knobs, slowly:
    //
    //   the 303's CUT OFF  breathing open and shut again through each pulse section
    //   the 101's FREQ     opening a little through the second melody
    //   the 950's LEVEL    the sea fading away over the last four bars
    //   the Mz7's LEVEL    and the last chord with it
    //
    Automator a { rack, song };
    const float cutoff = a.setting(Rack::M303, "cutoff");
    const float freq = a.setting(Rack::M101, "cutoff");
    const float level = a.setting(Rack::M950, "mixLevel");
    const float padLevel = a.setting(Rack::M7, "mixLevel");

    a.turn(Rack::M303, "cutoff", {
        { { 16 * bar, cutoff }, { 24 * bar, cutoff + 1.5f }, { 32 * bar - step, cutoff } },
        { { 40 * bar, cutoff }, { 48 * bar, cutoff + 2.0f }, { 56 * bar - step, cutoff } },
    });
    a.turn(Rack::M101, "cutoff", {
        { { 40 * bar, freq }, { 48 * bar, freq + 1.5f }, { 56 * bar - step, freq } },
    });
    if (whales)
        a.turn(Rack::M950, "mixLevel", {
            { { 60 * bar, level }, { 64 * bar - step, 0.0f } },
        });
    a.turn(Rack::M7, "mixLevel", {
        { { 60 * bar, padLevel }, { 64 * bar - step, 0.0f } },
    });

    song.left = 0;
    song.right = 64 * bar;
    song.cycle = false;
    song.cursor = 0;
    return song;
}

} // namespace debirth
