// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "DrumRack.h"

using namespace debirth;

DrumRack::DrumRack(Track<DrumPattern>& t, Channel& c, std::function<bool()> p,
                   const Theme& th, std::vector<VoiceSpec> list, Palette pal)
    : track(t), playing(p), theme(th), palette(std::move(pal)),
      patterns(t, p), voiceList(std::move(list)), strip(c, th.name.toUpperCase())
{
    const int pads = (int) voiceList.size() + 1;
    for (int pad = 0; pad < pads; ++pad)
    {
        auto v = std::make_unique<Pad>(pad == 0 ? "AC" : voiceList[(size_t) pad - 1].code);
        v->setTooltip(pad == 0 ? juce::String("Accent: every instrument struck on these steps is accented.")
                               : juce::String(voiceList[(size_t) pad - 1].name));
        v->setFace(palette.selectFace, palette.selectInk);
        const int r = rowOfPad(pad);
        v->onPress = [this, r] { row = r; showRow(); };
        addAndMakeVisible(*v);
        voices.push_back(std::move(v));
    }

    for (int s = 0; s < stepsPerPattern; ++s)
    {
        steps[s].setFace(palette.stepFace(s), palette.stepInk);
        steps[s].setLedColour(palette.ledColour);
        steps[s].setText(juce::String(s + 1));
        steps[s].onPress = [this, s]
        {
            auto& pat = track.edited();
            pat.set(row, s, ! pat.get(row, s));
            showRow();
        };
        addAndMakeVisible(steps[s]);
    }

    addAndMakeVisible(patterns);
    addAndMakeVisible(strip);
    showRow();
}

// which pattern row each select pad shows: AC first, then the instruments
int DrumRack::rowOfPad(int pad) const { return pad == 0 ? DrumPattern::accentRow : pad - 1; }

void DrumRack::showRow()
{
    const auto& pat = track.edited();
    const int length = pat.len();

    for (size_t pad = 0; pad < voices.size(); ++pad)
    {
        voices[pad]->setLit(rowOfPad((int) pad) == row);
        voices[pad]->setMarked(! pat.rowIsEmpty(rowOfPad((int) pad)));
    }

    for (int s = 0; s < stepsPerPattern; ++s)
    {
        steps[s].setLit(pat.get(row, s));
        steps[s].setAlpha(s < length ? 1.0f : 0.4f);
    }
}

void DrumRack::refresh()
{
    for (auto& c : controls) refreshControl(*c);
    strip.refresh();
    showRow();
}

void DrumRack::tick(bool blinkPhase)
{
    patterns.tick(blinkPhase);
    strip.tick();
    showRow();

    const int pos = playing() && track.current == track.selected ? track.position.load() : -1;
    for (int s = 0; s < stepsPerPattern; ++s)
        steps[s].setMarked(s == pos);
}

int DrumRack::preferredHeight() const
{
    return 8 + 30 + maxKnobs * knobHeight + labelHeight + 8 + 24 + 8 + 44 + 10;
}

void DrumRack::resized()
{
    auto r = getLocalBounds();
    patterns.setBounds(r.removeFromLeft(168).withHeight(juce::jmin(getHeight(), 212)));
    r.removeFromLeft(6);
    strip.setBounds(r.removeFromRight(84));
    r.removeFromRight(6);
    face = r;

    auto inner = r.reduced(10, 8);
    inner.removeFromTop(30);   // the name

    // the knob columns, the label under each
    auto knobArea = inner.removeFromTop(maxKnobs * knobHeight + labelHeight);
    const int w = knobArea.getWidth() / juce::jmax(1, (int) columns.size());
    for (auto& col : columns)
    {
        col.area = knobArea.removeFromLeft(w);
        auto k = col.area;
        for (auto* c : col.controls)
            c->setBounds(k.removeFromTop(knobHeight).withSizeKeepingCentre(juce::jmin(w, 56), knobHeight));
    }

    inner.removeFromTop(8);
    auto select = inner.removeFromTop(24);
    const float sw = (float) select.getWidth() / (float) voices.size();
    for (size_t pad = 0; pad < voices.size(); ++pad)
        voices[pad]->setBounds(select.getX() + juce::roundToInt(sw * (float) pad), select.getY(),
                               juce::roundToInt(sw) - 2, select.getHeight());

    inner.removeFromTop(8);
    auto keys = inner.removeFromTop(juce::jmin(inner.getHeight(), 44));
    const float kw = (float) keys.getWidth() / (float) stepsPerPattern;
    for (int s = 0; s < stepsPerPattern; ++s)
        steps[s].setBounds(keys.getX() + juce::roundToInt(kw * (float) s) + 1, keys.getY(),
                           juce::roundToInt(kw) - 3, keys.getHeight());
}

void DrumRack::paint(juce::Graphics& g)
{
    theme.drawFace(g, face);
    theme.drawTitle(g, face.reduced(12, 8).removeFromTop(26));

    // a coloured band under each column's knobs, with its name, as on the hardware
    const int n = (int) columns.size();
    for (int c = 0; c < n; ++c)
    {
        const auto& col = columns[(size_t) c];
        auto label = col.area.withTrimmedTop(col.area.getHeight() - labelHeight).reduced(3, 0);
        g.setColour(palette.band(c, n));
        g.fillRoundedRectangle(label.toFloat(), 2.0f);
        g.setColour(c == 0 || c + 1 == n ? theme.ink : palette.bandInk);
        g.setFont(fonts::bold(8.5f));
        g.drawFittedText(col.label, label, juce::Justification::centred, 2, 0.7f);

        if (c > 0)
        {
            g.setColour(theme.ink.withAlpha(0.18f));
            g.fillRect(col.area.getX(), col.area.getY(), 1, col.area.getHeight() - labelHeight - 2);
        }
    }

    if (! voices.empty())
    {
        const auto first = voices[0]->getBounds();
        g.setColour(theme.inkDim);
        g.setFont(fonts::bold(8.5f));
        g.drawText("INSTRUMENT", first.getX(), first.getY() - 9, 100, 9, juce::Justification::bottomLeft);
    }
}
