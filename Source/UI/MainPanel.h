// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Arrange/ArrangeView.h"
#include "DrumRack.h"
#include "EffectsBar.h"
#include "KeyPanel.h"
#include "MonoRack.h"
#include "FmRack.h"
#include "JunoRack.h"
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
// The × at the top of a machine's panel: takes it out of the rack, its patterns and knobs
// kept for when it comes back.
//
class RemoveButton final : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onPress;

    void paint(juce::Graphics&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { repaint(); }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains(e.getPosition()) && onPress) onPress();
    }
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

    // a machine taken out by its ×: the rack and the tracks are different
    std::function<void()> onMachinesChanged;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    void folded();

    DebirthProcessor& processor;
    int flashing = -1, flashTicks = 0;
    RemoveButton removers[debirth::Rack::channels];
    void remove(int machine);

    // a panel for every slot: the first of each machine, then the second
    MonoRack mz303, mz101;
    DrumRack mz808, mz909;
    SamplerRack mz950;
    FmRack mz7;
    JunoRack mz106;
    MonoRack mz303b, mz101b;
    DrumRack mz808b, mz909b;
    SamplerRack mz950b;
    FmRack mz7b;
    JunoRack mz106b;

    std::vector<juce::Component*> units()
    {
        return { &mz303, &mz101, &mz808, &mz909, &mz950, &mz7, &mz106,
                 &mz303b, &mz101b, &mz808b, &mz909b, &mz950b, &mz7b, &mz106b };
    }

    // f with each slot's panel, as its own type, and the slot
    template <class F>
    void eachUnit(F&& f)
    {
        f(mz303, 0); f(mz101, 1); f(mz808, 2); f(mz909, 3); f(mz950, 4); f(mz7, 5); f(mz106, 6);
        f(mz303b, 7); f(mz101b, 8); f(mz808b, 9); f(mz909b, 10); f(mz950b, 11); f(mz7b, 12); f(mz106b, 13);
    }
    template <class F>
    void eachUnit(F&& f) const { const_cast<RackView*>(this)->eachUnit(std::forward<F>(f)); }
};

//
// The master's meter, and LIMIT, beside the tabs where both views see them: what the rack
// puts out, left and right, after the MASTER and LIMIT. LIMIT's light is green while it is
// on and doing nothing, and orange while it is taking something off - held a moment, so a
// single peak is seen.
//
class MasterSection final : public juce::Component
{
public:
    explicit MasterSection(DebirthProcessor&);

    void tick();      // from the panel's timer
    void refresh();   // after a load

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DebirthProcessor& processor;
    LevelMeter meter;
    Pad limit { "LIMIT" };
    int limitIndex = -1;
    int limitingFor = 0;   // ticks left to show a reduction
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
    void saveFile(std::function<void(bool saved)> done = {});
    bool writeRack(const juce::File&);
    void askToSave(std::function<void(bool keep)> quit);
    void loadFile();
    void exportFile();
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
    Pad machines { "+ MACHINE" };   // a machine into the rack

    // in the rack, where the song is: a take's progress, the song playing, or the cursor
    arrange::LedText songPosition;
    MasterSection masterSection;

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
