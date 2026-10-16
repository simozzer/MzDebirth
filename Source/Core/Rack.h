// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Clock.h"
#include "Effects.h"
#include "Machines.h"
#include "Sampler.h"

#include <atomic>
#include <vector>

namespace debirth
{

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

    void prepare(double sampleRate, int maxBlock);
    void render(float* left, float* right, int n);

    // the transport
    std::atomic<bool>  playing { false };
    std::atomic<float> tempo   { 120.0f };
    std::atomic<float> shuffle { 0.0f };    // 0 to 1
    std::atomic<float> master  { 0.8f };

    Mz303 mz303;
    Mz101 mz101;
    Mz808 mz808;
    Mz909 mz909;
    Mz950 mz950;
    Channel channel[channels];

    // the effects units' knobs
    Knobs<EffectSettings> fx { controlsFx() };

    // any thread: the PCF's step, for its lights; -1 stopped
    int pcfStep() const { return pcfPosition.load(std::memory_order_relaxed); }

    // every machine's patterns and position, in rack order
    TrackBase& track(int machine);

private:
    void renderBlock(float* left, float* right, int n);
    void renderMachines(int from, int to);
    void renderPcfEnvelope(int n);

    Clock clock;
    int blockSize = 512;
    std::vector<float> buffers[channels];
    std::vector<float> sampleRight;          // the sampler's right side; buffers[M950] is its left
    float gainL[channels] {}, gainR[channels] {}, masterGain = 0;

    // the effects: an insert of each per channel, sharing the units' knobs
    struct Inserts
    {
        Distortion dist[2];
        PatternFilter pcf;
        Compressor comp;
    };
    Inserts inserts[channels];
    float sendGain[channels] {};

    // the PCF's envelope, struck on every step, and where in this block each strike lands
    struct Strike { int at; float level; };
    Strike strikes[64];
    int strikeCount = 0;
    float pcfEnv = 0;
    std::atomic<int> pcfPosition { -1 };
    std::vector<float> envelope;

    StepDelay delay;
    std::vector<float> sends;
    Compressor masterComp;
};

} // namespace debirth
