// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Processor.h"
#include "UI/MainPanel.h"

using namespace debirth;

//============================================================================== the first time

namespace
{
    void bassLine(Track<BassPattern>& track, const char* const notes[stepsPerPattern])
    {
        // each step: a note name and octave mark ("C", "C+", "A#-"), "." a rest; then
        // ">" for an accent and "~" for a slide
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        auto& p = track.patterns[0];

        for (int i = 0; i < stepsPerPattern; ++i)
        {
            juce::String s(notes[i]);
            BassStep st;
            st.accent = s.containsChar('>');
            st.slide = s.containsChar('~');
            s = s.removeCharacters(">~");
            st.gate = s != ".";
            st.octave = s.containsChar('+') ? 1 : (s.endsWithChar('-') ? -1 : 0);
            s = s.removeCharacters("+").trimCharactersAtEnd("-");
            for (int n = 0; n < 12; ++n)
                if (s == names[n]) st.note = n;
            if (s == "C'") st.note = 12;
            p.set(i, st);
        }
    }

    void drumRow(DrumPattern& p, int voice, const char* hits)
    {
        for (int i = 0; i < stepsPerPattern; ++i)
            p.set(voice, i, hits[i] == 'x');
    }

    template <class K>
    void knob(K& knobs, const char* id, float v) { knobs.set(knobs.indexOf(id), v); }

    //
    // Something to play the first time it is opened: a part for every machine, in A1.
    // A saved state, or NEW, replaces it.
    //
    void writeDemo(Rack& rack)
    {
        static const char* const acid[] = { "C>", "C", "C'~", "C", ".", "D#>", "C~", "A#-",
                                            "C>", "C+", ".", "G~", "F", "D#>~", "C", "C+" };
        bassLine(rack.mz303.track, acid);
        knob(rack.mz303.knobs, "cutoff", 3.0f);
        knob(rack.mz303.knobs, "res", 7.5f);
        knob(rack.mz303.knobs, "envMod", 6.5f);
        knob(rack.mz303.knobs, "decay", 4.0f);

        static const char* const lead[] = { ".", ".", "G", ".", ".", ".", "A#~", "C'",
                                            ".", ".", "G", ".", "F~", "G", ".", "." };
        bassLine(rack.mz101.track, lead);
        knob(rack.mz101.knobs, "saw", 6.0f);
        knob(rack.mz101.knobs, "pulse", 5.0f);
        knob(rack.mz101.knobs, "cutoff", 5.5f);
        knob(rack.mz101.knobs, "res", 4.0f);
        knob(rack.mz101.knobs, "portaMode", 1.0f);   // AUTO: the slides glide
        knob(rack.mz101.knobs, "portaTime", 2.5f);
        knob(rack.mz101.knobs, "release", 3.5f);
        rack.channel[Rack::M101].level = 0.55f;
        rack.channel[Rack::M101].pan = 0.3f;

        auto& d8 = rack.mz808.track.patterns[0];
        drumRow(d8, tr808::Voice::BD, "x...x...x...x..x");
        drumRow(d8, tr808::Voice::CP, "....x.......x...");
        drumRow(d8, tr808::Voice::CH, "x.x.x.x.x.x.x.x.");
        drumRow(d8, DrumPattern::accentRow, "x...x...x...x...");

        auto& d9 = rack.mz909.track.patterns[0];
        drumRow(d9, tr909::Voice::OH, "..x...x...x...x.");
        drumRow(d9, tr909::Voice::RC, "x.x.x.x.x.x.x.x.");
        drumRow(d9, tr909::Voice::RS, "......x......x..");
        rack.channel[Rack::M909].level = 0.55f;
        rack.channel[Rack::M909].pan = -0.25f;

        rack.tempo = 128.0f;
        rack.shuffle = 0.15f;
    }

    //
    // A few hits on the sampler, once it has a programme: a pedal hat and a rim shot, off the
    // beat, if the kit has them by those names - and its second and third keys if not.
    //
    void writeSamplerDemo(Mz950& s)
    {
        const auto spans = s.keySpans();
        if (spans.size() < 3) return;

        auto key = [&spans] (const char* name, int fallback)
        {
            for (const auto& sp : spans)
                if (juce::String(sp.sample).containsIgnoreCase(name)) return sp.low;
            return spans[(size_t) fallback].low;
        };

        auto& p = s.track.patterns[0];
        const int hat = key("PEDAL", 1), rim = key("RIM", 2);
        for (int step : { 3, 7, 11, 15 }) p.set(hat, step, true);
        p.set(rim, 10, true);
        p.set(rim, 13, true);
    }

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
    writeDemo(rack);

    std::string error;
    if (const auto disk = defaultDisk(); disk.existsAsFile()
        && rack.mz950.loadDisk(disk.getFullPathName().toStdString(), error))
        writeSamplerDemo(rack.mz950);

    startTimerHz(20);
}

void DebirthProcessor::timerCallback()
{
    rack.mz950.service();
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
}

juce::ValueTree DebirthProcessor::save() const
{
    juce::ValueTree root("MzDebirth");
    root.setProperty("version", 2, nullptr);
    root.setProperty("tempo", rack.tempo.load(), nullptr);
    root.setProperty("shuffle", rack.shuffle.load(), nullptr);
    root.setProperty("master", rack.master.load(), nullptr);

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

    juce::ValueTree mixer("Mixer");
    for (int c = 0; c < Rack::channels; ++c)
    {
        juce::ValueTree ch("Channel");
        ch.setProperty("machine", machineNames[c], nullptr);
        ch.setProperty("level", rack.channel[c].level.load(), nullptr);
        ch.setProperty("pan", rack.channel[c].pan.load(), nullptr);
        ch.setProperty("mute", rack.channel[c].mute.load(), nullptr);
        mixer.appendChild(ch, nullptr);
    }
    root.appendChild(mixer, nullptr);

    return root;
}

void DebirthProcessor::load(const juce::ValueTree& root)
{
    if (! root.hasType("MzDebirth")) return;
    if ((int) root.getProperty("version", 0) < 2) return;   // 0.1's racks: two machines, and gone

    startAfresh();

    rack.tempo = (float) root.getProperty("tempo", 120.0f);
    rack.shuffle = (float) root.getProperty("shuffle", 0.0f);
    rack.master = (float) root.getProperty("master", 0.8f);

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

    for (const auto& ch : root.getChildWithName("Mixer"))
    {
        for (int c = 0; c < Rack::channels; ++c)
        {
            if (ch.getProperty("machine").toString() != machineNames[c]) continue;
            rack.channel[c].level = (float) ch.getProperty("level", 0.7f);
            rack.channel[c].pan = (float) ch.getProperty("pan", 0.0f);
            rack.channel[c].mute = (bool) ch.getProperty("mute", false);
        }
    }

    ++loads;
}

void DebirthProcessor::startAfresh()
{
    rack.tempo = 120.0f;
    rack.shuffle = 0.0f;
    rack.master = 0.8f;

    rack.mz303.knobs.reset();
    rack.mz101.knobs.reset();
    rack.mz808.knobs.reset();
    rack.mz909.knobs.reset();
    rack.mz950.knobs.reset();

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
    }

    ++loads;
}

void DebirthProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = save().createXml())
        copyXmlToBinary(*xml, dest);
}

void DebirthProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        load(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DebirthProcessor();
}
