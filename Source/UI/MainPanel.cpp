// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "MainPanel.h"

#include <juce_audio_basics/juce_audio_basics.h>

using namespace debirth;

namespace
{
    constexpr int transportHeight = 64;
    constexpr int effectsHeight = 92;
    constexpr int tabHeight = 26;
    const char* const fileSuffix = ".mzdebirth";
}

//============================================================================== TransportBar

TransportBar::TransportBar(DebirthProcessor& p)
    : processor(p),
      tempo((int) Clock::minTempo * 10, (int) Clock::maxTempo * 10, 1200, 4,
            [&p] { return juce::roundToInt(p.rack.tempo.load() * 10.0f); },
            [&p] (int v) { p.rack.tempo = (float) v / 10.0f; }),
      shuffle("SHUFFLE", 0.0f, 1.0f, 0.0f,
              [&p] { return p.rack.shuffle.load(); }, [&p] (float v) { p.rack.shuffle = v; }),
      master("MASTER", 0.0f, 1.0f, 0.8f,
             [&p] { return p.rack.master.load(); }, [&p] (float v) { p.rack.master = v; }),
      keyPanel(p)
{
    play.setLedColour(colours::ledGreen);
    play.setTooltip("Play, from the top of each pattern. The space bar plays and stops.");
    play.onPress = [this] { processor.rack.playing = true; };
    stop.setTooltip("Stop.");
    stop.onPress = [this] { processor.rack.playing = false; };
    addAndMakeVisible(play);
    addAndMakeVisible(stop);

    tempo.setDecimals(1);
    tempo.setTooltip("Tempo, in beats a minute. Drag, or use the wheel - hold Shift for tenths; "
                     "double-click for 120.");
    tempo.setShown([&p] { return juce::roundToInt((p.rack.nudge.load() != 0 ? p.rack.tempoNow()
                                                                              : p.rack.tempo.load()) * 10.0f); });
    addAndMakeVisible(tempo);

    //
    // DRAG and PUSH, as on a CD deck: held, they slow the rack or hurry it, a little at once
    // and more the longer they are held, and let it go on release - the tempo is as it was,
    // the beat moved. The arrow keys do the same.
    //
    drag.setTooltip("DRAG: slows the rack while held, to pull it back into phase (or hold the left arrow key).");
    push.setTooltip("PUSH: hurries the rack while held, to pull it forward into phase (or hold the right arrow key).");
    drag.onPress   = [this] { padNudge = -1; nudge(); };
    push.onPress   = [this] { padNudge = +1; nudge(); };
    drag.onRelease = [this] { if (padNudge < 0) { padNudge = 0; nudge(); } };
    push.onRelease = [this] { if (padNudge > 0) { padNudge = 0; nudge(); } };
    drag.setArrow(-1);
    push.setArrow(+1);
    for (auto* b : { &drag, &push })
    {
        b->setLedColour(colours::stepOrange);
        addAndMakeVisible(*b);
    }

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

    fresh.setTooltip("Start again: the factory grooves in every bank, or an empty rack.");
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

    addAndMakeVisible(keyPanel);
}

void TransportBar::tick()
{
    const bool playing = processor.rack.playing.load();
    play.setLit(playing);
    keyPanel.tick();
    stop.setLit(! playing);
    tempo.repaint();

    // and an arrow key let go where it was not seen does not leave the rack bent
    if (keyNudge != 0 && ! juce::KeyPress::isKeyCurrentlyDown(keyNudge < 0 ? juce::KeyPress::leftKey
                                                                           : juce::KeyPress::rightKey))
        setKeyNudge(0);
}

void TransportBar::setKeyNudge(int dir)
{
    if (dir == keyNudge) return;
    keyNudge = dir;
    nudge();
}

// the pads and the keys together; both ways at once is neither
void TransportBar::nudge()
{
    const int dir = juce::jlimit(-1, 1, padNudge + keyNudge);
    processor.rack.nudge = dir;
    drag.setLit(dir < 0);
    push.setLit(dir > 0);
    tempo.repaint();
}

void TransportBar::refresh()
{
    shuffle.refresh();
    master.refresh();
    keyPanel.refresh();
    tempo.repaint();
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced(10, 8);
    r.removeFromLeft(200);   // the name

    play.setBounds(r.removeFromLeft(56));
    r.removeFromLeft(6);
    stop.setBounds(r.removeFromLeft(56));
    r.removeFromLeft(16);

    tempo.setBounds(r.removeFromLeft(88).withSizeKeepingCentre(88, 34).translated(0, 6));
    r.removeFromLeft(6);
    drag.setBounds(r.removeFromLeft(30).withSizeKeepingCentre(30, 34).translated(0, 6));
    r.removeFromLeft(4);
    push.setBounds(r.removeFromLeft(30).withSizeKeepingCentre(30, 34).translated(0, 6));
    r.removeFromLeft(10);
    shuffle.setBounds(r.removeFromLeft(56));
    r.removeFromLeft(8);
    master.setBounds(r.removeFromLeft(56));

    save.setBounds(r.removeFromRight(52).withSizeKeepingCentre(52, 30));
    r.removeFromRight(6);
    open.setBounds(r.removeFromRight(52).withSizeKeepingCentre(52, 30));
    r.removeFromRight(6);
    fresh.setBounds(r.removeFromRight(52).withSizeKeepingCentre(52, 30));

    keyPanel.setBounds(r.reduced(8, 0));
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
    g.drawText(juce::String("Created by Simon Moscrop"), name.removeFromBottom(11),
               juce::Justification::bottomLeft);

    g.setColour(colours::light.withAlpha(0.75f));
    g.setFont(fonts::bold(9.5f));
    g.drawText("TEMPO", tempo.getBounds().withY(tempo.getY() - 14).withHeight(12), juce::Justification::centredLeft);
    g.drawText("DRAG", drag.getBounds().withY(drag.getY() - 14).withHeight(12), juce::Justification::centred);
    g.drawText("PUSH", push.getBounds().withY(push.getY() - 14).withHeight(12), juce::Justification::centred);
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
    : processor(p),
      mz303(p.rack.mz303, p.rack.channel[Rack::M303], [&p] { return p.rack.playing.load(); },
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

    // the melodic machines' step rolls fold away; the drum machines' grids are their panels
    mz303.onFold = mz101.onFold = mz950.onFold = [this] { folded(); };
}

void RackView::folded()
{
    processor.folded[Rack::M303] = mz303.isFolded();
    processor.folded[Rack::M101] = mz101.isFolded();
    processor.folded[Rack::M950] = mz950.isFolded();
    if (onHeightChanged) onHeightChanged();
}

int RackView::unitTop(int machine)
{
    const auto all = units();
    return all[(size_t) juce::jlimit(0, (int) all.size() - 1, machine)]->getY();
}

void RackView::flash(int machine)
{
    flashing = machine;
    flashTicks = 30;   // a second, at the panel's 30 a second
    repaint();
}

void RackView::paintOverChildren(juce::Graphics& g)
{
    if (flashing < 0 || flashTicks <= 0) return;

    const auto r = units()[(size_t) flashing]->getBounds().toFloat().expanded(3.0f);
    g.setColour(colours::ledGreen.withAlpha(0.9f * (float) flashTicks / 30.0f));
    g.drawRoundedRectangle(r, 5.0f, 3.0f);
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

    if (flashTicks > 0 && --flashTicks >= 0) repaint();
}

void RackView::refresh()
{
    mz303.refresh();
    mz101.refresh();
    mz808.refresh();
    mz909.refresh();
    mz950.refresh();

    mz303.setFolded(processor.folded[Rack::M303]);
    mz101.setFolded(processor.folded[Rack::M101]);
    mz950.setFolded(processor.folded[Rack::M950]);
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
    : processor(p), transport(p), effects(p.rack), rack(p), arrangeView(p)
{
    setLookAndFeel(&look);
    setWantsKeyboardFocus(true);

    transport.onNew = [this]
    {
        // 1 the factory grooves, 2 an empty rack, 0 neither
        juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon, "Start again?",
            "Every pattern and knob will be replaced - with the factory grooves (four banks: acid, "
            "techno, electro and deep house), or with an empty rack to write your own.",
            "Factory grooves", "Empty rack", "Cancel", this,
            juce::ModalCallbackFunction::create([this] (int choice)
            {
                if (choice == 0) return;
                processor.rack.playing = false;
                processor.startAfresh(choice == 1);
            }));
    };
    transport.onLoad = [this] { loadFile(); };
    transport.onSave = [this] { saveFile(); };
    addAndMakeVisible(transport);
    addAndMakeVisible(effects);

    viewport.setViewedComponent(&rack, false);
    viewport.setScrollBarsShown(true, false);
    viewport.setScrollBarThickness(12);
    viewport.getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId, colours::screw.withAlpha(0.7f));
    viewport.setSingleStepSizes(40, 40);
    addAndMakeVisible(viewport);
    addChildComponent(arrangeView);

    rack.onHeightChanged = [this] { resized(); };
    arrangeView.onShowMachine = [this] (int machine) { showMachine(machine); };

    rackTab.setTooltip("The rack: the machines' panels, their patterns and the mixer (F1).");
    arrangeTab.setTooltip("The Arrange window: the song, its tracks and parts, as Cubase laid them out (F2).");
    rackTab.onPress = [this] { showView((int) DebirthProcessor::View::Rack); };
    arrangeTab.onPress = [this] { showView((int) DebirthProcessor::View::Arrange); };
    songMode.setTooltip("SONG MODE: the arrangement plays the machines. Off, as in ReBirth's PATTERN mode, "
                        "they play their patterns. Changed while playing, it waits for the bar.");
    songMode.onPress = [this]
    {
        auto& mode = processor.rack.mode;
        mode = mode.load() == Mode::Song ? Mode::Pattern : Mode::Song;
    };
    for (auto* t : { &rackTab, &arrangeTab, &songMode })
    {
        t->setLedColour(colours::ledGreen);
        addAndMakeVisible(*t);
    }

    //
    // Where to come back to: the version this is, and the page the next one will be on.
    // Nothing here checks for updates by itself - the app never goes online unasked.
    //
    projectLink.setButtonText("MzDebirth " JucePlugin_VersionString "  -  github.com/simozzer/MzDebirth");
    projectLink.setURL(juce::URL("https://github.com/simozzer/MzDebirth"));
    projectLink.setFont(fonts::bold(11.5f).withStyle(juce::Font::bold | juce::Font::underlined), false,
                        juce::Justification::centred);
    projectLink.setColour(juce::HyperlinkButton::textColourId, juce::Colour(0xff6ab0ff));   // a link's blue, and underlined
    projectLink.setTooltip("The project's page: the latest version and what's new in it, the source, "
                           "and where to report a problem. Opens in your browser.");
    projectLink.setWantsKeyboardFocus(false);   // or the space bar would open it rather than play
    addAndMakeVisible(projectLink);

    showView((int) processor.view);

    startTimerHz(30);
}

void MainPanel::showView(int v)
{
    const auto view = (DebirthProcessor::View) v;
    processor.view = view;
    const bool arranging = view == DebirthProcessor::View::Arrange;
    viewport.setVisible(! arranging);
    arrangeView.setVisible(arranging);
    rackTab.setLit(! arranging);
    arrangeTab.setLit(arranging);
    if (isShowing()) grabKeyboardFocus();
}

// From a track's RACK in the Arrange window: the rack, scrolled to that machine, which rings
// for a moment so the eye finds it.
void MainPanel::showMachine(int machine)
{
    showView((int) DebirthProcessor::View::Rack);
    viewport.setViewPosition(0, juce::jmax(0, rack.unitTop(machine) - gap));
    rack.flash(machine);
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
    effects.tick();

    // SONG MODE: lit when on; blinking while a change waits for the bar
    const bool playing = processor.rack.playing.load();
    const bool wanted = processor.rack.mode.load() == Mode::Song;
    const bool waiting = playing && wanted != (processor.rack.playingNow() == Mode::Song);
    songMode.setLit(waiting ? blinkPhase : wanted);

    if (arrangeView.isVisible()) arrangeView.tick();
    else                         rack.tick(blinkPhase);
}

void MainPanel::refresh()
{
    transport.refresh();
    effects.refresh();
    rack.refresh();
    resized();   // the step rolls may have folded or unfolded with the load
    arrangeView.refresh();
    showView((int) processor.view);
}

bool MainPanel::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey)
    {
        processor.rack.playing = ! processor.rack.playing.load();
        return true;
    }
    if (k == juce::KeyPress::F1Key) { showView((int) DebirthProcessor::View::Rack); return true; }
    if (k == juce::KeyPress::F2Key) { showView((int) DebirthProcessor::View::Arrange); return true; }

    // DRAG and PUSH work while held, which keyStateChanged follows; the presses are theirs
    if (k.getKeyCode() == juce::KeyPress::leftKey || k.getKeyCode() == juce::KeyPress::rightKey)
        return true;

    return arrangeView.isVisible() && arrangeView.handleKey(k);
}

bool MainPanel::keyStateChanged(bool)
{
    const bool left = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::leftKey);
    const bool right = juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::rightKey);
    transport.setKeyNudge((right ? 1 : 0) - (left ? 1 : 0));
    return false;
}

// a key let go in another window would never be seen going up
void MainPanel::focusLost(FocusChangeType) { transport.setKeyNudge(0); }

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
    r.removeFromTop(gap);
    effects.setBounds(r.removeFromTop(effectsHeight));
    r.removeFromTop(gap);

    auto tabs = r.removeFromTop(tabHeight);
    rackTab.setBounds(tabs.removeFromLeft(110));
    tabs.removeFromLeft(4);
    arrangeTab.setBounds(tabs.removeFromLeft(110));
    songMode.setBounds(tabs.removeFromRight(120));
    projectLink.setBounds(tabs.reduced(16, 0));
    r.removeFromTop(gap);

    viewport.setBounds(r);
    rack.setSize(r.getWidth() - viewport.getScrollBarThickness() - 4, rack.preferredHeight());
    arrangeView.setBounds(r.withTrimmedBottom(gap));
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
    : AudioProcessorEditor(&p), owner(p), panel(p)
{
    tooltips.setLookAndFeel(&panel.getLook());
    addAndMakeVisible(panel);
    panel.setBounds(0, 0, MainPanel::designWidth, MainPanel::designHeight);

    //
    // The width scales the panel, as it always has; the height is free. A taller window is a
    // taller panel - more of the rack, or of the song, to see at once - rather than a bigger
    // one. Never under 800, where the rack would be left a sliver.
    //
    // Read before the limits go on: setting them resizes the window to fit them, and resized()
    // would write that over the size the app was left at.
    const int w = p.windowWidth > 0 ? p.windowWidth : MainPanel::designWidth;
    const int h = p.windowHeight > 0 ? p.windowHeight : MainPanel::designHeight;

    setResizable(true, true);
    setResizeLimits(MainPanel::designWidth / 2, minHeight, MainPanel::designWidth * 2, 4000);
    setSize(juce::jlimit(MainPanel::designWidth / 2, MainPanel::designWidth * 2, w), juce::jmax(minHeight, h));
}

void DebirthEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::rail);
}

void DebirthEditor::resized()
{
    // scaled to the width; as tall, at that scale, as the window is
    const float scale = (float) getWidth() / (float) MainPanel::designWidth;
    panel.setBounds(0, 0, MainPanel::designWidth, juce::roundToInt((float) getHeight() / scale));
    panel.setTransform(juce::AffineTransform::scale(scale));

    owner.windowWidth = getWidth();
    owner.windowHeight = getHeight();
}
