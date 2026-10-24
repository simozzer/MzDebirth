// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Juno.h"

#include "J106/Banks.h"

#include <algorithm>
#include <fstream>
#include <iterator>

namespace debirth
{

const std::vector<Control<JunoSettings>>& controls106()
{
    using S = JunoSettings;
    static const std::vector<Control<S>> table = {
        { "freq",    "FREQ",    -5, 5, &S::freq },
        { "res",     "RES",     -5, 5, &S::res },
        { "env",     "ENV",     -5, 5, &S::env },
        { "attack",  "A",       -5, 5, &S::attack },
        { "release", "R",       -5, 5, &S::release },
        { "chorus",  "CHORUS",   0, 3, nullptr, &S::chorus, "PATCH|OFF|I|II" },
        { "morph",   "MORPH",    0, 1, &S::morph },
        { "volume",  "VOLUME", -20, 12, &S::volume },
    };
    return table;
}

Mz106::Mz106() : knobs(controls106())
{
    loadFactory();
}

//============================================================================== the bank

void Mz106::loadFactory()
{
    memory = j106::factoryBank();
    name = "FACTORY";
    path.clear();
    factory = true;
    programIndex = -1;
    sendBank();
    setMorphTarget(morphIndex);   // the same number in the new bank
    selectProgram(0);
}

bool Mz106::loadSyx(const std::string& file, std::string& error)
{
    std::ifstream in(file, std::ios::binary);
    if (! in) { error = "could not open it"; return false; }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // read over what is there, as a 106 takes a dump: a single patch lands in its own slot
    j106::Bank b = memory;
    int count = 0;
    if (! j106::readSysex(bytes.data(), bytes.size(), b, count, error) || count == 0)
    {
        if (error.empty()) error = "no Juno-106 patches in it";
        return false;
    }

    std::string title = file.substr(file.find_last_of("/\\") + 1);
    if (const auto dot = title.find_last_of('.'); dot != std::string::npos) title.resize(dot);
    std::transform(title.begin(), title.end(), title.begin(), [] (unsigned char ch) { return (char) std::toupper(ch); });

    setBank(b, title);
    path = file;
    error.clear();
    return true;
}

void Mz106::setBank(const j106::Bank& b, const std::string& title)
{
    memory = b;
    name = title;
    path.clear();

    factory = true;
    const auto& f = j106::factoryBank();
    for (size_t i = 0; factory && i < b.patches.size(); ++i) factory = b.patches[i].sameSound(f.patches[i]);
    if (factory) name = "FACTORY";

    programIndex = -1;
    sendBank();
    setMorphTarget(morphIndex);   // the same number in the new bank
    selectProgram(0);
}

std::vector<std::string> Mz106::programNames() const
{
    std::vector<std::string> names;
    for (size_t i = 0; i < memory.patches.size(); ++i)
    {
        auto n = j106::Bank::number((int) i);
        if (! memory.patches[i].name.empty()) n += " " + memory.patches[i].name;
        names.push_back(n);
    }
    return names;
}

bool Mz106::selectProgram(int index)
{
    if (index < 0 || index >= (int) memory.patches.size()) return false;
    programIndex = index;
    send(memory.patches[(size_t) index]);
    return true;
}

void Mz106::send(const j106::Patch& p)
{
    std::lock_guard<std::mutex> g(lock);
    pending = p;
    pending.name.clear();   // the audio thread copies it: no string to allocate
    pendingNew = true;
}

void Mz106::sendBank()
{
    std::lock_guard<std::mutex> g(lock);
    for (size_t i = 0; i < pendingBank.size(); ++i)
        pendingBank[i] = { memory.patches[i].slider, memory.patches[i].sw1, memory.patches[i].sw2 };
    pendingBankNew = true;
}

void Mz106::setMorphTarget(int index)
{
    morphIndex = index >= 0 && index < (int) memory.patches.size() ? index : -1;
    std::lock_guard<std::mutex> g(lock);
    if (morphIndex >= 0)
    {
        const auto& p = memory.patches[(size_t) morphIndex];
        pendingMorph = { p.slider, p.sw1, p.sw2 };
    }
    pendingMorphOn = morphIndex >= 0;
    pendingMorphNew = true;
}

//============================================================================== playing

void Mz106::prepare(double sampleRate)
{
    engine.prepare(sampleRate);
    std::fill(std::begin(held), std::end(held), false);
    std::fill(std::begin(songHeld), std::end(songHeld), (uint8_t) 0);
}

//
// Once a block: the patch, if the panel has chosen another, with the knobs laid over it. The
// engine takes the whole panel every block, as the plugin hands it over; notes held follow.
//
void Mz106::update()
{
    {
        std::unique_lock<std::mutex> g(lock, std::try_to_lock);
        if (g.owns_lock() && pendingNew)
        {
            patch = homePatch = pending;
            pendingNew = false;
        }
        if (g.owns_lock() && pendingBankNew)
        {
            audioBank = pendingBank;
            pendingBankNew = false;
        }
        if (g.owns_lock() && pendingMorphNew)
        {
            morphTo = pendingMorph;
            morphOn = pendingMorphOn;
            pendingMorphNew = false;
        }
    }

    knobsNow = knobs.read();
    apply();
}

void Mz106::songProgram(int index)
{
    if (index >= 0 && index < (int) audioBank.size())
    {
        const Sound& s = audioBank[(size_t) index];
        patch.slider = s.slider;
        patch.sw1 = s.sw1;
        patch.sw2 = s.sw2;
    }
    else
        patch = homePatch;
    apply();
}

void Mz106::apply()
{
    const JunoSettings& k = knobsNow;
    j106::Settings s;

    // MORPH: the sliders part of the way to the other patch's, as the 106's seven bits step
    // them; the switches - waveforms, range, chorus, HPF, the rest - at the half
    if (morphOn && k.morph > 0.0f)
    {
        j106::Patch m = patch;
        for (size_t i = 0; i < m.slider.size(); ++i)
            m.slider[i] = (uint8_t) std::lround(patch.slider[i] + (morphTo.slider[i] - patch.slider[i]) * k.morph);
        if (k.morph >= 0.5f) { m.sw1 = morphTo.sw1; m.sw2 = morphTo.sw2; }
        m.applyTo(s);
    }
    else
        patch.applyTo(s);

    auto nudge = [] (float v, float by) { return std::clamp(v + by, 0.0f, 10.0f); };
    s.cutoff  = nudge(s.cutoff, k.freq);
    s.res     = nudge(s.res, k.res);
    s.vcfEnv  = nudge(s.vcfEnv, k.env);
    s.attack  = nudge(s.attack, k.attack);
    s.release = nudge(s.release, k.release);
    if (k.chorus > 0) s.chorus = k.chorus - 1;   // OFF, I, II

    s.volume = k.volume;

    engine.setSettings(s);
}

void Mz106::render(float* left, float* right, int n)
{
    engine.render(left, right, n);
}

void Mz106::releaseAll()
{
    for (int k = 0; k < NotePattern::notes; ++k)
        if (held[k] || songHeld[k] > 0) engine.noteOff(k);
    std::fill(std::begin(held), std::end(held), false);
    std::fill(std::begin(songHeld), std::end(songHeld), (uint8_t) 0);
}

void Mz106::songNoteOn(int key, int)
{
    if (key < 0 || key >= NotePattern::notes) return;
    engine.noteOn(key);
    if (songHeld[key] < 255) ++songHeld[key];
}

void Mz106::songNoteOff(int key)
{
    if (key < 0 || key >= NotePattern::notes || songHeld[key] == 0) return;
    if (--songHeld[key] == 0) engine.noteOff(key);
}

void Mz106::start()
{
    track.rewind();
    releaseAll();
    playing = nullptr;
}

void Mz106::stop()
{
    releaseAll();
    track.position = -1;
    playing = nullptr;
}

void Mz106::step()
{
    int i = 0;
    const NotePattern& p = track.advance(i);
    playing = &p;
    playingStep = i;

    const bool tie = p.get(NotePattern::tieRow, i);

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
            engine.noteOn(k);
            held[k] = true;
        }
    }
}

void Mz106::gateOff()
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
