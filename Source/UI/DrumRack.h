// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Widgets.h"

//
// A drum machine's strip: its knobs in a column per instrument, as on the hardware, then
// the instrument select and the sixteen step keys - pick an instrument, or AC for the
// accents, and the keys show and set its steps. An instrument with steps written has a
// bright edge.
//
class DrumRack final : public juce::Component
{
public:
    // a column of the panel: its name under it, and the ids of its knobs, top to bottom
    struct ColumnSpec { const char* label; std::vector<const char*> ids; };

    // an instrument on the select row
    struct VoiceSpec { const char* code; const char* name; };

    // the colours along the step keys, and under each column - the 808's red to white
    struct Palette
    {
        std::function<juce::Colour(int step)> stepFace;
        juce::Colour stepInk, selectFace, selectInk;
        std::function<juce::Colour(int column, int columns)> band;
        juce::Colour bandInk, ledColour;
    };

    template <class Machine>
    DrumRack(Machine& m, debirth::Channel& channel, std::function<bool()> playing, const Theme& t,
             std::vector<ColumnSpec> layout, std::vector<VoiceSpec> voiceList, Palette palette)
        : DrumRack(m.track, channel, std::move(playing), t, std::move(voiceList), std::move(palette))
    {
        for (const auto& spec : layout)
        {
            Column col { spec.label, {}, {} };
            for (const char* id : spec.ids)
            {
                const int i = m.knobs.indexOf(id);
                jassert(i >= 0);
                controls.push_back(makeControl(m.knobs, i, theme));
                addAndMakeVisible(*controls.back());
                col.controls.push_back(controls.back().get());
            }
            maxKnobs = juce::jmax(maxKnobs, (int) col.controls.size());
            columns.push_back(std::move(col));
        }
    }

    void tick(bool blinkPhase);
    void refresh();
    void follow();   // the knobs and switches read back, as the song's automation moves them
    int preferredHeight() const;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    DrumRack(debirth::Track<debirth::DrumPattern>&, debirth::Channel&, std::function<bool()> playing,
             const Theme&, std::vector<VoiceSpec>, Palette);

    struct Column
    {
        const char* label;
        std::vector<juce::Component*> controls;
        juce::Rectangle<int> area;
    };

    static constexpr int knobHeight = 50, labelHeight = 24;

    void showRow();
    int rowOfPad(int pad) const;

    debirth::Track<debirth::DrumPattern>& track;
    std::function<bool()> playing;
    Theme theme;
    Palette palette;

    PatternPanel patterns;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<Column> columns;
    int maxKnobs = 1;

    std::vector<VoiceSpec> voiceList;
    std::vector<std::unique_ptr<Pad>> voices;   // AC first, then the instruments
    Pad steps[debirth::stepsPerPattern];
    int row = 0;                                  // the pattern row the keys show

    MixerStrip strip;
    juce::Rectangle<int> face;
};
