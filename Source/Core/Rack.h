// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Clock.h"
#include "Effects.h"
#include "Key.h"
#include "Machines.h"
#include "Sampler.h"
#include "SongPlayer.h"

#include <atomic>
#include <memory>
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
    std::atomic<float> level { 0.7f };
    std::atomic<float> pan   { 0.0f };
    std::atomic<bool>  mute  { false };

    std::atomic<bool>  dist  { false };
    std::atomic<bool>  pcf   { false };
    std::atomic<bool>  comp  { false };
    std::atomic<float> delay { 0.0f };    // the send, 0 to 1
};

//
// The whole rack: the clock, the five machines on it, top to bottom, and the mixer they go
// through. render() is called from the audio thread; the rest is the panel's, except where
// a machine says otherwise.
//
class Rack
{
public:
    enum { M303, M101, M808, M909, M950, channels };

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

    Mz303 mz303;
    Mz101 mz101;
    Mz808 mz808;
    Mz909 mz909;
    Mz950 mz950;
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

private:
    int beginBlock(int n);
    void endBlock();
    void dispatch(const SongPlayer::Due&);
    void startMachines();
    void stopMachines();
    void renderBlock(float* left, float* right, int n);
    void renderMachines(int from, int to);
    void renderPcfEnvelope(int n);

    Clock clock;
    std::atomic<float> clockTempo { 120.0f };
    int nudging = 0;          // the nudge being held, and for how long, in samples
    double nudgeHeld = 0;
    int blockSize = 512;
    std::vector<float> buffers[channels];
    std::vector<float> sampleRight;          // the sampler's right side; buffers[M950] is its left
    float gainL[channels] {}, gainR[channels] {}, masterGain = 0;

    // the effects: an insert of each per channel, sharing the units' knobs
    struct Inserts
    {
        Distortion dist[2];
        pcf::Filter pcf;
        Compressor comp;
    };
    Inserts inserts[channels];
    float sendGain[channels] {};

    // the PCF's envelope, struck on every step, and where in this block each strike lands
    struct Strike { int at; float level; };
    Strike strikes[64];
    int strikeCount = 0;
    pcf::Envelope pcfEnv;
    std::atomic<int> pcfPosition { -1 };
    std::vector<float> envelope;

    StepDelay delay;
    std::vector<float> sends;
    Compressor masterComp;

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
};

} // namespace debirth
