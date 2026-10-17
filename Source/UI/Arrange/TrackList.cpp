// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "TrackList.h"

using namespace debirth;

namespace arrange
{

TrackList::TrackList(State& s) : state(s)
{
    setTooltip("The tracks, one for each machine in the rack. Click to pick one; double-click its name "
               "to rename it; right-click for its colour. RACK goes to its machine, to turn the knobs. "
               "M mutes, S solos, SH swings it with the SHUFFLE knob.");
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
    const int widths[] = { 20, 20, 26, 36 };
    const int offsets[] = { 52, 28, 0, 86 };   // from the right, to each one's right edge
    const int w = widths[which];
    const int right = r.getRight() - 8 - offsets[which];
    return { right - w, r.getCentreY() - 10, w, 20 };
}

TrackList::Hit TrackList::hitAt(juce::Point<int> p, int& row) const
{
    row = rowAt(p.y);
    if (row < 0) return Hit::None;

    for (int w = 0; w < 4; ++w)
        if (switchBounds(row, w).contains(p))
            return w == 0 ? Hit::Mute : w == 1 ? Hit::Solo : w == 2 ? Hit::Shuffle : Hit::Rack;

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
        g.drawText(juce::String(t.name), r.withTrimmedLeft(30).withTrimmedRight(130).withHeight(trackHeight / 2 + 4).withTrimmedTop(4),
                   juce::Justification::centredLeft);
        g.setColour(colour.withAlpha(0.85f));
        g.setFont(fonts::bold(9.5f));
        g.drawText(juce::String(machineName(t.machine)).toUpperCase(),
                   r.withTrimmedLeft(30).withTrimmedRight(130).withTrimmedTop(trackHeight / 2 + 2).withTrimmedBottom(4),
                   juce::Justification::centredLeft);

        // M, S, SH, and RACK - never lit, it goes somewhere rather than switching something
        const bool states[4] = { t.mute, t.solo, t.shuffle, false };
        const char* const labels[4] = { "M", "S", "SH", "RACK" };
        const juce::Colour lit[4] = { colours::stepRed, colours::stepYellow, colours::stepOrange, colours::light };
        for (int w = 0; w < 4; ++w)
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
        case Hit::Rack:
            if (state.showMachine) state.showMachine(state.song().tracks[(size_t) row].machine);
            return;
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
    const int machine = t.machine;

    juce::PopupMenu colourMenu;
    for (int c = 0; c < trackColours; ++c)
    {
        juce::PopupMenu::Item item("        ");
        item.itemID = 200 + c;
        item.colour = trackColour(c);
        item.isTicked = t.colour == c;
        colourMenu.addItem(item);
    }

    //
    // No adding, deleting, moving or changing a track's machine: the tracks are the rack's
    // machines, one each and in its order, so there is nothing of that kind to choose.
    //
    juce::PopupMenu m;
    m.addSectionHeader(juce::String(t.name));
    m.addItem(1, "Rename...");
    m.addSubMenu("Colour", colourMenu);
    m.addSeparator();
    m.addItem(2, juce::String("Show the ") + machineName(machine) + " in the rack");

    juce::Component::SafePointer<TrackList> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [self, row, machine] (int id)
    {
        if (self == nullptr || id == 0) return;
        auto& st = self->state;

        if (id == 1) { self->rename(row); return; }
        if (id == 2) { if (st.showMachine) st.showMachine(machine); return; }

        st.doc().edit([row, id] (Song& s)
        {
            if (row < (int) s.tracks.size() && id >= 200) s.tracks[(size_t) row].colour = id - 200;
        });
        st.repaintAll();
    });
}

} // namespace arrange
