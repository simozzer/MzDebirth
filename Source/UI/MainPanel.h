// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "DrumRack.h"
#include "EffectsBar.h"
#include "KeyPanel.h"
#include "MonoRack.h"
#include "SamplerRack.h"
#include "Processor.h"

//
// Along the top: PLAY and STOP, the tempo, the shuffle, the master level, and the files.
// The space bar plays and stops too.
//
class TransportBar final : public juce::Component
{
public:
    explicit TransportBar(DebirthProcessor&);

    void tick();
    void refresh();

    std::function<void()> onNew, onLoad, onSave;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DebirthProcessor& processor;

    Pad play { "PLAY" }, stop { "STOP" };
    LedNumber tempo;
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
    int preferredHeight() const;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MonoRack mz303, mz101;
    DrumRack mz808, mz909;
    SamplerRack mz950;

    std::vector<juce::Component*> units() { return { &mz303, &mz101, &mz808, &mz909, &mz950 }; }
};

//
// The whole window, at one design size: the transport, and the rack scrolling under it.
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

    Look& getLook() { return look; }

private:
    void timerCallback() override;
    void refresh();
    void saveFile();
    void loadFile();

    DebirthProcessor& processor;
    Look look;

    TransportBar transport;
    EffectsBar effects;
    RackView rack;
    juce::Viewport viewport;

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

private:
    MainPanel panel;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebirthEditor)
};
