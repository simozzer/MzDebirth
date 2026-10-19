// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Arrange/ArrangeView.h"
#include "DrumRack.h"
#include "EffectsBar.h"
#include "KeyPanel.h"
#include "MonoRack.h"
#include "SamplerRack.h"
#include "Processor.h"

//
// Along the top: PLAY, STOP and REC, the tempo with DRAG and PUSH beside it, the shuffle, the
// master level, and the files. The space bar plays and stops too, and the left and right
// arrow keys drag and push while they are held.
//
class TransportBar final : public juce::Component
{
public:
    explicit TransportBar(DebirthProcessor&);

    void tick();
    void refresh();

    // PLAY, from the button or the space bar: with REC on, a take from the song cursor
    void start();

    // the arrow keys' share of the nudge: -1 left held, +1 right, 0 neither or both
    void setKeyNudge(int dir);

    std::function<void()> onNew, onLoad, onSave;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void nudge();

    DebirthProcessor& processor;

    Pad play { "PLAY" }, stop { "STOP" }, rec { "REC" };
    LedNumber tempo;
    Pad drag, push;
    int padNudge = 0, keyNudge = 0;
    Knob shuffle, master;
    KeyPanel keyPanel;
    Pad fresh { "NEW" }, open { "LOAD" }, save { "SAVE" };
};

//
// The machines, one above another, between wooden cheeks - taller than the window, so it
// scrolls under the transport.
//
class RackView final : public juce::Component
{
public:
    explicit RackView(DebirthProcessor&);

    void tick(bool blinkPhase);
    void refresh();
    void follow();   // every machine's knobs and switches read back, as automation moves them
    int preferredHeight() const;

    // where a machine's unit begins, to scroll to; and a moment's ring round it, to find it by
    int unitTop(int machine);
    void flash(int machine);

    // a step roll folded or unfolded: the rack is a different height
    std::function<void()> onHeightChanged;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    void folded();

    DebirthProcessor& processor;
    int flashing = -1, flashTicks = 0;

    MonoRack mz303, mz101;
    DrumRack mz808, mz909;
    SamplerRack mz950;

    std::vector<juce::Component*> units() { return { &mz303, &mz101, &mz808, &mz909, &mz950 }; }
};

//
// The whole window, at one design size: the transport, the effects units, and under them
// either the rack, scrolling, or the Arrange window - RACK and ARRANGE, or F1 and F2.
//
class MainPanel final : public juce::Component, private juce::Timer
{
public:
    explicit MainPanel(DebirthProcessor&);
    ~MainPanel() override;

    static constexpr int designWidth = 1280;
    static constexpr int designHeight = 860;
    static constexpr int cheek = 18;          // the wooden ends

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType) override;

    Look& getLook() { return look; }

private:
    void timerCallback() override;
    void refresh();
    void saveFile();
    void loadFile();
    void showView(int view);   // View::Rack or View::Arrange
    void showMachine(int machine);

    DebirthProcessor& processor;
    Look look;

    TransportBar transport;
    EffectsBar effects;
    RackView rack;
    juce::Viewport viewport;
    arrange::ArrangeView arrangeView;
    Pad rackTab { "RACK" }, arrangeTab { "ARRANGE" }, songMode { "SONG MODE" };

    // in the rack, where the song is: a take's progress, the song playing, or the cursor
    arrange::LedText songPosition;

    // This version, and where to find the next: the project's page, between the tabs.
    juce::HyperlinkButton projectLink;

    std::unique_ptr<juce::FileChooser> chooser;
    int seenLoads = -1;
    int ticks = 0;
};

class DebirthEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DebirthEditor(DebirthProcessor&);

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int minHeight = 800;   // the window's, in pixels

private:
    DebirthProcessor& owner;
    MainPanel panel;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebirthEditor)
};
