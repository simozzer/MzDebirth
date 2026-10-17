// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Widgets.h"

class DebirthProcessor;

//
// The key, as Mixed In Key and the Camelot wheel name it: drag up or down, or use the wheel,
// to go round the wheel's numbers; click to swap A (minor) and B (major).
//
class KeyDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit KeyDisplay(debirth::Rack&);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void step(int by);

    debirth::Rack& rack;
    int dragFrom = 0;
    bool dragged = false;
    float wheel = 0;
};

//
// The KEY section of the transport: the key, the keys that mix with it, and the bank it
// works on - the one the Mz303 is showing - with the key that bank is in; then the STYLE to
// write in, GENERATE, which fills the bank with new grooves in the key, and TRANSPOSE, which
// moves the bank's bass line and lead into it.
//
class KeyPanel final : public juce::Component
{
public:
    explicit KeyPanel(DebirthProcessor&);

    void tick();
    void refresh();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DebirthProcessor& processor;
    KeyDisplay display;
    Switch style;
    Pad generate { "GENERATE" }, transpose { "TRANSPOSE" };
    juce::Rectangle<int> text;
    int shownKey = -1, shownBank = -1, shownBankKey = -2;
};
