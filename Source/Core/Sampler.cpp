// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Sampler.h"

#include <algorithm>

namespace debirth
{

const std::vector<Control<SamplerSettings>>& controls950()
{
    using S = SamplerSettings;
    static const std::vector<Control<S>> table = {
        { "cutoff",        "FILTER",   -99,  99, &S::cutoff },
        { "resonance",     "RESO",       0, 100, &S::resonance },
        { "amount",        "ENV AMT",  -50,  50, &S::amount },
        { "attack",        "ATTACK",   -99,  99, &S::attack },
        { "decay",         "DECAY",    -99,  99, &S::decay },
        { "sustain",       "SUSTAIN",  -99,  99, &S::sustain },
        { "release",       "RELEASE",  -99,  99, &S::release },
        { "velToFilter",   "VEL>FILT", -99,  99, &S::velToFilter },
        { "velToLoudness", "VEL>LOUD", -99,  99, &S::velToLoudness },
        { "gain",          "GAIN",       0,   2, &S::gain },
    };
    return table;
}

//============================================================================== the disk

bool Mz950::loadDisk(const std::string& p, std::string& error)
{
    auto d = std::make_unique<s950::Disk>();
    if (! d->loadFile(p, error)) return false;

    disk = std::move(d);
    path = p;
    programIndex = -1;
    sameDisk = false;

    // the first programme that builds
    const int n = (int) programNames().size();
    for (int i = 0; i < n; ++i)
        if (selectProgram(i)) return true;

    error = "there are no programmes on it";
    return true;   // loaded, but silent
}

std::string Mz950::diskName() const
{
    return disk != nullptr ? disk->getName() : std::string();
}

const s950::Disk::Entry* Mz950::entry(int index) const
{
    if (disk == nullptr || index < 0) return nullptr;

    int seen = 0;
    for (const auto& e : disk->getEntries())
    {
        if (e.type != 'P') continue;
        if (seen++ == index) return &e;
    }
    return nullptr;
}

std::vector<std::string> Mz950::programNames() const
{
    std::vector<std::string> names;
    if (disk != nullptr)
        for (const auto& e : disk->getEntries())
            if (e.type == 'P') names.push_back(e.name);
    return names;
}

bool Mz950::selectProgram(int index)
{
    const auto* e = entry(index);
    if (e == nullptr) return false;

    // reusing the samples the last patch already decoded, where they are the same
    auto built = disk->buildPatch(*e, sameDisk ? patch.get() : nullptr);
    if (built == nullptr || built->keygroups.empty()) return false;

    programIndex = index;
    sameDisk = true;
    patch = built;

    std::lock_guard<std::mutex> g(lock);
    owed = engine == nullptr || ! engine->trySetPatch(patch);
    return true;
}

std::vector<Mz950::Span> Mz950::keySpans() const
{
    std::vector<Span> spans;
    if (const auto* e = entry(programIndex))
        for (const auto& k : disk->keygroups(*e))
            spans.push_back({ k.lowKey, k.highKey, k.zone1.name });

    std::sort(spans.begin(), spans.end(), [] (const Span& a, const Span& b) { return a.low < b.low; });
    return spans;
}

void Mz950::service()
{
    std::lock_guard<std::mutex> g(lock);
    if (engine == nullptr) return;

    engine->collectRetiredPatch();
    if (owed && patch != nullptr)
        owed = ! engine->trySetPatch(patch);
}

//============================================================================== playing

void Mz950::prepare(double sampleRate)
{
    std::lock_guard<std::mutex> g(lock);
    engine = std::make_unique<s950::Engine>(sampleRate);
    if (patch != nullptr) engine->setPatch(patch);
    owed = false;
    std::fill(std::begin(held), std::end(held), false);
}

void Mz950::update(bool running, double quarters, double bpm)
{
    if (engine == nullptr) return;

    const SamplerSettings s = knobs.read();
    auto& t = engine->trims;
    t.cutoff.store(s.cutoff, std::memory_order_relaxed);
    t.resonance.store(s.resonance, std::memory_order_relaxed);
    t.amount.store(s.amount, std::memory_order_relaxed);
    t.vcaAttack.store(s.attack, std::memory_order_relaxed);
    t.vcaDecay.store(s.decay, std::memory_order_relaxed);
    t.vcaSustain.store(s.sustain, std::memory_order_relaxed);
    t.vcaRelease.store(s.release, std::memory_order_relaxed);
    t.velToFilter.store(s.velToFilter, std::memory_order_relaxed);
    t.velToLoudness.store(s.velToLoudness, std::memory_order_relaxed);
    engine->gain.store(s.gain, std::memory_order_relaxed);

    engine->setTransport(bpm, quarters, running, 4.0);
}

void Mz950::render(float* left, float* right, int n)
{
    if (engine == nullptr)
    {
        std::fill(left, left + n, 0.0f);
        std::fill(right, right + n, 0.0f);
        return;
    }
    engine->render(left, right, n);
}

void Mz950::releaseAll()
{
    if (engine != nullptr)
        for (int k = 0; k < NotePattern::notes; ++k)
            if (held[k] || songHeld[k] > 0) engine->noteOff(k);
    std::fill(std::begin(held), std::end(held), false);
    std::fill(std::begin(songHeld), std::end(songHeld), (uint8_t) 0);
}

void Mz950::songNoteOn(int key, int vel)
{
    if (engine == nullptr || key < 0 || key >= NotePattern::notes) return;
    engine->noteOn(key, vel);
    if (songHeld[key] < 255) ++songHeld[key];
}

void Mz950::songNoteOff(int key)
{
    if (engine == nullptr || key < 0 || key >= NotePattern::notes || songHeld[key] == 0) return;
    if (--songHeld[key] == 0) engine->noteOff(key);
}

void Mz950::start()
{
    track.rewind();
    releaseAll();
    playing = nullptr;
}

void Mz950::stop()
{
    releaseAll();
    track.position = -1;
    playing = nullptr;
}

void Mz950::step()
{
    int i = 0;
    const NotePattern& p = track.advance(i);
    playing = &p;
    playingStep = i;

    if (engine == nullptr) return;

    const bool tie = p.get(NotePattern::tieRow, i);
    const int vel = p.get(NotePattern::accentRow, i) ? accentVelocity : velocity;

    for (int k = 0; k < NotePattern::notes; ++k)
    {
        const bool on = p.get(k, i);
        if (held[k])
        {
            if (on && tie) continue;   // carried on from the step before
            engine->noteOff(k);
            held[k] = false;
        }
        if (on)
        {
            engine->noteOn(k, vel);
            held[k] = true;
        }
    }
}

void Mz950::gateOff()
{
    if (engine == nullptr || playing == nullptr) return;

    // Let go half way, unless the next step is tied and has the same note on it.
    const NotePattern& p = *playing;
    const int next = (playingStep + 1) % p.len();
    const bool tieNext = p.get(NotePattern::tieRow, next);

    for (int k = 0; k < NotePattern::notes; ++k)
    {
        if (! held[k] || (tieNext && p.get(k, next))) continue;
        engine->noteOff(k);
        held[k] = false;
    }
}

} // namespace debirth
