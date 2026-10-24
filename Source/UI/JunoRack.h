// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "SamplerRack.h"
#include "Widgets.h"

//
// The Mz106: a bank and a patch from it on an LCD, its performance sliders, and the same note
// roll as the sampler's - chords, any number of notes on a step, accents and ties.
//
class JunoRack final : public juce::Component
{
public:
    JunoRack(debirth::Mz106&, debirth::Channel&, std::function<bool()> playing, const Theme&);

    void tick(bool blinkPhase);
    void refresh();
    void follow();   // the knobs read back, as the song's automation moves them

    int preferredHeight() const
    {
        const int top = 8 + 30 + 28 + 8 + 64 + 8 + FoldBar::height;
        return isFolded() ? juce::jmax(foldedMinimum, top + 8) : top + 2 + 12 + 26 * 10 + 10;
    }

    void setFolded(bool);
    bool isFolded() const { return ! fold.isOpen(); }
    std::function<void()> onFold;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void chooseSyx();
    void showBank();

    debirth::Mz106& juno;
    std::function<bool()> playing;
    Theme theme;

    PatternPanel patterns;
    juce::ComboBox bankBox, programBox, morphBox;
    Pad loadSyx { "LOAD .SYX" };
    Pad octaveDown { "OCT -" }, octaveUp { "OCT +" };
    std::vector<std::unique_ptr<juce::Component>> controls;
    FoldBar fold;
    SampleRoll roll;
    MixerStrip strip;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String message;
    juce::Rectangle<int> face;
};
