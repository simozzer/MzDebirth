// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Rack.h"

#include <algorithm>
#include <cmath>

namespace debirth
{

//============================================================================== the panels

const std::vector<Control<tb303::Settings>>& controls303()
{
    using S = tb303::Settings;
    static const std::vector<Control<S>> table = {
        { "tune",   "TUNING",  -100.0f, 100.0f, &S::tune },
        { "cutoff", "CUT OFF",    0.0f,  10.0f, &S::cutoff },
        { "res",    "RESO",       0.0f,  10.0f, &S::res },
        { "envMod", "ENV MOD",    0.0f,  10.0f, &S::envMod },
        { "decay",  "DECAY",      0.0f,  10.0f, &S::decay },
        { "accent", "ACCENT",     0.0f,  10.0f, &S::accent },
        { "wave",   "WAVEFORM",   0.0f,   1.0f, nullptr, &S::wave, "SAW|SQR" },
        { "volume", "VOLUME",     0.0f,  10.0f, &S::volume },
    };
    return table;
}

//
// The 101's panel, less what the rack does for it: its arpeggiator, its sequencer and its
// clock are the rack's now, and there is no bender to set the range of.
//
const std::vector<Control<sh101::Settings>>& controls101()
{
    using S = sh101::Settings;
    static const std::vector<Control<S>> table = {
        { "lfoRate",   "RATE",     0, 10, &S::lfoRate },
        { "lfoWave",   "WAVE",     0,  3, nullptr, &S::lfoWave,   "TRI|SQR|RND|NOISE" },
        { "lfoSync",   "SYNC",     0,  1, nullptr, &S::lfoSync,   "FREE|TEMPO" },
        { "vcoMod",    "MOD",      0, 10, &S::vcoMod },
        { "range",     "RANGE",    0,  3, nullptr, &S::range,     "16'|8'|4'|2'" },
        { "pw",        "PW",       0, 10, &S::pw },
        { "pwmSource", "PWM",      0,  2, nullptr, &S::pwmSource, "LFO|MAN|ENV" },
        { "pulse",     "PULSE",    0, 10, &S::pulse },
        { "saw",       "SAW",      0, 10, &S::saw },
        { "sub",       "SUB",      0, 10, &S::sub },
        { "subType",   "SUB WAVE", 0,  2, nullptr, &S::subType,   "-1 OCT|-2 OCT|-2 PLS" },
        { "noise",     "NOISE",    0, 10, &S::noise },
        { "portaMode", "PORTA",    0,  2, nullptr, &S::portaMode, "OFF|AUTO|ON" },
        { "portaTime", "TIME",     0, 10, &S::portaTime },
        { "cutoff",    "FREQ",     0, 10, &S::cutoff },
        { "res",       "RES",      0, 10, &S::res },
        { "vcfEnv",    "ENV",      0, 10, &S::vcfEnv },
        { "vcfMod",    "MOD",      0, 10, &S::vcfMod },
        { "vcfKybd",   "KYBD",     0, 10, &S::vcfKybd },
        { "vcaMode",   "VCA",      0,  1, nullptr, &S::vcaMode,   "ENV|GATE" },
        { "envTrig",   "TRIG",     0,  2, nullptr, &S::envTrig,   "G+T|GATE|LFO" },
        { "attack",    "A",        0, 10, &S::attack },
        { "decay",     "D",        0, 10, &S::decay },
        { "sustain",   "S",        0, 10, &S::sustain },
        { "release",   "R",        0, 10, &S::release },
        { "transpose", "TRANS",    0,  2, nullptr, &S::transpose, "LOW|MID|HIGH" },
        { "tune",      "TUNE",   -50, 50, &S::tune },
        { "volume",    "VOLUME",   0, 10, &S::volume },
    };
    return table;
}

const std::vector<Control<tr808::Settings>>& controls808()
{
    using S = tr808::Settings;
    static const std::vector<Control<S>> table = {
        { "accent",   "ACCENT", 0, 10, &S::accent },
        { "bdLevel",  "LEVEL",  0, 10, &S::bdLevel },
        { "bdTone",   "TONE",   0, 10, &S::bdTone },
        { "bdDecay",  "DECAY",  0, 10, &S::bdDecay },
        { "sdLevel",  "LEVEL",  0, 10, &S::sdLevel },
        { "sdTone",   "TONE",   0, 10, &S::sdTone },
        { "sdSnappy", "SNAPPY", 0, 10, &S::sdSnappy },
        { "ltLevel",  "LEVEL",  0, 10, &S::ltLevel },
        { "ltTuning", "TUNING", 0, 10, &S::ltTuning },
        { "mtLevel",  "LEVEL",  0, 10, &S::mtLevel },
        { "mtTuning", "TUNING", 0, 10, &S::mtTuning },
        { "htLevel",  "LEVEL",  0, 10, &S::htLevel },
        { "htTuning", "TUNING", 0, 10, &S::htTuning },
        { "rsLevel",  "LEVEL",  0, 10, &S::rsLevel },
        { "cpLevel",  "LEVEL",  0, 10, &S::cpLevel },
        { "cbLevel",  "LEVEL",  0, 10, &S::cbLevel },
        { "cyLevel",  "LEVEL",  0, 10, &S::cyLevel },
        { "cyTone",   "TONE",   0, 10, &S::cyTone },
        { "cyDecay",  "DECAY",  0, 10, &S::cyDecay },
        { "ohLevel",  "LEVEL",  0, 10, &S::ohLevel },
        { "ohDecay",  "DECAY",  0, 10, &S::ohDecay },
        { "chLevel",  "LEVEL",  0, 10, &S::chLevel },
        { "volume",   "VOLUME", 0, 10, &S::volume },
    };
    return table;
}

const std::vector<Control<tr909::Settings>>& controls909()
{
    using S = tr909::Settings;
    static const std::vector<Control<S>> table = {
        { "accent",   "ACCENT", 0, 10, &S::accent },
        { "bdLevel",  "LEVEL",  0, 10, &S::bdLevel },
        { "bdTune",   "TUNE",   0, 10, &S::bdTune },
        { "bdAttack", "ATTACK", 0, 10, &S::bdAttack },
        { "bdDecay",  "DECAY",  0, 10, &S::bdDecay },
        { "sdLevel",  "LEVEL",  0, 10, &S::sdLevel },
        { "sdTune",   "TUNE",   0, 10, &S::sdTune },
        { "sdTone",   "TONE",   0, 10, &S::sdTone },
        { "sdSnappy", "SNAPPY", 0, 10, &S::sdSnappy },
        { "ltLevel",  "LEVEL",  0, 10, &S::ltLevel },
        { "ltTune",   "TUNE",   0, 10, &S::ltTune },
        { "ltDecay",  "DECAY",  0, 10, &S::ltDecay },
        { "mtLevel",  "LEVEL",  0, 10, &S::mtLevel },
        { "mtTune",   "TUNE",   0, 10, &S::mtTune },
        { "mtDecay",  "DECAY",  0, 10, &S::mtDecay },
        { "htLevel",  "LEVEL",  0, 10, &S::htLevel },
        { "htTune",   "TUNE",   0, 10, &S::htTune },
        { "htDecay",  "DECAY",  0, 10, &S::htDecay },
        { "rsLevel",  "RIM",    0, 10, &S::rsLevel },
        { "cpLevel",  "CLAP",   0, 10, &S::cpLevel },
        { "hhLevel",  "LEVEL",  0, 10, &S::hhLevel },
        { "chDecay",  "CH DEC", 0, 10, &S::chDecay },
        { "ohDecay",  "OH DEC", 0, 10, &S::ohDecay },
        { "crLevel",  "LEVEL",  0, 10, &S::crLevel },
        { "crTune",   "TUNE",   0, 10, &S::crTune },
        { "rdLevel",  "LEVEL",  0, 10, &S::rdLevel },
        { "rdTune",   "TUNE",   0, 10, &S::rdTune },
        { "volume",   "VOLUME", 0, 10, &S::volume },
    };
    return table;
}

//============================================================================== Rack

Rack::Rack()
    : mz303(controls303(), 36),   // C2
      mz101(controls101(), 48),   // C3, the 101's own middle
      mz808(controls808()),
      mz909(controls909())
{
}

TrackBase& Rack::track(int machine)
{
    switch (machine)
    {
        case M303: return mz303.track;
        case M101: return mz101.track;
        case M808: return mz808.track;
        case M909: return mz909.track;
        default:   return mz950.track;
    }
}

void Rack::prepare(double sampleRate, int maxBlock)
{
    blockSize = maxBlock > 0 ? maxBlock : 512;
    for (auto& b : buffers) b.assign((size_t) blockSize, 0.0f);
    sampleRight.assign((size_t) blockSize, 0.0f);

    clock.prepare(sampleRate);
    clock.stop();

    mz303.prepare(sampleRate);
    mz101.prepare(sampleRate);
    mz808.prepare(sampleRate);
    mz909.prepare(sampleRate);
    mz950.prepare(sampleRate);

    mz303.stop();
    mz101.stop();
    mz808.stop();
    mz909.stop();
    mz950.stop();

    for (auto& in : inserts)
    {
        in.dist[0].prepare(sampleRate);
        in.dist[1].prepare(sampleRate);
        in.pcf.prepare(sampleRate);
        in.comp.prepare(sampleRate);
    }
    envelope.assign((size_t) blockSize, 0.0f);
    sends.assign((size_t) blockSize, 0.0f);
    pcfEnv.prepare(sampleRate);
    pcfPosition = -1;

    // sixteen steps at the slowest tempo, and some over
    delay.prepare(sampleRate, 16.0 * 15.0 / Clock::minTempo + 0.5);
    masterComp.prepare(sampleRate);
}

Rack::~Rack()
{
    delete playback;
    delete pending.exchange(nullptr);
    delete retired.exchange(nullptr);
}

void Rack::render(float* left, float* right, int n)
{
    while (n > 0)
    {
        const int chunk = beginBlock(n < blockSize ? n : blockSize);
        renderBlock(left, right, chunk);
        endBlock();
        if (nudging != 0) nudgeHeld += chunk;
        left += chunk;
        right += chunk;
        n -= chunk;
    }
}

//============================================================================== the song

void Rack::publish(std::unique_ptr<Playback> p)
{
    delete pending.exchange(p.release());   // one the audio thread never took is just replaced
}

void Rack::collect()
{
    delete retired.exchange(nullptr);
}

void Rack::dispatch(const SongPlayer::Due& d)
{
    const bool on = d.kind == SongPlayer::Kind::On;
    switch (d.machine)
    {
        case M303: if (on) mz303.songNoteOn(d.key, d.velocity, d.slide); else mz303.songNoteOff(d.key); break;
        case M101: if (on) mz101.songNoteOn(d.key, d.velocity, d.slide); else mz101.songNoteOff(d.key); break;
        case M808: if (on) mz808.hit(d.key, d.velocity); break;
        case M909: if (on) mz909.hit(d.key, d.velocity); break;
        case M950: if (on) mz950.songNoteOn(d.key, d.velocity); else mz950.songNoteOff(d.key); break;
        default: break;
    }
}

void Rack::startMachines()
{
    mz303.start(); mz101.start(); mz808.start(); mz909.start(); mz950.start();
}

void Rack::stopMachines()
{
    mz303.stop(); mz101.stop(); mz808.stop(); mz909.stop(); mz950.stop();
    player.forget();
}

//
// Before each block: the transport, a new song from the panel, a jump of the cursor, a
// change of mode - and how much to render, so that the block ends exactly where the cycle
// goes round, or on the bar a change of mode waits for.
//
int Rack::beginBlock(int n)
{
    // a new song, if the last one handed back has been collected
    if (retired.load() == nullptr)
        if (Playback* p = pending.exchange(nullptr))
        {
            retired.store(playback);
            playback = p;
            player.setPlayback(p);
        }

    // the tempo, bent by DRAG or PUSH while one is held; a change of hand starts afresh
    const int dir = nudge.load();
    if (dir != nudging) { nudging = dir; nudgeHeld = 0; }
    const double held = std::min(1.0, nudgeHeld / (nudgeRamp * clock.getRate()));
    const double bend = nudging * (nudgeFrom + (nudgeTo - nudgeFrom) * held);
    clock.setTempo(tempo.load() * (1.0 + (nudging != 0 ? bend : 0.0)));
    clockTempo.store((float) clock.getTempo(), std::memory_order_relaxed);
    clock.setShuffle(shuffle.load());

    // the transport, as the panel last left it
    const bool wanted = playing.load();
    if (wanted != clock.isRunning())
    {
        if (wanted)
        {
            playingMode = mode.load();
            const bool fromCursor = playingMode == Mode::Song && playback != nullptr;
            clock.start(fromCursor ? (double) playback->cursor / ticksPerStep : 0.0);
            startMachines();   // in SONG mode their patterns rest, but they start as they would
            player.forget();
        }
        else
        {
            clock.stop();
            stopMachines();
        }
    }

    if (! clock.isRunning())
    {
        playingMode = mode.load();
        modeNow = playingMode;
        songTick = -1;
        locateTo = -1;
        return n;
    }

    const double perStep = clock.samplesPerStep();
    auto samplesTo = [&] (double step) { return (int) std::ceil((step - clock.position()) * perStep - 1.0e-6); };

    // the song's cursor moved while it plays
    if (const int to = locateTo.exchange(-1); to >= 0 && playingMode == Mode::Song)
    {
        clock.locate((double) to / ticksPerStep);
        player.releaseAll([this] (int, const SongPlayer::Due& d) { dispatch(d); });
    }

    // A change of mode waits for the bar, as a new pattern does.
    const int wantedMode = mode.load();
    if (wantedMode != playingMode)
    {
        const int64_t bar = (clock.nextStep() + 15) / 16 * 16;
        const int toBar = samplesTo((double) bar);
        if (toBar <= 0)
        {
            playingMode = wantedMode;
            stopMachines();
            if (playingMode == Mode::Pattern) startMachines();
        }
        else if (toBar < n)
        {
            n = toBar;
        }
    }
    modeNow = playingMode;

    if (playingMode == Mode::Song && playback != nullptr && playback->cycle)
    {
        const int toEnd = samplesTo((double) playback->right / ticksPerStep);
        if (toEnd > 0 && toEnd < n) n = toEnd;
    }

    return n;
}

void Rack::endBlock()
{
    if (! clock.isRunning() || playingMode != Mode::Song)
    {
        songTick = -1;
        return;
    }

    // round the cycle: back to the left locator, and whatever was sounding let go
    if (playback != nullptr && playback->cycle
        && clock.position() >= (double) playback->right / ticksPerStep - 1.0e-6)
    {
        clock.locate((double) playback->left / ticksPerStep);
        player.releaseAll([this] (int, const SongPlayer::Due& d) { dispatch(d); });
    }

    songTick = clock.position() * ticksPerStep;
}

void Rack::renderMachines(int from, int to)
{
    const int n = to - from;
    if (n <= 0) return;

    mz303.render(buffers[M303].data() + from, n);
    mz101.render(buffers[M101].data() + from, n);
    mz808.render(buffers[M808].data() + from, n);
    mz909.render(buffers[M909].data() + from, n);
    mz950.render(buffers[M950].data() + from, sampleRight.data() + from, n);
}

void Rack::renderBlock(float* left, float* right, int n)
{
    const bool running = clock.isRunning();
    const bool song = running && playingMode == Mode::Song;
    const double quarters = clock.quarters(), bpm = clock.getTempo();
    mz303.update(running, quarters, bpm);
    mz101.update(running, quarters, bpm);
    mz808.update();
    mz909.update();
    mz950.update(running, quarters, bpm);

    int lane[channels];
    for (int m = 0; m < channels; ++m)
        lane[m] = track(m).shuffle.load() ? 1 : 0;

    // Render up to each event, then act on it, so every step lands on its sample. The PCF
    // is struck on every straight sixteenth while playing, at its pattern's level for the step.
    strikeCount = 0;
    int pos = 0;

    auto strike = [this] (int at, int l, Clock::Event e, int64_t step)
    {
        if (l == 0 && e == Clock::Event::Step && strikeCount < (int) (sizeof strikes / sizeof strikes[0]))
        {
            const int s = (int) (step % 16);
            strikes[strikeCount++] = { at, pcf::level((int) fx.get(fx.indexOf("pcfPattern")), s) };
            pcfPosition = s;
        }
    };

    if (! song)
    {
        clock.advance(n, [&] (int at, int l, Clock::Event e, int64_t step)
        {
            renderMachines(pos, at);
            pos = at > pos ? at : pos;

            const bool isStep = e == Clock::Event::Step;
            if (l == lane[M303]) { if (isStep) mz303.step(); else mz303.gateOff(); }
            if (l == lane[M101]) { if (isStep) mz101.step(); else mz101.gateOff(); }
            if (l == lane[M808] && isStep) mz808.step();
            if (l == lane[M909] && isStep) mz909.step();
            if (l == lane[M950]) { if (isStep) mz950.step(); else mz950.gateOff(); }

            strike(at, l, e, step);
        });
    }
    else
    {
        // In SONG mode the song's notes play the machines, and the clock's steps only strike
        // the PCF. The machines are still rendered up to each of the clock's events as well
        // as the song's, so they are cut into the same pieces either way and a pattern made
        // into a part sounds exactly as the pattern does.
        int dueCount = 0;
        player.advance(clock.position(), n, clock.samplesPerStep(), clock.getShuffle(),
                       [&] (int at, const SongPlayer::Due& d)
        {
            if (dueCount < maxSongDue) songDue[dueCount++] = { at, d };
        });

        int next = 0;
        clock.advance(n, [&] (int at, int l, Clock::Event e, int64_t step)
        {
            for (; next < dueCount && songDue[next].at <= at; ++next)
            {
                renderMachines(pos, songDue[next].at);
                pos = std::max(pos, songDue[next].at);
                dispatch(songDue[next].due);
            }
            renderMachines(pos, at);
            pos = at > pos ? at : pos;
            strike(at, l, e, step);
        });

        for (; next < dueCount; ++next)
        {
            renderMachines(pos, songDue[next].at);
            pos = std::max(pos, songDue[next].at);
            dispatch(songDue[next].due);
        }
    }
    renderMachines(pos, n);
    if (! running) pcfPosition = -1;

    // The effects units' knobs, once a block.
    const EffectSettings settings = fx.read();
    const pcf::Settings pcfPanel = pcfSettings(settings);
    renderPcfEnvelope(n);

    // The mixer. Each channel through its inserts, then its fader and pan, and its send to
    // the delay after the fader. Each gain ramps across the block from where it was, so
    // moving a fader does not click.
    const float inv = 1.0f / (float) n;
    float* send = sends.data();

    for (int i = 0; i < n; ++i) left[i] = right[i] = send[i] = 0.0f;

    for (int c = 0; c < channels; ++c)
    {
        const Channel& ch = channel[c];
        float* srcL = buffers[c].data();
        float* srcR = c == M950 ? sampleRight.data() : nullptr;

        Inserts& in = inserts[c];
        if (ch.dist.load())
        {
            in.dist[0].process(srcL, n, settings.distAmount, settings.distShape);
            if (srcR != nullptr) in.dist[1].process(srcR, n, settings.distAmount, settings.distShape);
        }
        if (ch.pcf.load())  in.pcf.process(srcL, srcR, envelope.data(), n, pcfPanel);
        else                in.pcf.reset();   // switched back in, it starts from rest
        if (ch.comp.load()) in.comp.process(srcL, srcR, n, settings.compThreshold, settings.compRatio);

        const float level = ch.mute.load() ? 0.0f : ch.level.load() * ch.level.load();
        const float angle = (ch.pan.load() + 1.0f) * 0.25f * 3.14159265f;
        const float toL = level * std::cos(angle) * 1.41421356f;
        const float toR = level * std::sin(angle) * 1.41421356f;
        const float toSend = level * ch.delay.load();

        if (srcR == nullptr) srcR = srcL;
        const float dl = (toL - gainL[c]) * inv, dr = (toR - gainR[c]) * inv, ds = (toSend - sendGain[c]) * inv;
        float gl = gainL[c], gr = gainR[c], gs = sendGain[c];

        for (int i = 0; i < n; ++i)
        {
            gl += dl;
            gr += dr;
            gs += ds;
            left[i] += srcL[i] * gl;
            right[i] += srcR[i] * gr;
            send[i] += 0.5f * (srcL[i] + srcR[i]) * gs;
        }

        gainL[c] = toL;
        gainR[c] = toR;
        sendGain[c] = toSend;
    }

    // The delay comes back into the mix, the whole of which can go through the COMP too.
    const double stepLength = clock.samplesPerStep() * (settings.delayTriplet != 0 ? 2.0 / 3.0 : 1.0);
    delay.process(send, left, right, n, stepLength * settings.delaySteps, settings.delayFeedback, settings.delayPan);

    if (settings.compMaster != 0)
        masterComp.process(left, right, n, settings.compThreshold, settings.compRatio);

    const float masterTo = master.load() * master.load();
    const float dm = (masterTo - masterGain) * inv;
    float gm = masterGain;
    for (int i = 0; i < n; ++i)
    {
        gm += dm;
        left[i] *= gm;
        right[i] *= gm;
    }
    masterGain = masterTo;
}

//
// The PCF's envelope across the block: struck to a step's level where each step begins,
// falling away by DECAY in between.
//
void Rack::renderPcfEnvelope(int n)
{
    pcfEnv.setDecay(fx.get(fx.indexOf("pcfDecay")));

    float* e = envelope.data();
    int next = 0;
    for (int i = 0; i < n; ++i)
    {
        while (next < strikeCount && strikes[next].at <= i)
            pcfEnv.strike(strikes[next++].level);
        e[i] = pcfEnv.next();
    }
}

} // namespace debirth
