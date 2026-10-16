// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Knobs.h"

#include <cstdint>
#include <vector>

namespace debirth
{

//
// The effects units' panels, as ReBirth had them: one of each, its knobs shared by every
// channel switched into it.
//
struct EffectSettings
{
    // DIST
    float distAmount = 0.5f;     // 0 to 1: the drive, 0 to 36 dB
    float distShape  = 0.25f;    // 0 to 1: soft, then hard, then a lopsided fuzz

    // PCF - the pattern-controlled filter
    int   pcfPattern = 0;        // 0 to 31
    float pcfFreq    = 0.35f;    // 0 to 1: where the filter rests, 40 Hz to 10 kHz
    float pcfQ       = 0.4f;     // 0 to 1: resonance
    float pcfAmount  = 0.6f;     // 0 to 1: how far a step's level opens it, up to seven octaves
    float pcfDecay   = 0.35f;    // 0 to 1: how fast it closes again, 15 ms to 1.5 s
    int   pcfMode    = 0;        // LP or BP

    // DELAY - a send
    int   delaySteps    = 3;     // 1 to 16 sixteenths
    int   delayTriplet  = 0;     // and two thirds of that
    float delayFeedback = 0.4f;  // 0 to 1
    float delayPan      = 0.0f;  // -1 to +1, where the echoes come back

    // COMP
    float compRatio     = 4.0f;  // 1 to 20
    float compThreshold = -18.0f;// dB, -40 to 0
    int   compMaster    = 0;     // the whole mix through it, too
};

namespace PcfMode { enum { LP, BP }; }

const std::vector<Control<EffectSettings>>& controlsFx();

constexpr int pcfPatterns = 32;

// A PCF pattern's level on a step, 0 to 1.
float pcfLevel(int pattern, int step);

//
// DIST. Driven, shaped, and filtered a little after, as a guitar amp's speaker would, so the
// aliasing the shaper makes does not glare. Level is made up as the drive goes up, so the
// knob changes the colour more than the volume.
//
class Distortion
{
public:
    void prepare(double rate);
    void process(float* x, int n, float amount, float shape);

private:
    static float shaper(float u, float shape);

    float lpCoef = 0.5f, lp = 0;
    float dcCoef = 0.9993f;
    float dcX = 0, dcY = 0;
};

//
// PCF. A state-variable filter whose cutoff is opened by an envelope - the rack strikes the
// envelope on every sixteenth, at that step's level in the pattern, and it dies away by
// DECAY - so the filter plays a rhythm of its own over whatever goes through it. The
// envelope is the rack's, one for every channel; each channel keeps its own filter.
//
class PatternFilter
{
public:
    void prepare(double rate) { fs = (float) rate; reset(); }
    void reset() { for (auto& s : state) s = {}; }

    // env: the envelope, a value a sample. right may be null, for a mono channel.
    void process(float* left, float* right, const float* env, int n, const EffectSettings&);

private:
    struct Svf { float ic1 = 0, ic2 = 0; };
    Svf state[2];
    float fs = 48000;
};

//
// COMP. A peak compressor, the two sides of a stereo channel linked, with its own make-up
// gain - half of what it takes off at the threshold - so switching it in does not just make
// things quieter.
//
class Compressor
{
public:
    void prepare(double rate);
    void process(float* left, float* right, int n, float thresholdDb, float ratio);

    float reduction() const { return lastReductionDb; }   // for a meter, if one is wanted

private:
    float attack = 0.99f, release = 0.999f, env = 0;
    float lastReductionDb = 0;
};

//
// DELAY. One delay line, fed by every channel's send, its time a number of steps at the
// rack's tempo. When the time changes, the read point slews to the new one rather than
// jumping, so it bends like a tape echo rather than clicking. The echoes are darkened a
// little on each pass and can never run away.
//
class StepDelay
{
public:
    void prepare(double rate, double maxSeconds);
    void clear();

    // adds the echoes of `in` to left and right
    void process(const float* in, float* left, float* right, int n,
                 double delaySamples, float feedback, float pan);

private:
    std::vector<float> line;
    int write = 0;
    double time = -1;
    float damp = 0, dampCoef = 0.4f;
};

} // namespace debirth
