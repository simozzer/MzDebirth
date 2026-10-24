// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Processor.h"
#include "Core/DemoSong.h"
#include "Core/Grooves.h"
#include "J106/Banks.h"
#include "UI/MainPanel.h"

#include <type_traits>

using namespace debirth;

//============================================================================== the first time

namespace
{
    //
    // One of the disks that come with Mz950 - DRUMS.hfe, say: where the installer puts it, or
    // failing that where the app was built from.
    //
    // On Windows that is the disks folder beside the app. A Mac app is a bundle, sealed by its
    // signature, so nothing can be put beside its executable; install.sh puts the disks in the
    // user's Application Support instead, as Mz950's own Mac install does.
    //
    juce::File mz950Disk(const juce::String& file)
    {
       #if JUCE_MAC
        const auto installed = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                   .getChildFile("Application Support/MzDebirth/Disks").getChildFile(file);
       #else
        const auto installed = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                   .getParentDirectory().getChildFile("disks").getChildFile(file);
       #endif
        if (installed.existsAsFile()) return installed;

       #ifdef MZ950_DISKS
        const auto built = juce::File(juce::String(MZ950_DISKS)).getChildFile(file);
        if (built.existsAsFile()) return built;
       #endif

        return {};
    }

    juce::File defaultDisk() { return mz950Disk("DRUMS.hfe"); }
}

DebirthProcessor::DebirthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Every change to the song is compiled and handed to the audio thread.
    song.onChange = [this] (const Song& s)
    {
        rack.publish(std::make_unique<Playback>(compile(s, &rack)));
        prepareSongPatches();
    };

    // Something to play the first time it is opened. A saved state replaces it.
    startAfresh(Fresh::AcidDemo);
    startTimerHz(20);
}

//
// The factory grooves, in every bank of every machine - the 950's on the drum disk that
// comes with it, which is loaded for them if it is not in already.
//
void DebirthProcessor::writeFactoryGrooves()
{
    // SeqCheck reads every one of them, so a problem here is a groove typed wrongly since
    const auto problems = writeGrooves(rack);
    jassert(problems.empty());
    juce::ignoreUnused(problems);

    const auto disk = defaultDisk();
    std::string error;
    if (disk.existsAsFile()
        && (rack.mz950.diskPath() == disk.getFullPathName().toStdString()
            || rack.mz950.loadDisk(disk.getFullPathName().toStdString(), error)))
    {
        rack.mz950.selectProgram(0);
        writeSamplerGrooves(rack.mz950);   // a kit without those sample names just goes without
    }
}

//============================================================================== the KEY section

void DebirthProcessor::generateInKey()
{
    const int bank = keyBank();
    generateBank(rack, bank, Key::fromIndex(rack.key.load()), rack.style.load(),
                 (unsigned) juce::Random::getSystemRandom().nextInt());

    // every machine to the same slot of that bank, so they play the new grooves together
    const int slot = bank * patternsPerBank + rack.mz303.track.selected.load() % patternsPerBank;
    for (int m = 0; m < Rack::channels; ++m)
    {
        auto& t = rack.track(m);
        t.selected = slot;
        if (! rack.playing.load()) t.current = slot;
    }

    ++loads;
}

void DebirthProcessor::transposeToKey()
{
    const int bank = keyBank();
    const int known = rack.bankKey[bank].load();
    const Key from = known >= 0 ? Key::fromIndex(known) : detectKey(rack, bank);
    transposeBank(rack, bank, from, Key::fromIndex(rack.key.load()));
    ++loads;
}

// the 950s' programmes the song's parts change to, built ahead so each lands on its beat
void DebirthProcessor::prepareSongPatches()
{
    for (const auto& t : song.song().tracks)
        if (Rack::typeOf(t.machine) == Rack::M950)
            for (const auto& p : t.parts)
                if (p.program >= 0) (t.machine == Rack::M950 ? rack.mz950 : rack.mz950b).prepareProgram(p.program);
}

void DebirthProcessor::timerCallback()
{
    rack.mz950.service();
    rack.mz950b.service();

    prepareSongPatches();
    rack.collect();   // the song the audio thread handed back

    if (taking())
    {
        if (rack.playing.load()) drainTake();
        else                     endTake();   // stopped, from wherever

        // an overdub handed back: over once every machine is the song's again, on the bar
        if (taking() && overdubEnding)
        {
            bool anyLive = false;
            for (const auto& ch : rack.channel) anyLive |= ch.live.load() || ch.liveWanted.load();
            if (! anyLive) endTake();
        }
    }

    // a take over and the rack stopped: back to the mode it was in
    if (modeAfterTake >= 0 && ! taking() && ! rack.playing.load())
    {
        rack.mode = modeAfterTake;
        modeAfterTake = -1;
    }
}

//============================================================================== recording

//
// PLAY. With REC on it is a take, as ReBirth recorded a song: every machine plays its
// patterns from the top, and what each plays goes into the song from the cursor's bar. The
// rack goes back to the mode it was in once the take is over and the rack has stopped - from
// SONG mode, PLAY then plays the song, take and all.
//
void DebirthProcessor::play()
{
    using namespace debirth;
    if (rack.playing.load()) return;

    if (recordReady)
    {
        bool all[Rack::channels];
        for (auto& a : all) a = true;

        Rack::Played stale;   // anything left from a take before
        while (rack.takePlayed(stale)) {}

        const int from = std::max(0, song.song().cursor);
        take.begin((from + ticksPerBar - 1) / ticksPerBar * ticksPerBar, all);
        beforeTake = song.song();
        takeShown = false;
        tweaks.clear();
        tweaksChanged = false;
        modeAfterTake = rack.mode.load();
        rack.mode = Mode::Pattern;
        rack.recording = true;
    }
    rack.playing = true;
}

//
// The take so far, into the song: the song as it was before the take, with the take put in.
// The first time it is an edit, to be undone; after that it is only adjusted. Should anything
// else change the song meanwhile - a load, an edit - the take stops there, as it is.
//
void DebirthProcessor::drainTake()
{
    bool more = false;
    debirth::Rack::Played p;
    while (rack.takePlayed(p))
    {
        take.add(rack, p);
        more = true;
    }
    if (! more && ! tweaksChanged) return;
    tweaksChanged = false;

    if (takeShown && song.version() != takeVersion)
    {
        rack.recording = false;
        recordReady = false;
        return;
    }

    auto put = [this] (debirth::Song& s)
    {
        s = beforeTake;
        take.apply(rack, s);
        putTweaks(s);
    };
    if (takeShown) song.adjust(put);
    else           song.edit(put);
    takeShown = true;
    takeVersion = song.version();
}

void DebirthProcessor::endTake()
{
    if (! taking()) return;

    // a control still held is recorded up to here
    const double at = takeTick();
    for (auto& t : tweaks)
        if (t.open)
        {
            if (at >= 0.0 && (int) at > t.points.back().tick) t.points.push_back({ (int) at, rack.paramGet(t.owner, t.param) });
            t.open = false;
            tweaksChanged = true;
        }

    rack.recording = false;
    drainTake();
    recordReady = false;
    overdubEnding = false;
}

bool DebirthProcessor::overdubbing() const
{
    return taking() && take.overdubbing();
}

//============================================================================== automation by hand

// which control a panel's table and place are: a machine's knobs, the effects units', or a mixer channel's
bool DebirthProcessor::controlOf(const void* table, int index, int& owner, int& param) const
{
    using debirth::Rack;
    for (int o = 0; o < Rack::owners; ++o)
    {
        const void* knobs = o == Rack::FX ? static_cast<const void*>(&rack.fx)
                                          : rack.visit(o, [] (const auto& x) { return static_cast<const void*>(&x.knobs); });
        if (table == knobs) { owner = o; param = index; return param >= 0 && param < rack.paramCount(o); }
    }

    for (int m = 0; m < Rack::channels; ++m)
        if (table == &rack.channel[m])
        {
            owner = m;
            param = rack.knobCount(m) + index;
            return index >= 0 && param < rack.paramCount(m);
        }
    return false;
}

double DebirthProcessor::takeTick() const
{
    if (! taking() || ! rack.playing.load()) return -1.0;
    if (overdubbing()) return rack.songPosition();
    const double clock = rack.clockPosition();
    return clock < 0.0 ? -1.0 : take.start() + clock;
}

void DebirthProcessor::controlMoved(const void* table, int index, float value)
{
    int owner = 0, param = 0;
    if (! controlOf(table, index, owner, param)) return;
    const double at = takeTick();
    if (at < 0.0) return;
    const int tick = (int) at;

    // on with the run this control is in, if a hand is on it; or a run of its own, a moment long
    Tweak* t = nullptr;
    for (auto it = tweaks.rbegin(); it != tweaks.rend() && t == nullptr; ++it)
        if (it->open && it->owner == owner && it->param == param) t = &*it;
    if (t == nullptr || (! t->points.empty() && tick < t->points.back().tick))   // round a cycle: a new run
    {
        if (t != nullptr) t->open = false;
        tweaks.push_back({ owner, param, {}, false });
        t = &tweaks.back();
    }

    if (! t->points.empty() && t->points.back().tick == tick) t->points.back().value = value;
    else                                                     t->points.push_back({ tick, value });
    tweaksChanged = true;
}

void DebirthProcessor::controlHeld(const void* table, int index, bool held)
{
    int owner = 0, param = 0;
    if (! controlOf(table, index, owner, param)) return;
    rack.held[owner][param] = held;

    const double at = takeTick();
    if (at < 0.0) return;
    const int tick = (int) at;
    const float value = rack.paramGet(owner, param);

    if (held)
    {
        tweaks.push_back({ owner, param, { { tick, value } }, true });
    }
    else
    {
        for (auto it = tweaks.rbegin(); it != tweaks.rend(); ++it)
            if (it->open && it->owner == owner && it->param == param)
            {
                if (tick > it->points.back().tick) it->points.push_back({ tick, value });
                it->open = false;
                break;
            }
    }
    tweaksChanged = true;
}

// the moves recorded so far, over the song's automation
void DebirthProcessor::putTweaks(debirth::Song& s) const
{
    for (const auto& t : tweaks)
    {
        if (t.points.empty()) continue;
        std::vector<debirth::AutoLane>* lanes = nullptr;
        if (t.owner == debirth::Rack::FX) lanes = &s.fx;
        else
            for (auto& track : s.tracks)
                if (track.machine == t.owner) lanes = &track.automation;
        if (lanes != nullptr) debirth::writeAutomation(*lanes, rack.paramId(t.owner, t.param), t.points);
    }
}

//
// REC, from the transport. Stopped, it readies a take for the next PLAY; the song playing, it
// starts an OVERDUB at once. Pressed during a take, it ends it there; during an overdub, it
// hands every machine back to the song at the next bar, where the overdub ends.
//
void DebirthProcessor::rec()
{
    using namespace debirth;

    if (overdubbing())
    {
        for (auto& ch : rack.channel) ch.liveWanted = false;
        overdubEnding = true;
        return;
    }
    if (taking())
    {
        endTake();
        return;
    }
    if (! rack.playing.load() || rack.playingNow() != Mode::Song)
    {
        recordReady = ! recordReady;
        return;
    }

    // the overdub: every machine, at the song's own steps, but only where a pattern is picked
    bool all[Rack::channels];
    for (auto& a : all) a = true;
    Rack::Played stale;
    while (rack.takePlayed(stale)) {}
    take.begin(0, all, true);
    beforeTake = song.song();
    takeShown = false;
    tweaks.clear();
    tweaksChanged = false;
    overdubEnding = false;
    recordReady = false;
    rack.recording = true;
}

void DebirthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    rack.prepare(sampleRate, samplesPerBlock);
    spare.assign((size_t) juce::jmax(1, samplesPerBlock), 0.0f);
}

bool DebirthProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto out = layout.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void DebirthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0 || n == 0) return;

    if (buffer.getNumChannels() >= 2)
    {
        rack.render(buffer.getWritePointer(0), buffer.getWritePointer(1), n);
        for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);
        return;
    }

    // one channel: render both sides, then fold them together
    if ((int) spare.size() < n) spare.resize((size_t) n);   // a host breaking its promise
    float* left = buffer.getWritePointer(0);
    rack.render(left, spare.data(), n);
    for (int i = 0; i < n; ++i) left[i] = 0.5f * (left[i] + spare[(size_t) i]);
}

juce::AudioProcessorEditor* DebirthProcessor::createEditor()
{
    return new DebirthEditor(*this);
}

//============================================================================== saving

namespace
{
    // the machines, as saved: their element names, in rack order - no spaces, since they are
    // XML elements, and listed with spaces between them
    const char* const machineNames[Rack::channels] = { "Mz303", "Mz101", "Mz808", "Mz909", "Mz950", "Mz7", "Mz106",
                                                       "Mz303-2", "Mz101-2", "Mz808-2", "Mz909-2", "Mz950-2", "Mz7-2", "Mz106-2" };

    template <class S>
    void saveKnobs(juce::ValueTree& t, const Knobs<S>& k)
    {
        for (int i = 0; i < k.size(); ++i)
            t.setProperty(k.control(i).id, k.get(i), nullptr);
    }

    template <class S>
    void loadKnobs(const juce::ValueTree& t, Knobs<S>& k)
    {
        for (int i = 0; i < k.size(); ++i)
            k.set(i, (float) t.getProperty(k.control(i).id, k.defaultOf(i)));
    }

    //
    // A pattern's words as text: for each word that is not empty, its index, a colon and its
    // value in hex. A step of a bass line is a word, and so is a row of a grid.
    //
    template <class Words>
    juce::String wordsToText(const Words& w, int count)
    {
        juce::StringArray a;
        for (int i = 0; i < count; ++i)
            if (const uint32_t v = w[i].load(); v != 0)
                a.add(juce::String(i) + ":" + juce::String::toHexString((int) v));
        return a.joinIntoString(" ");
    }

    template <class Words>
    void textToWords(const juce::String& text, Words& w, int count)
    {
        for (int i = 0; i < count; ++i) w[i].store(0);
        for (const auto& token : juce::StringArray::fromTokens(text, " ", ""))
        {
            const int i = token.upToFirstOccurrenceOf(":", false, false).getIntValue();
            if (token.containsChar(':') && i >= 0 && i < count)
                w[i].store((uint32_t) token.fromFirstOccurrenceOf(":", false, false).getHexValue32());
        }
    }

    template <class P, class Owner, class Words>
    void saveTrack(juce::ValueTree& t, const Track<P>& tr, Words Owner::* words, int count)
    {
        t.setProperty("selected", tr.selected.load(), nullptr);
        t.setProperty("shuffle", tr.shuffle.load(), nullptr);

        for (int i = 0; i < patternsPerTrack; ++i)
        {
            const auto& p = tr.patterns[i];
            juce::ValueTree pt("Pattern");
            pt.setProperty("index", i, nullptr);
            pt.setProperty("length", p.len(), nullptr);
            pt.setProperty("words", wordsToText(p.*words, count), nullptr);
            t.appendChild(pt, nullptr);
        }
    }

    template <class P, class Owner, class Words>
    void loadTrack(const juce::ValueTree& t, Track<P>& tr, Words Owner::* words, int count)
    {
        tr.selected = juce::jlimit(0, patternsPerTrack - 1, (int) t.getProperty("selected", 0));
        tr.current = tr.selected.load();
        tr.shuffle = (bool) t.getProperty("shuffle", true);

        for (const auto& pt : t)
        {
            const int i = pt.getProperty("index", -1);
            if (! pt.hasType("Pattern") || i < 0 || i >= patternsPerTrack) continue;
            auto& p = tr.patterns[i];
            p.length = juce::jlimit(1, stepsPerPattern, (int) pt.getProperty("length", stepsPerPattern));
            textToWords(pt.getProperty("words").toString(), p.*words, count);
        }
    }

    // the three kinds of pattern, each with where its words are
    void save(juce::ValueTree& t, const Track<BassPattern>& tr)  { saveTrack(t, tr, &BassPattern::steps, stepsPerPattern); }
    void save(juce::ValueTree& t, const Track<DrumPattern>& tr)  { saveTrack(t, tr, &DrumPattern::hits, DrumPattern::rows); }
    void save(juce::ValueTree& t, const Track<NotePattern>& tr)  { saveTrack(t, tr, &NotePattern::hits, NotePattern::rows); }
    void load(const juce::ValueTree& t, Track<BassPattern>& tr)  { loadTrack(t, tr, &BassPattern::steps, stepsPerPattern); }
    void load(const juce::ValueTree& t, Track<DrumPattern>& tr)  { loadTrack(t, tr, &DrumPattern::hits, DrumPattern::rows); }
    void load(const juce::ValueTree& t, Track<NotePattern>& tr)  { loadTrack(t, tr, &NotePattern::hits, NotePattern::rows); }

    template <class Machine>
    juce::ValueTree saveMachine(const char* name, const Machine& m)
    {
        juce::ValueTree t(name);
        saveKnobs(t, m.knobs);
        save(t, m.track);
        return t;
    }

    template <class Machine>
    void loadMachine(const juce::ValueTree& root, const char* name, Machine& m)
    {
        const auto t = root.getChildWithName(name);
        if (! t.isValid()) return;
        loadKnobs(t, m.knobs);
        load(t, m.track);
    }

    //
    // A machine as saved, with what its kind has besides its knobs and patterns. The sampler's
    // disk by where it is, not what is on it: a disk is a megabyte, and it is still on the disk
    // it came from. The Mz7's cartridge whole - 4 KB - since it may be an edit, or a .syx that
    // has since moved; one of the four it comes with by its number. The Mz106's bank likewise:
    // the factory's by name, or whole - 3 KB - when it came from a .syx.
    //
    template <class Machine>
    juce::ValueTree saveWhole(const char* name, const Machine& m)
    {
        auto t = saveMachine(name, m);
        if constexpr (std::is_same_v<Machine, Mz950>)
        {
            t.setProperty("disk", juce::String(m.diskPath()), nullptr);
            t.setProperty("program", m.program(), nullptr);
        }
        else if constexpr (std::is_same_v<Machine, Mz7>)
        {
            t.setProperty("bank", m.bank(), nullptr);
            t.setProperty("program", m.program(), nullptr);
            if (m.bank() < 0)
            {
                const auto s = dx7::cartridgeSysex(m.cartridge());
                t.setProperty("cartridge", juce::Base64::toBase64(s.data(), s.size()), nullptr);
                t.setProperty("cartridgeName", juce::String(m.cartridgeName()), nullptr);
            }
        }
        else if constexpr (std::is_same_v<Machine, Mz106>)
        {
            t.setProperty("program", m.program(), nullptr);
            t.setProperty("morphTo", m.morphTarget(), nullptr);
            if (! m.isFactory())
            {
                const auto s = j106::bankSysex(m.bank());
                t.setProperty("bank", juce::Base64::toBase64(s.data(), s.size()), nullptr);
                t.setProperty("bankName", juce::String(m.bankName()), nullptr);
            }
        }
        return t;
    }

    template <class Machine>
    void loadWhole(const juce::ValueTree& root, const char* name, Machine& m)
    {
        loadMachine(root, name, m);
        const auto t = root.getChildWithName(name);
        if (! t.isValid()) return;

        if constexpr (std::is_same_v<Machine, Mz950>)
        {
            // the disk, if it is still there, and not already in
            const auto disk = t.getProperty("disk").toString();
            std::string error;
            if (disk.isNotEmpty() && juce::File(disk).existsAsFile()
                && (disk.toStdString() == m.diskPath() || m.loadDisk(disk.toStdString(), error)))
                m.selectProgram((int) t.getProperty("program", 0));
        }
        else if constexpr (std::is_same_v<Machine, Mz7>)
        {
            const int b = t.getProperty("bank", 0);
            juce::MemoryOutputStream bytes;
            dx7::Cartridge c;
            int count = 0;
            std::string error, warning;
            if (b < 0 && juce::Base64::convertFromBase64(bytes, t.getProperty("cartridge").toString())
                && dx7::readSysex(static_cast<const uint8_t*>(bytes.getData()), bytes.getDataSize(), c, count, error, warning)
                && count == 32)
                m.setCartridge(c, t.getProperty("cartridgeName", "CARTRIDGE").toString().toStdString());
            else
                m.loadBank(juce::jmax(0, b));
            m.selectProgram((int) t.getProperty("program", 0));
        }
        else if constexpr (std::is_same_v<Machine, Mz106>)
        {
            juce::MemoryOutputStream bytes;
            j106::Bank b = j106::factoryBank();
            int count = 0;
            std::string error;
            if (juce::Base64::convertFromBase64(bytes, t.getProperty("bank").toString())
                && j106::readSysex(static_cast<const uint8_t*>(bytes.getData()), bytes.getDataSize(), b, count, error)
                && count > 0)
                m.setBank(b, t.getProperty("bankName", "BANK").toString().toStdString());
            else
                m.loadFactory();
            m.selectProgram((int) t.getProperty("program", 0));
            m.setMorphTarget((int) t.getProperty("morphTo", -1));
        }
    }

    //
    // The song: a track a child, a part a child of that, and a part's notes as text - each
    // note its tick, length, pitch and velocity, joined by colons.
    //
    //
    // Automation: a lane a child of its track (or of the song, for the effects units), its
    // control's id and its points as text - each point its tick and value, joined by a colon.
    //
    void saveAutomation(juce::ValueTree& parent, const std::vector<AutoLane>& lanes)
    {
        for (const auto& lane : lanes)
        {
            if (lane.points.empty()) continue;
            juce::ValueTree a("Automation");
            a.setProperty("param", juce::String(lane.param), nullptr);
            juce::StringArray points;
            for (const auto& p : lane.points) points.add(juce::String(p.tick) + ":" + juce::String(p.value, 4));
            a.setProperty("points", points.joinIntoString(" "), nullptr);
            parent.appendChild(a, nullptr);
        }
    }

    std::vector<AutoLane> loadAutomation(const juce::ValueTree& parent)
    {
        std::vector<AutoLane> lanes;
        for (const auto& a : parent)
        {
            if (! a.hasType("Automation")) continue;
            AutoLane lane;
            lane.param = a.getProperty("param").toString().toStdString();
            for (const auto& token : juce::StringArray::fromTokens(a.getProperty("points").toString(), " ", ""))
            {
                const auto f = juce::StringArray::fromTokens(token, ":", "");
                if (f.size() == 2) lane.points.push_back({ juce::jmax(0, f[0].getIntValue()), f[1].getFloatValue() });
            }
            std::stable_sort(lane.points.begin(), lane.points.end(), [] (const AutoPoint& x, const AutoPoint& y) { return x.tick < y.tick; });
            if (! lane.param.empty() && ! lane.points.empty()) lanes.push_back(std::move(lane));
        }
        return lanes;
    }

    juce::ValueTree saveSong(const Song& s, int mode)
    {
        juce::ValueTree t("Song");
        t.setProperty("mode", mode == Mode::Song ? "song" : "pattern", nullptr);
        t.setProperty("left", s.left, nullptr);
        t.setProperty("right", s.right, nullptr);
        t.setProperty("cycle", s.cycle, nullptr);
        t.setProperty("cursor", s.cursor, nullptr);

        for (const auto& track : s.tracks)
        {
            juce::ValueTree tt("Track");
            tt.setProperty("name", juce::String(track.name), nullptr);
            tt.setProperty("machine", machineName(track.machine), nullptr);
            tt.setProperty("mute", track.mute, nullptr);
            tt.setProperty("solo", track.solo, nullptr);
            tt.setProperty("shuffle", track.shuffle, nullptr);
            tt.setProperty("colour", track.colour, nullptr);

            for (const auto& part : track.parts)
            {
                juce::ValueTree pt("Part");
                pt.setProperty("name", juce::String(part.name), nullptr);
                pt.setProperty("start", part.start, nullptr);
                pt.setProperty("length", part.length, nullptr);
                pt.setProperty("muted", part.muted, nullptr);
                if (part.loop > 0) pt.setProperty("loop", part.loop, nullptr);
                if (part.program >= 0) pt.setProperty("program", part.program, nullptr);

                juce::StringArray notes;
                for (const auto& n : part.notes)
                    notes.add(juce::String(n.tick) + ":" + juce::String(n.length) + ":"
                              + juce::String(n.pitch) + ":" + juce::String(n.velocity));
                pt.setProperty("notes", notes.joinIntoString(" "), nullptr);
                tt.appendChild(pt, nullptr);
            }
            saveAutomation(tt, track.automation);
            t.appendChild(tt, nullptr);
        }
        saveAutomation(t, s.fx);
        return t;
    }

    Song loadSong(const juce::ValueTree& t)
    {
        Song s;
        s.left = juce::jmax(0, (int) t.getProperty("left", 0));
        s.right = juce::jmax(s.left, (int) t.getProperty("right", 4 * ticksPerBar));
        s.cycle = (bool) t.getProperty("cycle", false);
        s.cursor = juce::jmax(0, (int) t.getProperty("cursor", 0));

        for (const auto& tt : t)
        {
            if (! tt.hasType("Track")) continue;

            SongTrack track;
            track.name = tt.getProperty("name").toString().toStdString();
            track.machine = -1;
            for (int m = 0; m < Rack::channels; ++m)
                if (tt.getProperty("machine").toString() == machineName(m)) track.machine = m;
            if (track.machine < 0) continue;   // a machine this rack does not have
            track.mute = (bool) tt.getProperty("mute", false);
            track.solo = (bool) tt.getProperty("solo", false);
            track.shuffle = (bool) tt.getProperty("shuffle", true);
            track.colour = (int) tt.getProperty("colour", track.machine);

            for (const auto& pt : tt)
            {
                if (! pt.hasType("Part")) continue;

                Part part;
                part.name = pt.getProperty("name").toString().toStdString();
                part.start = juce::jmax(0, (int) pt.getProperty("start", 0));
                part.length = juce::jmax(1, (int) pt.getProperty("length", ticksPerBar));
                part.muted = (bool) pt.getProperty("muted", false);
                part.loop = juce::jmax(0, (int) pt.getProperty("loop", 0));
                part.program = juce::jmax(-1, (int) pt.getProperty("program", -1));

                for (const auto& token : juce::StringArray::fromTokens(pt.getProperty("notes").toString(), " ", ""))
                {
                    const auto f = juce::StringArray::fromTokens(token, ":", "");
                    if (f.size() != 4) continue;
                    Note n;
                    n.tick = juce::jmax(0, f[0].getIntValue());
                    n.length = juce::jmax(1, f[1].getIntValue());
                    n.pitch = juce::jlimit(0, 127, f[2].getIntValue());
                    n.velocity = juce::jlimit(1, 127, f[3].getIntValue());
                    part.notes.push_back(n);
                }
                part.sortNotes();
                track.parts.push_back(std::move(part));
            }
            track.automation = loadAutomation(tt);
            s.tracks.push_back(std::move(track));
        }
        s.fx = loadAutomation(t);
        return s;
    }
}

juce::ValueTree DebirthProcessor::save() const
{
    juce::ValueTree root("MzDebirth");
    root.setProperty("version", 3, nullptr);
    root.setProperty("tempo", std::round(rack.tempo.load() * 10.0) / 10.0, nullptr);   // 128.3, not 128.30000305
    root.setProperty("shuffle", rack.shuffle.load(), nullptr);
    root.setProperty("master", rack.master.load(), nullptr);
    root.setProperty("key", juce::String(Key::fromIndex(rack.key.load()).code()), nullptr);
    root.setProperty("style", styleName(rack.style.load()), nullptr);
    juce::StringArray bankKeys;
    for (int b = 0; b < banks; ++b)
        bankKeys.add(rack.bankKey[b].load() < 0 ? juce::String("-") : juce::String(Key::fromIndex(rack.bankKey[b].load()).code()));
    root.setProperty("bankKeys", bankKeys.joinIntoString(" "), nullptr);

    juce::StringArray foldedNames;
    for (int m = 0; m < Rack::channels; ++m)
        if (folded[(size_t) m]) foldedNames.add(machineNames[m]);
    root.setProperty("folded", foldedNames.joinIntoString(" "), nullptr);

    // the machines in the rack
    juce::StringArray inNames;
    for (int m = 0; m < Rack::channels; ++m)
        if (rack.channel[m].inRack.load()) inNames.add(machineNames[m]);
    root.setProperty("machines", inNames.joinIntoString(" "), nullptr);

    for (int m = 0; m < Rack::channels; ++m)
        root.appendChild(rack.visit(m, [m] (const auto& x) { return saveWhole(machineNames[m], x); }), nullptr);

    juce::ValueTree effects("Effects");
    saveKnobs(effects, rack.fx);
    root.appendChild(effects, nullptr);

    juce::ValueTree mixer("Mixer");
    for (int c = 0; c < Rack::channels; ++c)
    {
        juce::ValueTree ch("Channel");
        ch.setProperty("machine", machineNames[c], nullptr);
        ch.setProperty("level", rack.channel[c].level.load(), nullptr);
        ch.setProperty("pan", rack.channel[c].pan.load(), nullptr);
        ch.setProperty("mute", rack.channel[c].mute.load(), nullptr);
        ch.setProperty("dist", rack.channel[c].dist.load(), nullptr);
        ch.setProperty("pcf", rack.channel[c].pcf.load(), nullptr);
        ch.setProperty("comp", rack.channel[c].comp.load(), nullptr);
        ch.setProperty("delay", rack.channel[c].delay.load(), nullptr);
        ch.setProperty("reverb", rack.channel[c].reverb.load(), nullptr);
        mixer.appendChild(ch, nullptr);
    }
    root.appendChild(mixer, nullptr);

    auto songTree = saveSong(song.song(), rack.mode.load());
    songTree.setProperty("view", view == View::Arrange ? "arrange" : "rack", nullptr);
    root.appendChild(songTree, nullptr);

    return root;
}

void DebirthProcessor::load(const juce::ValueTree& root)
{
    if (! root.hasType("MzDebirth")) return;
    if ((int) root.getProperty("version", 0) < 2) return;   // 0.1's racks: two machines, and gone

    startAfresh(Fresh::Empty);

    rack.tempo = (float) root.getProperty("tempo", 120.0f);
    rack.shuffle = (float) root.getProperty("shuffle", 0.0f);
    rack.master = (float) root.getProperty("master", 0.8f);

    // the step rolls folded away: none, in a rack saved before they could be
    const auto foldedNames = juce::StringArray::fromTokens(root.getProperty("folded").toString(), false);
    for (int m = 0; m < Rack::channels; ++m)
        folded[(size_t) m] = foldedNames.contains(machineNames[m]);

    // the machines in the rack - in a rack saved before they could be chosen, the first of each
    const auto inNames = juce::StringArray::fromTokens(root.getProperty("machines").toString(), false);
    for (int m = 0; m < Rack::channels; ++m)
        rack.channel[m].inRack = root.hasProperty("machines") ? inNames.contains(machineNames[m]) : Rack::copyOf(m) == 0;

    // the key, as its code: "8A"; -1 for anything else
    auto keyOf = [] (const juce::String& code)
    {
        const int number = code.dropLastCharacters(1).getIntValue();
        const auto letter = code.getLastCharacter();
        if (number < 1 || number > 12 || (letter != 'A' && letter != 'B')) return -1;
        return Key { number, letter == 'B' }.index();
    };
    if (const int k = keyOf(root.getProperty("key").toString()); k >= 0) rack.key = k;
    for (int s = 0; s < Style::count; ++s)
        if (root.getProperty("style").toString() == styleName(s)) rack.style = s;
    const auto bankKeys = juce::StringArray::fromTokens(root.getProperty("bankKeys").toString(), " ", "");
    for (int b = 0; b < banks && b < bankKeys.size(); ++b)
        rack.bankKey[b] = keyOf(bankKeys[b]);

    for (int m = 0; m < Rack::channels; ++m)
        rack.visit(m, [&root, m] (auto& x) { loadWhole(root, machineNames[m], x); });

    if (const auto effects = root.getChildWithName("Effects"); effects.isValid())
        loadKnobs(effects, rack.fx);

    for (const auto& ch : root.getChildWithName("Mixer"))
    {
        for (int c = 0; c < Rack::channels; ++c)
        {
            if (ch.getProperty("machine").toString() != machineNames[c]) continue;
            rack.channel[c].level = (float) ch.getProperty("level", 0.7f);
            rack.channel[c].pan = (float) ch.getProperty("pan", 0.0f);
            rack.channel[c].mute = (bool) ch.getProperty("mute", false);
            rack.channel[c].dist = (bool) ch.getProperty("dist", false);
            rack.channel[c].pcf = (bool) ch.getProperty("pcf", false);
            rack.channel[c].comp = (bool) ch.getProperty("comp", false);
            rack.channel[c].delay = (float) ch.getProperty("delay", 0.0f);
            rack.channel[c].reverb = (float) ch.getProperty("reverb", 0.0f);   // none before 0.8
        }
    }

    // The song, from version 3 on; a rack from before has none, and gets a track a machine.
    if (const auto s = root.getChildWithName("Song"); s.isValid())
    {
        song.replace(loadSong(s));
        rack.mode = s.getProperty("mode").toString() == "song" ? Mode::Song : Mode::Pattern;
        view = s.getProperty("view").toString() == "arrange" ? View::Arrange : View::Rack;
    }

    markSaved();   // as it was saved
    ++loads;
}

void DebirthProcessor::startAfresh(Fresh fresh)
{
    rack.tempo = 120.0f;
    rack.shuffle = 0.0f;
    rack.master = 0.8f;
    for (auto& k : rack.bankKey) k = -1;   // the KEY and STYLE are the DJ's, and stay

    for (int m = 0; m < Rack::channels; ++m)
        rack.visit(m, [] (auto& x) { x.knobs.reset(); });
    rack.fx.reset();

    for (int m = 0; m < Rack::channels; ++m)
    {
        auto& t = rack.track(m);
        for (int i = 0; i < patternsPerTrack; ++i) t.clear(i);
        t.selected = 0;
        t.current = 0;
        t.shuffle = true;

        rack.channel[m].level = 0.7f;
        rack.channel[m].pan = 0.0f;
        rack.channel[m].mute = false;
        rack.channel[m].dist = false;
        rack.channel[m].pcf = false;
        rack.channel[m].comp = false;
        rack.channel[m].delay = 0.0f;
        rack.channel[m].reverb = 0.0f;
        rack.channel[m].stopWanted = false;   // every machine playing again
    }

    // With the factory grooves comes the acid demo made from them, ready to play in the
    // Arrange window, on the voices it was written for: the drum disk's first kit, the Mz7's
    // E.PIANO 1 and the Mz106's ARP PULSE. An empty rack starts with an empty song, and its
    // patterns.
    if (fresh == Fresh::AcidDemo)
    {
        writeFactoryGrooves();
        voiceForAcid(rack);
        for (int m = 0; m < Rack::types; ++m) rack.channel[m].inRack = true;   // it plays the first of each
        rack.tempo = demoTempo;
        song.replace(demoSong(rack));
        rack.mode = Mode::Song;
        view = View::Arrange;
    }
    //
    // The ambient demo has the factory grooves in the banks too, for the rack to play
    // afterwards - all but the 950's, which want the drum disk, and the 950 has the WHALES
    // disk in instead, on WHALE SEA. The Mz7 is on WARM PAD, the Mz106 on SEQ PLUCK.
    //
    else if (fresh == Fresh::AmbientDemo)
    {
        writeFactoryGrooves();
        const auto disk = mz950Disk("WHALES.hfe");
        const bool whales = voiceForAmbient(rack, disk.existsAsFile() ? disk.getFullPathName().toStdString() : std::string());
        dressForAmbient(rack);
        rack.tempo = ambientTempo;
        for (int m : { Rack::M303, Rack::M101, Rack::M950, Rack::M7, Rack::M106 })
            rack.channel[m].inRack = true;   // the machines it plays; the drum machines as they were
        song.replace(ambientSong(rack, whales));
        rack.mode = Mode::Song;
        view = View::Arrange;
    }
    else
    {
        song.replace(defaultSong());
        rack.mode = Mode::Pattern;
    }

    markSaved();   // nothing to save yet
    ++loads;
}

void DebirthProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto tree = save();
    if (windowWidth > 0 && windowHeight > 0)
        tree.setProperty("window", juce::String(windowWidth) + " " + juce::String(windowHeight), nullptr);
    if (rackFile != juce::File())
        tree.setProperty("rackFile", rackFile.getFullPathName(), nullptr);   // the session's, not the rack's

    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, dest);
}

void DebirthProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
    {
        const auto tree = juce::ValueTree::fromXml(*xml);
        const auto window = juce::StringArray::fromTokens(tree.getProperty("window").toString(), false);
        if (window.size() == 2)
        {
            windowWidth = window[0].getIntValue();
            windowHeight = window[1].getIntValue();
        }
        load(tree);
        rackFile = tree.hasProperty("rackFile") ? juce::File(tree.getProperty("rackFile").toString()) : juce::File();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DebirthProcessor();
}

//============================================================================== saved, or not

// the rack as saved, less where you are in it: the song's cursor, the view, the folds
juce::String DebirthProcessor::fingerprint() const
{
    auto t = save();
    t.removeProperty("folded", nullptr);
    auto s = t.getChildWithName("Song");
    s.removeProperty("view", nullptr);
    s.removeProperty("cursor", nullptr);
    return t.toXmlString();
}

void DebirthProcessor::markSaved()
{
    savedPrint = fingerprint();
}

bool DebirthProcessor::hasUnsavedChanges() const
{
    return fingerprint() != savedPrint;
}
