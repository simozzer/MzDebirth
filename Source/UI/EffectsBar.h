// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Widgets.h"

//
// The PCF's pattern, drawn: a bar a step, as tall as the level the filter is struck at, the
// step playing lit.
//
class PcfDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PcfDisplay(debirth::Rack&);

    void tick();
    void paint(juce::Graphics&) override;

private:
    debirth::Rack& rack;
    int shownPattern = -1, shownStep = -2;
};

//
// The effects units, along under the transport where they are always to hand, as ReBirth
// kept them: DIST, PCF, DELAY and COMP. Which channels go through them is on each channel's
// mixer strip.
//
class EffectsBar final : public juce::Component
{
public:
    explicit EffectsBar(debirth::Rack&);

    void tick();
    void refresh();
    void follow();   // the knobs and switches read back, as the song's automation moves them

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Unit
    {
        const char* name;
        std::vector<juce::Component*> parts;
        juce::Rectangle<int> area;
        int width;
    };

    juce::Component* control(const char* id);
    Pad* toggle(const char* id, const char* text, const char* tip);

    debirth::Rack& rack;
    Theme theme;

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::unique_ptr<LedNumber> pcfPattern, delaySteps;
    PcfDisplay pcfDisplay;
    std::vector<Unit> units;
    std::vector<std::pair<Pad*, int>> toggles;   // a pad, and the control it switches
};
