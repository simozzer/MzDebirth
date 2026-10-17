// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Song.h"

#include "Rack.h"

#include <algorithm>

namespace debirth
{

int machineKind(int machine)
{
    switch (machine)
    {
        case Rack::M303: case Rack::M101: return MachineKind::Mono;
        case Rack::M808: case Rack::M909: return MachineKind::Drums;
        default:                          return MachineKind::Notes;
    }
}

const char* machineName(int machine)
{
    static const char* const names[Rack::channels] = { "Mz303", "Mz101", "Mz808", "Mz909", "Mz950" };
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
        return machine == Rack::M808 ? tr808::voiceInfo(voice).note : tr909::voiceInfo(voice).note;
    }

    int drumVoice(int machine, int note)
    {
        return machine == Rack::M808 ? tr808::voiceForNote(note) : tr909::voiceForNote(note);
    }

    int drumVoices(int machine)
    {
        return machine == Rack::M808 ? Mz808::voices : Mz909::voices;
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
    void samplerNotes(Part& part, const Mz950& m, int pattern, int repeats)
    {
        const NotePattern& p = m.track.patterns[pattern];
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

    switch (machine)
    {
        case Rack::M303: monoNotes(part, rack.mz303, pattern, repeats); break;
        case Rack::M101: monoNotes(part, rack.mz101, pattern, repeats); break;
        case Rack::M808: drumNotes(part, rack.mz808, machine, pattern, repeats); break;
        case Rack::M909: drumNotes(part, rack.mz909, machine, pattern, repeats); break;
        default:         samplerNotes(part, rack.mz950, pattern, repeats); break;
    }

    part.sortNotes();
    return part;
}

//============================================================================== compile

Playback compile(const Song& song)
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

        for (const auto& part : t.parts)
        {
            if (part.muted) continue;

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
        if (a.machine != b.machine) return a.machine < b.machine;
        return a.key < b.key;
    });

    // A mono line plays one note at a time: one that starts before the last has ended
    // slides from it.
    int lastEnd[Rack::channels];
    std::fill(std::begin(lastEnd), std::end(lastEnd), -1);
    for (auto& e : out.events)
    {
        if (machineKind(e.machine) != MachineKind::Mono) continue;
        e.slide = lastEnd[e.machine] > e.tick;
        lastEnd[e.machine] = e.endTick;
    }

    return out;
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
