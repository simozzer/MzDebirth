// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Widgets.h"

//
// A mono line's steps, as a piano roll: a column a step, a row a note from C to the C above,
// and under them the switches each step has - UP and DOWN an octave, ACCENT if the synth has
// one, and SLIDE.
//
// Click a cell to put that note on that step, or click the note there (or right-click) to
// make it a rest; drag along to keep going. Steps past the pattern's length are shaded, and
// kept.
//
class NoteRoll final : public juce::Component
{
public:
    NoteRoll(debirth::Track<debirth::BassPattern>&, bool hasAccent);

    void tick(bool playing);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    enum class Kind { Note, Up, Down, Accent, Slide };

    static constexpr int notes = 13, labelWidth = 40, headerHeight = 12;

    int rows() const { return notes + (hasAccent ? 4 : 3); }
    Kind kindOf(int row) const;
    juce::Rectangle<int> cell(int step, int row) const;
    int stepAt(int x) const;
    int rowAt(int y) const;
    void apply(int step, int row);

    debirth::Track<debirth::BassPattern>& track;
    const bool hasAccent;

    // what a drag does to each step it crosses, chosen by the first one
    bool paintOn = true;
    int lastStep = -1;
    int shownPosition = -2;
};

//
// A mono synth's strip: its pattern section, its panel with the step roll under the knobs,
// and its mixer channel.
//
// The panel's controls are laid out in rows of sections, each a name over a run of
// controls in the order the machine's table lists them.
//
class MonoRack final : public juce::Component
{
public:
    struct Section { const char* name; int count; };
    using Row = std::vector<Section>;

    template <class Line>
    MonoRack(Line& line, debirth::Channel& channel, std::function<bool()> playing,
             const Theme& t, std::vector<Row> layout, bool hasAccent)
        : MonoRack(line.track, channel, std::move(playing), t, std::move(layout), hasAccent)
    {
        for (int i = 0; i < line.knobs.size(); ++i)
        {
            controls.push_back(makeControl(line.knobs, i, theme));
            addAndMakeVisible(*controls.back());
        }
        jassert((int) controls.size() == countOf(rows));
    }

    void tick(bool blinkPhase);
    void refresh();
    int preferredHeight() const;

    // the step roll folded away, and told of when its bar is clicked - the height changes
    void setFolded(bool);
    bool isFolded() const { return ! fold.isOpen(); }
    std::function<void()> onFold;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    MonoRack(debirth::Track<debirth::BassPattern>&, debirth::Channel&, std::function<bool()> playing,
             const Theme&, std::vector<Row>, bool hasAccent);

    static int countOf(const std::vector<Row>&);

    static constexpr int controlWidth = 54, controlHeight = 64, sectionGap = 12, sectionLabel = 13;

    std::function<bool()> playing;
    Theme theme;
    std::vector<Row> rows;

    PatternPanel patterns;
    std::vector<std::unique_ptr<juce::Component>> controls;
    FoldBar fold;
    NoteRoll roll;
    MixerStrip strip;

    juce::Rectangle<int> face, titleArea;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> sectionAreas;
};
