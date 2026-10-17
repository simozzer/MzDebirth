// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <cmath>
#include <cstdint>

namespace debirth
{

//
// The rack's clock, in sixteenth notes. It does not play anything: it says, to the sample,
// where in the next block each step begins and where its gate closes, half a step later.
//
// It runs two lanes side by side, as ReBirth's machines each have their own SHUFFLE switch:
// lane 0 straight, and lane 1 shuffled, its off-beat sixteenths held back by up to a third
// of a step - at full shuffle a triplet swing, two to one.
//
// Position is kept in steps rather than samples, so a tempo change mid-bar moves on from
// where it is rather than jumping.
//
class Clock
{
public:
    enum class Event { Step, GateOff };

    static constexpr double minTempo = 40, maxTempo = 300;
    static constexpr double maxSwing = 1.0 / 3.0;   // of a step, at full shuffle

    void prepare(double sampleRate) { rate = sampleRate; }

    void setTempo(double bpm)       { tempo = bpm < minTempo ? minTempo : (bpm > maxTempo ? maxTempo : bpm); }
    void setShuffle(double amount)  { shuffle = amount < 0 ? 0 : (amount > 1 ? 1 : amount); }

    double samplesPerStep() const   { return rate * 15.0 / tempo; }
    double getTempo() const         { return tempo; }
    double getShuffle() const       { return shuffle; }
    double getRate() const          { return rate; }

    // where the next block starts, in quarter notes from PLAY - for the synced LFOs
    double quarters() const         { return phase / 4.0; }

    // From PLAY, or in SONG mode from wherever the song's cursor is, in steps.
    void start(double fromStep = 0)
    {
        locate(fromStep);
        running = true;
    }

    // Jumps to a step - the song's cycle going round, or its cursor moved while playing. A
    // gate waiting to close is forgotten: whatever was sounding is let go by the caller.
    void locate(double step)
    {
        phase = step;
        for (auto& l : lanes) l = Lane { (int64_t) std::ceil(step - 1.0e-9), 0, false };
    }

    // where the next block starts, in steps from the top of the song (or from PLAY)
    double position() const { return phase; }

    // the next straight step to begin
    int64_t nextStep() const { return lanes[0].next; }

    void stop()            { running = false; }
    bool isRunning() const { return running; }

    //
    // Calls f(offset, lane, event, step) for each event in the next n samples, in order,
    // then moves on by n. An event lands on the first sample at or after its time.
    //
    template <class F>
    void advance(int n, F&& f)
    {
        if (! running) return;

        const double perStep = samplesPerStep();
        const double end = phase + (double) n / perStep;

        for (;;)
        {
            // the earliest thing due, in either lane; straight first on a tie
            int lane = -1;
            bool off = false;
            double t = 0;

            for (int i = 0; i < 2; ++i)
            {
                const Lane& l = lanes[i];
                const double stepAt = (double) l.next + delay(i, l.next);
                const bool o = l.offDue && l.offAt <= stepAt;
                const double ti = o ? l.offAt : stepAt;
                if (lane < 0 || ti < t) { lane = i; off = o; t = ti; }
            }

            if (t >= end) break;
            const int at = (int) std::ceil((t - phase) * perStep - 1.0e-6);
            if (at >= n) break;

            Lane& l = lanes[lane];
            if (off)
            {
                l.offDue = false;
                f(at < 0 ? 0 : at, lane, Event::GateOff, l.next - 1);
            }
            else
            {
                l.offAt = t + 0.5;
                l.offDue = true;
                f(at < 0 ? 0 : at, lane, Event::Step, l.next++);
            }
        }

        phase = end;
    }

private:
    struct Lane
    {
        int64_t next = 0;      // the next step to begin
        double offAt = 0;      // and when the one sounding lets go, in steps
        bool offDue = false;
    };

    double delay(int lane, int64_t step) const
    {
        return lane == 1 && (step & 1) ? shuffle * maxSwing : 0.0;
    }

    double rate = 48000, tempo = 120, shuffle = 0;
    double phase = 0;
    Lane lanes[2];
    bool running = false;
};

} // namespace debirth
