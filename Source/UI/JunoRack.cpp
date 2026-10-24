// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "JunoRack.h"

using namespace debirth;

namespace
{
    const juce::Colour lcd { 0xffff8a4c };

    void styleBox(juce::ComboBox& b)
    {
        b.setColour(juce::ComboBox::backgroundColourId, colours::display);
        b.setColour(juce::ComboBox::textColourId, lcd);
        b.setColour(juce::ComboBox::arrowColourId, lcd);
        b.setColour(juce::ComboBox::outlineColourId, juce::Colours::black);
    }

    constexpr int factoryId = 1, loadedId = 2;
}

JunoRack::JunoRack(Mz106& j, Channel& c, std::function<bool()> p, const Theme& t)
    : juno(j), playing(p), theme(t), patterns(j.track, c, p), roll(j.track), strip(c, t.name.toUpperCase())
{
    addAndMakeVisible(patterns);
    patterns.onChange = [this] { roll.repaint(); };

    styleBox(bankBox);
    bankBox.setTooltip("The bank: the 128 patches Mz106 comes with, or a .syx you loaded.");
    bankBox.onChange = [this]
    {
        if (bankBox.getSelectedId() == factoryId && ! juno.isFactory())
        {
            juno.loadFactory();
            juno.knobs.reset();
            message.clear();
            refresh();
        }
    };
    addAndMakeVisible(bankBox);

    loadSyx.setShowsLed(false);
    loadSyx.setTooltip("Open Juno-106 patches (.syx) - a bank, or a single patch, which goes into its own slot.");
    loadSyx.onPress = [this] { chooseSyx(); };
    addAndMakeVisible(loadSyx);

    styleBox(programBox);
    programBox.setTooltip("The patch to play: groups A and B, banks 1 to 8, patches 1 to 8, as a 106 numbers them.");
    programBox.onChange = [this]
    {
        const int i = programBox.getSelectedItemIndex();
        if (i >= 0 && i != juno.program() && juno.selectProgram(i))
        {
            juno.knobs.reset();
            follow();
        }
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

    for (int i = 0; i < juno.knobs.size(); ++i)
    {
        controls.push_back(makeControl(juno.knobs, i, theme));
        addAndMakeVisible(*controls.back());
    }

    fold.setInk(t.ink);
    fold.onToggle = [this] { roll.setVisible(fold.isOpen()); if (onFold) onFold(); };
    addAndMakeVisible(fold);
    addAndMakeVisible(roll);
    addAndMakeVisible(strip);

    roll.setSpans({ { 0, NotePattern::notes - 1, "" } });   // every key plays
    roll.setLowest(48);
    showBank();
}

void JunoRack::chooseSyx()
{
    auto folder = juce::File(juce::String(juno.syxPath())).getParentDirectory();
    if (! folder.isDirectory()) folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser>("Open Juno-106 patches", folder, "*.syx;*.SYX");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;

            std::string error;
            if (! juno.loadSyx(file.getFullPathName().toStdString(), error))
                message = "NO JUNO-106 PATCHES IN IT";
            else
            {
                juno.knobs.reset();
                message.clear();
            }
            refresh();
        });
}

void JunoRack::showBank()
{
    bankBox.clear(juce::dontSendNotification);
    bankBox.addItem("FACTORY", factoryId);
    if (! juno.isFactory())
    {
        bankBox.addItem(juce::String(juno.bankName()), loadedId);
        bankBox.setSelectedId(loadedId, juce::dontSendNotification);
    }
    else
        bankBox.setSelectedId(factoryId, juce::dontSendNotification);

    programBox.clear(juce::dontSendNotification);
    const auto names = juno.programNames();
    for (size_t i = 0; i < names.size(); ++i)
        programBox.addItem(juce::String(names[i]), (int) i + 1);
    programBox.setSelectedItemIndex(juno.program(), juce::dontSendNotification);

    if (message.isNotEmpty()) programBox.setTooltip(message);
    repaint();
}

void JunoRack::follow()
{
    for (auto& c : controls) refreshControl(*c);
    strip.refresh();
}

void JunoRack::refresh()
{
    follow();
    showBank();
}

void JunoRack::tick(bool blinkPhase)
{
    patterns.tick(blinkPhase);
    strip.tick();
    roll.tick(playing());
}

void JunoRack::resized()
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

    auto row = inner.removeFromTop(28);
    bankBox.setBounds(row.removeFromLeft(200));
    row.removeFromLeft(8);
    loadSyx.setBounds(row.removeFromLeft(96));
    row.removeFromLeft(8);
    programBox.setBounds(row.removeFromLeft(220));
    octaveUp.setBounds(row.removeFromRight(64));
    row.removeFromRight(6);
    octaveDown.setBounds(row.removeFromRight(64));

    inner.removeFromTop(8);
    auto knobs = inner.removeFromTop(64);
    const int w = juce::jmin(72, knobs.getWidth() / juce::jmax(1, (int) controls.size()));
    for (auto& c : controls)
        c->setBounds(knobs.removeFromLeft(w));

    inner.removeFromTop(8);
    fold.setBounds(inner.removeFromTop(FoldBar::height));
    if (folded) return;
    inner.removeFromTop(2);
    roll.setBounds(inner);
}

void JunoRack::setFolded(bool f)
{
    if (f == isFolded()) return;
    fold.setOpen(! f);
    roll.setVisible(! f);
}

void JunoRack::paint(juce::Graphics& g)
{
    theme.drawFace(g, face);
    theme.drawTitle(g, face.reduced(12, 8).removeFromTop(26));
}
