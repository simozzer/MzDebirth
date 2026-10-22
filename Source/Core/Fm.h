// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Knobs.h"
#include "Patterns.h"

#include "DX7/Engine.h"
#include "DX7/Patch.h"

#include <mutex>
#include <string>
#include <vector>

namespace debirth
{

//
// The Mz7's front panel, as the rack shows it: a few performance controls laid over the voice,
// which move every operator from where the voice has it, and the output gain. The voice
// itself is the DX7's, from a cartridge; these are what a player reaches for mid-song.
//
struct FmSettings
{
    float bright  = 0.0f;   // -40 to +40: every modulator's output level, so the brightness
    float attack  = 0.0f;   // -50 to +50: every operator's R1 - down is slower
    float decay   = 0.0f;   // -50 to +50: R2 and R3
    float release = 0.0f;   // -50 to +50: R4
    float wheel   = 0.0f;   // 0 to 127: the mod wheel, so whatever vibrato the voice gives it
    float gain    = 1.0f;   // 0 to 2
};

const std::vector<Control<FmSettings>>& controls7();

//
// The Mz7's engine, played from a pattern, with the cartridge it is playing from.
//
// THREADS. The cartridge and the voice belong to the message thread. The voice reaches the
// audio thread through a lock the audio thread only ever tries - if the panel happens to
// hold it, the voice arrives a block later rather than the audio waiting. The engine itself
// is the audio thread's alone.
//
class Mz7
{
public:
    Mz7();

    Knobs<FmSettings> knobs;
    Track<NotePattern> track;

    static constexpr int velocity = 100, accentVelocity = 127;

    // ---------------------------------------------------------------- message thread

    // one of the four cartridges Mz7 comes with, or a .syx from disk
    void loadBank(int bank);
    bool loadSyx(const std::string& path, std::string& error);

    int bank() const { return bankIndex; }                 // -1: from a file
    const std::string& syxPath() const { return path; }
    std::string cartridgeName() const;

    std::vector<std::string> programNames() const;
    int  program() const { return programIndex; }
    bool selectProgram(int index);

    const dx7::Cartridge& cartridge() const { return cart; }
    void setCartridge(const dx7::Cartridge& c, const std::string& name);   // a saved rack's own

    // ---------------------------------------------------------------- audio thread

    void prepare(double sampleRate);
    void update();
    void render(float* out, int n);

    void start();
    void stop();
    void step();
    void gateOff();

    void songNoteOn(int key, int vel);
    void songNoteOff(int key);

    int activeVoices() const { return voicesSounding.load(std::memory_order_relaxed); }

private:
    void releaseAll();
    void send(const dx7::Patch&);

    dx7::Cartridge cart;
    std::string path, name;
    int bankIndex = 0, programIndex = 0;

    // the voice, handed to the audio thread
    std::mutex lock;
    dx7::Patch pending;
    bool pendingNew = false;

    // the audio thread's
    dx7::Engine engine;
    dx7::Patch voice;              // as chosen
    FmSettings applied;            // the performance controls last laid over it
    bool dirty = true;
    std::atomic<int> voicesSounding { 0 };

    bool held[NotePattern::notes] {};
    uint8_t songHeld[NotePattern::notes] {};
    const NotePattern* playing = nullptr;
    int playingStep = 0;
};

} // namespace debirth
