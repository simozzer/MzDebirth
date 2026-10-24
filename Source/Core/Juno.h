// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Knobs.h"
#include "Patterns.h"

#include "J106/Engine.h"
#include "J106/Patch.h"

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace debirth
{

//
// The Mz106's front panel, as the rack shows it: the sliders a player reaches for mid-song,
// laid over the patch as offsets - 0 is the patch's own setting - and CHORUS, the patch's own
// unless it is switched to OFF, I or II here, and VOLUME, as the plugin has it.
//
struct JunoSettings
{
    float freq    = 0.0f;   // -5 to +5: the VCF's FREQ slider, from where the patch has it
    float res     = 0.0f;   // -5 to +5: RES
    float env     = 0.0f;   // -5 to +5: the VCF's ENV
    float attack  = 0.0f;   // -5 to +5: the envelope's A
    float release = 0.0f;   // -5 to +5: and R
    int   chorus  = 0;      // the patch's, or OFF, I or II
    float morph   = 0.0f;   // 0 to 1: from the patch to the MORPH TO patch
    float volume  = 0.0f;   // -20 to +12 dB: a single-note line wants it up, a chord doesn't
};

const std::vector<Control<JunoSettings>>& controls106();

//
// The Mz106's engine, played from a pattern, with the bank of patches it is playing from.
//
// THREADS. The bank and the patch chosen from it belong to the message thread. The patch
// reaches the audio thread through a lock the audio thread only ever tries - if the panel
// happens to hold it, the patch arrives a block later rather than the audio waiting. The
// engine itself is the audio thread's alone.
//
// A Juno-106 has no velocity, so neither does this: an accent plays as any other note.
//
class Mz106
{
public:
    Mz106();

    Knobs<JunoSettings> knobs;
    Track<NotePattern> track;

    static constexpr int velocity = 100, accentVelocity = 127;

    // ---------------------------------------------------------------- message thread

    // the 128 patches Mz106 comes with, or a .syx from disk
    void loadFactory();
    bool loadSyx(const std::string& path, std::string& error);
    bool isFactory() const { return factory; }
    const std::string& syxPath() const { return path; }
    std::string bankName() const { return name; }

    std::vector<std::string> programNames() const;   // "A11 FAT BASS", and on
    int  program() const { return programIndex; }
    bool selectProgram(int index);

    // MORPH: a second patch the MORPH knob moves towards - its sliders step by step, its
    // switches flipping half way. -1 for none.
    int  morphTarget() const { return morphIndex; }
    void setMorphTarget(int index);

    const j106::Bank& bank() const { return memory; }
    void setBank(const j106::Bank& b, const std::string& name);   // a saved rack's own

    // ---------------------------------------------------------------- audio thread

    void prepare(double sampleRate);
    void update();
    void render(float* left, float* right, int n);

    void start();
    void stop();
    void step();
    void gateOff();

    void songNoteOn(int key, int vel);
    void songNoteOff(int key);

    int activeVoices() const { return engine.activeVoices(); }

    // a part starting on a patch of its own, by number - -1 the one picked on the panel. At
    // once, so the part's first note plays on it.
    void songProgram(int index);

private:
    void releaseAll();
    void send(const j106::Patch&);
    void sendBank();   // the bank's sounds, for the song's patch changes
    void apply();      // the patch, with the knobs over it, to the engine

    // a patch as the audio thread keeps it: its sliders and switches - no name, so copying
    // one never allocates
    struct Sound { std::array<uint8_t, 16> slider {}; uint8_t sw1 = 0, sw2 = 0; };

    j106::Bank memory;
    std::string path, name;
    bool factory = true;
    int programIndex = 0;
    int morphIndex = -1;

    // the patch, handed to the audio thread
    std::mutex lock;
    j106::Patch pending;
    bool pendingNew = false;
    std::array<Sound, 128> pendingBank;
    bool pendingBankNew = false;
    Sound pendingMorph;
    bool pendingMorphOn = false, pendingMorphNew = false;

    // the audio thread's
    j106::Engine engine;
    j106::Patch patch;
    j106::Patch homePatch;              // as the panel chose it
    std::array<Sound, 128> audioBank;   // the bank, the audio thread's copy
    JunoSettings knobsNow;              // the knobs as last read
    Sound morphTo;                      // the MORPH TO patch
    bool morphOn = false;
    bool held[NotePattern::notes] {};
    uint8_t songHeld[NotePattern::notes] {};
    const NotePattern* playing = nullptr;
    int playingStep = 0;
};

} // namespace debirth
