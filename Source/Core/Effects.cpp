// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Effects.h"

#include <algorithm>
#include <cmath>

namespace debirth
{

namespace
{
    constexpr float pi = 3.14159265358979f;

    float onePole(float hz, float rate) { return 1.0f - std::exp(-2.0f * pi * hz / rate); }
    float timeCoef(float seconds, float rate) { return std::exp(-1.0f / (seconds * rate)); }
}

const std::vector<Control<EffectSettings>>& controlsFx()
{
    using S = EffectSettings;
    static const std::vector<Control<S>> table = {
        { "distAmount",    "AMOUNT",   0,   1, &S::distAmount },
        { "distShape",     "SHAPE",    0,   1, &S::distShape },
        { "pcfPattern",    "PATTERN",  0,  31, nullptr, &S::pcfPattern },
        { "pcfFreq",       "FREQ",     0,   1, &S::pcfFreq },
        { "pcfQ",          "Q",        0,   1, &S::pcfQ },
        { "pcfAmount",     "AMT",      0,   1, &S::pcfAmount },
        { "pcfDecay",      "DEC",      0,   1, &S::pcfDecay },
        { "pcfMode",       "MODE",     0,   1, nullptr, &S::pcfMode, "LP|BP" },
        { "delaySteps",    "STEPS",    1,  16, nullptr, &S::delaySteps },
        { "delayTriplet",  "TRIPLET",  0,   1, nullptr, &S::delayTriplet, "OFF|ON" },
        { "delayFeedback", "F.BACK",   0,   1, &S::delayFeedback },
        { "delayPan",      "PAN",     -1,   1, &S::delayPan },
        { "compRatio",     "RATIO",    1,  20, &S::compRatio },
        { "compThreshold", "THRES",  -40,   0, &S::compThreshold },
        { "compMaster",    "MASTER",   0,   1, nullptr, &S::compMaster, "OFF|ON" },
        { "reverbDecay",   "DECAY",    0,   1, &S::reverbDecay },
        { "reverbTone",    "TONE",     0,   1, &S::reverbTone },
        { "reverbPre",     "PRE",      0,   1, &S::reverbPre },
    };
    return table;
}

//============================================================================== PCF

pcf::Settings pcfSettings(const EffectSettings& s)
{
    pcf::Settings p;
    p.pattern = s.pcfPattern;
    p.freq    = s.pcfFreq;
    p.q       = s.pcfQ;
    p.amount  = s.pcfAmount;
    p.decay   = s.pcfDecay;
    p.mode    = s.pcfMode;
    return p;
}

//============================================================================== Distortion

void Distortion::prepare(double rate)
{
    lpCoef = onePole(7000.0f, (float) rate);
    dcCoef = 1.0f - 2.0f * pi * 5.0f / (float) rate;   // 5 Hz: low enough not to tilt a bass note's clipped top
    lp = dcX = dcY = 0;
}

float Distortion::shaper(float u, float shape)
{
    // soft (tanh) into hard (a clip) over the first half of SHAPE, then hard into a fuzz
    // that leans one way, for the even harmonics
    const float soft = std::tanh(u);
    const float hard = std::clamp(u, -1.0f, 1.0f);
    if (shape <= 0.5f)
    {
        const float s = shape * 2.0f;
        return soft + (hard - soft) * s;
    }
    const float fuzz = std::tanh(u + 0.7f * std::fabs(u)) * 0.85f;
    const float s = (shape - 0.5f) * 2.0f;
    return hard + (fuzz - hard) * s;
}

void Distortion::process(float* x, int n, float amount, float shape)
{
    const float drive = std::pow(2.0f, std::clamp(amount, 0.0f, 1.0f) * 6.0f);   // 1 to 64
    const float makeup = 0.3f / std::max(1.0e-3f, std::fabs(shaper(0.3f * drive, shape)));

    for (int i = 0; i < n; ++i)
    {
        const float y = shaper(x[i] * drive, shape) * makeup;
        lp += (y - lp) * lpCoef;

        // the fuzz is lopsided, so take its DC back out
        const float out = lp - dcX + dcCoef * dcY;
        dcX = lp;
        dcY = out;
        x[i] = out;
    }
}

//============================================================================== Compressor

void Compressor::prepare(double rate)
{
    attack = timeCoef(0.004f, (float) rate);
    release = timeCoef(0.12f, (float) rate);
    env = 0;
}

void Compressor::process(float* left, float* right, int n, float thresholdDb, float ratio)
{
    ratio = std::max(1.0f, ratio);
    const float slope = 1.0f - 1.0f / ratio;
    const float makeupDb = -thresholdDb * slope * 0.5f;
    float most = 0;

    for (int i = 0; i < n; ++i)
    {
        const float peak = std::max(std::fabs(left[i]), right != nullptr ? std::fabs(right[i]) : 0.0f);
        const float coef = peak > env ? attack : release;
        env = peak + (env - peak) * coef;

        const float levelDb = 20.0f * std::log10(env + 1.0e-9f);
        const float over = levelDb - thresholdDb;

        // a soft knee, 6 dB wide
        float reductionDb = 0;
        if (over > 3.0f)        reductionDb = over * slope;
        else if (over > -3.0f)  reductionDb = slope * (over + 3.0f) * (over + 3.0f) / 12.0f;
        most = std::max(most, reductionDb);

        const float gain = std::pow(10.0f, (makeupDb - reductionDb) / 20.0f);
        left[i] *= gain;
        if (right != nullptr) right[i] *= gain;
    }

    lastReductionDb = most;
}

//============================================================================== StepDelay

void StepDelay::prepare(double rate, double maxSeconds)
{
    line.assign((size_t) (rate * maxSeconds) + 8, 0.0f);
    dampCoef = onePole(4500.0f, (float) rate);
    clear();
}

void StepDelay::clear()
{
    std::fill(line.begin(), line.end(), 0.0f);
    write = 0;
    time = -1;
    damp = 0;
}

void StepDelay::process(const float* in, float* left, float* right, int n,
                        double delaySamples, float feedback, float pan)
{
    if (line.size() < 8) return;

    const int size = (int) line.size();
    const double target = std::clamp(delaySamples, 1.0, (double) size - 4);
    if (time < 0) time = target;

    const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.25f * pi;
    const float gl = std::cos(angle) * 1.41421356f, gr = std::sin(angle) * 1.41421356f;
    feedback = std::clamp(feedback, 0.0f, 0.95f);

    for (int i = 0; i < n; ++i)
    {
        time += (target - time) * 0.0004;

        double read = (double) write - time;
        while (read < 0) read += size;
        const int r0 = (int) read;
        const int r1 = r0 + 1 < size ? r0 + 1 : 0;
        const float frac = (float) (read - r0);
        const float y = line[(size_t) r0] + (line[(size_t) r1] - line[(size_t) r0]) * frac;

        damp += (y - damp) * dampCoef;
        line[(size_t) write] = in[i] + std::tanh(damp * feedback);
        if (++write >= size) write = 0;

        left[i] += y * gl;
        right[i] += y * gr;
    }
}

//============================================================================== REVERB

namespace
{
    // Freeverb's lengths, in samples at 44.1 kHz: no two share a factor, so their echoes
    // never pile up on the same moment. The right side runs 23 samples longer.
    constexpr int combLengths[8]   = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    constexpr int allPassLengths[4] = { 556, 441, 341, 225 };
    constexpr int stereoSpread = 23;

    // what goes in, scaled so eight combs ringing together come back at about the level sent -
    // Freeverb's own calibration: its 0.015 in, and its wet of a third times three out
    constexpr float inputGain = 0.015f, wetGain = 1.0f;

    // a decaying tail runs down into numbers so small they slow some processors to a crawl
    inline float flush(float x) { return std::abs(x) < 1.0e-20f ? 0.0f : x; }
}

void Reverb::prepare(double sampleRate)
{
    rate = sampleRate;
    const double k = rate / 44100.0;
    for (int side = 0; side < 2; ++side)
    {
        for (int i = 0; i < 8; ++i)
            combs[side][i].buf.assign((size_t) std::max(1.0, (combLengths[i] + side * stereoSpread) * k), 0.0f);
        for (int i = 0; i < 4; ++i)
            allPasses[side][i].buf.assign((size_t) std::max(1.0, (allPassLengths[i] + side * stereoSpread) * k), 0.0f);
    }
    pre.assign((size_t) (rate * maxPreDelay) + 2, 0.0f);
    clear();
}

void Reverb::clear()
{
    for (auto& side : combs)
        for (auto& c : side) { std::fill(c.buf.begin(), c.buf.end(), 0.0f); c.pos = 0; c.store = 0; }
    for (auto& side : allPasses)
        for (auto& a : side) { std::fill(a.buf.begin(), a.buf.end(), 0.0f); a.pos = 0; }
    std::fill(pre.begin(), pre.end(), 0.0f);
    prePos = 0;
}

void Reverb::process(const float* in, float* left, float* right, int n,
                     float decay, float tone, float preDelay)
{
    if (pre.size() < 2) return;

    // DECAY: how much each pass round a comb keeps - Freeverb's room size. TONE: how much of
    // the top each pass loses, the damping, from much (dark) to little (bright).
    const float feedback = 0.70f + 0.28f * std::clamp(decay, 0.0f, 1.0f);
    const float damp = 0.05f + 0.6f * (1.0f - std::clamp(tone, 0.0f, 1.0f));
    const int preSize = (int) pre.size();
    const int preSamples = std::clamp((int) (std::clamp(preDelay, 0.0f, 1.0f) * maxPreDelay * rate), 0, preSize - 1);

    for (int i = 0; i < n; ++i)
    {
        // the pre-delay: the send, a moment late
        pre[(size_t) prePos] = in[i];
        int r = prePos - preSamples;
        if (r < 0) r += preSize;
        const float x = pre[(size_t) r] * inputGain;
        if (++prePos >= preSize) prePos = 0;

        float out[2] = { 0.0f, 0.0f };
        for (int side = 0; side < 2; ++side)
        {
            float sum = 0.0f;
            for (auto& c : combs[side])
            {
                const float y = c.buf[(size_t) c.pos];
                c.store = flush(y * (1.0f - damp) + c.store * damp);
                c.buf[(size_t) c.pos] = x + c.store * feedback;
                if (++c.pos >= (int) c.buf.size()) c.pos = 0;
                sum += y;
            }
            for (auto& a : allPasses[side])
            {
                const float b = a.buf[(size_t) a.pos];
                a.buf[(size_t) a.pos] = flush(sum + b * 0.5f);
                if (++a.pos >= (int) a.buf.size()) a.pos = 0;
                sum = b - sum;
            }
            out[side] = sum;
        }

        left[i] += out[0] * wetGain;
        right[i] += out[1] * wetGain;
    }
}

} // namespace debirth
