// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "SamplerRack.h"

#include <juce_audio_basics/juce_audio_basics.h>

using namespace debirth;

namespace
{
    const juce::Colour rollBack  { 0xff1b1c1f };
    const juce::Colour rollBlack { 0xff141517 };
    const juce::Colour rollLine  { 0xff2c2e33 };
    const juce::Colour answered  { 0xff22323a };   // a key the programme plays
    const juce::Colour hitColour { 0xff4fb3d9 };
    const juce::Colour lcd       { 0xff9fd27a };

    // as the S950 names them: middle C is C3
    juce::String noteName(int n) { return juce::MidiMessage::getMidiNoteName(n, true, true, 3); }
    bool isBlack(int n) { return juce::MidiMessage::isMidiNoteBlack(n); }
}

//============================================================================== SampleRoll

SampleRoll::SampleRoll(Track<NotePattern>& t) : track(t) {}

void SampleRoll::setSpans(std::vector<Mz950::Span> s)
{
    spans = std::move(s);
    repaint();
}

void SampleRoll::setLowest(int note)
{
    low = juce::jlimit(0, NotePattern::notes - visibleNotes, note);
    repaint();
}

int SampleRoll::patternRowOf(int row) const
{
    if (row < visibleNotes) return low + visibleNotes - 1 - row;   // highest at the top
    return row == visibleNotes ? NotePattern::accentRow : NotePattern::tieRow;
}

juce::Rectangle<int> SampleRoll::cell(int step, int row) const
{
    const float w = (float) (getWidth() - labelWidth) / (float) stepsPerPattern;
    const float h = (float) (getHeight() - headerHeight) / (float) rows;
    const int x0 = labelWidth + juce::roundToInt(w * (float) step);
    const int x1 = labelWidth + juce::roundToInt(w * (float) (step + 1));
    const int y0 = headerHeight + juce::roundToInt(h * (float) row);
    const int y1 = headerHeight + juce::roundToInt(h * (float) (row + 1));
    return { x0, y0, x1 - x0, y1 - y0 };
}

int SampleRoll::stepAt(int x) const
{
    const float w = (float) (getWidth() - labelWidth) / (float) stepsPerPattern;
    return x < labelWidth ? -1 : juce::jlimit(0, stepsPerPattern - 1, (int) ((float) (x - labelWidth) / w));
}

int SampleRoll::rowAt(int y) const
{
    const float h = (float) (getHeight() - headerHeight) / (float) rows;
    return y < headerHeight ? -1 : juce::jlimit(0, rows - 1, (int) ((float) (y - headerHeight) / h));
}

void SampleRoll::apply(int step, int row)
{
    track.edited().set(patternRowOf(row), step, paintOn);
    repaint();
}

void SampleRoll::mouseDown(const juce::MouseEvent& e)
{
    const int step = stepAt(e.x), row = rowAt(e.y);
    if (step < 0 || row < 0) return;

    paintOn = ! e.mods.isPopupMenu() && ! track.edited().get(patternRowOf(row), step);
    lastStep = step;
    apply(step, row);
}

void SampleRoll::mouseDrag(const juce::MouseEvent& e)
{
    const int step = stepAt(e.x);
    if (step < 0 || step == lastStep) return;

    // along the row it started on
    const int row = rowAt(e.getMouseDownY());
    if (row < 0) return;
    lastStep = step;
    apply(step, row);
}

void SampleRoll::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // over the key names, the wheel moves the keys; anywhere else it scrolls the rack
    if (e.x >= labelWidth) { Component::mouseWheelMove(e, w); return; }

    wheel += w.deltaY * 12.0f;
    const int whole = (int) wheel;
    if (whole != 0)
    {
        wheel -= (float) whole;
        setLowest(low + whole);
    }
}

void SampleRoll::tick(bool playing)
{
    const int pos = playing && track.current == track.selected ? track.position.load() : -1;
    if (pos != shownPosition)
    {
        shownPosition = pos;
        repaint();
    }
}

void SampleRoll::paint(juce::Graphics& g)
{
    const auto& p = track.edited();
    const int length = p.len();

    g.fillAll(rollBack);

    for (int row = 0; row < rows; ++row)
    {
        const auto r = cell(0, row).withX(0).withRight(getWidth());
        const int pr = patternRowOf(row);

        juce::String label;
        if (row < visibleNotes)
        {
            const Mz950::Span* span = nullptr;
            for (const auto& s : spans)
                if (pr >= s.low && pr <= s.high) { span = &s; break; }

            if (span != nullptr)       { g.setColour(answered.brighter(isBlack(pr) ? 0.0f : 0.15f)); g.fillRect(r); }
            else if (isBlack(pr))      { g.setColour(rollBlack); g.fillRect(r); }

            label = noteName(pr);
            if (span != nullptr && span->low == span->high)
                label = juce::String(span->sample).trim() + "  " + label;

            g.setColour(colours::light.withAlpha(span != nullptr ? 0.9f : 0.45f));
        }
        else
        {
            if (row == visibleNotes) { g.setColour(juce::Colours::black); g.fillRect(r.withHeight(2)); }
            label = row == visibleNotes ? "ACCENT" : "TIE";
            g.setColour(colours::stepYellow);
        }

        g.setFont(fonts::bold(8.5f));
        g.drawFittedText(label, r.withWidth(labelWidth - 4), juce::Justification::centredRight, 1, 0.8f);
    }

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
    for (int row = 1; row < rows; ++row)
        g.fillRect(labelWidth, cell(0, row).getY(), getWidth() - labelWidth, 1);

    // the notes, joined across ties; then the accent and tie marks
    for (int s = 0; s < stepsPerPattern; ++s)
    {
        const float dim = s < length ? 1.0f : 0.35f;
        const int next = s + 1 < stepsPerPattern ? s + 1 : -1;
        const bool nextTied = next >= 0 && p.get(NotePattern::tieRow, next);

        for (int row = 0; row < visibleNotes; ++row)
        {
            const int note = patternRowOf(row);
            if (! p.get(note, s)) continue;
            auto r = cell(s, row).toFloat().reduced(1.5f, 1.0f);
            if (nextTied && p.get(note, next)) r = r.withRight((float) cell(next, row).getX() + 2.0f);
            g.setColour(hitColour.withMultipliedAlpha(dim));
            g.fillRoundedRectangle(r, 2.0f);
        }

        if (p.get(NotePattern::accentRow, s))
        {
            g.setColour(colours::ledRed.withMultipliedAlpha(dim));
            g.fillEllipse(cell(s, visibleNotes).toFloat().withSizeKeepingCentre(7.0f, 7.0f));
        }
        if (p.get(NotePattern::tieRow, s))
        {
            g.setColour(colours::ledGreen.withMultipliedAlpha(dim));
            g.fillEllipse(cell(s, visibleNotes + 1).toFloat().withSizeKeepingCentre(7.0f, 7.0f));
        }
    }

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

//============================================================================== SamplerRack

SamplerRack::SamplerRack(Mz950& s, Channel& c, std::function<bool()> p, const Theme& t)
    : sampler(s), playing(p), theme(t), patterns(s.track, c, p), roll(s.track), strip(c, t.name.toUpperCase())
{
    addAndMakeVisible(patterns);

    loadDisk.setShowsLed(false);
    loadDisk.setTooltip("Open an S900/S950 disk image - .hfe, or a plain sector image.");
    loadDisk.onPress = [this] { chooseDisk(); };
    addAndMakeVisible(loadDisk);

    programBox.setTooltip("The programme on the disk to play.");
    programBox.setColour(juce::ComboBox::backgroundColourId, colours::display);
    programBox.setColour(juce::ComboBox::textColourId, lcd);
    programBox.setColour(juce::ComboBox::arrowColourId, lcd);
    programBox.setColour(juce::ComboBox::outlineColourId, juce::Colours::black);
    programBox.onChange = [this]
    {
        const int i = programBox.getSelectedItemIndex();
        if (i >= 0 && i != sampler.program() && sampler.selectProgram(i))
            showDisk();
    };
    addAndMakeVisible(programBox);

    octaveDown.setShowsLed(false);
    octaveUp.setShowsLed(false);
    octaveDown.setTooltip("Show the keys an octave lower. The wheel over the key names moves them a key at a time.");
    octaveUp.setTooltip("Show the keys an octave higher.");
    octaveDown.onPress = [this] { roll.setLowest(roll.lowest() - 12); };
    octaveUp.onPress = [this] { roll.setLowest(roll.lowest() + 12); };
    addAndMakeVisible(octaveDown);
    addAndMakeVisible(octaveUp);

    for (int i = 0; i < sampler.knobs.size(); ++i)
    {
        controls.push_back(makeControl(sampler.knobs, i, theme));
        addAndMakeVisible(*controls.back());
    }

    fold.setInk(t.ink);
    fold.onToggle = [this] { roll.setVisible(fold.isOpen()); if (onFold) onFold(); };
    addAndMakeVisible(fold);
    addAndMakeVisible(roll);
    addAndMakeVisible(strip);
    showDisk();
}

void SamplerRack::chooseDisk()
{
    auto folder = juce::File(juce::String(sampler.diskPath())).getParentDirectory();
    if (! folder.isDirectory()) folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser>("Open an S950 disk", folder, "*.hfe;*.img;*.ima;*.dsk;*");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;

            std::string error;
            if (! sampler.loadDisk(file.getFullPathName().toStdString(), error))
                message = "NOT A DISK: " + juce::String(error);
            else
                message = error.empty() ? juce::String() : juce::String(error).toUpperCase();
            showDisk();
        });
}

void SamplerRack::showDisk()
{
    programBox.clear(juce::dontSendNotification);
    const auto names = sampler.programNames();
    for (size_t i = 0; i < names.size(); ++i)
        programBox.addItem(juce::String(names[i]).trim(), (int) i + 1);
    programBox.setSelectedItemIndex(sampler.program(), juce::dontSendNotification);
    programBox.setEnabled(! names.empty());

    diskText = sampler.diskName().empty() ? juce::String("NO DISK")
                                          : juce::File(juce::String(sampler.diskPath())).getFileName().toUpperCase();

    // show the keys from the lowest the programme plays
    auto spans = sampler.keySpans();
    if (! spans.empty()) roll.setLowest(spans.front().low);
    roll.setSpans(std::move(spans));
    repaint();
}

void SamplerRack::follow()
{
    for (auto& c : controls) refreshControl(*c);
    strip.refresh();
}

void SamplerRack::refresh()
{
    for (auto& c : controls) refreshControl(*c);
    strip.refresh();
    showDisk();
}

void SamplerRack::tick(bool blinkPhase)
{
    patterns.tick(blinkPhase);
    strip.tick();
    roll.tick(playing());
}

void SamplerRack::resized()
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
    inner.removeFromTop(30);   // the name

    auto diskRow = inner.removeFromTop(28);
    loadDisk.setBounds(diskRow.removeFromLeft(96));
    diskRow.removeFromLeft(8);
    display = diskRow.removeFromLeft(260);
    diskRow.removeFromLeft(8);
    programBox.setBounds(diskRow.removeFromLeft(220));
    octaveUp.setBounds(diskRow.removeFromRight(64));
    diskRow.removeFromRight(6);
    octaveDown.setBounds(diskRow.removeFromRight(64));

    inner.removeFromTop(8);
    auto knobs = inner.removeFromTop(64);
    const int w = juce::jmin(72, knobs.getWidth() / juce::jmax(1, (int) controls.size()));
    for (auto& c : controls)
        c->setBounds(knobs.removeFromLeft(w));

    // the bar under the knobs, open or folded - it never moves; only the roll comes and goes
    inner.removeFromTop(8);
    fold.setBounds(inner.removeFromTop(FoldBar::height));
    if (folded) return;
    inner.removeFromTop(2);
    roll.setBounds(inner);
}

void SamplerRack::setFolded(bool f)
{
    if (f == isFolded()) return;
    fold.setOpen(! f);
    roll.setVisible(! f);
}

void SamplerRack::paint(juce::Graphics& g)
{
    theme.drawFace(g, face);
    theme.drawTitle(g, face.reduced(12, 8).removeFromTop(26));

    // the disk, on an LCD
    g.setColour(colours::display);
    g.fillRoundedRectangle(display.toFloat(), 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(display.toFloat().reduced(0.5f), 2.0f, 1.0f);
    g.setColour(lcd);
    g.setFont(fonts::digits(13.0f));
    g.drawFittedText(message.isNotEmpty() ? message : diskText, display.reduced(8, 0), juce::Justification::centredLeft, 1, 0.7f);
}
