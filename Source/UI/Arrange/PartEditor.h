// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "ArrangeState.h"

namespace arrange
{

//
// A part opened for editing, as Cubase opened one from the Arrange window:
//
//   KEY EDIT   for the 303, the 101 and the 950 - a row a semitone, the keys down the side
//              (the 950's named by the samples its programme plays); notes drawn with the
//              pencil, moved and stretched with the arrow; a 303 note that overlaps the next
//              slides into it, and is drawn joined to it
//   DRUM EDIT  for the 808 and the 909 - a row an instrument, from the drum map; click a
//              step to strike it, click a hit to take it away, Alt-click to accent it
//
// Both have a velocity lane along the bottom - drag across it to draw the velocities - and
// Q, the grid notes are drawn and quantized to. Every gesture is one undo.
//
class PartEditor final : public juce::Component, private juce::ScrollBar::Listener
{
public:
    PartEditor(State&, PartRef);
    ~PartEditor() override;

    std::function<void()> onClose;

    bool isOpenOn(PartRef r) const { return r == ref; }
    bool stillValid() const;

    void tick();                                  // from the panel's timer
    bool handleKey(const juce::KeyPress&);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class EditTool { Arrow, Pencil, Eraser, count };
    enum class Drag { None, Move, Resize, Draw, Box, Velocity };

    // the rows: a semitone each in KEY EDIT, top note first; an instrument each in DRUM EDIT
    int rows() const;
    int rowHeight() const { return drums ? 22 : 11; }
    int pitchOfRow(int row) const;
    int rowOfPitch(int pitch) const;               // -1 if no row plays it
    juce::String rowLabel(int row) const;

    juce::Rectangle<int> grid() const;             // where the notes are drawn
    juce::Rectangle<int> velocityLane() const;
    // the part's first tick sits a little in from the edge, so a hit on it shows whole
    static constexpr float inset = 8.0f;
    float xOf(double tick) const  { return (float) grid().getX() + inset + (float) ((tick - scrollTick) * pxPerTick); }
    double tickAt(float x) const  { return (x - (float) grid().getX() - inset) / pxPerTick + scrollTick; }
    float yOfRow(int row) const   { return (float) (grid().getY() + row * rowHeight() - scrollY); }
    int rowAt(float y) const;
    juce::Rectangle<float> noteBounds(const debirth::Note&) const;
    int noteAt(juce::Point<float>) const;          // the index of the note there, or -1
    bool onRightEdge(juce::Point<float>, int note) const;

    int quantum() const;
    int snap(double tick) const;

    void load();                                   // the part's notes, from the song
    void commit();                                 // and back, as one undo
    void quantize();
    void deleteSelected();
    void setTool(EditTool);
    void update();

    void scrollBarMoved(juce::ScrollBar*, double) override;

    State& state;
    PartRef ref;
    bool drums = false;
    int machine = 0;
    juce::Colour colour;

    std::vector<debirth::Note> notes;              // the part's, as being edited
    std::set<int> picked;
    int partLength = debirth::ticksPerBar;
    int seenVersion = -1;

    EditTool tool = EditTool::Pencil;
    int q = 2;                                     // the quanta's index: 1/16
    double pxPerTick = 0.5, scrollTick = 0.0;
    int scrollY = 0;

    Drag drag = Drag::None;
    juce::Point<float> downAt;
    float lastX = 0.0f;                            // the velocity lane: where the last drag was
    int dragNote = -1;
    std::vector<debirth::Note> before;             // the notes as the drag found them
    juce::Rectangle<float> box;

    Pad back { "< ARRANGE" }, tools[3], qPad, quantizeButton { "QUANTIZE" }, zoomOut { "-" }, zoomIn { "+" };
    juce::ScrollBar hBar { false }, vBar { true };
    juce::Rectangle<int> header;
};

} // namespace arrange
