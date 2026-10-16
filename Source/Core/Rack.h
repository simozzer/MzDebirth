// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Clock.h"
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
struct Channel
{
    std::atomic<float> level { 0.7f };
    std::atomic<float> pan   { 0.0f };
    std::atomic<bool>  mute  { false };
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

    // every machine's patterns and position, in rack order
    TrackBase& track(int machine);

private:
    void renderBlock(float* left, float* right, int n);
    void renderMachines(int from, int to);

    Clock clock;
    int blockSize = 512;
    std::vector<float> buffers[channels];
    std::vector<float> sampleRight;          // the sampler's right side; buffers[M950] is its left
    float gainL[channels] {}, gainR[channels] {}, masterGain = 0;
};

} // namespace debirth
