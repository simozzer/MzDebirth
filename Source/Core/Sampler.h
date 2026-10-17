// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Knobs.h"
#include "Patterns.h"

#include "S950/Disk.h"
#include "S950/Engine.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace debirth
{

//
// The Mz950's front panel, as the rack shows it: the player's trims, which move every
// keygroup of the programme from where the disk has it, and the output gain. All but GAIN
// are offsets, 0 being the disk's own setting.
//
struct SamplerSettings
{
    float cutoff        = 0.0f;   // -99 to +99
    float resonance     = 0.0f;   // 0 to 100 - not an offset; the S950 has none
    float amount        = 0.0f;   // the filter envelope, -50 to +50
    float attack        = 0.0f;   // the VCA envelope, -99 to +99 each
    float decay         = 0.0f;
    float sustain       = 0.0f;
    float release       = 0.0f;
    float velToFilter   = 0.0f;
    float velToLoudness = 0.0f;
    float gain          = 0.7f;   // 0 to 2
};

const std::vector<Control<SamplerSettings>>& controls950();

//
// The Mz950's engine, played from a pattern, with the disk it is playing from.
//
// THREADS. The disk, the programme and the patch built from it belong to the message
// thread: loading a disk decodes it and building a patch decodes its samples, neither of
// which can happen where a late finish is a click. The patch is handed to the engine
// through its own two-slot exchange, which can refuse while the audio thread has not taken
// the last one - so service() is called on a timer and keeps trying, and collects the patch
// the engine has finished with. The engine itself is built in prepare(), since it works out
// every voice's constants from the sample rate; a lock keeps that and service() apart, and
// is never taken by the audio thread.
//
class Mz950
{
public:
    Mz950() : knobs(controls950()) {}

    Knobs<SamplerSettings> knobs;
    Track<NotePattern> track;

    static constexpr int velocity = 100, accentVelocity = 127;

    // ---------------------------------------------------------------- message thread

    bool loadDisk(const std::string& path, std::string& error);
    const std::string& diskPath() const { return path; }
    std::string diskName() const;

    std::vector<std::string> programNames() const;
    int  program() const { return programIndex; }
    bool selectProgram(int index);

    // the keys the programme answers, low to high, and the name of the sample each starts on
    struct Span { int low, high; std::string sample; };
    std::vector<Span> keySpans() const;

    void service();

    // ---------------------------------------------------------------- audio thread

    void prepare(double sampleRate);
    void update(bool playing, double quarters, double bpm);
    void render(float* left, float* right, int n);

    void start();
    void stop();
    void step();
    void gateOff();

    // Played from the song, a note at a time. Two notes of the same key overlapping are
    // struck twice and let go when the last of them ends.
    void songNoteOn(int key, int vel);
    void songNoteOff(int key);

    int activeVoices() const { return engine != nullptr ? engine->getActiveVoices() : 0; }

private:
    void releaseAll();
    const s950::Disk::Entry* entry(int index) const;

    std::unique_ptr<s950::Disk> disk;
    std::string path;
    int programIndex = -1;
    s950::PatchPtr patch;
    bool sameDisk = false;   // the patch came off this disk, so its samples can be reused

    std::mutex lock;   // prepare() against service(); never the audio thread
    std::unique_ptr<s950::Engine> engine;
    bool owed = false;

    // what is sounding, and from which step
    bool held[NotePattern::notes] {};
    uint8_t songHeld[NotePattern::notes] {};   // the song's notes sounding, a count a key
    const NotePattern* playing = nullptr;
    int playingStep = 0;
};

} // namespace debirth
