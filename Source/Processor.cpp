// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Processor.h"
#include "Core/DemoSong.h"
#include "Core/Grooves.h"
#include "UI/MainPanel.h"

using namespace debirth;

//============================================================================== the first time

namespace
{
    //
    // The drum disk that comes with Mz950: in the disks folder beside the app, where the
    // installer puts it, or failing that where the app was built from.
    //
    juce::File defaultDisk()
    {
        const auto installed = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                   .getParentDirectory().getChildFile("disks").getChildFile("DRUMS.hfe");
        if (installed.existsAsFile()) return installed;

       #ifdef MZ950_DISKS
        const auto built = juce::File(juce::String(MZ950_DISKS)).getChildFile("DRUMS.hfe");
        if (built.existsAsFile()) return built;
       #endif

        return {};
    }
}

DebirthProcessor::DebirthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Every change to the song is compiled and handed to the audio thread.
    song.onChange = [this] (const Song& s) { rack.publish(std::make_unique<Playback>(compile(s))); };

    // Something to play the first time it is opened. A saved state replaces it.
    startAfresh(true);
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

void DebirthProcessor::timerCallback()
{
    rack.mz950.service();
    rack.collect();   // the song the audio thread handed back
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
    // the machines, as saved: their element names, in rack order
    const char* const machineNames[Rack::channels] = { "Mz303", "Mz101", "Mz808", "Mz909", "Mz950" };

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
    // The song: a track a child, a part a child of that, and a part's notes as text - each
    // note its tick, length, pitch and velocity, joined by colons.
    //
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

                juce::StringArray notes;
                for (const auto& n : part.notes)
                    notes.add(juce::String(n.tick) + ":" + juce::String(n.length) + ":"
                              + juce::String(n.pitch) + ":" + juce::String(n.velocity));
                pt.setProperty("notes", notes.joinIntoString(" "), nullptr);
                tt.appendChild(pt, nullptr);
            }
            t.appendChild(tt, nullptr);
        }
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
            s.tracks.push_back(std::move(track));
        }
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

    root.appendChild(saveMachine(machineNames[Rack::M303], rack.mz303), nullptr);
    root.appendChild(saveMachine(machineNames[Rack::M101], rack.mz101), nullptr);
    root.appendChild(saveMachine(machineNames[Rack::M808], rack.mz808), nullptr);
    root.appendChild(saveMachine(machineNames[Rack::M909], rack.mz909), nullptr);

    // The sampler's disk is saved by where it is, not what is on it: a disk is a megabyte,
    // and it is still on the disk it came from.
    auto sampler = saveMachine(machineNames[Rack::M950], rack.mz950);
    sampler.setProperty("disk", juce::String(rack.mz950.diskPath()), nullptr);
    sampler.setProperty("program", rack.mz950.program(), nullptr);
    root.appendChild(sampler, nullptr);

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

    startAfresh(false);

    rack.tempo = (float) root.getProperty("tempo", 120.0f);
    rack.shuffle = (float) root.getProperty("shuffle", 0.0f);
    rack.master = (float) root.getProperty("master", 0.8f);

    // the step rolls folded away: none, in a rack saved before they could be
    const auto foldedNames = juce::StringArray::fromTokens(root.getProperty("folded").toString(), false);
    for (int m = 0; m < Rack::channels; ++m)
        folded[(size_t) m] = foldedNames.contains(machineNames[m]);

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

    loadMachine(root, machineNames[Rack::M303], rack.mz303);
    loadMachine(root, machineNames[Rack::M101], rack.mz101);
    loadMachine(root, machineNames[Rack::M808], rack.mz808);
    loadMachine(root, machineNames[Rack::M909], rack.mz909);
    loadMachine(root, machineNames[Rack::M950], rack.mz950);

    // The sampler's disk, if it is still there, and not already in.
    const auto sampler = root.getChildWithName(machineNames[Rack::M950]);
    const auto disk = sampler.getProperty("disk").toString();
    if (disk.isNotEmpty() && juce::File(disk).existsAsFile())
    {
        std::string error;
        if (disk.toStdString() == rack.mz950.diskPath() || rack.mz950.loadDisk(disk.toStdString(), error))
            rack.mz950.selectProgram((int) sampler.getProperty("program", 0));
    }

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
        }
    }

    // The song, from version 3 on; a rack from before has none, and gets a track a machine.
    if (const auto s = root.getChildWithName("Song"); s.isValid())
    {
        song.replace(loadSong(s));
        rack.mode = s.getProperty("mode").toString() == "song" ? Mode::Song : Mode::Pattern;
        view = s.getProperty("view").toString() == "arrange" ? View::Arrange : View::Rack;
    }

    ++loads;
}

void DebirthProcessor::startAfresh(bool factoryGrooves)
{
    rack.tempo = 120.0f;
    rack.shuffle = 0.0f;
    rack.master = 0.8f;
    for (auto& k : rack.bankKey) k = -1;   // the KEY and STYLE are the DJ's, and stay

    rack.mz303.knobs.reset();
    rack.mz101.knobs.reset();
    rack.mz808.knobs.reset();
    rack.mz909.knobs.reset();
    rack.mz950.knobs.reset();
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
    }

    // With the factory grooves comes the demo song made from them, ready to play in the
    // Arrange window; an empty rack starts with an empty song, and its patterns.
    if (factoryGrooves)
    {
        writeFactoryGrooves();
        rack.tempo = demoTempo;
        song.replace(demoSong(rack));
        rack.mode = Mode::Song;
        view = View::Arrange;
    }
    else
    {
        song.replace(defaultSong());
        rack.mode = Mode::Pattern;
    }

    ++loads;
}

void DebirthProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto tree = save();
    if (windowWidth > 0 && windowHeight > 0)
        tree.setProperty("window", juce::String(windowWidth) + " " + juce::String(windowHeight), nullptr);

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
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DebirthProcessor();
}
