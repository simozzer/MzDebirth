// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "TrackList.h"

using namespace debirth;

namespace arrange
{

TrackList::TrackList(State& s) : state(s)
{
    setTooltip("The tracks. Click to pick one; double-click its name to rename it; right-click for "
               "its machine, colour, and to add or delete tracks. M mutes, S solos, SH swings it with "
               "the SHUFFLE knob.");
}

void TrackList::setActivity(const std::array<bool, Rack::channels>& a)
{
    if (a != active) { active = a; repaint(); }
}

juce::Rectangle<int> TrackList::rowBounds(int row) const
{
    return { 0, rulerHeight + row * trackHeight - state.scrollY, getWidth(), trackHeight };
}

int TrackList::rowAt(int y) const
{
    if (y < rulerHeight) return -1;
    const int row = (y - rulerHeight + state.scrollY) / trackHeight;
    return row < (int) state.song().tracks.size() ? row : -1;
}

juce::Rectangle<int> TrackList::switchBounds(int row, int which) const
{
    const auto r = rowBounds(row);
    const int w = which == 2 ? 26 : 20;
    const int right = r.getRight() - 8 - (which == 0 ? 52 : which == 1 ? 28 : 0);
    return { right - w, r.getCentreY() - 10, w, 20 };
}

TrackList::Hit TrackList::hitAt(juce::Point<int> p, int& row) const
{
    row = rowAt(p.y);
    if (row < 0) return Hit::None;

    for (int w = 0; w < 3; ++w)
        if (switchBounds(row, w).contains(p))
            return w == 0 ? Hit::Mute : w == 1 ? Hit::Solo : Hit::Shuffle;

    const auto r = rowBounds(row);
    if (p.x < 12) return Hit::Colour;
    if (p.y > r.getY() + trackHeight / 2 + 2) return Hit::Machine;
    return Hit::Name;
}

void TrackList::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1d2026));

    // the header, level with the ruler
    auto head = getLocalBounds().removeFromTop(rulerHeight);
    g.setColour(colours::mixerEdge);
    g.fillRect(head);
    g.setColour(colours::light.withAlpha(0.8f));
    g.setFont(fonts::bold(10.0f));
    g.drawText("TRACK", head.reduced(14, 0), juce::Justification::centredLeft);

    const auto& tracks = state.song().tracks;
    g.saveState();
    g.reduceClipRegion(getLocalBounds().withTrimmedTop(rulerHeight));

    for (int i = 0; i < (int) tracks.size(); ++i)
    {
        const auto r = rowBounds(i);
        if (r.getBottom() < rulerHeight || r.getY() > getHeight()) continue;
        const auto& t = tracks[(size_t) i];
        const auto colour = trackColour(t.colour);

        g.setColour(i == state.track ? juce::Colour(0xff323844) : (i % 2 == 0 ? juce::Colour(0xff22262d) : juce::Colour(0xff1f2329)));
        g.fillRect(r);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float) getWidth());

        // the colour, and the activity light
        g.setColour(colour);
        g.fillRect(r.withWidth(6).reduced(0, 2));
        const auto led = juce::Rectangle<float>(7.0f, 7.0f).withCentre({ 20.0f, (float) r.getY() + 14.0f });
        const bool on = t.machine >= 0 && t.machine < Rack::channels && active[(size_t) t.machine];
        if (on)
        {
            g.setColour(colours::ledGreen.withAlpha(0.35f));
            g.fillEllipse(led.expanded(3.0f));
        }
        g.setColour(on ? colours::ledGreen : colours::ledGreen.darker(2.5f));
        g.fillEllipse(led);

        // the name, and the machine under it
        g.setColour(t.mute ? colours::light.withAlpha(0.4f) : colours::light);
        g.setFont(fonts::bold(12.0f));
        g.drawText(juce::String(t.name), r.withTrimmedLeft(30).withTrimmedRight(90).withHeight(trackHeight / 2 + 4).withTrimmedTop(4),
                   juce::Justification::centredLeft);
        g.setColour(colour.withAlpha(0.85f));
        g.setFont(fonts::bold(9.5f));
        g.drawText(juce::String(machineName(t.machine)).toUpperCase(),
                   r.withTrimmedLeft(30).withTrimmedRight(90).withTrimmedTop(trackHeight / 2 + 2).withTrimmedBottom(4),
                   juce::Justification::centredLeft);

        // M, S, SH
        const bool states[3] = { t.mute, t.solo, t.shuffle };
        const char* const labels[3] = { "M", "S", "SH" };
        const juce::Colour lit[3] = { colours::stepRed, colours::stepYellow, colours::stepOrange };
        for (int w = 0; w < 3; ++w)
        {
            const auto b = switchBounds(i, w).toFloat();
            g.setColour(states[w] ? lit[w] : juce::Colour(0xff2c3038));
            g.fillRoundedRectangle(b, 3.0f);
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);
            g.setColour(states[w] ? colours::ink : colours::light.withAlpha(0.6f));
            g.setFont(fonts::bold(9.5f));
            g.drawText(labels[w], b, juce::Justification::centred);
        }
    }
    g.restoreState();

    g.setColour(juce::Colours::black);
    g.drawVerticalLine(getWidth() - 1, 0.0f, (float) getHeight());
}

void TrackList::mouseDown(const juce::MouseEvent& e)
{
    int row = -1;
    const Hit hit = hitAt(e.getPosition(), row);
    if (row < 0) return;

    state.track = row;

    if (e.mods.isPopupMenu())
    {
        state.repaintAll();
        menu(row);
        return;
    }

    auto toggle = [&] (bool SongTrack::* field)
    {
        state.doc().edit([row, field] (Song& s) { auto& t = s.tracks[(size_t) row]; t.*field = ! (t.*field); });
    };

    switch (hit)
    {
        case Hit::Mute:    toggle(&SongTrack::mute); break;
        case Hit::Solo:    toggle(&SongTrack::solo); break;
        case Hit::Shuffle: toggle(&SongTrack::shuffle); break;
        case Hit::Colour:
        case Hit::Machine: menu(row); break;
        default: break;
    }
    state.repaintAll();
}

void TrackList::mouseDoubleClick(const juce::MouseEvent& e)
{
    int row = -1;
    if (hitAt(e.getPosition(), row) == Hit::Name && row >= 0) rename(row);
}

void TrackList::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int total = (int) state.song().tracks.size() * trackHeight;
    const int visible = getHeight() - rulerHeight;
    state.scrollY = juce::jlimit(0, juce::jmax(0, total - visible + trackHeight), state.scrollY - (int) (w.deltaY * 120.0f));
    state.repaintAll();
}

void TrackList::addTrack()
{
    const auto& tracks = state.song().tracks;
    const int machine = tracks.empty() ? 0 : tracks[(size_t) juce::jlimit(0, (int) tracks.size() - 1, state.track)].machine;

    int same = 1;
    for (const auto& t : tracks) same += t.machine == machine ? 1 : 0;

    state.doc().edit([machine, same] (Song& s)
    {
        SongTrack t;
        t.machine = machine;
        t.name = std::string(machineName(machine)) + " " + std::to_string(same);
        t.colour = (int) s.tracks.size() % trackColours;
        s.tracks.push_back(t);
    });
    state.track = (int) state.song().tracks.size() - 1;
    state.selection.clear();
    state.repaintAll();
}

void TrackList::rename(int row)
{
    if (row < 0 || row >= (int) state.song().tracks.size()) return;

    auto* w = new juce::AlertWindow("Rename the track", {}, juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", juce::String(state.song().tracks[(size_t) row].name));
    w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<TrackList> self(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([self, w, row] (int result)
    {
        if (result != 1 || self == nullptr) return;
        const auto name = w->getTextEditorContents("name").trim().toStdString();
        if (name.empty()) return;
        self->state.doc().edit([row, name] (Song& s)
        {
            if (row < (int) s.tracks.size()) s.tracks[(size_t) row].name = name;
        });
        self->state.repaintAll();
    }), true);
}

void TrackList::menu(int row)
{
    const auto& t = state.song().tracks[(size_t) row];

    juce::PopupMenu machines, colourMenu;
    // a track with parts moves only to a machine that reads its notes the same way
    for (int m = 0; m < Rack::channels; ++m)
        machines.addItem(100 + m, machineName(m), t.parts.empty() || machineKind(m) == machineKind(t.machine),
                         t.machine == m);
    for (int c = 0; c < trackColours; ++c)
    {
        juce::PopupMenu::Item item("        ");
        item.itemID = 200 + c;
        item.colour = trackColour(c);
        item.isTicked = t.colour == c;
        colourMenu.addItem(item);
    }

    const int count = (int) state.song().tracks.size();
    juce::PopupMenu m;
    m.addSectionHeader(juce::String(t.name));
    m.addItem(1, "Rename...");
    m.addSubMenu("Machine", machines);
    m.addSubMenu("Colour", colourMenu);
    m.addSeparator();
    m.addItem(2, "Move up", row > 0);
    m.addItem(3, "Move down", row < count - 1);
    m.addSeparator();
    m.addItem(4, "Add a track");
    m.addItem(5, "Delete this track", count > 1);

    juce::Component::SafePointer<TrackList> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [self, row] (int id)
    {
        if (self == nullptr || id == 0) return;
        auto& st = self->state;

        if (id == 1) { self->rename(row); return; }
        if (id == 4) { self->addTrack(); return; }

        st.doc().edit([row, id] (Song& s)
        {
            auto& tracks = s.tracks;
            if (row >= (int) tracks.size()) return;

            if (id == 2 && row > 0)                          std::swap(tracks[(size_t) row], tracks[(size_t) row - 1]);
            else if (id == 3 && row + 1 < (int) tracks.size()) std::swap(tracks[(size_t) row], tracks[(size_t) row + 1]);
            else if (id == 5 && tracks.size() > 1)           tracks.erase(tracks.begin() + row);
            else if (id >= 200)                              tracks[(size_t) row].colour = id - 200;
            else if (id >= 100)
            {
                // a part's notes mean something else to a machine of another kind, so a track
                // only moves to one of its own kind - or is empty
                auto& t = tracks[(size_t) row];
                const int to = id - 100;
                if (t.parts.empty() || machineKind(to) == machineKind(t.machine)) t.machine = to;
            }
        });

        if (id == 2) st.track = row - 1;
        if (id == 3) st.track = row + 1;
        if (id == 5) st.track = juce::jlimit(0, (int) st.song().tracks.size() - 1, row);
        st.selection.clear();
        st.repaintAll();
    });
}

} // namespace arrange
