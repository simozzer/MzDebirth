// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "PartEditor.h"

#include <algorithm>
#include <cmath>

using namespace debirth;

namespace arrange
{

namespace
{
    constexpr int headerHeight = 40;
    constexpr int velocityHeight = 70;
    constexpr int scrollBar = 12;

    const int quanta[] = { 192, 96, 48, 24, 64, 32 };
    const char* const quantumNames[] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    constexpr int quantumCount = 6;

    const char* const noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // as Cubase named them: middle C, 60, is C3
    juce::String noteName(int pitch) { return juce::String(noteNames[pitch % 12]) + juce::String(pitch / 12 - 2); }

    bool isBlack(int pitch) { const int k = pitch % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }

    int drumVoices(int machine) { return machine == Rack::M808 ? Mz808::voices : Mz909::voices; }

    const char* drumName(int machine, int voice)
    {
        return machine == Rack::M808 ? tr808::voiceInfo(voice).name : tr909::voiceInfo(voice).name;
    }

    int drumNote(int machine, int voice)
    {
        return machine == Rack::M808 ? tr808::voiceInfo(voice).note : tr909::voiceInfo(voice).note;
    }

    int drumVoice(int machine, int note)
    {
        return machine == Rack::M808 ? tr808::voiceForNote(note) : tr909::voiceForNote(note);
    }
}

PartEditor::PartEditor(State& s, PartRef r) : state(s), ref(r)
{
    const auto& track = state.song().tracks[(size_t) ref.first];
    machine = track.machine;
    drums = machineKind(machine) == MachineKind::Drums;
    colour = trackColour(track.colour);

    back.setShowsLed(false);
    back.setTooltip("Back to the Arrange window (Escape).");
    back.onPress = [this] { if (onClose) onClose(); };

    const char* const toolNames[] = { "ARROW", drums ? "STICK" : "PENCIL", "ERASER" };
    const char* const toolTips[] = {
        "ARROW: pick notes (shift for more), drag to move, drag a note's end to stretch it, drag across "
        "empty space to pick by a box.",
        drums ? "STICK: click a step to strike it; click a hit to take it away; Alt-click to accent it."
              : "PENCIL: click to draw a note a Q long, or drag to draw it longer. On a note, as the arrow.",
        "ERASER: click a note away.",
    };
    for (int t = 0; t < 3; ++t)
    {
        tools[t].setText(toolNames[t]);
        tools[t].setLedColour(colours::stepOrange);
        tools[t].setTooltip(toolTips[t]);
        tools[t].onPress = [this, t] { setTool((EditTool) t); };
        addAndMakeVisible(tools[t]);
    }

    qPad.setShowsLed(false);
    qPad.setTooltip("Q: the grid notes are drawn on and quantized to.");
    qPad.onPress = [this] { q = (q + 1) % quantumCount; update(); };
    quantizeButton.setShowsLed(false);
    quantizeButton.setTooltip("Move the picked notes - or all of them, with none picked - on to the Q grid (Q key).");
    quantizeButton.onPress = [this] { quantize(); };
    zoomOut.setShowsLed(false);
    zoomIn.setShowsLed(false);
    zoomOut.onPress = [this] { pxPerTick = juce::jmax(0.02, pxPerTick / 1.5); update(); };
    zoomIn.onPress = [this] { pxPerTick = juce::jmin(8.0, pxPerTick * 1.5); update(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &back, &qPad, &quantizeButton, &zoomOut, &zoomIn })
        addAndMakeVisible(c);

    for (auto* b : { &hBar, &vBar })
    {
        b->setColour(juce::ScrollBar::thumbColourId, colours::screw.withAlpha(0.7f));
        b->setAutoHide(false);
        b->addListener(this);
        addAndMakeVisible(b);
    }

    setWantsKeyboardFocus(false);
    load();

    // open on the notes: in KEY EDIT, the middle of the ones there are, or C3
    if (! drums)
    {
        int lo = 127, hi = 0;
        for (const auto& n : notes) { lo = juce::jmin(lo, n.pitch); hi = juce::jmax(hi, n.pitch); }
        const int middle = notes.empty() ? 60 : (lo + hi) / 2;
        scrollY = juce::jmax(0, (127 - middle) * rowHeight() - 180);
    }
    setTool(tool);
}

PartEditor::~PartEditor()
{
    hBar.removeListener(this);
    vBar.removeListener(this);
}

bool PartEditor::stillValid() const
{
    const auto& s = state.song();
    return ref.first >= 0 && ref.first < (int) s.tracks.size()
        && ref.second >= 0 && ref.second < (int) s.tracks[(size_t) ref.first].parts.size()
        && s.tracks[(size_t) ref.first].machine == machine;
}

//============================================================================== rows

int PartEditor::rows() const { return drums ? drumVoices(machine) : 128; }

int PartEditor::pitchOfRow(int row) const
{
    return drums ? drumNote(machine, row) : 127 - row;
}

int PartEditor::rowOfPitch(int pitch) const
{
    if (! drums) return 127 - juce::jlimit(0, 127, pitch);
    const int v = drumVoice(machine, pitch);
    return v >= 0 && v < drumVoices(machine) ? v : -1;
}

juce::String PartEditor::rowLabel(int row) const
{
    return drums ? juce::String(drumName(machine, row)) : noteName(pitchOfRow(row));
}

juce::Rectangle<int> PartEditor::grid() const
{
    auto r = getLocalBounds().withTrimmedTop(headerHeight);
    r.removeFromLeft(drums ? 150 : 76);
    r.removeFromRight(scrollBar);
    r.removeFromBottom(scrollBar + velocityHeight + 6);
    return r;
}

juce::Rectangle<int> PartEditor::velocityLane() const
{
    const auto g = grid();
    return { g.getX(), g.getBottom() + 6, g.getWidth(), velocityHeight };
}

int PartEditor::rowAt(float y) const
{
    const int row = (int) std::floor((y - (float) grid().getY() + (float) scrollY) / (float) rowHeight());
    return row >= 0 && row < rows() ? row : -1;
}

juce::Rectangle<float> PartEditor::noteBounds(const Note& n) const
{
    const int row = rowOfPitch(n.pitch);
    const float y = yOfRow(juce::jmax(0, row));
    if (drums)
    {
        const float d = (float) rowHeight() * 0.62f;
        return juce::Rectangle<float>(d, d).withCentre({ xOf(n.tick), y + (float) rowHeight() * 0.5f });
    }
    const float x0 = xOf(n.tick), x1 = xOf(n.tick + n.length);
    return { x0, y + 1.0f, juce::jmax(3.0f, x1 - x0), (float) rowHeight() - 2.0f };
}

int PartEditor::noteAt(juce::Point<float> p) const
{
    for (int i = (int) notes.size() - 1; i >= 0; --i)
        if (rowOfPitch(notes[(size_t) i].pitch) >= 0 && noteBounds(notes[(size_t) i]).expanded(drums ? 2.0f : 0.0f).contains(p))
            return i;
    return -1;
}

bool PartEditor::onRightEdge(juce::Point<float> p, int i) const
{
    if (drums || i < 0) return false;
    const auto b = noteBounds(notes[(size_t) i]);
    return p.x >= b.getRight() - 5.0f && b.getWidth() > 10.0f;
}

int PartEditor::quantum() const { return quanta[q]; }

int PartEditor::snap(double tick) const
{
    return juce::jlimit(0, juce::jmax(0, partLength - 1), (int) std::lround(tick / quantum()) * quantum());
}

//============================================================================== the part

void PartEditor::load()
{
    if (! stillValid()) return;
    const auto& part = state.song().tracks[(size_t) ref.first].parts[(size_t) ref.second];

    // the picked notes, found again after the song has sorted them
    std::vector<Note> wasPicked;
    for (int i : picked) if (i < (int) notes.size()) wasPicked.push_back(notes[(size_t) i]);

    notes = part.notes;
    partLength = part.length;
    colour = trackColour(state.song().tracks[(size_t) ref.first].colour);
    seenVersion = state.doc().version();

    picked.clear();
    for (const auto& w : wasPicked)
        for (int i = 0; i < (int) notes.size(); ++i)
            if (notes[(size_t) i] == w && picked.count(i) == 0) { picked.insert(i); break; }

    update();
}

void PartEditor::commit()
{
    if (! stillValid()) return;
    const auto r = ref;
    const auto edited = notes;
    state.doc().edit([r, edited] (Song& s)
    {
        auto& part = s.tracks[(size_t) r.first].parts[(size_t) r.second];
        part.notes = edited;
        part.sortNotes();
    });
    load();
    state.repaintAll();
}

void PartEditor::quantize()
{
    const bool all = picked.empty();
    for (int i = 0; i < (int) notes.size(); ++i)
        if (all || picked.count(i)) notes[(size_t) i].tick = snap(notes[(size_t) i].tick);
    commit();
}

void PartEditor::deleteSelected()
{
    if (picked.empty()) return;
    std::vector<Note> kept;
    for (int i = 0; i < (int) notes.size(); ++i)
        if (picked.count(i) == 0) kept.push_back(notes[(size_t) i]);
    notes = kept;
    picked.clear();
    commit();
}

void PartEditor::setTool(EditTool t)
{
    tool = t;
    for (int i = 0; i < 3; ++i) tools[i].setLit((int) tool == i);
}

void PartEditor::update()
{
    const auto g = grid();
    if (g.getWidth() > 0)
    {
        const double length = juce::jmax((double) partLength, scrollTick + g.getWidth() / pxPerTick);
        hBar.setRangeLimits(0.0, length, juce::dontSendNotification);
        hBar.setCurrentRange(scrollTick, g.getWidth() / pxPerTick, juce::dontSendNotification);

        const int total = rows() * rowHeight();
        scrollY = juce::jlimit(0, juce::jmax(0, total - g.getHeight()), scrollY);
        vBar.setRangeLimits(0.0, juce::jmax((double) total, (double) g.getHeight()), juce::dontSendNotification);
        vBar.setCurrentRange(scrollY, g.getHeight(), juce::dontSendNotification);
    }
    qPad.setText(juce::String("Q ") + quantumNames[q]);
    repaint();
}

void PartEditor::scrollBarMoved(juce::ScrollBar* b, double start)
{
    if (b == &hBar) scrollTick = start;
    else            scrollY = (int) start;
    repaint();
}

void PartEditor::tick()
{
    if (drag == Drag::None && state.doc().version() != seenVersion) load();
    if (state.songPlaying()) repaint();
}

bool PartEditor::handleKey(const juce::KeyPress& k)
{
    const int code = k.getKeyCode();
    const bool ctrl = k.getModifiers().isCtrlDown() || k.getModifiers().isCommandDown();

    if (code == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { deleteSelected(); return true; }
    if (! ctrl && (code == 'Q' || code == 'q')) { quantize(); return true; }
    if (ctrl && (code == 'A' || code == 'a'))
    {
        picked.clear();
        for (int i = 0; i < (int) notes.size(); ++i) picked.insert(i);
        repaint();
        return true;
    }
    return false;
}

//============================================================================== painting

void PartEditor::paint(juce::Graphics& g)
{
    g.fillAll(colours::rail);

    // the header
    header = getLocalBounds().removeFromTop(headerHeight);
    drawFace(g, header.toFloat().reduced(0.0f, 2.0f), colours::mixer);
    if (stillValid())
    {
        const auto& track = state.song().tracks[(size_t) ref.first];
        const auto& part = track.parts[(size_t) ref.second];
        g.setColour(colours::light);
        g.setFont(fonts::bold(13.0f));
        g.drawText(juce::String(drums ? "DRUM EDIT  " : "KEY EDIT  ") + juce::String(track.name)
                       + (part.name.empty() ? juce::String() : "  /  " + juce::String(part.name)),
                   back.getRight() + 14, 0, 320, headerHeight, juce::Justification::centredLeft);
        g.setColour(colours::light.withAlpha(0.6f));
        g.setFont(fonts::bold(10.0f));
        g.drawText("FROM " + position(part.start).substring(0, 7) + "  TO " + position(part.end()).substring(0, 7),
                   back.getRight() + 14, headerHeight - 14, 320, 12, juce::Justification::centredLeft);
    }

    const auto gr = grid();
    const auto labels = juce::Rectangle<int>(0, gr.getY(), gr.getX(), gr.getHeight());

    // the rows: the keys, or the instruments
    g.saveState();
    g.reduceClipRegion(labels.getUnion(gr));
    for (int row = 0; row < rows(); ++row)
    {
        const float y = yOfRow(row);
        if (y > (float) gr.getBottom() || y + (float) rowHeight() < (float) gr.getY()) continue;
        const auto rr = juce::Rectangle<float>(0.0f, y, (float) gr.getRight(), (float) rowHeight());
        const int pitch = pitchOfRow(row);

        if (drums)
        {
            g.setColour(row % 2 == 0 ? juce::Colour(0xff20242a) : juce::Colour(0xff1c2025));
            g.fillRect(rr);
        }
        else
        {
            g.setColour(isBlack(pitch) ? juce::Colour(0xff191c21) : juce::Colour(0xff21252b));
            g.fillRect(rr);
            if (pitch % 12 == 0)
            {
                g.setColour(juce::Colours::black.withAlpha(0.6f));
                g.drawHorizontalLine(juce::roundToInt(y + (float) rowHeight()) - 1, 0.0f, (float) gr.getRight());
            }
        }
    }

    // the label column: piano keys, or the drum map's names
    for (int row = 0; row < rows(); ++row)
    {
        const float y = yOfRow(row);
        if (y > (float) gr.getBottom() || y + (float) rowHeight() < (float) gr.getY()) continue;
        const auto key = juce::Rectangle<float>(0.0f, y, (float) labels.getWidth() - 2.0f, (float) rowHeight());
        const int pitch = pitchOfRow(row);

        if (drums)
        {
            g.setColour(juce::Colour(0xff2a2f37));
            g.fillRect(key.reduced(0.0f, 0.5f));
            g.setColour(colours::light.withAlpha(0.85f));
            g.setFont(fonts::bold(10.0f));
            g.drawText(rowLabel(row), key.reduced(8.0f, 0.0f), juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(isBlack(pitch) ? juce::Colour(0xff111111) : juce::Colour(0xffe9e9e4));
            g.fillRect(isBlack(pitch) ? key.withWidth(key.getWidth() * 0.62f) : key);
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.drawHorizontalLine(juce::roundToInt(y + (float) rowHeight()) - 1, 0.0f, key.getRight());
            if (pitch % 12 == 0)
            {
                g.setColour(colours::ink);
                g.setFont(fonts::bold(9.0f));
                g.drawText(noteName(pitch), key.reduced(4.0f, 0.0f), juce::Justification::centredRight);
            }
        }
    }

    // the 950: the samples its programme plays, named on their keys
    if (! drums && machine == Rack::M950)
    {
        g.setFont(fonts::bold(8.5f));
        for (const auto& span : state.processor.rack.mz950.keySpans())
        {
            const float y = yOfRow(rowOfPitch(span.high));
            const float h = (float) ((span.high - span.low + 1) * rowHeight());
            g.setColour(colour.withAlpha(0.18f));
            g.fillRect(juce::Rectangle<float>((float) gr.getX(), y, (float) gr.getWidth(), h));
            g.setColour(colour.brighter(0.3f));
            g.drawText(juce::String(span.sample), juce::Rectangle<float>((float) gr.getX() + 4.0f, y, 160.0f, juce::jmin(h, 12.0f)),
                       juce::Justification::centredLeft);
        }
    }
    g.restoreState();

    // the grid: the beats and Q lines, and beyond the end of the part shaded
    g.saveState();
    g.reduceClipRegion(gr);
    for (int t = (int) std::floor(scrollTick / quantum()) * quantum(); ; t += quantum())
    {
        const float x = xOf(t);
        if (x > (float) gr.getRight()) break;
        const bool bar = t % ticksPerBar == 0, beat = t % ticksPerQuarter == 0;
        g.setColour(juce::Colours::black.withAlpha(bar ? 0.75f : beat ? 0.45f : 0.2f));
        g.drawVerticalLine(juce::roundToInt(x), (float) gr.getY(), (float) gr.getBottom());
    }
    const float end = xOf(partLength);
    if (end < (float) gr.getRight())
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(juce::Rectangle<float>(end, (float) gr.getY(), (float) gr.getRight() - end, (float) gr.getHeight()));
    }

    // a mono line's slides: a note overlapping the next is drawn joined to it
    if (machineKind(machine) == MachineKind::Mono)
    {
        g.setColour(colour.brighter(0.4f));
        for (size_t i = 0; i + 1 < notes.size(); ++i)
        {
            const auto& a = notes[i];
            for (size_t j = i + 1; j < notes.size(); ++j)
            {
                const auto& b = notes[j];
                if (b.tick <= a.tick) continue;
                if (b.tick < a.tick + a.length)
                {
                    const auto ra = noteBounds(a), rb = noteBounds(b);
                    g.drawLine(rb.getX(), ra.getCentreY(), rb.getX() + 6.0f, rb.getCentreY(), 2.0f);
                }
                break;
            }
        }
    }

    // the notes
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[(size_t) i];
        if (rowOfPitch(n.pitch) < 0) continue;
        const auto r = noteBounds(n);
        if (r.getRight() < (float) gr.getX() || r.getX() > (float) gr.getRight()) continue;
        const bool sel = picked.count(i) > 0;
        const float strength = 0.45f + 0.55f * (float) n.velocity / 127.0f;
        const auto c = n.tick >= partLength ? colours::screw : colour.withMultipliedBrightness(strength);

        if (drums)
        {
            juce::Path d;
            d.addQuadrilateral(r.getCentreX(), r.getY(), r.getRight(), r.getCentreY(), r.getCentreX(), r.getBottom(), r.getX(), r.getCentreY());
            g.setColour(c);
            g.fillPath(d);
            g.setColour(sel ? colours::light : juce::Colours::black.withAlpha(0.7f));
            g.strokePath(d, juce::PathStrokeType(sel ? 2.0f : 1.0f));
        }
        else
        {
            g.setColour(c);
            g.fillRect(r);
            g.setColour(sel ? colours::light : juce::Colours::black.withAlpha(0.7f));
            g.drawRect(r, sel ? 2.0f : 1.0f);
        }
    }

    if (drag == Drag::Box)
    {
        g.setColour(colours::light.withAlpha(0.12f));
        g.fillRect(box);
        g.setColour(colours::light.withAlpha(0.6f));
        g.drawRect(box, 1.0f);
    }

    // where the song is playing, if in this part
    if (state.songPlaying() && stillValid())
    {
        const double at = state.now() - state.song().tracks[(size_t) ref.first].parts[(size_t) ref.second].start;
        if (at >= 0.0 && at < partLength)
        {
            g.setColour(colours::ledRed);
            g.fillRect(xOf(at) - 0.5f, (float) gr.getY(), 1.5f, (float) gr.getHeight());
        }
    }
    g.restoreState();

    // the velocity lane
    const auto vl = velocityLane();
    g.setColour(juce::Colour(0xff15181c));
    g.fillRect(vl);
    g.setColour(colours::light.withAlpha(0.5f));
    g.setFont(fonts::bold(9.0f));
    g.drawText("VELOCITY", 0, vl.getY(), vl.getX() - 6, 14, juce::Justification::centredRight);
    g.saveState();
    g.reduceClipRegion(vl);
    for (int i = 0; i < (int) notes.size(); ++i)
    {
        const auto& n = notes[(size_t) i];
        const float x = xOf(n.tick);
        const float h = (float) (vl.getHeight() - 4) * (float) n.velocity / 127.0f;
        g.setColour(picked.count(i) ? colours::light : colour.withMultipliedBrightness(0.5f + 0.5f * (float) n.velocity / 127.0f));
        g.fillRect(x, (float) vl.getBottom() - h, 3.0f, h);
    }
    g.restoreState();

    g.setColour(juce::Colours::black);
    g.drawRect(gr);
    g.drawRect(vl);
}

void PartEditor::resized()
{
    auto h = getLocalBounds().removeFromTop(headerHeight).reduced(8, 6);
    back.setBounds(h.removeFromLeft(100));

    zoomIn.setBounds(h.removeFromRight(30));
    h.removeFromRight(4);
    zoomOut.setBounds(h.removeFromRight(30));
    h.removeFromRight(16);
    quantizeButton.setBounds(h.removeFromRight(90));
    h.removeFromRight(4);
    qPad.setBounds(h.removeFromRight(70));
    h.removeFromRight(16);
    for (int t = 2; t >= 0; --t)
    {
        tools[t].setBounds(h.removeFromRight(80));
        h.removeFromRight(4);
    }

    const auto gr = grid();
    vBar.setBounds(gr.getRight(), gr.getY(), scrollBar, gr.getHeight());
    const auto vl = velocityLane();
    hBar.setBounds(gr.getX(), vl.getBottom(), gr.getWidth(), scrollBar);

    // the whole part across the window, to start with
    if (gr.getWidth() > 0 && seenVersion >= 0 && scrollTick == 0.0)
        pxPerTick = juce::jlimit(0.02, 8.0, ((double) gr.getWidth() - 2.0 * inset) / juce::jmax(ticksPerBar, partLength));
    update();
}

//============================================================================== the mouse

void PartEditor::mouseMove(const juce::MouseEvent& e)
{
    const int i = noteAt(e.position);
    if (! grid().contains(e.getPosition())) setMouseCursor(juce::MouseCursor::NormalCursor);
    else if (tool == EditTool::Eraser)      setMouseCursor(i >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    else if (i >= 0)                        setMouseCursor(onRightEdge(e.position, i) ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    else                                    setMouseCursor(tool == EditTool::Pencil ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
}

void PartEditor::mouseDown(const juce::MouseEvent& e)
{
    downAt = e.position;
    lastX = e.position.x;
    before = notes;
    drag = Drag::None;
    dragNote = -1;

    // the velocity lane: drawn across
    if (velocityLane().contains(e.getPosition()))
    {
        drag = Drag::Velocity;
        mouseDrag(e);
        return;
    }
    if (! grid().contains(e.getPosition())) return;

    const int i = noteAt(e.position);
    const int row = rowAt(e.position.y);

    if (tool == EditTool::Eraser)
    {
        if (i >= 0)
        {
            notes.erase(notes.begin() + i);
            picked.clear();
            commit();
        }
        return;
    }

    // DRUM EDIT's stick: strike a step, take a hit away, or accent it
    if (drums && tool == EditTool::Pencil)
    {
        if (i >= 0)
        {
            if (e.mods.isAltDown()) notes[(size_t) i].velocity = notes[(size_t) i].velocity >= 110 ? 100 : 127;
            else                    notes.erase(notes.begin() + i);
        }
        else if (row >= 0)
        {
            Note n;
            n.tick = juce::jlimit(0, partLength - 1, (int) std::floor(tickAt(e.position.x) / quantum()) * quantum());
            n.length = ticksPerStep / 2;
            n.pitch = pitchOfRow(row);
            n.velocity = e.mods.isAltDown() ? 127 : 100;
            notes.push_back(n);
        }
        picked.clear();
        commit();
        return;
    }

    if (i >= 0)
    {
        // pick it, and drag it - or its end
        if (e.mods.isShiftDown()) { if (! picked.erase(i)) picked.insert(i); repaint(); return; }
        if (picked.count(i) == 0) picked = { i };
        dragNote = i;
        drag = onRightEdge(e.position, i) ? Drag::Resize : Drag::Move;
    }
    else if (tool == EditTool::Pencil && row >= 0)
    {
        // a new note, a Q long - longer as it is dragged
        Note n;
        n.tick = juce::jlimit(0, partLength - 1, (int) std::floor(tickAt(e.position.x) / quantum()) * quantum());
        n.length = quantum();
        n.pitch = pitchOfRow(row);
        n.velocity = 100;
        notes.push_back(n);
        dragNote = (int) notes.size() - 1;
        picked = { dragNote };
        before = notes;
        drag = Drag::Draw;
    }
    else
    {
        if (! e.mods.isShiftDown()) picked.clear();
        drag = Drag::Box;
        box = {};
    }
    repaint();
}

void PartEditor::mouseDrag(const juce::MouseEvent& e)
{
    switch (drag)
    {
        case Drag::Velocity:
        {
            // every note whose start the mouse passes over takes the velocity at its height
            const auto vl = velocityLane();
            const int v = juce::jlimit(1, 127, (int) std::lround((vl.getBottom() - e.position.y) / (vl.getHeight() - 4) * 127.0f));
            const float x0 = juce::jmin(lastX, e.position.x) - 3.0f;
            const float x1 = juce::jmax(lastX, e.position.x) + 3.0f;
            for (int i = 0; i < (int) notes.size(); ++i)
            {
                const float x = xOf(notes[(size_t) i].tick);
                if (x >= x0 && x <= x1 && (picked.empty() || picked.count(i))) notes[(size_t) i].velocity = v;
            }
            lastX = e.position.x;
            break;
        }

        case Drag::Move:
        {
            if (dragNote < 0) break;
            const Note& anchor = before[(size_t) dragNote];
            const int dt = snap(anchor.tick + (e.position.x - downAt.x) / pxPerTick) - anchor.tick;
            const int dr = (int) std::floor((e.position.y - (float) grid().getY() + (float) scrollY) / (float) rowHeight())
                         - (int) std::floor((downAt.y - (float) grid().getY() + (float) scrollY) / (float) rowHeight());
            for (int i : picked)
            {
                Note n = before[(size_t) i];
                n.tick = juce::jlimit(0, partLength - 1, n.tick + dt);
                const int row = juce::jlimit(0, rows() - 1, rowOfPitch(n.pitch) + dr);
                n.pitch = pitchOfRow(row);
                notes[(size_t) i] = n;
            }
            break;
        }

        case Drag::Resize:
        case Drag::Draw:
        {
            if (dragNote < 0) break;
            const Note& anchor = before[(size_t) dragNote];
            const int end = (int) std::lround((anchor.tick + anchor.length + (e.position.x - downAt.x) / pxPerTick) / quantum()) * quantum();
            const int dl = juce::jmax(quantum() / 4, end - anchor.tick) - anchor.length;
            for (int i : picked)
                notes[(size_t) i].length = juce::jmax(1, before[(size_t) i].length + dl);
            break;
        }

        case Drag::Box:
            box = juce::Rectangle<float>(downAt, e.position);
            break;

        default: break;
    }
    repaint();
}

void PartEditor::mouseUp(const juce::MouseEvent&)
{
    if (drag == Drag::Box)
    {
        for (int i = 0; i < (int) notes.size(); ++i)
            if (rowOfPitch(notes[(size_t) i].pitch) >= 0 && noteBounds(notes[(size_t) i]).intersects(box)) picked.insert(i);
        box = {};
        drag = Drag::None;
        repaint();
        return;
    }

    const bool changed = notes != before;
    drag = Drag::None;
    if (changed) commit();
    else         repaint();
}

void PartEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        const double at = tickAt(e.position.x);
        pxPerTick = juce::jlimit(0.02, 8.0, pxPerTick * std::pow(1.25, w.deltaY * 4.0));
        scrollTick = juce::jmax(0.0, at - (e.position.x - (float) grid().getX() - inset) / pxPerTick);
    }
    else if (e.mods.isShiftDown())
    {
        scrollTick = juce::jmax(0.0, scrollTick - w.deltaY * 200.0 / pxPerTick);
    }
    else
    {
        scrollY -= (int) (w.deltaY * 160.0f);
    }
    update();
}

} // namespace arrange
