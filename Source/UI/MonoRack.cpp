// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "MonoRack.h"

using namespace debirth;

namespace
{
    const char* const noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };

    bool isBlack(int note) { return juce::String(noteNames[note]).containsChar('#'); }

    // note rows run from the C above at the top (row 0) to C at the bottom (row 12)
    int noteOfRow(int row) { return 12 - row; }

    const juce::Colour rollBack   { 0xff1b1c1f };
    const juce::Colour rollBlack  { 0xff141517 };
    const juce::Colour rollLine   { 0xff2c2e33 };
    const juce::Colour noteColour { 0xffef6a22 };
}

//============================================================================== NoteRoll

NoteRoll::NoteRoll(Track<BassPattern>& t, bool accent) : track(t), hasAccent(accent) {}

NoteRoll::Kind NoteRoll::kindOf(int row) const
{
    if (row < notes) return Kind::Note;
    switch (row - notes)
    {
        case 0:  return Kind::Up;
        case 1:  return Kind::Down;
        case 2:  return hasAccent ? Kind::Accent : Kind::Slide;
        default: return Kind::Slide;
    }
}

juce::Rectangle<int> NoteRoll::cell(int step, int row) const
{
    const float w = (float) (getWidth() - labelWidth) / (float) stepsPerPattern;
    const float h = (float) (getHeight() - headerHeight) / (float) rows();
    const int x0 = labelWidth + juce::roundToInt(w * (float) step);
    const int x1 = labelWidth + juce::roundToInt(w * (float) (step + 1));
    const int y0 = headerHeight + juce::roundToInt(h * (float) row);
    const int y1 = headerHeight + juce::roundToInt(h * (float) (row + 1));
    return { x0, y0, x1 - x0, y1 - y0 };
}

int NoteRoll::stepAt(int x) const
{
    const float w = (float) (getWidth() - labelWidth) / (float) stepsPerPattern;
    return x < labelWidth ? -1 : juce::jlimit(0, stepsPerPattern - 1, (int) ((float) (x - labelWidth) / w));
}

int NoteRoll::rowAt(int y) const
{
    const float h = (float) (getHeight() - headerHeight) / (float) rows();
    return y < headerHeight ? -1 : juce::jlimit(0, rows() - 1, (int) ((float) (y - headerHeight) / h));
}

void NoteRoll::apply(int step, int row)
{
    auto& p = track.edited();
    BassStep s = p.get(step);

    switch (kindOf(row))
    {
        case Kind::Note:
            if (paintOn) { s.gate = true; s.note = noteOfRow(row); }
            else         s.gate = false;
            break;
        case Kind::Up:     s.octave = paintOn ? 1 : (s.octave == 1 ? 0 : s.octave); break;
        case Kind::Down:   s.octave = paintOn ? -1 : (s.octave == -1 ? 0 : s.octave); break;
        case Kind::Accent: s.accent = paintOn; break;
        case Kind::Slide:  s.slide = paintOn; break;
    }

    p.set(step, s);
    repaint();
}

void NoteRoll::mouseDown(const juce::MouseEvent& e)
{
    const int step = stepAt(e.x), row = rowAt(e.y);
    if (step < 0 || row < 0) return;

    const BassStep s = track.edited().get(step);

    // the first cell decides: on where it was off, off where it was on
    switch (kindOf(row))
    {
        case Kind::Note:   paintOn = ! e.mods.isPopupMenu() && ! (s.gate && s.note == noteOfRow(row)); break;
        case Kind::Up:     paintOn = s.octave != 1; break;
        case Kind::Down:   paintOn = s.octave != -1; break;
        case Kind::Accent: paintOn = ! s.accent; break;
        case Kind::Slide:  paintOn = ! s.slide; break;
    }

    lastStep = step;
    apply(step, row);
}

void NoteRoll::mouseDrag(const juce::MouseEvent& e)
{
    const int step = stepAt(e.x), row = rowAt(e.y);
    if (step < 0 || row < 0 || step == lastStep) return;

    // a drag along the notes paints at whatever height it is; along a switch row, that row
    const int startRow = rowAt(e.getMouseDownY());
    lastStep = step;
    apply(step, startRow < notes ? juce::jmin(row, notes - 1) : startRow);
}

void NoteRoll::tick(bool playing)
{
    const int pos = playing && track.current == track.selected ? track.position.load() : -1;
    if (pos != shownPosition)
    {
        shownPosition = pos;
        repaint();
    }
}

void NoteRoll::paint(juce::Graphics& g)
{
    const auto& p = track.edited();
    const int length = p.len();

    g.fillAll(rollBack);

    // the rows: black keys darker, and their names
    for (int row = 0; row < rows(); ++row)
    {
        const auto r = cell(0, row).withX(0).withRight(getWidth());
        const auto kind = kindOf(row);
        const bool note = kind == Kind::Note;
        if (note && isBlack(noteOfRow(row))) { g.setColour(rollBlack); g.fillRect(r); }
        if (row == notes) { g.setColour(juce::Colours::black); g.fillRect(r.withHeight(2)); }

        g.setColour(note ? colours::light.withAlpha(isBlack(noteOfRow(row)) ? 0.55f : 0.85f) : colours::stepYellow);
        g.setFont(fonts::bold(note ? 9.0f : 8.5f));
        const juce::String name = note ? juce::String(noteNames[noteOfRow(row)])
                                : kind == Kind::Up ? "UP" : kind == Kind::Down ? "DOWN" : kind == Kind::Accent ? "ACCENT" : "SLIDE";
        g.drawText(name, r.withWidth(labelWidth - 4), juce::Justification::centredRight);
    }

    // the columns, grouped in fours, with the step numbers over them
    for (int s = 0; s < stepsPerPattern; ++s)
    {
        const auto col = cell(s, 0).withY(headerHeight).withBottom(getHeight());
        if ((s / 4) % 2 == 1) { g.setColour(juce::Colours::white.withAlpha(0.03f)); g.fillRect(col); }
        if (s == shownPosition) { g.setColour(colours::ledRed.withAlpha(0.22f)); g.fillRect(col); }

        g.setColour(s == shownPosition ? colours::ledRed : colours::light.withAlpha(0.6f));
        g.setFont(fonts::bold(8.5f));
        g.drawText(juce::String(s + 1), col.withY(0).withHeight(headerHeight), juce::Justification::centred);

        g.setColour(s % 4 == 0 ? juce::Colours::black : rollLine);
        g.fillRect(col.getX(), headerHeight, 1, getHeight() - headerHeight);
    }
    g.setColour(rollLine);
    for (int row = 1; row < rows(); ++row)
        g.fillRect(labelWidth, cell(0, row).getY(), getWidth() - labelWidth, 1);

    // the steps
    for (int s = 0; s < stepsPerPattern; ++s)
    {
        const BassStep st = p.get(s);
        const float dim = s < length ? 1.0f : 0.35f;

        if (st.gate)
        {
            auto r = cell(s, noteOfRow(st.note)).toFloat().reduced(1.5f, 1.0f);
            // a slide reaches over into the next step
            if (st.slide && s < stepsPerPattern - 1) r = r.withRight((float) cell(s + 1, 0).getX() + 2.0f);
            const bool bright = hasAccent && st.accent;
            g.setColour((bright ? noteColour.brighter(0.35f) : noteColour).withMultipliedAlpha(dim));
            g.fillRoundedRectangle(r, 2.0f);
            if (st.octave != 0)
            {
                g.setColour(juce::Colours::black.withAlpha(0.8f * dim));
                g.setFont(fonts::bold(8.0f));
                g.drawText(st.octave > 0 ? "+" : "-", r, juce::Justification::centred);
            }
        }

        for (int row = notes; row < rows(); ++row)
        {
            const auto kind = kindOf(row);
            const bool on = kind == Kind::Up ? st.octave > 0 : kind == Kind::Down ? st.octave < 0
                          : kind == Kind::Accent ? st.accent : st.slide;
            if (! on) continue;
            const auto c = kind == Kind::Accent ? colours::ledRed : kind == Kind::Slide ? colours::ledGreen : colours::stepYellow;
            g.setColour(c.withMultipliedAlpha(dim));
            g.fillEllipse(cell(s, row).toFloat().withSizeKeepingCentre(7.0f, 7.0f));
        }
    }

    // past the end of the pattern
    if (length < stepsPerPattern)
    {
        const int x = cell(length, 0).getX();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRect(x, headerHeight, getWidth() - x, getHeight() - headerHeight);
        g.setColour(colours::ledRed);
        g.fillRect(x - 1, 0, 2, getHeight());
    }

    g.setColour(juce::Colours::black);
    g.drawRect(getLocalBounds());
}

//============================================================================== MonoRack

MonoRack::MonoRack(Track<BassPattern>& track, Channel& c, std::function<bool()> p,
                   const Theme& t, std::vector<Row> layout, bool hasAccent)
    : playing(p), theme(t), rows(std::move(layout)),
      patterns(track, c, p), roll(track, hasAccent), strip(c, t.name.toUpperCase())
{
    addAndMakeVisible(patterns);
    fold.setInk(t.ink);
    fold.onToggle = [this] { roll.setVisible(fold.isOpen()); if (onFold) onFold(); };
    addAndMakeVisible(fold);
    addAndMakeVisible(roll);
    addAndMakeVisible(strip);
}

void MonoRack::setFolded(bool f)
{
    if (f == isFolded()) return;
    fold.setOpen(! f);
    roll.setVisible(! f);
}

int MonoRack::countOf(const std::vector<Row>& rs)
{
    int n = 0;
    for (const auto& r : rs)
        for (const auto& s : r) n += s.count;
    return n;
}

int MonoRack::preferredHeight() const
{
    // The rows of controls, the fold bar, then the roll: a row of notes 11 high, and the
    // header; never shorter than the pattern section beside it.
    //
    // Folded, the controls and the bar under them - it stays where it was, and only the roll
    // goes - and the pattern section and mixer strip pack down beside them.
    const bool labelled = rows.size() > 1;
    if (isFolded())
    {
        const int knobRows = (int) rows.size() * (controlHeight + (labelled ? sectionLabel : 0));
        return juce::jmax(foldedMinimum, 8 + (labelled ? 28 : 0) + knobRows + 8 + FoldBar::height + 8);
    }
    const int knobs = (int) rows.size() * (controlHeight + (labelled ? sectionLabel : 0)) + 34;
    return juce::jmax(PatternPanel::height, knobs + 12 + FoldBar::height + 2 + 17 * 11 + 16);
}

void MonoRack::refresh()
{
    for (auto& c : controls) refreshControl(*c);
    strip.refresh();
    roll.repaint();
}

void MonoRack::tick(bool blinkPhase)
{
    patterns.tick(blinkPhase);
    strip.tick();
    roll.tick(playing());
}

void MonoRack::resized()
{
    const bool folded = isFolded();
    patterns.setCompact(folded);
    strip.setCompact(folded);

    auto r = getLocalBounds();
    patterns.setBounds(r.removeFromLeft(168).withHeight(folded ? getHeight() : juce::jmin(getHeight(), PatternPanel::height)));
    r.removeFromLeft(6);
    strip.setBounds(r.removeFromRight(84));
    r.removeFromRight(6);
    face = r;

    auto inner = r.reduced(10, 8);
    const bool labelled = rows.size() > 1;

    // One row: the name to the left of the knobs. More: the name above them.
    if (labelled) titleArea = inner.removeFromTop(28);
    else          titleArea = inner.withHeight(controlHeight).withWidth(200);

    sectionAreas.clear();
    size_t next = 0;
    for (const auto& row : rows)
    {
        auto line = inner.removeFromTop(controlHeight + (labelled ? sectionLabel : 0));
        if (! labelled) line.removeFromLeft(titleArea.getWidth());

        // spread the controls across the line, sections apart
        int count = 0;
        for (const auto& s : row) count += s.count;
        const int gaps = (int) row.size() - 1;
        const int w = juce::jmin(labelled ? controlWidth : 100, (line.getWidth() - gaps * sectionGap) / juce::jmax(1, count));

        int x = line.getX();
        for (const auto& s : row)
        {
            const juce::Rectangle<int> area(x, line.getY(), w * s.count, line.getHeight());
            if (labelled) sectionAreas.emplace_back(s.name, area.withHeight(sectionLabel - 2));
            for (int i = 0; i < s.count && next < controls.size(); ++i)
                controls[next++]->setBounds(x + i * w, line.getBottom() - controlHeight, w, controlHeight);
            x += w * s.count + sectionGap;
        }
    }

    // The bar under the controls, open or folded: it never moves, so it is where the mouse
    // left it to unfold again. Only the roll under it comes and goes.
    inner.removeFromTop(8);
    fold.setBounds(inner.removeFromTop(FoldBar::height));
    if (folded) return;
    inner.removeFromTop(2);
    roll.setBounds(inner);
}

void MonoRack::paint(juce::Graphics& g)
{
    theme.drawFace(g, face);

    if (rows.size() > 1)
    {
        theme.drawTitle(g, titleArea);
    }
    else
    {
        // the 303's way: the name large, and what it is under it
        auto t = titleArea.withTrimmedTop(6);
        g.setColour(theme.title);
        g.setFont(fonts::bold(26.0f));
        g.drawText(theme.name, t.removeFromTop(30), juce::Justification::bottomLeft);
        g.setColour(theme.inkDim);
        g.setFont(fonts::bold(10.5f));
        g.drawText(theme.subtitle, t, juce::Justification::topLeft);
    }

    // each section's name, with a rule under it
    for (const auto& [name, area] : sectionAreas)
    {
        g.setColour(theme.ink);
        g.setFont(fonts::bold(9.0f));
        g.drawText(name, area, juce::Justification::centred);
        g.setColour(theme.ink.withAlpha(0.4f));
        g.fillRect(area.getX() + 2, area.getBottom(), area.getWidth() - 4, 1);
    }
}
