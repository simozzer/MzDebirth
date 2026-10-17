// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace colours
{
    // the rack
    const juce::Colour wood      { 0xff5b3a21 };
    const juce::Colour woodGrain { 0xff4a2f1a };
    const juce::Colour rail      { 0xff26272b };
    const juce::Colour screw     { 0xff8d8f93 };

    // the transport and the mixer
    const juce::Colour metal     { 0xff3a3d44 };
    const juce::Colour mixer     { 0xff3c4f63 };
    const juce::Colour mixerEdge { 0xff22303e };
    const juce::Colour display   { 0xff160606 };
    const juce::Colour ledRed    { 0xffff3b22 };
    const juce::Colour ledOff    { 0xff4a1712 };
    const juce::Colour ledGreen  { 0xff52e05a };

    // the 303
    const juce::Colour silver    { 0xffc6c7c3 };
    const juce::Colour silverDim { 0xffadaeaa };
    const juce::Colour ink       { 0xff141414 };
    const juce::Colour inkDim    { 0xff5a5c5f };

    // the 808
    const juce::Colour charcoal  { 0xff2c2b29 };
    const juce::Colour stepRed   { 0xffd8321f };
    const juce::Colour stepOrange{ 0xffef7d22 };
    const juce::Colour stepYellow{ 0xfff1c420 };
    const juce::Colour stepWhite { 0xffe9e4d6 };
    const juce::Colour cream     { 0xffe9e4d6 };

    // the pads
    const juce::Colour pad       { 0xff1b1b1d };
    const juce::Colour light     { 0xffe9e9e6 };
}

//
// Knobs and faders. A knob takes its cap and pointer from the slider's own colours, so one
// look serves the 303's black knobs on silver and the 808's on charcoal:
//
//   rotarySliderFillColourId     the cap
//   thumbColourId                the pointer
//   rotarySliderOutlineColourId  the scale round it
//
class Look final : public juce::LookAndFeel_V4
{
public:
    Look();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    int getSliderThumbRadius(juce::Slider&) override { return 8; }
};

namespace fonts
{
    inline juce::Font bold(float size)  { return juce::FontOptions(size, juce::Font::bold); }
    inline juce::Font plain(float size) { return juce::FontOptions(size); }
    juce::Font digits(float size);
}
