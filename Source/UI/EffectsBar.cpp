// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "EffectsBar.h"

using namespace debirth;

//============================================================================== PcfDisplay

PcfDisplay::PcfDisplay(Rack& r) : rack(r)
{
    setTooltip("The PCF pattern: on each step the filter is opened by that much, and closes again by DEC.");
}

void PcfDisplay::tick()
{
    const int pattern = (int) rack.fx.get(rack.fx.indexOf("pcfPattern"));
    const int step = rack.pcfStep();
    if (pattern == shownPattern && step == shownStep) return;
    shownPattern = pattern;
    shownStep = step;
    repaint();
}

void PcfDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::display);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);

    const auto inner = r.reduced(4.0f);
    const float w = inner.getWidth() / (float) stepsPerPattern;
    for (int s = 0; s < stepsPerPattern; ++s)
    {
        const float level = pcfLevel(shownPattern, s);
        const float h = juce::jmax(1.5f, inner.getHeight() * level);
        const juce::Rectangle<float> bar(inner.getX() + w * (float) s + 1.0f, inner.getBottom() - h, w - 2.0f, h);
        g.setColour(s == shownStep ? colours::ledRed.brighter(0.3f) : colours::ledRed.withAlpha(s % 4 == 0 ? 0.75f : 0.5f));
        g.fillRect(bar);
    }
}

//============================================================================== EffectsBar

EffectsBar::EffectsBar(Rack& r)
    : rack(r),
      theme { "", "", colours::mixer, colours::light, colours::light.withAlpha(0.6f), colours::light,
              juce::Colour(0xff15181d), colours::stepOrange },
      pcfDisplay(r)
{
    auto& fx = rack.fx;

    pcfPattern = std::make_unique<LedNumber>(1, pcfPatterns, 1, 2,
        [&fx] { return (int) fx.get(fx.indexOf("pcfPattern")) + 1; },
        [&fx] (int v) { fx.set(fx.indexOf("pcfPattern"), (float) (v - 1)); });
    pcfPattern->setTooltip("Which of the 32 PCF patterns plays the filter. Drag up or down, or use the wheel.");
    addAndMakeVisible(*pcfPattern);
    addAndMakeVisible(pcfDisplay);

    delaySteps = std::make_unique<LedNumber>(1, stepsPerPattern, 3, 2,
        [&fx] { return (int) fx.get(fx.indexOf("delaySteps")); },
        [&fx] (int v) { fx.set(fx.indexOf("delaySteps"), (float) v); });
    delaySteps->setTooltip("The delay's time, in sixteenths at the rack's tempo. Drag up or down, or use the wheel.");
    addAndMakeVisible(*delaySteps);

    units = {
        { "DIST", { control("distAmount"), control("distShape") }, {}, 150 },
        { "PCF",  { pcfPattern.get(), &pcfDisplay, control("pcfFreq"), control("pcfQ"), control("pcfAmount"),
                    control("pcfDecay"), control("pcfMode") }, {}, 470 },
        { "DELAY", { delaySteps.get(), toggle("delayTriplet", "3", "Triplet: two thirds of the steps."),
                     control("delayFeedback"), control("delayPan") }, {}, 290 },
        { "COMP", { control("compRatio"), control("compThreshold"),
                    toggle("compMaster", "MASTER", "The whole mix through the COMP, as well as the channels switched into it.") },
          {}, 230 },
    };
}

juce::Component* EffectsBar::control(const char* id)
{
    auto c = makeControl(rack.fx, rack.fx.indexOf(id), theme);
    if (auto* k = dynamic_cast<Knob*>(c.get()))
    {
        const juce::String s(id);
        if (s == "compRatio")          k->setValueText([] (double v) { return juce::String(v, 1) + ":1"; });
        else if (s == "compThreshold") k->setValueText([] (double v) { return juce::String(v, 1) + " dB"; });
        else if (s == "delayPan")      k->setValueText([] (double v)
        {
            const int p = juce::roundToInt(v * 100.0);
            return p == 0 ? juce::String("C") : (p < 0 ? "L" + juce::String(-p) : "R" + juce::String(p));
        });
        else k->setValueText([] (double v) { return juce::String(juce::roundToInt(v * 100.0)); });
    }
    addAndMakeVisible(*c);
    owned.push_back(std::move(c));
    return owned.back().get();
}

Pad* EffectsBar::toggle(const char* id, const char* text, const char* tip)
{
    auto pad = std::make_unique<Pad>(text);
    const int i = rack.fx.indexOf(id);
    auto* p = pad.get();
    p->setTooltip(tip);
    p->setLedColour(colours::ledGreen);
    p->getProperties().set("width", juce::String(text).length() > 1 ? 64 : 34);
    p->onPress = [this, i, p]
    {
        rack.fx.set(i, rack.fx.get(i) > 0.5f ? 0.0f : 1.0f);
        p->setLit(rack.fx.get(i) > 0.5f);
    };
    toggles.emplace_back(p, i);
    addAndMakeVisible(*p);
    owned.push_back(std::move(pad));
    return p;
}

void EffectsBar::tick()
{
    pcfDisplay.tick();
    pcfPattern->repaint();
    delaySteps->repaint();
}

void EffectsBar::refresh()
{
    for (auto& c : owned) refreshControl(*c);
    for (auto& [pad, i] : toggles) pad->setLit(rack.fx.get(i) > 0.5f);
    pcfDisplay.tick();
    repaint();
}

void EffectsBar::resized()
{
    auto r = getLocalBounds();

    int total = 0;
    for (auto& u : units) total += u.width;
    const int gap = 6;
    const float scale = (float) (r.getWidth() - gap * ((int) units.size() - 1)) / (float) total;

    for (auto& u : units)
    {
        u.area = r.removeFromLeft(juce::roundToInt((float) u.width * scale));
        r.removeFromLeft(gap);

        auto inner = u.area.reduced(8, 6);
        inner.removeFromTop(18);   // the name
        const int y = inner.getY(), h = inner.getHeight();

        // a readout, a toggle or the pattern take their own widths; the knobs share the rest
        int x = inner.getX();
        for (auto* p : u.parts)
        {
            if (p == pcfPattern.get() || p == delaySteps.get())
            {
                p->setBounds(x, y + 14, 40, 30);
                x += 46;
            }
            else if (p == &pcfDisplay)
            {
                p->setBounds(x, y + 6, 120, h - 12);
                x += 128;
            }
            else if (dynamic_cast<Pad*>(p) != nullptr)
            {
                const int w = (int) p->getProperties()["width"];
                p->setBounds(x, y + 14, w, 28);
                x += w + 6;
            }
            else
            {
                p->setBounds(x, y, 54, h);
                x += 56;
            }
        }
    }
}

void EffectsBar::paint(juce::Graphics& g)
{
    for (const auto& u : units)
    {
        theme.drawFace(g, u.area);

        auto tag = u.area.reduced(8, 6).removeFromTop(15).withWidth(60);
        g.setColour(colours::mixerEdge);
        g.fillRoundedRectangle(tag.toFloat(), 2.0f);
        g.setColour(colours::light);
        g.setFont(fonts::bold(10.5f));
        g.drawText(u.name, tag, juce::Justification::centred);

        if (juce::String(u.name) == "PCF" || juce::String(u.name) == "DELAY")
        {
            const auto* led = juce::String(u.name) == "PCF" ? (juce::Component*) pcfPattern.get() : delaySteps.get();
            g.setColour(colours::light.withAlpha(0.75f));
            g.setFont(fonts::bold(9.0f));
            g.drawText(juce::String(u.name) == "PCF" ? "PATTERN" : "STEPS",
                       led->getBounds().withY(led->getY() - 13).withHeight(12).withWidth(60), juce::Justification::centredLeft);
        }
    }
}
