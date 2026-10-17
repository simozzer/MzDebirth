// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Widgets.h"

//
// The sampler's steps, as a grid of notes two octaves high, any number of them on a step,
// with a row of accents and a row of ties under them. A tied step carries on the notes it
// shares with the step before, rather than striking them again.
//
// The keys the programme answers are lit; a keygroup of one key - a drum, usually - is
// named by its sample.
//
class SampleRoll final : public juce::Component
{
public:
    explicit SampleRoll(debirth::Track<debirth::NotePattern>&);

    static constexpr int visibleNotes = 24;

    void setSpans(std::vector<debirth::Mz950::Span>);
    void setLowest(int note);
    int lowest() const { return low; }

    void tick(bool playing);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    static constexpr int labelWidth = 92, headerHeight = 12;
    static constexpr int rows = visibleNotes + 2;   // then ACCENT, then TIE

    juce::Rectangle<int> cell(int step, int row) const;
    int stepAt(int x) const;
    int rowAt(int y) const;
    int patternRowOf(int row) const;                // the NotePattern row a screen row shows
    void apply(int step, int row);

    debirth::Track<debirth::NotePattern>& track;
    std::vector<debirth::Mz950::Span> spans;
    int low = 36;

    bool paintOn = true;
    int lastStep = -1;
    int shownPosition = -2;
    float wheel = 0;
};

class SamplerRack final : public juce::Component
{
public:
    SamplerRack(debirth::Mz950&, debirth::Channel&, std::function<bool()> playing, const Theme&);

    void tick(bool blinkPhase);
    void refresh();

    // the name, the disk row, the knobs, the fold bar and the roll - or folded, no roll; the
    // bar stays where it is either way
    int preferredHeight() const
    {
        const int top = 8 + 30 + 28 + 8 + 64 + 8 + FoldBar::height;
        return isFolded() ? juce::jmax(foldedMinimum, top + 8) : top + 2 + 12 + 26 * 10 + 10;
    }

    // the step roll folded away, and told of when its bar is clicked - the height changes
    void setFolded(bool);
    bool isFolded() const { return ! fold.isOpen(); }
    std::function<void()> onFold;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void chooseDisk();
    void showDisk();      // the disk and programme have changed: everything that shows them

    debirth::Mz950& sampler;
    std::function<bool()> playing;
    Theme theme;

    PatternPanel patterns;
    Pad loadDisk { "LOAD DISK" };
    juce::ComboBox programBox;
    Pad octaveDown { "OCT -" }, octaveUp { "OCT +" };
    std::vector<std::unique_ptr<juce::Component>> controls;
    FoldBar fold;
    SampleRoll roll;
    MixerStrip strip;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String diskText, message;
    juce::Rectangle<int> face, display;
};
