// MzDebirth - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Look.h"

juce::Font fonts::digits(float size)
{
    return juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size, juce::Font::bold);
}

Look::Look()
{
    setColour(juce::PopupMenu::backgroundColourId, colours::rail);
    setColour(juce::PopupMenu::textColourId, colours::light);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::stepRed);
    setColour(juce::BubbleComponent::backgroundColourId, colours::rail);
    setColour(juce::BubbleComponent::outlineColourId, colours::screw);
    setColour(juce::TooltipWindow::textColourId, colours::light);
    setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff111214));
    setColour(juce::TooltipWindow::outlineColourId, colours::stepRed);
    setColour(juce::Slider::textBoxTextColourId, colours::light);
}

void Look::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float startAngle, float endAngle, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 5.0f;
    const auto c = bounds.getCentre();

    // the scale: eleven marks
    g.setColour(s.findColour(juce::Slider::rotarySliderOutlineColourId));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const auto dir = juce::Point<float>(std::sin(a), -std::cos(a));
        const bool major = i % 5 == 0;
        g.drawLine({ c + dir * (radius + 1.5f), c + dir * (radius + (major ? 5.0f : 3.5f)) }, major ? 1.3f : 0.8f);
    }

    // a skirt, then the cap, lit from above
    const auto cap = s.findColour(juce::Slider::rotarySliderFillColourId);
    const auto skirt = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(c);
    g.setColour(cap.darker(0.7f));
    g.fillEllipse(skirt);

    const auto top = skirt.reduced(radius * 0.18f);
    g.setGradientFill(juce::ColourGradient(cap.brighter(0.25f), top.getCentreX(), top.getY(),
                                           cap.darker(0.35f), top.getCentreX(), top.getBottom(), false));
    g.fillEllipse(top);
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawEllipse(top, 0.8f);

    // the pointer
    const float a = startAngle + (endAngle - startAngle) * sliderPos;
    const auto dir = juce::Point<float>(std::sin(a), -std::cos(a));
    g.setColour(s.findColour(juce::Slider::thumbColourId));
    g.drawLine({ c + dir * (radius * 0.2f), c + dir * (radius * 0.9f) }, 2.0f);
}

void Look::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPos, float, float,
                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    // On its side, where a strip is folded down too short for it to stand: the same slot
    // and cap, turned, without the marks there is no room for.
    if (style == juce::Slider::LinearHorizontal)
    {
        const float cy = (float) y + (float) height * 0.5f;
        const float left = (float) x, right = (float) (x + width);

        g.setColour(juce::Colour(0xff0d0d0e));
        g.fillRoundedRectangle(left - 4.0f, cy - 2.5f, (right - left) + 8.0f, 5.0f, 2.0f);

        const juce::Rectangle<float> cap(sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4b4f), cap.getX(), cap.getY(),
                                               juce::Colour(0xff17171a), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle(cap, 2.0f);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(cap, 2.0f, 1.0f);
        g.setColour(s.findColour(juce::Slider::thumbColourId));
        g.fillRect(sliderPos - 0.75f, cap.getY() + 2.0f, 1.5f, cap.getHeight() - 4.0f);
        return;
    }

    // a fader: a slot, marks either side, and a cap with a line across it
    const float cx = (float) x + (float) width * 0.5f;
    const float top = (float) y, bottom = (float) (y + height);

    g.setColour(s.findColour(juce::Slider::rotarySliderOutlineColourId));
    for (int i = 0; i <= 10; ++i)
    {
        const float ty = bottom - (bottom - top) * (float) i / 10.0f;
        const float w = i % 5 == 0 ? 6.0f : 3.5f;
        g.fillRect(cx - 9.0f - w, ty - 0.5f, w, 1.0f);
        g.fillRect(cx + 9.0f, ty - 0.5f, w, 1.0f);
    }

    g.setColour(juce::Colour(0xff0d0d0e));
    g.fillRoundedRectangle(cx - 2.5f, top - 4.0f, 5.0f, (bottom - top) + 8.0f, 2.0f);

    const juce::Rectangle<float> cap(cx - 8.0f, sliderPos - 9.0f, 16.0f, 18.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4b4f), cap.getX(), cap.getY(),
                                           juce::Colour(0xff17171a), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(cap, 2.0f, 1.0f);
    g.setColour(s.findColour(juce::Slider::thumbColourId));
    g.fillRect(cap.getX() + 2.0f, sliderPos - 0.75f, cap.getWidth() - 4.0f, 1.5f);
}
