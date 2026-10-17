// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "ArrangeTransport.h"
#include "PartEditor.h"
#include "PartLane.h"
#include "Ruler.h"
#include "TrackList.h"

namespace arrange
{

//
// THE ARRANGE WINDOW, after Cubase on the Atari ST: the tracks down the left, the parts along
// each one's lane under a ruler of bars, the toolbox along the top, the transport along the
// bottom. The rack's machines are still where they were, a click away, for the sound; this
// is where the song is put together.
//
class ArrangeView final : public juce::Component, private juce::ScrollBar::Listener
{
public:
    explicit ArrangeView(DebirthProcessor&);
    ~ArrangeView() override;

    void tick();      // from the panel's timer
    void refresh();   // after a load

    bool handleKey(const juce::KeyPress&);

    void paint(juce::Graphics&) override;
    void resized() override;

    // a part opened in KEY EDIT or DRUM EDIT, over the lanes; and closed again
    void openPart(PartRef);
    void closeEditor();

    // a track's RACK: the window shows the rack, at that machine
    std::function<void(int machine)> onShowMachine;

private:
    void scrollBarMoved(juce::ScrollBar*, double) override;
    juce::Rectangle<int> editorArea() const;
    void update();       // the scroll bars and tool lights, after anything changes
    void zoom(double by);

    State state;
    TrackList tracks;
    Ruler ruler;
    PartLane lane;
    ArrangeTransport transport;
    juce::ScrollBar hBar { false }, vBar { true };
    std::unique_ptr<PartEditor> editor;

    Pad tools[Tool::count];
    Pad snap, zoomOut { "-" }, zoomIn { "+" }, undo { "UNDO" }, redo { "REDO" };

    juce::Rectangle<int> toolbar;
    int seenVersion = -1;
    std::array<uint32_t, debirth::Rack::channels> lastHits {};
};

} // namespace arrange
