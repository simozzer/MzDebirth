// MzDebirth - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "KeyPanel.h"

#include "Processor.h"
#include "Core/Grooves.h"

using namespace debirth;

//============================================================================== KeyDisplay

KeyDisplay::KeyDisplay(Rack& r) : rack(r)
{
    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    setTooltip("The key, as Mixed In Key shows it on the Camelot wheel: A minor, B major. "
               "Drag up or down, or use the wheel, to go round; click to swap A and B.");
}

void KeyDisplay::step(int by)
{
    Key k = Key::fromIndex(rack.key.load());
    k.number = ((k.number - 1 + by) % 12 + 12) % 12 + 1;
    rack.key = k.index();
    repaint();
}

void KeyDisplay::mouseDown(const juce::MouseEvent&)
{
    dragFrom = Key::fromIndex(rack.key.load()).number;
    dragged = false;
}

void KeyDisplay::mouseDrag(const juce::MouseEvent& e)
{
    const int by = -e.getDistanceFromDragStartY() / 8;
    if (by == 0 && ! dragged) return;
    dragged = true;
    Key k = Key::fromIndex(rack.key.load());
    k.number = ((dragFrom - 1 + by) % 12 + 12) % 12 + 1;
    rack.key = k.index();
    repaint();
}

void KeyDisplay::mouseUp(const juce::MouseEvent&)
{
    if (dragged) return;
    Key k = Key::fromIndex(rack.key.load());
    k.major = ! k.major;
    rack.key = k.index();
    repaint();
}

void KeyDisplay::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    wheel += w.deltaY * 8.0f;
    const int whole = (int) wheel;
    if (whole != 0) { wheel -= (float) whole; step(whole); }
}

void KeyDisplay::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(colours::display);
    g.fillRoundedRectangle(r, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), 2.0f, 1.0f);

    const Key k = Key::fromIndex(rack.key.load());
    g.setFont(fonts::digits(r.getHeight() * 0.78f));
    g.setColour(colours::ledOff.withAlpha(0.4f));
    g.drawText("88B", r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
    g.setColour(k.major ? colours::ledGreen : colours::ledRed);
    g.drawText(k.code(), r.reduced(4.0f, 0.0f), juce::Justification::centredRight);
}

//============================================================================== KeyPanel

KeyPanel::KeyPanel(DebirthProcessor& p)
    : processor(p),
      display(p.rack),
      style("STYLE", { "ACID", "TECHNO", "ELECTRO", "DEEP" },
            [&p] { return p.rack.style.load(); }, [&p] (int v) { p.rack.style = v; })
{
    addAndMakeVisible(display);

    style.setLegendInk(colours::light);
    style.setPadWidth(68);
    addAndMakeVisible(style);

    generate.setShowsLed(false);
    generate.setTooltip("Fill the bank the Mz303 shows, on every machine, with eight new grooves in the key and "
                        "the style - grooves, fills and a breakdown, as the factory banks are laid out. "
                        "Every press writes new ones. Every machine is moved to that bank, to play them together.");
    generate.onPress = [this] { processor.generateInKey(); };
    addAndMakeVisible(generate);

    transpose.setShowsLed(false);
    transpose.setTooltip("Move the bank the Mz303 shows - its bass lines and leads, and the Mz950's notes if it is "
                         "playing a tuned sound - into the key, degree for degree. The drums are left as they are.");
    transpose.onPress = [this] { processor.transposeToKey(); };
    addAndMakeVisible(transpose);
}

void KeyPanel::tick()
{
    const int key = processor.rack.key.load();
    const int bank = processor.keyBank();
    const int bankKey = processor.rack.bankKey[bank].load();
    if (key == shownKey && bank == shownBank && bankKey == shownBankKey) return;

    shownKey = key;
    shownBank = bank;
    shownBankKey = bankKey;
    display.repaint();
    repaint();
}

void KeyPanel::refresh()
{
    style.refresh();
    shownKey = -1;
    tick();
}

void KeyPanel::resized()
{
    auto r = getLocalBounds();
    display.setBounds(r.removeFromLeft(62).withSizeKeepingCentre(62, 34).translated(0, 6));
    r.removeFromLeft(8);
    text = r.removeFromLeft(112);
    r.removeFromLeft(6);
    style.setBounds(r.removeFromLeft(70));
    r.removeFromLeft(6);
    transpose.setBounds(r.removeFromRight(78).withSizeKeepingCentre(78, 30).translated(0, 6));
    r.removeFromRight(6);
    generate.setBounds(r.removeFromRight(76).withSizeKeepingCentre(76, 30).translated(0, 6));
}

void KeyPanel::paint(juce::Graphics& g)
{
    const Key k = Key::fromIndex(processor.rack.key.load());

    g.setColour(colours::light.withAlpha(0.75f));
    g.setFont(fonts::bold(9.5f));
    g.drawText("KEY", display.getBounds().withY(display.getY() - 14).withHeight(12), juce::Justification::centredLeft);

    // the key's name, what mixes with it, and the bank's key
    auto t = text.withTrimmedTop(6);
    g.setColour(colours::light);
    g.setFont(fonts::bold(11.0f));
    g.drawText(juce::String(k.name()).toUpperCase(), t.removeFromTop(14), juce::Justification::centredLeft);

    juce::String mixes;
    const auto keys = k.compatible();
    for (size_t i = 1; i < keys.size(); ++i) mixes << keys[i].code() << "  ";
    g.setColour(colours::light.withAlpha(0.7f));
    g.setFont(fonts::bold(9.0f));
    g.drawText("MIXES WITH  " + mixes.trim(), t.removeFromTop(13), juce::Justification::centredLeft);

    const int bank = processor.keyBank();
    const int bankKey = processor.rack.bankKey[bank].load();
    const juce::String inKey = bankKey < 0 ? juce::String("?") : juce::String(Key::fromIndex(bankKey).code());
    g.setColour(bankKey == k.index() ? colours::ledGreen : colours::stepOrange);
    g.drawText("BANK " + juce::String::charToString((juce::juce_wchar) ('A' + bank)) + " IS IN " + inKey,
               t.removeFromTop(13), juce::Justification::centredLeft);
}
