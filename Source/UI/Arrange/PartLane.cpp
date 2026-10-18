// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "PartLane.h"
#include "Ruler.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace debirth;

namespace arrange
{

std::vector<std::pair<int, Part>> PartLane::clipboard;

namespace
{
    // A part placed on a track, kept in order of start; returns where it went.
    void place(std::vector<Part>& parts, Part p)
    {
        const auto at = std::upper_bound(parts.begin(), parts.end(), p.start,
                                         [] (int start, const Part& q) { return start < q.start; });
        parts.insert(at, std::move(p));
    }

    // Where a part is now, after an edit has moved things about: the one at that start, of
    // that length and name.
    int find(const std::vector<Part>& parts, const Part& p)
    {
        for (int i = 0; i < (int) parts.size(); ++i)
            if (parts[(size_t) i].start == p.start && parts[(size_t) i].length == p.length
                && parts[(size_t) i].name == p.name && parts[(size_t) i].notes.size() == p.notes.size())
                return i;
        return -1;
    }

    bool valid(const Song& s, PartRef r)
    {
        return r.first >= 0 && r.first < (int) s.tracks.size()
            && r.second >= 0 && r.second < (int) s.tracks[(size_t) r.first].parts.size();
    }

    const Part& partOf(const Song& s, PartRef r) { return s.tracks[(size_t) r.first].parts[(size_t) r.second]; }

    juce::String patternName(int pattern)
    {
        return juce::String::charToString((juce::juce_wchar) ('A' + pattern / patternsPerBank)) + juce::String(pattern % patternsPerBank + 1);
    }
}

PartLane::PartLane(State& s) : state(s) {}

//============================================================================== geometry

int PartLane::rowAt(float y) const
{
    const int row = (int) std::floor((y + (float) state.scrollY) / (float) trackHeight);
    return row >= 0 && row < (int) state.song().tracks.size() ? row : -1;
}

juce::Rectangle<float> PartLane::partBounds(int track, const Part& p) const
{
    const float x0 = (float) state.xOf(p.start), x1 = (float) state.xOf(p.end());
    return { x0, rowY(track) + 3.0f, juce::jmax(3.0f, x1 - x0), (float) trackHeight - 6.0f };
}

bool PartLane::partAt(juce::Point<float> pos, PartRef& ref) const
{
    const int row = rowAt(pos.y);
    if (row < 0) return false;
    const auto& parts = state.song().tracks[(size_t) row].parts;
    for (int i = (int) parts.size() - 1; i >= 0; --i)   // the last drawn, on top
        if (partBounds(row, parts[(size_t) i]).contains(pos))
        {
            ref = { row, i };
            return true;
        }
    return false;
}

bool PartLane::onRightEdge(juce::Point<float> pos, PartRef ref) const
{
    if (! valid(state.song(), ref)) return false;
    const auto b = partBounds(ref.first, partOf(state.song(), ref));
    return pos.x >= b.getRight() - 6.0f && b.getWidth() > 12.0f;
}

int PartLane::dragTicks(const juce::MouseEvent& e) const
{
    if (! valid(state.song(), anchor)) return 0;
    const auto& song = state.song();
    const int from = partOf(song, anchor).start;
    int to = state.snapped(from + (e.position.x - downAt.x) / state.pxPerTick);

    // nothing picked goes before the top of the song
    int earliest = from;
    for (const auto& r : state.selection)
        if (valid(song, r)) earliest = juce::jmin(earliest, partOf(song, r).start);
    to = juce::jmax(to, from - earliest);
    return to - from;
}

int PartLane::dragTracks(const juce::MouseEvent& e) const
{
    return (int) std::floor((e.position.y + (float) state.scrollY) / (float) trackHeight)
         - (int) std::floor((downAt.y + (float) state.scrollY) / (float) trackHeight);
}

int PartLane::targetTrack(int from, int by) const
{
    const auto& tracks = state.song().tracks;
    const int to = juce::jlimit(0, (int) tracks.size() - 1, from + by);
    return machineKind(tracks[(size_t) to].machine) == machineKind(tracks[(size_t) from].machine) ? to : from;
}

//============================================================================== painting

void PartLane::paintPart(juce::Graphics& g, juce::Rectangle<float> r, const Part& p, juce::Colour colour,
                         bool selected, int machine) const
{
    const auto fill = p.muted ? colour.withMultipliedSaturation(0.2f).withAlpha(0.35f) : colour.withAlpha(0.8f);
    g.setColour(fill);
    g.fillRoundedRectangle(r, 3.0f);

    // a miniature of the notes, over the range of pitches the part uses
    if (! p.notes.empty() && r.getWidth() > 6.0f)
    {
        int lo = 127, hi = 0;
        for (const auto& n : p.notes) { lo = juce::jmin(lo, n.pitch); hi = juce::jmax(hi, n.pitch); }
        const auto body = r.reduced(2.0f, 3.0f).withTrimmedTop(11.0f);
        const bool drums = machineKind(machine) == MachineKind::Drums;

        g.setColour(juce::Colours::black.withAlpha(p.muted ? 0.3f : 0.65f));
        for (const auto& n : p.notes)
        {
            if (n.tick >= p.length) continue;
            const float y = hi == lo ? body.getCentreY()
                                     : body.getBottom() - (float) (n.pitch - lo) / (float) (hi - lo) * (body.getHeight() - 2.0f) - 1.0f;
            const float x0 = r.getX() + (float) (n.tick * state.pxPerTick);
            const float x1 = drums ? x0 + 1.5f : juce::jmin(r.getRight(), r.getX() + (float) ((n.tick + n.length) * state.pxPerTick));
            g.fillRect(x0, y - 1.0f, juce::jmax(1.5f, x1 - x0), 2.0f);
        }
    }

    if (p.name.size() > 0 && r.getWidth() > 18.0f)
    {
        g.setColour(colours::ink.withAlpha(p.muted ? 0.5f : 0.9f));
        g.setFont(fonts::bold(9.5f));
        g.drawText(juce::String(p.name), r.reduced(4.0f, 1.0f).withHeight(12.0f), juce::Justification::centredLeft);
    }

    g.setColour(selected ? colours::light : juce::Colours::black.withAlpha(0.6f));
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, selected ? 2.0f : 1.0f);
}

void PartLane::paint(juce::Graphics& g)
{
    const auto& song = state.song();
    const auto& tracks = song.tracks;

    g.fillAll(juce::Colour(0xff181b20));

    // the lanes
    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        const float y = rowY(i);
        if (y > (float) getHeight() || y + trackHeight < 0.0f) continue;
        g.setColour(i == state.track ? juce::Colour(0xff272c35) : (i % 2 == 0 ? juce::Colour(0xff1f2329) : juce::Colour(0xff1c1f25)));
        g.fillRect(0.0f, y, (float) getWidth(), (float) trackHeight);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawHorizontalLine(juce::roundToInt(y) + trackHeight - 1, 0.0f, (float) getWidth());
    }

    // the cycle, faintly, and the grid
    const float lx = (float) state.xOf(song.left), rx = (float) state.xOf(song.right);
    g.setColour(song.cycle ? colours::ledGreen.withAlpha(0.06f) : colours::light.withAlpha(0.025f));
    g.fillRect(lx, 0.0f, rx - lx, (float) getHeight());
    drawGrid(g, state, getLocalBounds(), true);

    // the parts
    for (int t = 0; t < (int) tracks.size(); ++t)
    {
        const float y = rowY(t);
        if (y > (float) getHeight() || y + trackHeight < 0.0f) continue;
        const auto colour = trackColour(tracks[(size_t) t].colour);
        const auto& parts = tracks[(size_t) t].parts;

        for (int i = 0; i < (int) parts.size(); ++i)
        {
            const auto r = partBounds(t, parts[(size_t) i]);
            if (r.getRight() < 0.0f || r.getX() > (float) getWidth()) continue;
            const bool picked = state.selection.count({ t, i }) > 0;
            const bool moving = (picked && drag == Drag::Move) || (drag == Drag::Resize && anchor == PartRef { t, i });
            paintPart(g, r, parts[(size_t) i], colour.withAlpha(moving ? 0.4f : 1.0f), picked, tracks[(size_t) t].machine);
        }
    }

    // what a drag would do
    const auto mouse = getMouseXYRelative().toFloat();
    if ((drag == Drag::Move || drag == Drag::Copy) && valid(song, anchor))
    {
        const int dt = state.snapped(partOf(song, anchor).start + (mouse.x - downAt.x) / state.pxPerTick) - partOf(song, anchor).start;
        const int dr = (int) std::floor((mouse.y + (float) state.scrollY) / (float) trackHeight)
                     - (int) std::floor((downAt.y + (float) state.scrollY) / (float) trackHeight);
        for (const auto& r : state.selection)
        {
            if (! valid(song, r)) continue;
            Part ghost = partOf(song, r);
            ghost.start = juce::jmax(0, ghost.start + dt);
            const int to = targetTrack(r.first, dr);
            paintPart(g, partBounds(to, ghost), ghost, trackColour(tracks[(size_t) r.first].colour).withAlpha(0.75f), true,
                      tracks[(size_t) r.first].machine);
        }
    }
    else if (drag == Drag::Resize && valid(song, anchor))
    {
        // the part as it will be, its loop filling out the length dragged to
        Part ghost = partOf(song, anchor);
        resizePart(ghost, resizeTo - ghost.start);
        paintPart(g, partBounds(anchor.first, ghost), ghost, trackColour(tracks[(size_t) anchor.first].colour).withAlpha(0.75f),
                  true, tracks[(size_t) anchor.first].machine);
        g.setColour(colours::light);
        g.drawRoundedRectangle(partBounds(anchor.first, ghost), 3.0f, 1.5f);
    }
    else if (drag == Drag::Draw && drawTrack >= 0)
    {
        const float x0 = (float) state.xOf(drawFrom), x1 = (float) state.xOf(drawTo);
        g.setColour(trackColour(tracks[(size_t) drawTrack].colour).withAlpha(0.5f));
        g.fillRoundedRectangle(x0, rowY(drawTrack) + 3.0f, x1 - x0, (float) trackHeight - 6.0f, 3.0f);
    }
    else if (drag == Drag::Box)
    {
        g.setColour(colours::light.withAlpha(0.12f));
        g.fillRect(box);
        g.setColour(colours::light.withAlpha(0.6f));
        g.drawRect(box, 1.0f);
    }

    // the cursor, and where the song is playing
    const float cx = (float) state.xOf(song.cursor);
    g.setColour(colours::light.withAlpha(0.5f));
    g.drawVerticalLine(juce::roundToInt(cx), 0.0f, (float) getHeight());
    if (state.songPlaying())
    {
        g.setColour(colours::ledRed);
        g.fillRect((float) state.xOf(state.now()) - 0.5f, 0.0f, 1.5f, (float) getHeight());
    }
}

//============================================================================== the mouse

void PartLane::mouseMove(const juce::MouseEvent& e)
{
    PartRef ref;
    const bool over = partAt(e.position, ref);

    switch (state.tool)
    {
        case Tool::Pencil:   setMouseCursor(juce::MouseCursor::CrosshairCursor); break;
        case Tool::Eraser:
        case Tool::Glue:
        case Tool::Mute:     setMouseCursor(over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); break;
        case Tool::Scissors: setMouseCursor(over ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor); break;
        default:
            setMouseCursor(over && onRightEdge(e.position, ref) ? juce::MouseCursor::LeftRightResizeCursor
                         : over ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }
}

void PartLane::mouseDown(const juce::MouseEvent& e)
{
    downAt = e.position;
    drag = Drag::None;

    if (e.mods.isPopupMenu())
    {
        toolbox(e);
        return;
    }

    const int row = rowAt(e.position.y);
    if (row >= 0) state.track = row;

    PartRef ref;
    const bool over = partAt(e.position, ref);
    const auto& song = state.song();

    switch (state.tool)
    {
        case Tool::Arrow:
            if (! over)
            {
                if (! e.mods.isShiftDown()) state.selection.clear();
                drag = Drag::Box;
                box = {};
                break;
            }
            if (e.mods.isShiftDown())
            {
                if (! state.selection.erase(ref)) state.selection.insert(ref);
                break;
            }
            if (state.selection.count(ref) == 0) state.selection = { ref };
            anchor = ref;
            if (onRightEdge(e.position, ref))
            {
                drag = Drag::Resize;
                resizeTo = partOf(song, ref).end();
            }
            else
            {
                drag = e.mods.isAltDown() || e.mods.isCtrlDown() ? Drag::Copy : Drag::Move;
            }
            break;

        case Tool::Pencil:
            if (row < 0) break;
            drag = Drag::Draw;
            drawTrack = row;
            drawFrom = state.snappedDown(state.tickAt(e.position.x));
            drawTo = drawFrom + juce::jmax(state.grid(), ticksPerBar);
            break;

        case Tool::Eraser:
            if (over)
            {
                state.doc().edit([ref] (Song& s) { s.tracks[(size_t) ref.first].parts.erase(s.tracks[(size_t) ref.first].parts.begin() + ref.second); });
                state.selection.clear();
            }
            break;

        case Tool::Mute:
            if (over) state.doc().edit([ref] (Song& s) { auto& p = s.tracks[(size_t) ref.first].parts[(size_t) ref.second]; p.muted = ! p.muted; });
            break;

        case Tool::Scissors:
            if (over)
            {
                const Part& p = partOf(song, ref);
                const int cut = state.snapped(state.tickAt(e.position.x));
                if (cut > p.start && cut < p.end())
                {
                    state.doc().edit([ref, cut] (Song& s)
                    {
                        auto& parts = s.tracks[(size_t) ref.first].parts;
                        Part left = parts[(size_t) ref.second], right = left;
                        const int offset = cut - left.start;

                        left.length = offset;
                        left.notes.erase(std::remove_if(left.notes.begin(), left.notes.end(),
                                                        [offset] (const Note& n) { return n.tick >= offset; }), left.notes.end());

                        right.start = cut;
                        right.length -= offset;
                        right.notes.clear();
                        for (auto n : parts[(size_t) ref.second].notes)
                            if (n.tick >= offset) { n.tick -= offset; right.notes.push_back(n); }

                        parts.erase(parts.begin() + ref.second);
                        place(parts, left);
                        place(parts, right);
                    });
                    state.selection.clear();
                }
            }
            break;

        case Tool::Glue:
            if (over)
            {
                const auto& parts = song.tracks[(size_t) ref.first].parts;
                if (ref.second + 1 < (int) parts.size())
                {
                    state.doc().edit([ref] (Song& s)
                    {
                        auto& ps = s.tracks[(size_t) ref.first].parts;
                        Part& a = ps[(size_t) ref.second];
                        const Part b = ps[(size_t) ref.second + 1];
                        const int offset = b.start - a.start;
                        for (auto n : b.notes) { n.tick += offset; a.notes.push_back(n); }
                        a.length = juce::jmax(a.length, offset + b.length);
                        a.sortNotes();
                        ps.erase(ps.begin() + ref.second + 1);
                    });
                    state.selection = { ref };
                }
            }
            break;

        default: break;
    }

    state.repaintAll();
}

void PartLane::mouseDrag(const juce::MouseEvent& e)
{
    switch (drag)
    {
        case Drag::Resize:
            if (valid(state.song(), anchor))
            {
                const Part& p = partOf(state.song(), anchor);
                resizeTo = juce::jmax(p.start + juce::jmin(state.grid(), ticksPerBar),
                                      state.snapped(p.end() + (e.position.x - downAt.x) / state.pxPerTick));
            }
            break;

        case Drag::Draw:
        {
            const int a = state.snappedDown(state.tickAt(downAt.x));
            const int b = state.snapped(state.tickAt(e.position.x));
            drawFrom = juce::jmin(a, b);
            drawTo = juce::jmax(drawFrom + state.grid(), juce::jmax(a, b));
            break;
        }

        case Drag::Box:
            box = juce::Rectangle<float>(downAt, e.position);
            break;

        default: break;
    }
    repaint();
}

void PartLane::mouseUp(const juce::MouseEvent& e)
{
    const auto& song = state.song();
    const bool moved = e.getDistanceFromDragStart() > 3;

    if ((drag == Drag::Move || drag == Drag::Copy) && moved && valid(song, anchor))
    {
        const int dt = dragTicks(e), dr = dragTracks(e);
        if (dt != 0 || dr != 0 || drag == Drag::Copy)
        {
            const bool copy = drag == Drag::Copy;
            const auto picked = state.selection;
            std::vector<std::pair<int, Part>> placed;

            state.doc().edit([&] (Song& s)
            {
                std::vector<std::pair<int, Part>> going;
                for (const auto& r : picked)
                    if (valid(s, r))
                    {
                        Part p = partOf(s, r);
                        p.start += dt;
                        going.emplace_back(targetTrack(r.first, dr), p);
                    }

                // the originals out first, from the back, so the indices hold
                if (! copy)
                    for (auto it = picked.rbegin(); it != picked.rend(); ++it)
                        if (valid(s, *it)) s.tracks[(size_t) it->first].parts.erase(s.tracks[(size_t) it->first].parts.begin() + it->second);

                for (auto& [t, p] : going) place(s.tracks[(size_t) t].parts, p);
                placed = going;
            });

            state.selection.clear();
            for (const auto& [t, p] : placed)
                if (const int i = find(state.song().tracks[(size_t) t].parts, p); i >= 0) state.selection.insert({ t, i });
        }
    }
    else if (drag == Drag::Resize && valid(song, anchor))
    {
        const PartRef r = anchor;
        const int length = juce::jmax(1, resizeTo - partOf(song, r).start);
        if (length != partOf(song, r).length)
            state.doc().edit([r, length] (Song& s) { resizePart(s.tracks[(size_t) r.first].parts[(size_t) r.second], length); });
    }
    else if (drag == Drag::Draw && drawTrack >= 0 && drawTo > drawFrom)
    {
        const int t = drawTrack, from = drawFrom, to = drawTo;
        Part p;
        p.start = from;
        p.length = to - from;
        state.doc().edit([t, p] (Song& s) { place(s.tracks[(size_t) t].parts, p); });
        if (const int i = find(state.song().tracks[(size_t) t].parts, p); i >= 0) state.selection = { { t, i } };
    }
    else if (drag == Drag::Box && ! box.isEmpty())
    {
        for (int t = 0; t < (int) song.tracks.size(); ++t)
            for (int i = 0; i < (int) song.tracks[(size_t) t].parts.size(); ++i)
                if (partBounds(t, song.tracks[(size_t) t].parts[(size_t) i]).intersects(box))
                    state.selection.insert({ t, i });
    }

    drag = Drag::None;
    drawTrack = -1;
    box = {};
    state.repaintAll();
}

void PartLane::mouseDoubleClick(const juce::MouseEvent& e)
{
    PartRef ref;
    if (state.tool == Tool::Arrow && partAt(e.position, ref) && onOpenPart)
    {
        drag = Drag::None;
        onOpenPart(ref);
    }
}

void PartLane::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        // zoom about the mouse
        const double at = state.tickAt(e.position.x);
        state.pxPerTick = juce::jlimit(4.0 / ticksPerBar, 2000.0 / ticksPerBar, state.pxPerTick * std::pow(1.25, w.deltaY * 4.0));
        state.scrollTick = juce::jmax(0.0, at - e.position.x / state.pxPerTick);
    }
    else if (e.mods.isShiftDown() || std::abs(w.deltaX) > std::abs(w.deltaY))
    {
        const float d = std::abs(w.deltaX) > std::abs(w.deltaY) ? w.deltaX : w.deltaY;
        state.scrollTick = juce::jmax(0.0, state.scrollTick - d * 200.0 / state.pxPerTick);
    }
    else
    {
        const int total = (int) state.song().tracks.size() * trackHeight;
        state.scrollY = juce::jlimit(0, juce::jmax(0, total - getHeight() + trackHeight), state.scrollY - (int) (w.deltaY * 120.0f));
    }
    state.repaintAll();
}

//============================================================================== edits

void PartLane::deleteSelection()
{
    if (state.selection.empty()) return;
    const auto picked = state.selection;
    state.doc().edit([picked] (Song& s)
    {
        for (auto it = picked.rbegin(); it != picked.rend(); ++it)
            if (valid(s, *it)) s.tracks[(size_t) it->first].parts.erase(s.tracks[(size_t) it->first].parts.begin() + it->second);
    });
    state.selection.clear();
    state.repaintAll();
}

void PartLane::copySelection()
{
    clipboard.clear();
    for (const auto& r : state.selection)
        if (valid(state.song(), r)) clipboard.emplace_back(r.first, partOf(state.song(), r));
}

void PartLane::paste()
{
    // At the cursor, on the track picked - each part to a track as far from it as it was
    // from the first copied - keeping their spacing.
    if (clipboard.empty() || state.song().tracks.empty()) return;

    int earliest = clipboard.front().second.start, top = clipboard.front().first;
    for (const auto& [t, p] : clipboard) { earliest = juce::jmin(earliest, p.start); top = juce::jmin(top, t); }

    const int at = state.song().cursor;
    const int base = juce::jlimit(0, (int) state.song().tracks.size() - 1, state.track);
    std::vector<std::pair<int, Part>> going;
    for (const auto& [t, p] : clipboard)
    {
        const auto& tracks = state.song().tracks;
        const int to = juce::jlimit(0, (int) tracks.size() - 1, base + t - top);
        if (machineKind(tracks[(size_t) to].machine) != machineKind(tracks[(size_t) juce::jlimit(0, (int) tracks.size() - 1, t)].machine)) continue;
        Part q = p;
        q.start = at + p.start - earliest;
        going.emplace_back(to, q);
    }
    if (going.empty()) return;

    state.doc().edit([going] (Song& s) { for (const auto& [t, p] : going) place(s.tracks[(size_t) t].parts, p); });

    state.selection.clear();
    int end = at;
    for (const auto& [t, p] : going)
    {
        if (const int i = find(state.song().tracks[(size_t) t].parts, p); i >= 0) state.selection.insert({ t, i });
        end = juce::jmax(end, p.end());
    }
    state.doc().adjust([end] (Song& s) { s.cursor = end; });   // ready to paste again after
    state.repaintAll();
}

void PartLane::repeatSelection(int times)
{
    if (state.selection.empty() || times < 1) return;

    // the picked parts, as a block, copied on after themselves
    const auto& song = state.song();
    int from = std::numeric_limits<int>::max(), to = 0;
    for (const auto& r : state.selection)
        if (valid(song, r)) { from = juce::jmin(from, partOf(song, r).start); to = juce::jmax(to, partOf(song, r).end()); }
    if (to <= from) return;

    const auto picked = state.selection;
    const int span = to - from;
    state.doc().edit([picked, span, times] (Song& s)
    {
        std::vector<std::pair<int, Part>> going;
        for (const auto& r : picked)
            if (valid(s, r))
                for (int k = 1; k <= times; ++k)
                {
                    Part p = partOf(s, r);
                    p.start += span * k;
                    going.emplace_back(r.first, p);
                }
        for (auto& [t, p] : going) place(s.tracks[(size_t) t].parts, p);
    });
    state.selection.clear();
    state.repaintAll();
}

void PartLane::insertPattern(int track, int pattern, int at)
{
    const int machine = state.song().tracks[(size_t) track].machine;
    Part p = partFromPattern(state.processor.rack, machine, pattern, 1);
    p.start = at;
    state.doc().edit([track, p] (Song& s) { place(s.tracks[(size_t) track].parts, p); });
    if (const int i = find(state.song().tracks[(size_t) track].parts, p); i >= 0) state.selection = { { track, i } };
    state.repaintAll();
}

//============================================================================== the toolbox

void PartLane::toolbox(const juce::MouseEvent& e)
{
    PartRef ref;
    const bool over = partAt(e.position, ref);
    const int row = rowAt(e.position.y);
    const int at = state.snappedDown(state.tickAt(e.position.x));
    if (over && state.selection.count(ref) == 0) state.selection = { ref };
    if (row >= 0) state.track = row;
    state.repaintAll();

    juce::PopupMenu m;
    m.addSectionHeader("TOOLS");
    for (int t = 0; t < Tool::count; ++t) m.addItem(1 + t, toolName(t), true, state.tool == t);

    juce::PopupMenu snaps;
    for (int s = 0; s < Snap::count; ++s) snaps.addItem(20 + s, snapName(s), true, state.snap == s);
    m.addSubMenu("Snap", snaps);

    if (over)
    {
        const auto& p = partOf(state.song(), ref);
        m.addSeparator();
        m.addSectionHeader(p.name.empty() ? juce::String("PART") : "PART " + juce::String(p.name));
        m.addItem(30, "Edit", onOpenPart != nullptr);
        m.addItem(31, "Rename...");
        juce::PopupMenu repeat;
        for (int k : { 1, 2, 3, 4, 8, 16 }) repeat.addItem(40 + k, juce::String(k) + (k == 1 ? " time" : " times"));
        m.addSubMenu("Repeat", repeat);
        m.addItem(32, p.muted ? "Unmute" : "Mute");
        m.addItem(33, "Delete");
    }

    if (row >= 0)
    {
        // the rack's patterns for this track's machine, as parts
        const int machine = state.song().tracks[(size_t) row].machine;
        juce::PopupMenu fromRack;
        for (int b = 0; b < banks; ++b)
        {
            juce::PopupMenu bank;
            for (int i = 0; i < patternsPerBank; ++i)
            {
                const int pattern = b * patternsPerBank + i;
                const bool written = ! partFromPattern(state.processor.rack, machine, pattern, 1).notes.empty();
                bank.addItem(100 + pattern, patternName(pattern), written);
            }
            fromRack.addSubMenu(juce::String("Bank ") + juce::String::charToString((juce::juce_wchar) ('A' + b)), bank);
        }
        m.addSeparator();
        m.addSectionHeader(juce::String("TRACK ") + juce::String(state.song().tracks[(size_t) row].name).toUpperCase());
        m.addSubMenu(juce::String("Insert a pattern from the ") + machineName(machine) + " at " + position(at).substring(0, 7), fromRack);
        m.addItem(34, "Insert an empty bar here");
    }

    juce::Component::SafePointer<PartLane> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(),
                    [self, ref, row, at] (int id)
    {
        if (self == nullptr || id == 0) return;
        auto& st = self->state;

        if (id >= 1 && id <= Tool::count)   st.tool = id - 1;
        else if (id >= 20 && id < 20 + Snap::count) st.snap = id - 20;
        else if (id == 30 && self->onOpenPart) self->onOpenPart(ref);
        else if (id == 31)
        {
            auto* w = new juce::AlertWindow("Rename the part", {}, juce::MessageBoxIconType::NoIcon, self.getComponent());
            w->addTextEditor("name", juce::String(partOf(st.song(), ref).name));
            w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
            w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            w->enterModalState(true, juce::ModalCallbackFunction::create([self, w, ref] (int result)
            {
                if (result != 1 || self == nullptr) return;
                const auto name = w->getTextEditorContents("name").trim().toStdString();
                self->state.doc().edit([ref, name] (Song& s) { if (valid(s, ref)) s.tracks[(size_t) ref.first].parts[(size_t) ref.second].name = name; });
                self->state.repaintAll();
            }), true);
        }
        else if (id == 32) st.doc().edit([ref] (Song& s) { if (valid(s, ref)) { auto& p = s.tracks[(size_t) ref.first].parts[(size_t) ref.second]; p.muted = ! p.muted; } });
        else if (id == 33) self->deleteSelection();
        else if (id == 34 && row >= 0)
        {
            Part p;
            p.start = at;
            st.doc().edit([row, p] (Song& s) { place(s.tracks[(size_t) row].parts, p); });
        }
        else if (id > 40 && id <= 56) self->repeatSelection(id - 40);
        else if (id >= 100 && row >= 0) self->insertPattern(row, id - 100, at);

        st.repaintAll();
    });
}

} // namespace arrange
