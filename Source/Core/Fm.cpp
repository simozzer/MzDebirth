// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Fm.h"

#include "DX7/Algorithms.h"
#include "DX7/Banks.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace debirth
{

const std::vector<Control<FmSettings>>& controls7()
{
    using S = FmSettings;
    static const std::vector<Control<S>> table = {
        { "bright",  "BRIGHT",  -40,  40, &S::bright },
        { "attack",  "ATTACK",  -50,  50, &S::attack },
        { "decay",   "DECAY",   -50,  50, &S::decay },
        { "release", "RELEASE", -50,  50, &S::release },
        { "wheel",   "MOD",       0, 127, &S::wheel },
        { "gain",    "GAIN",      0,   2, &S::gain },
    };
    return table;
}

Mz7::Mz7() : knobs(controls7())
{
    loadBank(0);
}

//============================================================================== the cartridge

void Mz7::loadBank(int b)
{
    b = std::clamp(b, 0, ::banks::count() - 1);
    cart = ::banks::get(b);
    name = ::banks::name(b);
    path.clear();
    bankIndex = b;
    programIndex = -1;
    selectProgram(0);
}

bool Mz7::loadSyx(const std::string& file, std::string& error)
{
    std::ifstream in(file, std::ios::binary);
    if (! in) { error = "could not open it"; return false; }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    dx7::Cartridge c;
    int count = 0;
    std::string warning;
    if (! dx7::readSysex(bytes.data(), bytes.size(), c, count, error, warning)) return false;

    // A single voice goes into the first slot of what is there, as the DX7 takes one.
    if (count == 1) { auto v = c.voices[0]; c = cart; c.voices[0] = v; }

    std::string title = file.substr(file.find_last_of("/\\") + 1);
    if (const auto dot = title.find_last_of('.'); dot != std::string::npos) title.resize(dot);
    std::transform(title.begin(), title.end(), title.begin(), [] (unsigned char ch) { return (char) std::toupper(ch); });

    setCartridge(c, title);
    path = file;
    error = warning;
    return true;
}

void Mz7::setCartridge(const dx7::Cartridge& c, const std::string& title)
{
    cart = c;
    name = title;
    path.clear();
    bankIndex = -1;
    for (int b = 0; b < ::banks::count(); ++b)
        if (::banks::get(b).voices == c.voices) { bankIndex = b; name = ::banks::name(b); }
    programIndex = -1;
    selectProgram(0);
}

std::string Mz7::cartridgeName() const { return name; }

std::vector<std::string> Mz7::programNames() const
{
    std::vector<std::string> names;
    for (const auto& v : cart.voices)
    {
        auto n = v.name();
        while (! n.empty() && n.back() == ' ') n.pop_back();
        names.push_back(n);
    }
    return names;
}

bool Mz7::selectProgram(int index)
{
    if (index < 0 || index >= 32) return false;
    programIndex = index;
    send(cart.voices[(size_t) index]);
    return true;
}

void Mz7::send(const dx7::Patch& p)
{
    std::lock_guard<std::mutex> g(lock);
    pending = p;
    pendingNew = true;
}

//============================================================================== playing

void Mz7::prepare(double sampleRate)
{
    engine.prepare(sampleRate);
    std::fill(std::begin(held), std::end(held), false);
    dirty = true;
}

//
// Once a block: the voice, if the panel has chosen another, and the performance controls laid
// over it, if they have moved. Held notes follow either, as they would on the DX7.
//
void Mz7::update()
{
    {
        std::unique_lock<std::mutex> g(lock, std::try_to_lock);
        if (g.owns_lock() && pendingNew)
        {
            voice = pending;
            pendingNew = false;
            dirty = true;
        }
    }

    const FmSettings s = knobs.read();
    engine.controller(1, (int) s.wheel);

    if (! dirty && s.bright == applied.bright && s.attack == applied.attack
        && s.decay == applied.decay && s.release == applied.release)
        return;

    applied = s;
    dirty = false;

    // BRIGHT moves the modulators, the operators that shape rather than sound; ATTACK, DECAY
    // and RELEASE lengthen or shorten every operator's stages - up is longer.
    dx7::Patch p = voice;
    const auto& alg = dx7::algorithms[p[dx7::Patch::Algorithm] & 31];
    auto nudge = [] (uint8_t& v, float by) { v = (uint8_t) std::clamp((int) std::lround(v + by), 0, 99); };

    for (int op = 1; op <= 6; ++op)
    {
        if (! ((alg.carriers >> op) & 1)) nudge(p.op(op, dx7::Patch::OutputLevel), s.bright);
        nudge(p.op(op, dx7::Patch::R1), -s.attack);
        nudge(p.op(op, dx7::Patch::R2), -s.decay);
        nudge(p.op(op, dx7::Patch::R3), -s.decay);
        nudge(p.op(op, dx7::Patch::R4), -s.release);
    }
    engine.setPatch(p);
}

void Mz7::render(float* out, int n)
{
    engine.render(out, n);
    const float g = applied.gain;
    for (int i = 0; i < n; ++i) out[i] *= g;
    voicesSounding.store(engine.activeVoices(), std::memory_order_relaxed);
}

void Mz7::releaseAll()
{
    for (int k = 0; k < NotePattern::notes; ++k)
        if (held[k] || songHeld[k] > 0) engine.noteOff(k);
    std::fill(std::begin(held), std::end(held), false);
    std::fill(std::begin(songHeld), std::end(songHeld), (uint8_t) 0);
}

void Mz7::songNoteOn(int key, int vel)
{
    if (key < 0 || key >= NotePattern::notes) return;
    engine.noteOn(key, vel);
    if (songHeld[key] < 255) ++songHeld[key];
}

void Mz7::songNoteOff(int key)
{
    if (key < 0 || key >= NotePattern::notes || songHeld[key] == 0) return;
    if (--songHeld[key] == 0) engine.noteOff(key);
}

void Mz7::start()
{
    track.rewind();
    releaseAll();
    playing = nullptr;
}

void Mz7::stop()
{
    releaseAll();
    track.position = -1;
    playing = nullptr;
}

void Mz7::step()
{
    int i = 0;
    const NotePattern& p = track.advance(i);
    playing = &p;
    playingStep = i;

    const bool tie = p.get(NotePattern::tieRow, i);
    const int vel = p.get(NotePattern::accentRow, i) ? accentVelocity : velocity;

    for (int k = 0; k < NotePattern::notes; ++k)
    {
        const bool on = p.get(k, i);
        if (held[k])
        {
            if (on && tie) continue;   // carried on from the step before
            engine.noteOff(k);
            held[k] = false;
        }
        if (on)
        {
            engine.noteOn(k, vel);
            held[k] = true;
        }
    }
}

void Mz7::gateOff()
{
    if (playing == nullptr) return;

    // Let go half way, unless the next step is tied and has the same note on it.
    const NotePattern& p = *playing;
    const int next = (playingStep + 1) % p.len();
    const bool tieNext = p.get(NotePattern::tieRow, next);

    for (int k = 0; k < NotePattern::notes; ++k)
    {
        if (! held[k] || (tieNext && p.get(k, next))) continue;
        engine.noteOff(k);
        held[k] = false;
    }
}

} // namespace debirth
