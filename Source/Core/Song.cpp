// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Song.h"

#include "Rack.h"

#include <algorithm>

namespace debirth
{

int machineKind(int machine)
{
    switch (Rack::typeOf(machine))
    {
        case Rack::M303: case Rack::M101: return MachineKind::Mono;
        case Rack::M808: case Rack::M909: return MachineKind::Drums;
        default:                          return MachineKind::Notes;
    }
}

bool takesPatches(int machine)
{
    const int t = Rack::typeOf(machine);
    return t == Rack::M950 || t == Rack::M7 || t == Rack::M106;
}

const char* machineName(int machine)
{
    static const char* const names[Rack::channels] = { "Mz303", "Mz101", "Mz808", "Mz909", "Mz950", "Mz7", "Mz106",
                                                       "Mz303 2", "Mz101 2", "Mz808 2", "Mz909 2", "Mz950 2", "Mz7 2", "Mz106 2" };
    return names[std::clamp(machine, 0, Rack::channels - 1)];
}

void Part::sortNotes()
{
    std::stable_sort(notes.begin(), notes.end(), [] (const Note& a, const Note& b)
    {
        return a.tick != b.tick ? a.tick < b.tick : a.pitch < b.pitch;
    });
}

int Song::end() const
{
    int e = 0;
    for (const auto& t : tracks)
        for (const auto& p : t.parts)
            e = std::max(e, p.end());
    return e;
}

Song defaultSong()
{
    Song s;
    for (int m = 0; m < Rack::channels; ++m)
    {
        SongTrack t;
        t.name = machineName(m);
        t.machine = m;
        t.colour = m;
        s.tracks.push_back(t);
    }
    return s;
}

Song oneTrackPerMachine(Song s)
{
    std::vector<SongTrack> merged;

    for (int m = 0; m < Rack::channels; ++m)
    {
        SongTrack out;
        out.name = machineName(m);
        out.machine = m;
        out.colour = m;

        bool found = false, allMuted = true;
        for (const auto& t : s.tracks)
            if (t.machine == m) allMuted &= t.mute;

        for (auto& t : s.tracks)
        {
            if (t.machine != m) continue;
            if (! found)
            {
                out.name = t.name;
                out.colour = t.colour;
                out.shuffle = t.shuffle;
                found = true;
            }
            out.solo |= t.solo;

            // muted with the whole machine, the track says so; muted alone, its parts do
            for (auto& p : t.parts)
            {
                if (t.mute && ! allMuted) p.muted = true;
                out.parts.push_back(std::move(p));
            }
            for (auto& lane : t.automation)
                writeAutomation(out.automation, lane.param, lane.points);
        }
        out.mute = found && allMuted;

        std::stable_sort(out.parts.begin(), out.parts.end(), [] (const Part& a, const Part& b) { return a.start < b.start; });
        merged.push_back(std::move(out));
    }

    s.tracks = std::move(merged);
    return s;
}

//============================================================================== pattern to part

namespace
{
    // A drum machine's voice as a note, and back: the General MIDI drum map, as the
    // machines answer MIDI with.
    int drumNote(int machine, int voice)
    {
        return Rack::typeOf(machine) == Rack::M808 ? tr808::voiceInfo(voice).note : tr909::voiceInfo(voice).note;
    }

    int drumVoice(int machine, int note)
    {
        return Rack::typeOf(machine) == Rack::M808 ? tr808::voiceForNote(note) : tr909::voiceForNote(note);
    }

    int drumVoices(int machine)
    {
        return Rack::typeOf(machine) == Rack::M808 ? Mz808::voices : Mz909::voices;
    }

    constexpr int plain = 100, accented = 127;
    constexpr int half = ticksPerStep / 2;

    template <class Line>
    void monoNotes(Part& part, const Line& line, int pattern, int repeats)
    {
        const BassPattern& p = line.track.patterns[pattern];
        const int len = p.len();

        for (int r = 0; r < repeats; ++r)
        {
            for (int i = 0; i < len; ++i)
            {
                const BassStep s = p.get(i);
                if (! s.gate) continue;

                // Let go half way; a slide holds on to the next step, and into a note there it
                // overlaps it, so the two slide - but into a rest it ends where the rest begins.
                Note n;
                n.tick = (r * len + i) * ticksPerStep;
                n.pitch = s.midiNote(line.rootNote);
                n.velocity = s.accent ? accented : plain;
                n.length = ! s.slide ? half
                         : p.get((i + 1) % len).gate ? ticksPerStep + ticksPerStep / 8
                         : ticksPerStep;
                part.notes.push_back(n);
            }
        }
    }

    template <class Machine>
    void drumNotes(Part& part, const Machine& m, int machine, int pattern, int repeats)
    {
        const DrumPattern& p = m.track.patterns[pattern];
        const int len = p.len();

        for (int r = 0; r < repeats; ++r)
            for (int i = 0; i < len; ++i)
                for (int v = 0; v < drumVoices(machine); ++v)
                    if (p.get(v, i))
                    {
                        Note n;
                        n.tick = (r * len + i) * ticksPerStep;
                        n.pitch = drumNote(machine, v);
                        n.velocity = p.get(DrumPattern::accentRow, i) ? accented : plain;
                        part.notes.push_back(n);
                    }
    }

    // The 950's: a tied step carries on a note the step before had, so a run of them is one
    // note, from the step that struck it to half way through the last.
    void samplerNotes(Part& part, const Track<NotePattern>& t, int pattern, int repeats)
    {
        const NotePattern& p = t.patterns[pattern];
        const int len = p.len();
        const int total = len * repeats;

        for (int k = 0; k < NotePattern::notes; ++k)
        {
            int from = -1;
            auto close = [&] (int last)
            {
                if (from < 0) return;
                Note n;
                n.tick = from * ticksPerStep;
                n.length = (last - from) * ticksPerStep + half;
                n.pitch = k;
                n.velocity = p.get(NotePattern::accentRow, from % len) ? accented : plain;
                part.notes.push_back(n);
                from = -1;
            };

            for (int u = 0; u < total; ++u)
            {
                const int i = u % len;
                if (! p.get(k, i)) { close(u - 1); continue; }
                if (from >= 0 && p.get(NotePattern::tieRow, i)) continue;   // carried on
                close(u - 1);
                from = u;
            }
            close(total - 1);
        }
    }
}

Part partFromPattern(Rack& rack, int machine, int pattern, int repeats)
{
    pattern = std::clamp(pattern, 0, patternsPerTrack - 1);
    repeats = std::max(1, repeats);

    Part part;
    part.name = std::string(1, (char) ('A' + pattern / patternsPerBank)) + std::to_string(pattern % patternsPerBank + 1);
    part.length = rack.track(machine).pattern(pattern).len() * repeats * ticksPerStep;

    rack.visit(machine, [&] (auto& x)
    {
        using T = std::decay_t<decltype(x)>;
        if constexpr (isMono<T>)       monoNotes(part, x, pattern, repeats);
        else if constexpr (isDrums<T>) drumNotes(part, x, machine, pattern, repeats);
        else                           samplerNotes(part, x.track, pattern, repeats);
    });

    part.sortNotes();
    return part;
}

//============================================================================== automation

float automationAt(const std::vector<AutoPoint>& points, double tick, bool stepped)
{
    if (points.empty()) return 0.0f;
    if (tick <= points.front().tick) return points.front().value;
    if (tick >= points.back().tick) return points.back().value;

    // the first point after the tick, and the one before it
    const auto after = std::upper_bound(points.begin(), points.end(), tick,
                                        [] (double t, const AutoPoint& p) { return t < (double) p.tick; });
    const auto before = after - 1;
    if (stepped || after->tick == before->tick) return before->value;
    const double f = (tick - before->tick) / (double) (after->tick - before->tick);
    return before->value + (float) f * (after->value - before->value);
}

void writeAutomation(std::vector<AutoLane>& lanes, const std::string& param, const std::vector<AutoPoint>& recorded)
{
    if (recorded.empty()) return;

    auto lane = std::find_if(lanes.begin(), lanes.end(), [&] (const AutoLane& l) { return l.param == param; });
    if (lane == lanes.end())
    {
        lanes.push_back({ param, {} });
        lane = lanes.end() - 1;
    }

    int from = recorded.front().tick, to = from;
    for (const auto& p : recorded) { from = std::min(from, p.tick); to = std::max(to, p.tick); }

    auto& pts = lane->points;
    pts.erase(std::remove_if(pts.begin(), pts.end(), [from, to] (const AutoPoint& p) { return p.tick >= from && p.tick <= to; }),
              pts.end());
    pts.insert(pts.end(), recorded.begin(), recorded.end());
    std::stable_sort(pts.begin(), pts.end(), [] (const AutoPoint& a, const AutoPoint& b) { return a.tick < b.tick; });
}

namespace
{
    // the notes of the first `period` ticks, repeated from the start to `length`
    std::vector<Note> looped(const std::vector<Note>& notes, int period, int length)
    {
        std::vector<Note> out;
        for (const auto& n : notes)
            if (n.tick < period) out.push_back(n);
        const size_t base = out.size();
        for (int offset = period; offset < length; offset += period)
            for (size_t i = 0; i < base; ++i)
                if (out[i].tick + offset < length)
                {
                    Note n = out[i];
                    n.tick += offset;
                    out.push_back(n);
                }
        return out;
    }

    std::vector<Note> heard(const std::vector<Note>& notes, int length)
    {
        std::vector<Note> out;
        for (const auto& n : notes)
            if (n.tick < length) out.push_back(n);
        return out;
    }
}

void resizePart(Part& p, int length)
{
    length = std::max(1, length);

    // the loop, if the notes heard are still it repeated; if not, the part as it stands
    int period = p.loop > 0 ? p.loop : p.length;
    if (period != p.length)
    {
        Part a = p, b = p;
        a.notes = heard(p.notes, p.length);
        b.notes = heard(looped(p.notes, period, p.length), p.length);
        a.sortNotes();
        b.sortNotes();
        if (a.notes != b.notes) period = p.length;
    }

    p.notes = looped(p.notes, period, std::max(length, period));
    p.length = length;
    p.loop = period;
    p.sortNotes();
}

void clearRange(SongTrack& track, int from, int to)
{
    if (to <= from) return;

    // the piece of a part from one tick to another of it, its notes moved to match
    auto piece = [] (const Part& p, int a, int b)
    {
        Part out = p;
        out.start = p.start + a;
        out.length = b - a;
        out.notes.clear();
        for (Note n : p.notes)
        {
            if (n.tick < a || n.tick >= b) continue;
            n.tick -= a;
            n.length = std::min(n.length, out.length - n.tick);
            out.notes.push_back(n);
        }
        return out;
    };

    std::vector<Part> kept;
    for (const auto& p : track.parts)
    {
        if (p.end() <= from || p.start >= to) { kept.push_back(p); continue; }
        if (p.start < from) kept.push_back(piece(p, 0, from - p.start));
        if (p.end() > to)   kept.push_back(piece(p, to - p.start, p.length));
    }
    track.parts = std::move(kept);
}

//============================================================================== compile

Playback compile(const Song& song, const Rack* rack)
{
    Playback out;
    out.left = song.left;
    out.right = song.right;
    out.cycle = song.cycle && song.right > song.left;
    out.cursor = std::max(0, song.cursor);

    bool anySolo = false;
    for (const auto& t : song.tracks) anySolo |= t.solo;

    for (const auto& t : song.tracks)
    {
        if (t.mute || (anySolo && ! t.solo)) continue;
        if (t.machine < 0 || t.machine >= Rack::channels) continue;
        const int kind = machineKind(t.machine);

        // A track whose parts choose their patches changes patch at the start of every part,
        // its own or back to the panel's, so a part plays on the same patch however it is
        // come to. One whose parts don't is left on whatever the panel has.
        bool patches = false;
        if (takesPatches(t.machine))
            for (const auto& part : t.parts) patches |= part.program >= 0;

        for (const auto& part : t.parts)
        {
            if (part.muted) continue;

            if (patches)
            {
                SongEvent e {};
                e.tick = e.endTick = part.start;
                e.machine = (int16_t) t.machine;
                e.key = (int16_t) std::max(-1, part.program);
                e.shuffle = t.shuffle;
                e.program = true;
                out.events.push_back(e);
            }

            for (const auto& n : part.notes)
            {
                if (n.tick < 0 || n.tick >= part.length) continue;   // outside the part: not heard

                int key = std::clamp(n.pitch, 0, 127);
                if (kind == MachineKind::Drums && (key = drumVoice(t.machine, n.pitch)) < 0) continue;

                SongEvent e {};
                e.tick = part.start + n.tick;
                e.endTick = e.tick + std::max(1, n.length);
                e.machine = (int16_t) t.machine;
                e.key = (int16_t) key;
                e.velocity = (uint8_t) std::clamp(n.velocity, 1, 127);
                e.shuffle = t.shuffle;
                out.events.push_back(e);
            }
        }
    }

    std::stable_sort(out.events.begin(), out.events.end(), [] (const SongEvent& a, const SongEvent& b)
    {
        if (a.tick != b.tick) return a.tick < b.tick;
        if (a.program != b.program) return a.program;   // a patch changes before its notes start
        if (a.machine != b.machine) return a.machine < b.machine;
        return a.key < b.key;
    });

    // A mono line plays one note at a time: one that starts before the last has ended
    // slides from it.
    int lastEnd[Rack::channels];
    std::fill(std::begin(lastEnd), std::end(lastEnd), -1);
    for (auto& e : out.events)
    {
        if (machineKind(e.machine) != MachineKind::Mono || e.program) continue;
        e.slide = lastEnd[e.machine] > e.tick;
        lastEnd[e.machine] = e.endTick;
    }

    // the automation, its controls found on the rack; a lane for a control it hasn't is let be
    if (rack != nullptr)
    {
        auto add = [&] (int owner, const std::vector<AutoLane>& lanes)
        {
            for (const auto& lane : lanes)
            {
                const int i = rack->paramIndex(owner, lane.param);
                if (i < 0 || lane.points.empty()) continue;
                out.automation.push_back({ owner, i, rack->paramStepped(owner, i), lane.points });
            }
        };
        for (const auto& t : song.tracks)
            if (t.machine >= 0 && t.machine < Rack::channels) add(t.machine, t.automation);
        add(Rack::FX, song.fx);
    }

    return out;
}

int Playback::programAt(int machine, int tick) const
{
    int at = -2;
    for (const auto& e : events)
    {
        if (e.tick > tick) break;
        if (e.program && e.machine == machine) at = e.key;
    }
    return at;
}

//============================================================================== SongDocument

void SongDocument::edit(const std::function<void(Song&)>& change)
{
    undoStack.push_back(current);
    if (undoStack.size() > maxUndo) undoStack.erase(undoStack.begin());
    redoStack.clear();
    change(current);
    changed();
}

void SongDocument::adjust(const std::function<void(Song&)>& change)
{
    change(current);
    changed();
}

void SongDocument::replace(Song s)
{
    current = oneTrackPerMachine(std::move(s));   // however it was saved or written
    undoStack.clear();
    redoStack.clear();
    changed();
}

bool SongDocument::undo()
{
    if (undoStack.empty()) return false;
    redoStack.push_back(std::move(current));
    current = std::move(undoStack.back());
    undoStack.pop_back();
    changed();
    return true;
}

bool SongDocument::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back(std::move(current));
    current = std::move(redoStack.back());
    redoStack.pop_back();
    changed();
    return true;
}

void SongDocument::changed()
{
    ++changes;
    if (onChange) onChange(current);
}

} // namespace debirth
