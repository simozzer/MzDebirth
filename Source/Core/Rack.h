// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Clock.h"
#include "Effects.h"
#include "Fm.h"
#include "Juno.h"
#include "Key.h"
#include "Machines.h"
#include "Sampler.h"
#include "SongPlayer.h"

#include <atomic>
#include <type_traits>
#include <memory>
#include <string>
#include <vector>

namespace debirth
{

// What plays the machines: their patterns, as ReBirth's PATTERN mode, or the song.
namespace Mode { enum { Pattern, Song }; }

//
// A channel of the mixer. The fader is 0 to 1, drawn to a square law; pan is -1 to +1,
// at constant power - and on the sampler, which is stereo, a balance.
//
// As on ReBirth's mixer, each channel can be switched through the DIST, the PCF and the
// COMP, in that order, before its fader; and sends to the DELAY after it.
//
struct Channel
{
    //
    // Whether the machine is in the rack at all: one left out has no panel, no track, no
    // channel on the mixer, and costs nothing - it is not rendered. Most racks want only some
    // of the seven. The panel sets it; the audio thread stops the machine when it goes.
    //
    std::atomic<bool> inRack { true };

    std::atomic<float> level { 0.7f };
    std::atomic<float> pan   { 0.0f };
    std::atomic<bool>  mute  { false };

    std::atomic<bool>  dist  { false };
    std::atomic<bool>  pcf   { false };
    std::atomic<bool>  comp  { false };
    std::atomic<float> delay  { 0.0f };   // the sends, 0 to 1
    std::atomic<float> reverb { 0.0f };

    //
    // The machine's STOP, kept with its channel because every panel of it already holds this.
    // The panel wants it; the audio thread makes it so at the start of the next bar, when
    // the machine lets go of its notes and plays nothing more - and wanted back, comes in
    // again at the top of its pattern, on a bar. Stopped is the audio thread's to write.
    //
    std::atomic<bool> stopWanted { false };
    std::atomic<bool> stopped    { false };

    //
    // While the song plays, a pattern picked on the machine takes it over from its track: from
    // the next bar the machine plays its pattern, from the top, and the song's notes for it go
    // by. The same pattern picked again hands it back to the song, on a bar. The panel wants
    // it; live is the audio thread's, and both are let go when the rack stops.
    //
    std::atomic<bool> liveWanted { false };
    std::atomic<bool> live       { false };

    // For the meter: the loudest sample out of the channel, after its fader, pan and the
    // MASTER - what it puts into the output - since the panel last took it. The audio thread
    // raises it; the panel takes it, leaving 0.
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
};

// what a machine is, by its type: each kind is played its own way
template <class T> constexpr bool isMono   = std::is_same_v<T, Mz303> || std::is_same_v<T, Mz101>;
template <class T> constexpr bool isDrums  = std::is_same_v<T, Mz808> || std::is_same_v<T, Mz909>;
template <class T> constexpr bool isStereo = std::is_same_v<T, Mz950> || std::is_same_v<T, Mz106>;

//
// The whole rack: the clock, the machines on it, top to bottom, and the mixer they go
// through. render() is called from the audio thread; the rest is the panel's, except where
// a machine says otherwise.
//
class Rack
{
public:
    //
    // THE SLOTS. The kinds of machine, in rack order - and the rack's slots: the first of each
    // kind, in that order, then a second of each. A slot's kind never changes - slot % types -
    // so the second Mz303 is always slot M303 + types, and a song's track for it is always a
    // mono line's. The first of each is where it always was; the seconds start out of the rack.
    //
    enum { M303, M101, M808, M909, M950, M7, M106, types };
    static constexpr int copies = 2;
    enum { channels = types * copies };
    static constexpr int typeOf(int m) { return ((m % types) + types) % types; }
    static constexpr int copyOf(int m) { return m / types; }

    // the slots as the panels and the tracks show them: each machine's second under its first
    static constexpr int inOrder(int i) { return i / copies + (i % copies) * types; }

    Rack();
    ~Rack();

    void prepare(double sampleRate, int maxBlock);
    void render(float* left, float* right, int n);

    // the transport
    std::atomic<bool>  playing { false };
    std::atomic<int>   mode    { Mode::Pattern };   // what plays the machines; a change waits for the bar
    std::atomic<float> tempo   { 120.0f };
    std::atomic<float> shuffle { 0.0f };    // 0 to 1

    //
    // DRAG and PUSH, as on a CD deck: held from the panel, -1 slows the rack, +1 hurries it,
    // 0 lets it go. It bends the clock rather than the tempo - nudgeFrom at once, building to
    // nudgeTo over nudgeRamp seconds held - and lets go of it on release, so the beat stays
    // wherever the nudge left it. That is how a DJ pulls the rack into phase with a record.
    //
    std::atomic<int> nudge { 0 };
    static constexpr double nudgeFrom = 0.03, nudgeTo = 0.10, nudgeRamp = 2.0;

    // any thread: the tempo the clock is running at, nudge and all
    float tempoNow() const { return clockTempo.load(std::memory_order_relaxed); }
    std::atomic<float> master  { 0.8f };

    //
    // The master's meter, raised by the audio thread and taken by the panel, leaving 0: the
    // loudest sample going out, left and right, after LIMIT; the loudest before it - over 1,
    // and without LIMIT that would have clipped; and the most LIMIT took off, in dB.
    //
    std::atomic<float> masterPeakL { 0.0f }, masterPeakR { 0.0f };
    std::atomic<float> masterOver { 0.0f };
    std::atomic<float> limiting { 0.0f };

    Mz303 mz303;
    Mz101 mz101;
    Mz808 mz808;
    Mz909 mz909;
    Mz950 mz950;
    Mz7 mz7;
    Mz106 mz106;

    // the second of each, slots M303 + types and on
    Mz303 mz303b;
    Mz101 mz101b;
    Mz808 mz808b;
    Mz909 mz909b;
    Mz950 mz950b;
    Mz7 mz7b;
    Mz106 mz106b;

    // f with the machine in slot m, as its own type
    template <class F>
    decltype(auto) visit(int m, F&& f)
    {
        switch (m)
        {
            case M303 + types: return f(mz303b);
            case M101:         return f(mz101);
            case M101 + types: return f(mz101b);
            case M808:         return f(mz808);
            case M808 + types: return f(mz808b);
            case M909:         return f(mz909);
            case M909 + types: return f(mz909b);
            case M950:         return f(mz950);
            case M950 + types: return f(mz950b);
            case M7:           return f(mz7);
            case M7 + types:   return f(mz7b);
            case M106:         return f(mz106);
            case M106 + types: return f(mz106b);
            default:           return f(mz303);
        }
    }
    template <class F>
    decltype(auto) visit(int m, F&& f) const { return const_cast<Rack&>(*this).visit(m, std::forward<F>(f)); }
    Channel channel[channels];

    // the effects units' knobs
    Knobs<EffectSettings> fx { controlsFx() };

    // The key the panel's KEY is set to (a Key::index()), the style GENERATE writes in, and
    // the key each bank was written or moved into: -1 where nobody knows - a bank written by
    // hand - and TRANSPOSE has to work it out from the notes.
    std::atomic<int> key   { Key { 8, false }.index() };   // 8A, A minor
    std::atomic<int> style { 0 };
    std::atomic<int> bankKey[banks] { -1, -1, -1, -1 };

    // any thread: the PCF's step, for its lights; -1 stopped
    int pcfStep() const { return pcfPosition.load(std::memory_order_relaxed); }

    // every machine's patterns and position, in rack order
    TrackBase& track(int machine);

    //
    // THE SONG, as the audio thread plays it. The panel compiles the song after every edit
    // and hands it over here; the one it replaces comes back, to be deleted by collect(),
    // which the panel calls on a timer - the audio thread neither allocates nor frees.
    //
    void publish(std::unique_ptr<Playback>);   // message thread
    void collect();                            // message thread

    // any thread: move the song's position while it plays, in ticks
    void locate(int tick) { locateTo = tick; }

    // any thread: the mode playing now, which a change waits for the bar to become
    int playingNow() const { return modeNow.load(std::memory_order_relaxed); }

    // any thread: where the song is, in ticks; -1 unless playing in SONG mode
    double songPosition() const { return songTick.load(std::memory_order_relaxed); }

    // the samples the output runs late by: LIMIT's lookahead
    int latency() const { return limiter.latency(); }

    //
    // RECORDING. While it is on and the machines play their patterns, the audio thread logs
    // each step an armed machine plays - which pattern, and which of its steps - into a
    // queue the message thread empties, a step at a time, into a Recorder. The queue is a
    // fixed ring with one writer and one reader, so the audio thread neither allocates nor
    // waits; were it ever full, a step would be dropped rather than the audio held up.
    //
    struct Played
    {
        int64_t step;      // the clock's sixteenth since PLAY
        int8_t machine;    // -1: the clock's own, logged on every straight step
        int8_t pattern;
        int8_t index;      // the pattern's step
    };

    std::atomic<bool> recording { false };
    bool takePlayed(Played&);   // message thread: the next step logged, if there is one

    // any thread: where the clock is, in ticks from where PLAY started it; -1 stopped
    double clockPosition() const { return clockAt.load(std::memory_order_relaxed); }

    //
    // THE CONTROLS the song's automation moves, by owner - a machine, or FX, the effects
    // units - and index: a machine's knobs and switches as its table lists them, then its
    // mixer channel's (level, pan, the sends, MUTE and the inserts); the effects units' as
    // their table lists them. A control is saved by its id.
    //
    static constexpr int FX = channels, owners = channels + 1, maxParams = 64;

    int knobCount(int owner) const;   // a machine's own, before its mixer channel's
    int paramCount(int owner) const;
    std::string paramId(int owner, int i) const;
    std::string paramName(int owner, int i) const;
    int paramIndex(int owner, const std::string& id) const;   // -1 if it has none of that id
    float paramLo(int owner, int i) const;
    float paramHi(int owner, int i) const;
    bool paramStepped(int owner, int i) const;
    float paramGet(int owner, int i) const;
    void paramSet(int owner, int i, float);   // any thread

    // a control held by hand on the panel: the song's automation leaves it be meanwhile
    std::atomic<bool> held[owners][maxParams] {};

private:
    int beginBlock(int n);
    void endBlock();
    void dispatch(const SongPlayer::Due&);
    void songProgram(int machine, int program);   // a part's patch, -1 the panel's
    void songProgramsAt(int tick);                 // every machine's, as the song has them there
    void startMachines();
    void stopMachines();
    void startMachine(int m);   // one machine, as startMachines and stopMachines do them all
    void stopMachine(int m);
    void stepMachine(int m, bool isStep);   // a step, or the half way between, on one machine
    void applyStops();          // at the start of a bar: each machine's STOP, as wanted
    void applyLive();           // and in SONG mode, each machine's pattern or its track
    void applyAutomation();     // the song's automation, where the song is, once a block
    void renderBlock(float* left, float* right, int n);
    void renderMachines(int from, int to);
    void renderPcfEnvelope(int n);

    Clock clock;
    std::atomic<float> clockTempo { 120.0f };
    int nudging = 0;          // the nudge being held, and for how long, in samples
    double nudgeHeld = 0;
    int blockSize = 512;
    std::vector<float> buffers[channels];
    std::vector<float> rights[channels];   // the right side of a stereo machine - a 950, a 106; buffers[] its left
    bool wasInRack[channels] {};             // the audio thread's
    float gainL[channels] {}, gainR[channels] {}, masterGain = 0;

    // the effects: an insert of each per channel, sharing the units' knobs
    struct Inserts
    {
        Distortion dist[2];
        pcf::Filter pcf;
        Compressor comp;
    };
    Inserts inserts[channels];
    float sendGain[channels] {}, reverbGain[channels] {};   // the DELAY's sends, and the REVERB's

    // the PCF's envelope, struck on every step, and where in this block each strike lands
    struct Strike { int at; float level; };
    Strike strikes[64];
    int strikeCount = 0;
    pcf::Envelope pcfEnv;
    std::atomic<int> pcfPosition { -1 };
    std::vector<float> envelope;

    StepDelay delay;
    Reverb reverb;
    std::vector<float> sends, reverbSends;
    Compressor masterComp;
    Limiter limiter;

    // the song: the one playing, the next handed over, and the last handed back
    Playback* playback = nullptr;
    std::atomic<Playback*> pending { nullptr }, retired { nullptr };
    SongPlayer player;
    static constexpr int maxSongDue = 512;
    struct SongDue { int at; SongPlayer::Due due; };
    SongDue songDue[maxSongDue];
    int playingMode = Mode::Pattern;
    std::atomic<int> modeNow { Mode::Pattern };
    std::atomic<int> locateTo { -1 };
    std::atomic<double> songTick { -1.0 };
    std::atomic<double> clockAt { -1.0 };

    // the steps logged while recording: written by the audio thread, read by the panel
    static constexpr int maxPlayed = 8192;
    Played played[maxPlayed];
    std::atomic<int> playedIn { 0 }, playedOut { 0 };
    void logPlayed(int64_t step, int machine);
};

} // namespace debirth
