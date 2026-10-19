// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "TrackList.h"

using namespace debirth;

namespace arrange
{

TrackList::TrackList(State& s) : state(s)
{
    setTooltip("The tracks, one for each machine in the rack. Click to pick one; double-click its name "
               "to rename it; right-click for its colour. RACK goes to its machine, to turn the knobs. "
               "M mutes, S solos, SH swings it with the SHUFFLE knob. A: it has automation - right-click "
               "to clear it. FX, under them, holds the effects units' automation.");
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
    return row < rowCount(state.song()) ? row : -1;
}

juce::Rectangle<int> TrackList::switchBounds(int row, int which) const
{
    const auto r = rowBounds(row);
    const int widths[] = { 20, 20, 26, 36 };
    const int offsets[] = { 52, 28, 0, 86 };   // from the right, to each one's right edge
    const int w = widths[which];
    const int right = r.getRight() - 8 - offsets[which];
    return { right - w, r.getY() + 6, w, 20 };   // on the name's line
}

// a row's name, on its top line; the machine's goes under it
juce::Rectangle<int> TrackList::nameBounds(juce::Rectangle<int> row) const
{
    return { 30, row.getY() + 5, juce::jmax(0, row.getWidth() - 160), 20 };
}

// the automation selector, along the foot of a row
juce::Rectangle<int> TrackList::selectorBounds(int row) const
{
    const auto r = rowBounds(row);
    return { 30, r.getY() + 44, juce::jmax(0, r.getWidth() - 38), 20 };
}

TrackList::Hit TrackList::hitAt(juce::Point<int> p, int& row) const
{
    row = rowAt(p.y);
    if (row < 0) return Hit::None;
    if (selectorBounds(row).contains(p)) return Hit::Shown;
    if (row == fxRow(state.song())) return Hit::Fx;

    for (int w = 0; w < 4; ++w)
        if (switchBounds(row, w).contains(p))
            return w == 0 ? Hit::Mute : w == 1 ? Hit::Solo : w == 2 ? Hit::Shuffle : Hit::Rack;

    const auto r = rowBounds(row);
    if (p.x < 12) return Hit::Colour;
    if (p.y > r.getY() + 24) return Hit::Machine;
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
        g.drawText(juce::String(t.name), nameBounds(r),
                   juce::Justification::centredLeft);
        g.setColour(colour.withAlpha(0.85f));
        g.setFont(fonts::bold(9.5f));
        g.drawText(juce::String(machineName(t.machine)).toUpperCase(),
                   nameBounds(r).translated(0, 20).withHeight(14),
                   juce::Justification::centredLeft);

        if (! t.automation.empty()) paintBadge(g, r);
        paintSelector(g, i, t.machine, t.automation);

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

    // FX: the effects units' automation, which no one machine's track owns
    {
        const auto r = rowBounds(fxRow(state.song()));
        g.setColour(juce::Colour(0xff1a1c21));
        g.fillRect(r);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawHorizontalLine(r.getBottom() - 1, 0.0f, (float) getWidth());
        g.setColour(colours::light.withAlpha(0.8f));
        g.fillRect(r.withWidth(6).reduced(0, 2));

        g.setColour(colours::light);
        g.setFont(fonts::bold(12.0f));
        g.drawText("FX", nameBounds(r),
                   juce::Justification::centredLeft);
        g.setColour(colours::light.withAlpha(0.6f));
        g.setFont(fonts::bold(9.5f));
        g.drawText("DIST  PCF  DELAY  REVERB  COMP",
                   nameBounds(r).withRight(r.getRight() - 8).translated(0, 20).withHeight(14),
                   juce::Justification::centredLeft);

        if (! state.song().fx.empty()) paintBadge(g, r);
        paintSelector(g, fxRow(state.song()), Rack::FX, state.song().fx);
    }
    g.restoreState();

    g.setColour(juce::Colours::black);
    g.drawVerticalLine(getWidth() - 1, 0.0f, (float) getHeight());
}

// a control's name, as the selector lists it: a mixer channel's said to be the mixer's
juce::String TrackList::controlName(int owner, const std::string& param) const
{
    const auto& rack = state.processor.rack;
    const int i = rack.paramIndex(owner, param);
    if (i < 0) return juce::String(param);
    const juce::String name(rack.paramName(owner, i));
    return owner != Rack::FX && i >= rack.knobCount(owner) ? "MIXER " + name : name;
}

//
// What the row's lane draws of its automation: every lane, none, or one control's. Dim, and
// doing nothing, while the row has none.
//
void TrackList::paintSelector(juce::Graphics& g, int row, int owner, const std::vector<AutoLane>& lanes) const
{
    const auto b = selectorBounds(row).toFloat();
    const bool any = ! lanes.empty();

    g.setColour(juce::Colour(0xff15171b).withAlpha(any ? 1.0f : 0.5f));
    g.fillRoundedRectangle(b, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

    juce::String text = "NO AUTOMATION";
    if (any)
    {
        const auto& shown = state.shownAutomation[(size_t) owner];
        bool found = false;
        for (const auto& lane : lanes) found |= lane.param == shown;
        text = shown == "-" ? juce::String("AUTOMATION: HIDDEN")
             : found        ? "AUTOMATION: " + controlName(owner, shown)
                            : "AUTOMATION: ALL (" + juce::String((int) lanes.size()) + ")";
    }

    g.setColour(colours::light.withAlpha(any ? 0.85f : 0.35f));
    g.setFont(fonts::bold(9.5f));
    g.drawText(text, b.reduced(8.0f, 0.0f).withTrimmedRight(14.0f), juce::Justification::centredLeft, true);

    if (any)
    {
        juce::Path arrow;
        const float cx = b.getRight() - 12.0f, cy = b.getCentreY();
        arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.fillPath(arrow);
    }
}

void TrackList::automationMenu(int row)
{
    const auto& song = state.song();
    const bool fx = row == fxRow(song);
    if (! fx && row >= (int) song.tracks.size()) return;
    const int owner = fx ? Rack::FX : song.tracks[(size_t) row].machine;
    const auto lanes = fx ? song.fx : song.tracks[(size_t) row].automation;
    if (lanes.empty() || owner < 0 || owner >= Rack::owners) return;

    const auto& shown = state.shownAutomation[(size_t) owner];
    bool found = false;
    for (const auto& lane : lanes) found |= lane.param == shown;

    juce::PopupMenu m;
    m.addSectionHeader("Automation shown");
    m.addItem(1, "All", true, shown.empty() || (shown != "-" && ! found));
    m.addItem(2, "None", true, shown == "-");
    m.addSeparator();
    for (int i = 0; i < (int) lanes.size(); ++i)
        m.addItem(100 + i, controlName(owner, lanes[(size_t) i].param), true, lanes[(size_t) i].param == shown);

    juce::Component::SafePointer<TrackList> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [self, owner, lanes] (int id)
    {
        if (self == nullptr || id == 0) return;
        auto& shownNow = self->state.shownAutomation[(size_t) owner];
        if (id == 1)      shownNow.clear();
        else if (id == 2) shownNow = "-";
        else if (id >= 100 && id - 100 < (int) lanes.size()) shownNow = lanes[(size_t) (id - 100)].param;
        self->state.repaintAll();
    });
}

// A, on a row that has automation
void TrackList::paintBadge(juce::Graphics& g, juce::Rectangle<int> row) const
{
    const auto b = juce::Rectangle<float>((float) row.getRight() - 152.0f, (float) row.getY() + 8.0f, 16.0f, 16.0f);
    g.setColour(colours::stepYellow.withAlpha(0.85f));
    g.fillRoundedRectangle(b, 3.0f);
    g.setColour(colours::ink);
    g.setFont(fonts::bold(10.0f));
    g.drawText("A", b, juce::Justification::centred);
}

void TrackList::mouseDown(const juce::MouseEvent& e)
{
    int row = -1;
    const Hit hit = hitAt(e.getPosition(), row);
    if (row < 0) return;
    if (hit == Hit::Shown)
    {
        automationMenu(row);
        return;
    }
    if (hit == Hit::Fx)
    {
        if (e.mods.isPopupMenu()) fxMenu();
        return;
    }

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
    const int total = rowCount(state.song()) * trackHeight;
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
    m.addItem(3, "Clear automation", ! t.automation.empty());

    juce::Component::SafePointer<TrackList> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [self, row, machine] (int id)
    {
        if (self == nullptr || id == 0) return;
        auto& st = self->state;

        if (id == 1) { self->rename(row); return; }
        if (id == 2) { if (st.showMachine) st.showMachine(machine); return; }
        if (id == 3)
        {
            st.doc().edit([row] (Song& s) { if (row < (int) s.tracks.size()) s.tracks[(size_t) row].automation.clear(); });
            st.repaintAll();
            return;
        }

        st.doc().edit([row, id] (Song& s)
        {
            if (row < (int) s.tracks.size() && id >= 200) s.tracks[(size_t) row].colour = id - 200;
        });
        st.repaintAll();
    });
}

void TrackList::fxMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader("FX");
    m.addItem(1, "Clear automation", ! state.song().fx.empty());

    juce::Component::SafePointer<TrackList> self(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [self] (int id)
    {
        if (self == nullptr || id != 1) return;
        self->state.doc().edit([] (Song& s) { s.fx.clear(); });
        self->state.repaintAll();
    });
}

} // namespace arrange
