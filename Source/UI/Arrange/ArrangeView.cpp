// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "ArrangeView.h"

#include <algorithm>
#include <cmath>

using namespace debirth;

namespace arrange
{

namespace
{
    constexpr int toolbarHeight = 36;
    constexpr int transportHeight = 46;
    constexpr int trackListWidth = 250;
    constexpr int bar = 12;   // the scroll bars

    const char* const toolTips[Tool::count] = {
        "ARROW: pick parts (shift for more), drag to move, Alt-drag to copy, drag the right edge to "
        "resize. Double-click a part to edit it.",
        "PENCIL: drag out a new, empty part - or click for a bar's.",
        "ERASER: click a part to delete it.",
        "SCISSORS: click a part to cut it in two there.",
        "GLUE: click a part to join it to the next on its track.",
        "MUTE: click a part to silence it, or to let it sound again.",
    };
}

ArrangeView::ArrangeView(DebirthProcessor& p)
    : state(p), tracks(state), ruler(state), lane(state), transport(state)
{
    state.changed = [this] { update(); };
    state.showMachine = [this] (int machine) { if (onShowMachine) onShowMachine(machine); };

    for (int t = 0; t < Tool::count; ++t)
    {
        tools[t].setText(toolName(t));
        tools[t].setLedColour(colours::stepOrange);
        tools[t].setTooltip(toolTips[t]);
        tools[t].onPress = [this, t] { state.tool = t; update(); };
        addAndMakeVisible(tools[t]);
    }

    snap.setShowsLed(false);
    snap.setTooltip("SNAP: what parts move and size to - a bar, a beat, a sixteenth, or nothing.");
    snap.onPress = [this] { state.snap = (state.snap + 1) % Snap::count; update(); };
    zoomOut.setShowsLed(false);
    zoomIn.setShowsLed(false);
    zoomOut.setTooltip("Zoom out. Ctrl and the wheel zoom about the mouse.");
    zoomIn.setTooltip("Zoom in. Ctrl and the wheel zoom about the mouse.");
    zoomOut.onPress = [this] { zoom(1.0 / 1.5); };
    zoomIn.onPress = [this] { zoom(1.5); };
    undo.setShowsLed(false);
    redo.setShowsLed(false);
    undo.setTooltip("Undo the last edit to the song (Ctrl+Z).");
    redo.setTooltip("Redo it (Ctrl+Y).");
    undo.onPress = [this] { state.doc().undo(); state.selection.clear(); update(); };
    redo.onPress = [this] { state.doc().redo(); state.selection.clear(); update(); };
    exportSong.setShowsLed(false);
    exportSong.setTooltip("Export the song to a WAV or FLAC file - the whole song, or between the locators when "
                          "CYCLE is on - rendered on a copy of the rack, so playing goes on undisturbed.");
    exportSong.onPress = [this] { if (onExport) onExport(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &snap, &zoomOut, &zoomIn, &undo, &redo, &exportSong })
        addAndMakeVisible(c);

    lane.onOpenPart = [this] (PartRef r) { openPart(r); };

    for (auto* b : { &hBar, &vBar })
    {
        b->setColour(juce::ScrollBar::thumbColourId, colours::screw.withAlpha(0.7f));
        b->addListener(this);
        b->setAutoHide(false);
        addAndMakeVisible(b);
    }

    addAndMakeVisible(tracks);
    addAndMakeVisible(ruler);
    addAndMakeVisible(lane);
    addAndMakeVisible(transport);
    update();
}

ArrangeView::~ArrangeView()
{
    hBar.removeListener(this);
    vBar.removeListener(this);
}

juce::Rectangle<int> ArrangeView::editorArea() const
{
    return getLocalBounds().withTrimmedBottom(transportHeight + 4);
}

void ArrangeView::openPart(PartRef r)
{
    closeEditor();
    editor = std::make_unique<PartEditor>(state, r);
    // closed after the click that asked for it is done with the editor
    juce::Component::SafePointer<ArrangeView> self(this);
    editor->onClose = [self]
    {
        juce::MessageManager::callAsync([self] { if (self != nullptr) self->closeEditor(); });
    };
    addAndMakeVisible(*editor);
    editor->setBounds(editorArea());
}

void ArrangeView::closeEditor()
{
    if (editor == nullptr) return;
    removeChildComponent(editor.get());
    editor.reset();
    update();
}

void ArrangeView::zoom(double by)
{
    // about the middle of the lanes
    const double middle = state.tickAt(lane.getWidth() * 0.5);
    state.pxPerTick = juce::jlimit(4.0 / ticksPerBar, 2000.0 / ticksPerBar, state.pxPerTick * by);
    state.scrollTick = juce::jmax(0.0, middle - lane.getWidth() * 0.5 / state.pxPerTick);
    update();
}

void ArrangeView::update()
{
    const auto& song = state.song();

    // keep the picks good: an edit may have taken a track or part away
    state.track = juce::jlimit(0, juce::jmax(0, (int) song.tracks.size() - 1), state.track);
    for (auto it = state.selection.begin(); it != state.selection.end(); )
    {
        const bool ok = it->first < (int) song.tracks.size() && it->second < (int) song.tracks[(size_t) it->first].parts.size();
        it = ok ? std::next(it) : state.selection.erase(it);
    }

    // the scroll bars: the song and some room after it; the tracks and one more
    const double width = lane.getWidth() > 0 ? lane.getWidth() / state.pxPerTick : (double) ticksPerBar * 16;
    const double length = std::max({ (double) song.end(), (double) song.right, state.now(), state.scrollTick + width })
                        + 16.0 * ticksPerBar;
    hBar.setRangeLimits(0.0, length, juce::dontSendNotification);
    hBar.setCurrentRange(state.scrollTick, width, juce::dontSendNotification);

    const int total = (rowCount(song) + 1) * trackHeight;
    state.scrollY = juce::jlimit(0, juce::jmax(0, total - lane.getHeight()), state.scrollY);
    vBar.setRangeLimits(0.0, juce::jmax((double) total, (double) lane.getHeight()), juce::dontSendNotification);
    vBar.setCurrentRange(state.scrollY, lane.getHeight(), juce::dontSendNotification);

    for (int t = 0; t < Tool::count; ++t) tools[t].setLit(state.tool == t);
    snap.setText(juce::String("SNAP ") + snapName(state.snap));
    undo.setAlpha(state.doc().canUndo() ? 1.0f : 0.45f);
    redo.setAlpha(state.doc().canRedo() ? 1.0f : 0.45f);

    tracks.repaint();
    ruler.repaint();
    lane.repaint();
}

void ArrangeView::scrollBarMoved(juce::ScrollBar* b, double start)
{
    if (b == &hBar) state.scrollTick = start;
    else            state.scrollY = (int) start;
    tracks.repaint();
    ruler.repaint();
    lane.repaint();
}

void ArrangeView::tick()
{
    auto& rack = state.processor.rack;

    // which machines are sounding
    std::array<bool, Rack::channels> active {};
    active[Rack::M303] = rack.mz303.view().gateIsOn();
    active[Rack::M101] = rack.mz101.view().gateIsOn();
    auto hits = [] (const auto& engine, int voices)
    {
        uint32_t n = 0;
        for (int v = 0; v < voices; ++v) n += engine.hitCount(v);
        return n;
    };
    const uint32_t h808 = hits(rack.mz808.view(), Mz808::voices), h909 = hits(rack.mz909.view(), Mz909::voices);
    active[Rack::M808] = h808 != lastHits[Rack::M808];
    active[Rack::M909] = h909 != lastHits[Rack::M909];
    lastHits[Rack::M808] = h808;
    lastHits[Rack::M909] = h909;
    active[Rack::M950] = rack.mz950.activeVoices() > 0;
    active[Rack::M7]   = rack.mz7.activeVoices() > 0;
    tracks.setActivity(active);

    transport.tick();

    // an editor open on a part an undo has taken away goes
    if (editor != nullptr)
    {
        if (editor->stillValid()) editor->tick();
        else                      closeEditor();
    }

    if (state.doc().version() != seenVersion)
    {
        seenVersion = state.doc().version();
        update();
    }

    // the song playing, or a take recording into it: keep it in view, a page at a time as
    // Cubase did
    if (state.songPlaying() || state.processor.taking())
    {
        if (state.follow && lane.getWidth() > 0)
        {
            const double x = state.xOf(state.now());
            if (x < 0.0 || x > lane.getWidth() - 20.0)
            {
                state.scrollTick = juce::jmax(0.0, std::floor(state.now() / ticksPerBar) * ticksPerBar);
                update();
            }
        }
        ruler.repaint();
        lane.repaint();
    }
}

void ArrangeView::refresh()
{
    closeEditor();
    state.selection.clear();
    state.scrollTick = 0.0;
    state.scrollY = 0;

    // the whole song in view, as far as it will go and still be read
    const int end = state.song().end();
    if (end > 0 && lane.getWidth() > 0)
        state.pxPerTick = juce::jlimit(12.0 / ticksPerBar, 64.0 / ticksPerBar,
                                       lane.getWidth() / (double) (end + ticksPerBar));
    update();
}

bool ArrangeView::handleKey(const juce::KeyPress& k)
{
    const int code = k.getKeyCode();
    const bool ctrl = k.getModifiers().isCtrlDown() || k.getModifiers().isCommandDown();

    // the ST's numeric keypad
    if (code == juce::KeyPress::numberPadDivide)  { transport.toggleCycle(); return true; }
    if (code == juce::KeyPress::numberPad0)       { state.processor.rack.playing = false; return true; }
    if (code == juce::KeyPress::numberPadDecimalPoint) { transport.toStart(); return true; }
    if (code == juce::KeyPress::returnKey && ! ctrl) { transport.playFromCursor(); return true; }

    if (ctrl && (code == 'Z' || code == 'z')) { state.doc().undo(); state.selection.clear(); update(); return true; }
    if (ctrl && (code == 'Y' || code == 'y')) { state.doc().redo(); state.selection.clear(); update(); return true; }

    // with a part open, the rest of the keys are its editor's
    if (editor != nullptr) return editor->handleKey(k);

    if (ctrl && (code == 'C' || code == 'c')) { lane.copySelection(); return true; }
    if (ctrl && (code == 'V' || code == 'v')) { lane.paste(); return true; }
    if (ctrl && (code == 'A' || code == 'a'))
    {
        state.selection.clear();
        const auto& song = state.song();
        for (int t = 0; t < (int) song.tracks.size(); ++t)
            for (int i = 0; i < (int) song.tracks[(size_t) t].parts.size(); ++i) state.selection.insert({ t, i });
        update();
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { lane.deleteSelection(); return true; }

    return false;
}

void ArrangeView::resized()
{
    auto r = getLocalBounds();

    transport.setBounds(r.removeFromBottom(transportHeight));
    r.removeFromBottom(4);

    toolbar = r.removeFromTop(toolbarHeight);
    {
        auto t = toolbar.reduced(8, 4);
        for (auto& p : tools)
        {
            p.setBounds(t.removeFromLeft(82));
            t.removeFromLeft(4);
        }
        t.removeFromLeft(12);
        snap.setBounds(t.removeFromLeft(90));
        t.removeFromLeft(12);
        zoomOut.setBounds(t.removeFromLeft(30));
        t.removeFromLeft(4);
        zoomIn.setBounds(t.removeFromLeft(30));

        redo.setBounds(t.removeFromRight(60));
        t.removeFromRight(4);
        undo.setBounds(t.removeFromRight(60));
        t.removeFromRight(16);
        exportSong.setBounds(t.removeFromRight(70));
    }

    tracks.setBounds(r.removeFromLeft(trackListWidth).withTrimmedBottom(bar));
    vBar.setBounds(r.removeFromRight(bar).withTrimmedTop(rulerHeight).withTrimmedBottom(bar));
    hBar.setBounds(r.removeFromBottom(bar));
    ruler.setBounds(r.removeFromTop(rulerHeight));
    lane.setBounds(r);

    if (editor != nullptr) editor->setBounds(editorArea());
    update();
}

void ArrangeView::paint(juce::Graphics& g)
{
    g.fillAll(colours::rail);
    drawFace(g, toolbar.toFloat(), colours::mixer);
}

} // namespace arrange
