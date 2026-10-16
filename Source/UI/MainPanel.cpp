// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "MainPanel.h"

#include <juce_audio_basics/juce_audio_basics.h>

using namespace debirth;

namespace
{
    constexpr int transportHeight = 64;
    const char* const fileSuffix = ".mzdebirth";
}

//============================================================================== TransportBar

TransportBar::TransportBar(DebirthProcessor& p)
    : processor(p),
      tempo((int) Clock::minTempo, (int) Clock::maxTempo, 120, 3,
            [&p] { return juce::roundToInt(p.rack.tempo.load()); },
            [&p] (int v) { p.rack.tempo = (float) v; }),
      shuffle("SHUFFLE", 0.0f, 1.0f, 0.0f,
              [&p] { return p.rack.shuffle.load(); }, [&p] (float v) { p.rack.shuffle = v; }),
      master("MASTER", 0.0f, 1.0f, 0.8f,
             [&p] { return p.rack.master.load(); }, [&p] (float v) { p.rack.master = v; })
{
    play.setLedColour(colours::ledGreen);
    play.setTooltip("Play, from the top of each pattern. The space bar plays and stops.");
    play.onPress = [this] { processor.rack.playing = true; };
    stop.setTooltip("Stop.");
    stop.onPress = [this] { processor.rack.playing = false; };
    addAndMakeVisible(play);
    addAndMakeVisible(stop);

    tempo.setTooltip("Tempo, in beats a minute. Drag, or use the wheel; double-click for 120.");
    addAndMakeVisible(tempo);

    shuffle.setColours(juce::Colour(0xff17181b), colours::stepOrange, colours::light);
    shuffle.setValueText([] (double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; });
    shuffle.setTooltip("Holds back every other sixteenth, on the machines whose SHUFFLE is lit.");
    master.setColours(juce::Colour(0xff17181b), colours::light, colours::light);
    master.setValueText([] (double v)
    {
        return v <= 0.0 ? juce::String("-inf dB") : juce::String(juce::Decibels::gainToDecibels(v * v), 1) + " dB";
    });
    addAndMakeVisible(shuffle);
    addAndMakeVisible(master);

    fresh.setTooltip("Start again: every pattern cleared, every knob back where it was.");
    open.setTooltip("Load a rack saved before.");
    save.setTooltip("Save the rack - every pattern, knob and fader - to a file.");
    fresh.onPress = [this] { if (onNew) onNew(); };
    open.onPress = [this] { if (onLoad) onLoad(); };
    save.onPress = [this] { if (onSave) onSave(); };
    for (auto* b : { &fresh, &open, &save })
    {
        b->setShowsLed(false);
        addAndMakeVisible(*b);
    }
}

void TransportBar::tick()
{
    const bool playing = processor.rack.playing.load();
    play.setLit(playing);
    stop.setLit(! playing);
    tempo.repaint();
}

void TransportBar::refresh()
{
    shuffle.refresh();
    master.refresh();
    tempo.repaint();
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced(10, 8);
    r.removeFromLeft(220);   // the name

    play.setBounds(r.removeFromLeft(64));
    r.removeFromLeft(6);
    stop.setBounds(r.removeFromLeft(64));
    r.removeFromLeft(24);

    tempo.setBounds(r.removeFromLeft(78).withSizeKeepingCentre(78, 34).translated(0, 6));
    r.removeFromLeft(18);
    shuffle.setBounds(r.removeFromLeft(56));
    r.removeFromLeft(8);
    master.setBounds(r.removeFromLeft(56));

    save.setBounds(r.removeFromRight(64).withSizeKeepingCentre(64, 30));
    r.removeFromRight(6);
    open.setBounds(r.removeFromRight(64).withSizeKeepingCentre(64, 30));
    r.removeFromRight(6);
    fresh.setBounds(r.removeFromRight(64).withSizeKeepingCentre(64, 30));
}

void TransportBar::paint(juce::Graphics& g)
{
    drawFace(g, getLocalBounds().toFloat(), colours::metal);

    auto r = getLocalBounds().reduced(14, 8);
    g.setColour(colours::light);
    g.setFont(fonts::bold(30.0f));
    auto name = r.removeFromLeft(200);
    g.drawText("MzDebirth", name.withTrimmedBottom(10), juce::Justification::centredLeft);
    g.setColour(colours::light.withAlpha(0.55f));
    g.setFont(fonts::bold(9.0f));
    g.drawText(juce::String::fromUTF8("Copyright \xc2\xa9 2026 Simon Moscrop"), name.removeFromBottom(11),
               juce::Justification::bottomLeft);

    g.setColour(colours::light.withAlpha(0.75f));
    g.setFont(fonts::bold(9.5f));
    g.drawText("TEMPO", tempo.getBounds().withY(tempo.getY() - 14).withHeight(12), juce::Justification::centredLeft);
}

//============================================================================== the machines' looks

namespace
{
    Theme theme303()
    {
        return { "Mz303", "BASS LINE", colours::silver, colours::ink, colours::inkDim, colours::ink,
                 juce::Colour(0xff1d1d1f), colours::light, true };
    }

    Theme theme101()
    {
        return { "Mz101", "SYNTHESIZER", juce::Colour(0xff3d3f44), juce::Colour(0xffe6e6e2), juce::Colour(0xff9a9ca0),
                 juce::Colour(0xffe6e6e2), juce::Colour(0xff1c1d20), juce::Colour(0xfff0a030) };
    }

    Theme theme808()
    {
        return { "Mz808", "RHYTHM COMPOSER", colours::charcoal, colours::cream, colours::cream.withAlpha(0.7f),
                 colours::stepOrange, juce::Colour(0xff121212), colours::stepOrange };
    }

    const juce::Colour orange909 { 0xffe2571e };

    Theme theme909()
    {
        return { "Mz909", "RHYTHM COMPOSER", juce::Colour(0xffc9cac5), juce::Colour(0xff1c1c1c), juce::Colour(0xff55575a),
                 orange909, juce::Colour(0xff2a2a2a), colours::light, true };
    }

    Theme theme950()
    {
        return { "Mz950", "SAMPLER", juce::Colour(0xffcfcabd), juce::Colour(0xff222222), juce::Colour(0xff5c5a55),
                 juce::Colour(0xffc8281e), juce::Colour(0xff2b2b2b), colours::light };
    }

    std::vector<MonoRack::Row> layout303() { return { { { "", 8 } } }; }

    std::vector<MonoRack::Row> layout101()
    {
        return {
            { { "MODULATOR", 3 }, { "VCO", 4 }, { "SOURCE MIXER", 5 }, { "PORTAMENTO", 2 } },
            { { "VCF", 5 }, { "VCA  /  ENV", 6 }, { "OUTPUT", 3 } },
        };
    }

    std::vector<DrumRack::ColumnSpec> layout808()
    {
        return {
            { "ACCENT",  { "accent" } },
            { "BASS\nDRUM", { "bdLevel", "bdTone", "bdDecay" } },
            { "SNARE\nDRUM", { "sdLevel", "sdTone", "sdSnappy" } },
            { "LOW TOM\nLOW CONGA", { "ltLevel", "ltTuning" } },
            { "MID TOM\nMID CONGA", { "mtLevel", "mtTuning" } },
            { "HI TOM\nHI CONGA", { "htLevel", "htTuning" } },
            { "RIM SHOT\nCLAVES", { "rsLevel" } },
            { "HAND CLAP\nMARACAS", { "cpLevel" } },
            { "COW\nBELL", { "cbLevel" } },
            { "CYMBAL", { "cyLevel", "cyTone", "cyDecay" } },
            { "OPEN\nHIHAT", { "ohLevel", "ohDecay" } },
            { "CLSD\nHIHAT", { "chLevel" } },
            { "MASTER", { "volume" } },
        };
    }

    std::vector<DrumRack::ColumnSpec> layout909()
    {
        return {
            { "ACCENT",  { "accent" } },
            { "BASS\nDRUM", { "bdLevel", "bdTune", "bdAttack", "bdDecay" } },
            { "SNARE\nDRUM", { "sdLevel", "sdTune", "sdTone", "sdSnappy" } },
            { "LOW\nTOM", { "ltLevel", "ltTune", "ltDecay" } },
            { "MID\nTOM", { "mtLevel", "mtTune", "mtDecay" } },
            { "HI\nTOM", { "htLevel", "htTune", "htDecay" } },
            { "RIM SHOT\nHAND CLAP", { "rsLevel", "cpLevel" } },
            { "HI HAT", { "hhLevel", "chDecay", "ohDecay" } },
            { "CRASH", { "crLevel", "crTune" } },
            { "RIDE", { "rdLevel", "rdTune" } },
            { "MASTER", { "volume" } },
        };
    }

    template <class Info>
    std::vector<DrumRack::VoiceSpec> voicesOf(int count, Info info)
    {
        std::vector<DrumRack::VoiceSpec> v;
        for (int i = 0; i < count; ++i) v.push_back({ info(i).code, info(i).name });
        return v;
    }

    juce::Colour face808(int step)
    {
        static const juce::Colour faces[] = { colours::stepRed, colours::stepOrange, colours::stepYellow, colours::stepWhite };
        return faces[(step / 4) % 4];
    }

    DrumRack::Palette palette808()
    {
        return { face808, colours::ink, colours::pad, colours::cream,
                 [] (int c, int n) { return c == 0 || c == n - 1 ? colours::metal : face808((c - 1) * 16 / (n - 2)); },
                 colours::ink, colours::ledRed };
    }

    DrumRack::Palette palette909()
    {
        return { [] (int step) { return (step / 4) % 2 == 0 ? juce::Colour(0xffeeeee9) : juce::Colour(0xffd9d9d3); },
                 colours::ink, juce::Colour(0xff2a2a2c), colours::light,
                 [] (int c, int n) { return c == 0 || c == n - 1 ? juce::Colour(0xffa3a39e) : orange909; },
                 juce::Colours::white, colours::ledRed };
    }

    constexpr int gap = 8;
}

//============================================================================== RackView

RackView::RackView(DebirthProcessor& p)
    : mz303(p.rack.mz303, p.rack.channel[Rack::M303], [&p] { return p.rack.playing.load(); },
            theme303(), layout303(), true),
      mz101(p.rack.mz101, p.rack.channel[Rack::M101], [&p] { return p.rack.playing.load(); },
            theme101(), layout101(), false),
      mz808(p.rack.mz808, p.rack.channel[Rack::M808], [&p] { return p.rack.playing.load(); },
            theme808(), layout808(), voicesOf(tr808::Voice::count, tr808::voiceInfo), palette808()),
      mz909(p.rack.mz909, p.rack.channel[Rack::M909], [&p] { return p.rack.playing.load(); },
            theme909(), layout909(), voicesOf(tr909::Voice::count, tr909::voiceInfo), palette909()),
      mz950(p.rack.mz950, p.rack.channel[Rack::M950], [&p] { return p.rack.playing.load(); }, theme950())
{
    for (auto* u : units()) addAndMakeVisible(u);
}

int RackView::preferredHeight() const
{
    return gap + mz303.preferredHeight() + gap + mz101.preferredHeight() + gap + mz808.preferredHeight()
         + gap + mz909.preferredHeight() + gap + mz950.preferredHeight() + gap;
}

void RackView::tick(bool blinkPhase)
{
    mz303.tick(blinkPhase);
    mz101.tick(blinkPhase);
    mz808.tick(blinkPhase);
    mz909.tick(blinkPhase);
    mz950.tick(blinkPhase);
}

void RackView::refresh()
{
    mz303.refresh();
    mz101.refresh();
    mz808.refresh();
    mz909.refresh();
    mz950.refresh();
}

void RackView::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop(gap);
    const int heights[] = { mz303.preferredHeight(), mz101.preferredHeight(), mz808.preferredHeight(),
                            mz909.preferredHeight(), mz950.preferredHeight() };
    int i = 0;
    for (auto* u : units())
    {
        u->setBounds(r.removeFromTop(heights[i++]));
        r.removeFromTop(gap);
    }
}

void RackView::paint(juce::Graphics& g)
{
    g.fillAll(colours::rail);
}

//============================================================================== MainPanel

MainPanel::MainPanel(DebirthProcessor& p)
    : processor(p), transport(p), rack(p)
{
    setLookAndFeel(&look);
    setWantsKeyboardFocus(true);

    transport.onNew = [this]
    {
        juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Start again?",
            "Every pattern will be cleared and every knob put back.", "Start again", "Cancel", this,
            juce::ModalCallbackFunction::create([this] (int ok)
            {
                if (ok == 0) return;
                processor.rack.playing = false;
                processor.startAfresh();
            }));
    };
    transport.onLoad = [this] { loadFile(); };
    transport.onSave = [this] { saveFile(); };
    addAndMakeVisible(transport);

    viewport.setViewedComponent(&rack, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(12);
    viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, colours::screw.withAlpha(0.7f));
    viewport.setSingleStepSizes(40, 40);
    addAndMakeVisible(viewport);

    startTimerHz(30);
}

MainPanel::~MainPanel()
{
    setLookAndFeel(nullptr);
}

void MainPanel::timerCallback()
{
    if (processor.loadCount() != seenLoads)
    {
        seenLoads = processor.loadCount();
        refresh();
    }

    const bool blinkPhase = (++ticks / 8) % 2 == 0;
    transport.tick();
    rack.tick(blinkPhase);
}

void MainPanel::refresh()
{
    transport.refresh();
    rack.refresh();
}

bool MainPanel::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey)
    {
        processor.rack.playing = ! processor.rack.playing.load();
        return true;
    }
    return false;
}

static juce::File racksFolder()
{
    auto f = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MzDebirth");
    f.createDirectory();
    return f;
}

void MainPanel::saveFile()
{
    chooser = std::make_unique<juce::FileChooser>("Save the rack", racksFolder(), juce::String("*") + fileSuffix);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File()) return;
            file = file.withFileExtension(fileSuffix);

            if (auto xml = processor.save().createXml(); xml == nullptr || ! xml->writeTo(file))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Not saved",
                                                       "Could not write " + file.getFullPathName());
        });
}

void MainPanel::loadFile()
{
    chooser = std::make_unique<juce::FileChooser>("Load a rack", racksFolder(), juce::String("*") + fileSuffix);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;

            const auto xml = juce::XmlDocument::parse(file);
            const auto tree = xml != nullptr ? juce::ValueTree::fromXml(*xml) : juce::ValueTree();
            if (! tree.hasType("MzDebirth"))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Not loaded",
                                                       file.getFileName() + " is not a MzDebirth rack.");
                return;
            }
            processor.load(tree);
        });
}

void MainPanel::resized()
{
    auto r = getLocalBounds().withTrimmedLeft(cheek).withTrimmedRight(cheek);
    r.removeFromTop(gap);
    transport.setBounds(r.removeFromTop(transportHeight));

    viewport.setBounds(r);
    rack.setSize(r.getWidth() - viewport.getScrollBarThickness() - 4, rack.preferredHeight());
}

void MainPanel::paint(juce::Graphics& g)
{
    g.fillAll(colours::rail);

    // the wooden cheeks, with a grain
    juce::Random grain(338);
    for (auto side : { getLocalBounds().removeFromLeft(cheek), getLocalBounds().removeFromRight(cheek) })
    {
        g.setColour(colours::wood);
        g.fillRect(side);
        for (int i = 0; i < 24; ++i)
        {
            g.setColour(colours::woodGrain.withAlpha(0.4f + 0.4f * grain.nextFloat()));
            const float x = (float) side.getX() + grain.nextFloat() * (float) cheek;
            g.drawLine(x, 0.0f, x + grain.nextFloat() * 4.0f - 2.0f, (float) getHeight(), 0.6f + grain.nextFloat());
        }
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawRect(side);
    }

    // a screw at each end of the transport
    for (int x : { cheek / 2, getWidth() - cheek / 2 })
    {
        const auto s = juce::Rectangle<float>(8.0f, 8.0f).withCentre({ (float) x, (float) transport.getY() + 12.0f });
        g.setColour(colours::screw);
        g.fillEllipse(s);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawLine(s.getX() + 1.5f, s.getCentreY(), s.getRight() - 1.5f, s.getCentreY(), 1.2f);
    }
}

//============================================================================== DebirthEditor

DebirthEditor::DebirthEditor(DebirthProcessor& p)
    : AudioProcessorEditor(&p), panel(p)
{
    tooltips.setLookAndFeel(&panel.getLook());
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, MainPanel::designWidth, MainPanel::designHeight);

    setResizable(true, true);
    setResizeLimits(MainPanel::designWidth / 2, MainPanel::designHeight / 2,
                    MainPanel::designWidth * 2, MainPanel::designHeight * 2);
    getConstrainer()->setFixedAspectRatio((double) MainPanel::designWidth / MainPanel::designHeight);
    setSize(MainPanel::designWidth, MainPanel::designHeight);
}

void DebirthEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::rail);
}

void DebirthEditor::resized()
{
    panel.setTransform(juce::AffineTransform::scale((float) getWidth() / (float) MainPanel::designWidth));
}
