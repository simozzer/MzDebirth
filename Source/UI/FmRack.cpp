// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "FmRack.h"

#include "DX7/Banks.h"

using namespace debirth;

namespace
{
    const juce::Colour lcd { 0xff9fd27a };

    void styleBox(juce::ComboBox& b)
    {
        b.setColour(juce::ComboBox::backgroundColourId, colours::display);
        b.setColour(juce::ComboBox::textColourId, lcd);
        b.setColour(juce::ComboBox::arrowColourId, lcd);
        b.setColour(juce::ComboBox::outlineColourId, juce::Colours::black);
    }
}

FmRack::FmRack(Mz7& f, Channel& c, std::function<bool()> p, const Theme& t)
    : fm(f), playing(p), theme(t), patterns(f.track, c, p), roll(f.track), strip(c, t.name.toUpperCase())
{
    addAndMakeVisible(patterns);
    patterns.onChange = [this] { roll.repaint(); };

    styleBox(bankBox);
    bankBox.setTooltip("The cartridge: one of the four Mz7 comes with, or a .syx you loaded.");
    bankBox.onChange = [this]
    {
        const int id = bankBox.getSelectedId();
        if (id >= 1 && id <= ::banks::count() && id - 1 != fm.bank())
        {
            fm.loadBank(id - 1);
            fm.knobs.reset();
            message.clear();
            refresh();
        }
    };
    addAndMakeVisible(bankBox);

    loadSyx.setShowsLed(false);
    loadSyx.setTooltip("Open a DX7 cartridge (.syx) - any of the thousands made for the DX7 will do.");
    loadSyx.onPress = [this] { chooseSyx(); };
    addAndMakeVisible(loadSyx);

    styleBox(programBox);
    programBox.setTooltip("The voice on the cartridge to play.");
    programBox.onChange = [this]
    {
        const int i = programBox.getSelectedItemIndex();
        if (i >= 0 && i != fm.program() && fm.selectProgram(i))
        {
            fm.knobs.reset();
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

    for (int i = 0; i < fm.knobs.size(); ++i)
    {
        controls.push_back(makeControl(fm.knobs, i, theme));
        addAndMakeVisible(*controls.back());
    }

    fold.setInk(t.ink);
    fold.onToggle = [this] { roll.setVisible(fold.isOpen()); if (onFold) onFold(); };
    addAndMakeVisible(fold);
    addAndMakeVisible(roll);
    addAndMakeVisible(strip);

    roll.setSpans({ { 0, NotePattern::notes - 1, "" } });   // every key plays
    roll.setLowest(48);
    showCartridge();
}

void FmRack::chooseSyx()
{
    auto folder = juce::File(juce::String(fm.syxPath())).getParentDirectory();
    if (! folder.isDirectory()) folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser>("Open a DX7 cartridge", folder, "*.syx;*.SYX");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File()) return;

            std::string error;
            if (! fm.loadSyx(file.getFullPathName().toStdString(), error))
                message = "NOT A DX7 CARTRIDGE";
            else
            {
                fm.knobs.reset();
                message = juce::String(error).toUpperCase();
            }
            refresh();
        });
}

void FmRack::showCartridge()
{
    bankBox.clear(juce::dontSendNotification);
    for (int b = 0; b < ::banks::count(); ++b)
        bankBox.addItem(juce::String(b + 1) + "  " + juce::String(::banks::name(b)), b + 1);
    if (fm.bank() < 0)
    {
        bankBox.addItem(juce::String(fm.cartridgeName()), 100);
        bankBox.setSelectedId(100, juce::dontSendNotification);
    }
    else
        bankBox.setSelectedId(fm.bank() + 1, juce::dontSendNotification);

    programBox.clear(juce::dontSendNotification);
    const auto names = fm.programNames();
    for (size_t i = 0; i < names.size(); ++i)
        programBox.addItem(juce::String(i + 1).paddedLeft('0', 2) + "  " + juce::String(names[i]), (int) i + 1);
    programBox.setSelectedItemIndex(fm.program(), juce::dontSendNotification);

    if (message.isNotEmpty()) programBox.setTooltip(message);
    repaint();
}

void FmRack::follow()
{
    for (auto& c : controls) refreshControl(*c);
    strip.refresh();
}

void FmRack::refresh()
{
    follow();
    showCartridge();
}

void FmRack::tick(bool blinkPhase)
{
    patterns.tick(blinkPhase);
    strip.tick();
    roll.tick(playing());
}

void FmRack::resized()
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

void FmRack::setFolded(bool f)
{
    if (f == isFolded()) return;
    fold.setOpen(! f);
    roll.setVisible(! f);
}

void FmRack::paint(juce::Graphics& g)
{
    theme.drawFace(g, face);
    theme.drawTitle(g, face.reduced(12, 8).removeFromTop(26));
}
