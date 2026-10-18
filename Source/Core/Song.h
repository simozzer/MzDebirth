// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace debirth
{

class Rack;

//
// THE SONG. The arrangement the Arrange window shows, as Cubase on the Atari ST laid one
// out: tracks, each playing one of the machines, and on them parts, each holding notes of
// any length at any position. Time is in ticks, 192 to the quarter note as Cubase's were, so
// a sixteenth - a step of a pattern - is 48. Always 4/4; the tempo is the rack's.
//
// A song belongs to the message thread. The audio thread plays a compiled copy of it - a
// Playback, below - handed over whole after every edit.
//
constexpr int ticksPerQuarter = 192;
constexpr int ticksPerStep    = ticksPerQuarter / 4;
constexpr int ticksPerBar     = ticksPerQuarter * 4;

// What a machine is played with: a mono line (303, 101), a drum map (808, 909) or notes (950).
namespace MachineKind { enum { Mono, Drums, Notes }; }
int machineKind(int machine);

// The machines' names, in rack order, as saved and as the track list shows them.
const char* machineName(int machine);

struct Note
{
    int tick     = 0;     // from the start of its part
    int length   = ticksPerStep / 2;
    int pitch    = 60;    // a MIDI note; on a drum machine, its drum map's
    int velocity = 100;   // 1 to 127

    bool operator==(const Note& o) const
    {
        return tick == o.tick && length == o.length && pitch == o.pitch && velocity == o.velocity;
    }
};

struct Part
{
    int start  = 0;            // ticks from the top of the song
    int length = ticksPerBar;
    std::string name;
    bool muted = false;
    std::vector<Note> notes;   // kept in order of tick

    int end() const { return start + length; }
    void sortNotes();
};

struct SongTrack
{
    std::string name;
    int machine  = 0;          // which machine it plays, in rack order
    bool mute    = false;
    bool solo    = false;
    bool shuffle = true;       // whether it swings with the transport's SHUFFLE
    int colour   = 0;          // an index into the Arrange window's colours
    std::vector<Part> parts;   // kept in order of start
};

struct Song
{
    std::vector<SongTrack> tracks;
    int left   = 0;            // the locators, in ticks
    int right  = 4 * ticksPerBar;
    bool cycle = false;        // play round from right back to left
    int cursor = 0;            // where PLAY starts from

    // the end of the last part
    int end() const;
};

// A track for each machine, empty, as a new rack starts with.
Song defaultSong();

//
// The song with exactly one track for each machine, in rack order - so track n plays
// machine n, and the Arrange window's tracks are the rack's machines, one for one.
//
// A song with more than one track for a machine (0.5 and before let a song have as many as
// it liked) has them merged into the first, which keeps its name, colour and switches. A
// part from a muted track comes over muted, so the song sounds as it did; a track's solo
// carries over to the machine's. A machine with no track gets an empty one.
//
Song oneTrackPerMachine(Song);

//
// A machine's pattern as a part: its steps as notes, played `repeats` times over. An accent
// is velocity 127 and anything else 100; a slid bass note overlaps the next, and a tied 950
// note carries on as one long note - so the part plays as the pattern does.
//
Part partFromPattern(Rack&, int machine, int pattern, int repeats);

//
// Wipe a track from one tick to another, as a take recorded over it does, tape-fashion: a
// part inside goes, one across the edge is cut there - its notes beyond dropped, and those
// sounding over the edge cut short - and one across the whole range is cut in two.
//
void clearRange(SongTrack&, int from, int to);

//
// The song as the audio thread plays it: every note, in order of its start. Mutes and solos
// are already applied, and a mono line's notes that overlap the note before are marked as
// slides.
//
// SHUFFLE swings a note's start and end by the sixteenth each falls in, as the pattern
// clock swings its steps: anything in an off-beat sixteenth of a swinging track is held
// back. So a half-step gate moves with its note, and a slide into a rest lets go where the
// rest begins, swung or not - just as a pattern plays them.
//
struct SongEvent
{
    int tick;              // when it starts
    int endTick;           // and ends
    int16_t machine;
    int16_t key;           // the note; on a drum machine, the voice
    uint8_t velocity;
    bool slide;            // a mono line: slides on from the note sounding
    bool shuffle;          // the track swings
};

struct Playback
{
    std::vector<SongEvent> events;   // by tick, then machine, then key
    int left = 0, right = 0;
    bool cycle = false;
    int cursor = 0;                  // where PLAY starts from
};

Playback compile(const Song&);

//
// The song being edited, with as many levels of undo as anyone wants: a song is small, so
// each edit keeps the whole song as it was. Every change calls onChange, which the
// processor uses to hand the audio thread a new Playback.
//
class SongDocument
{
public:
    SongDocument() : current(defaultSong()) {}

    const Song& song() const { return current; }

    // an edit that can be undone
    void edit(const std::function<void(Song&)>& change);

    // a change that is not worth an undo - the cursor moving, the locators while dragged
    void adjust(const std::function<void(Song&)>& change);

    // a whole new song - loaded, or started afresh - forgetting the undo history
    void replace(Song s);

    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    bool undo();
    bool redo();

    // bumped by every change, so the panel knows to read itself back
    int version() const { return changes; }

    std::function<void(const Song&)> onChange;

private:
    void changed();

    Song current;
    std::vector<Song> undoStack, redoStack;
    int changes = 0;

    static constexpr size_t maxUndo = 100;
};

} // namespace debirth
