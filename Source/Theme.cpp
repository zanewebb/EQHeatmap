#include "Theme.h"

namespace eq
{

// ---- Palette ----------------------------------------------------------------

juce::Colour magma (float t) noexcept
{
    t = juce::jlimit (0.0f, 1.0f, t);

    // 11 stops approximated from matplotlib's magma. Linear interpolation between stops
    // is close enough to perceptually uniform for visualization purposes and avoids the
    // green→yellow→red banding of the original palette.
    static constexpr float stops[][3] = {
        { 0.001f, 0.001f, 0.014f },
        { 0.027f, 0.014f, 0.183f },
        { 0.114f, 0.066f, 0.371f },
        { 0.244f, 0.103f, 0.526f },
        { 0.379f, 0.142f, 0.591f },
        { 0.516f, 0.171f, 0.582f },
        { 0.665f, 0.196f, 0.527f },
        { 0.819f, 0.230f, 0.439f },
        { 0.948f, 0.327f, 0.343f },
        { 0.989f, 0.518f, 0.349f },
        { 0.987f, 0.991f, 0.749f },
    };
    constexpr int n = (int) (sizeof (stops) / sizeof (stops[0]));
    const float scaled = t * (n - 1);
    const int   i0     = juce::jlimit (0, n - 2, (int) std::floor (scaled));
    const float f      = scaled - (float) i0;
    auto lerp = [] (float a, float b, float k) { return a + (b - a) * k; };
    return juce::Colour::fromFloatRGBA (
        lerp (stops[i0][0], stops[i0 + 1][0], f),
        lerp (stops[i0][1], stops[i0 + 1][1], f),
        lerp (stops[i0][2], stops[i0 + 1][2], f),
        1.0f);
}

// ---- LookAndFeel ------------------------------------------------------------

HeatTheme::HeatTheme()
{
    using LF = juce::LookAndFeel_V4;

    setColour (juce::ResizableWindow::backgroundColourId, Brand::bg);

    // Sliders
    setColour (juce::Slider::backgroundColourId,    Brand::panelEdge);
    setColour (juce::Slider::trackColourId,         Brand::accent);
    setColour (juce::Slider::thumbColourId,         Brand::text);
    setColour (juce::Slider::textBoxTextColourId,   Brand::text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);

    // Toggle buttons
    setColour (juce::ToggleButton::textColourId,        Brand::text);
    setColour (juce::ToggleButton::tickColourId,        Brand::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Brand::textDim);

    // Labels
    setColour (juce::Label::textColourId,               Brand::text);

    // Groups
    setColour (juce::GroupComponent::outlineColourId,   Brand::panelEdge);
    setColour (juce::GroupComponent::textColourId,      Brand::textDim);

    // Popup / hint
    setColour (juce::PopupMenu::backgroundColourId,     Brand::panel);
    setColour (juce::PopupMenu::textColourId,           Brand::text);
}

juce::Font HeatTheme::baseFont (float height, bool bold) const
{
   #if JUCE_VERSION >= 0x080000
    juce::FontOptions opts ("Inter", height, bold ? juce::Font::bold : juce::Font::plain);
    return juce::Font (opts);
   #else
    return juce::Font (juce::Font::getDefaultSansSerifFontName(),
                       height, bold ? juce::Font::bold : juce::Font::plain);
   #endif
}

juce::Font HeatTheme::getLabelFont        (juce::Label&)               { return baseFont (13.0f); }
juce::Font HeatTheme::getSliderPopupFont  (juce::Slider&)              { return baseFont (12.0f); }
juce::Font HeatTheme::getTextButtonFont   (juce::TextButton&, int)     { return baseFont (13.0f, true); }
juce::Font HeatTheme::getComboBoxFont     (juce::ComboBox&)            { return baseFont (13.0f); }

juce::Label* HeatTheme::createSliderTextBox (juce::Slider& slider)
{
    auto* L = juce::LookAndFeel_V4::createSliderTextBox (slider);
    L->setFont (baseFont (12.0f));
    L->setColour (juce::Label::textColourId, Brand::textDim);
    L->setJustificationType (juce::Justification::centredRight);
    return L;
}

void HeatTheme::drawLinearSlider (juce::Graphics& g,
                                  int x, int y, int width, int height,
                                  float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                  juce::Slider::SliderStyle style,
                                  juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        // Fall back to default for anything we don't custom-style.
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height,
                                                sliderPos, sliderPos, sliderPos, style, slider);
        return;
    }

    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float trackH = 4.0f;
    const float midY   = bounds.getCentreY();

    auto track = juce::Rectangle<float> (bounds.getX(), midY - trackH * 0.5f, bounds.getWidth(), trackH);

    // Background track
    g.setColour (Brand::panelEdge);
    g.fillRoundedRectangle (track, trackH * 0.5f);

    // Filled portion (accent)
    const auto fillW = juce::jlimit (0.0f, bounds.getWidth(), sliderPos - bounds.getX());
    auto fill = track.withWidth (fillW);
    auto fillColour = slider.isEnabled() ? Brand::accent : Brand::accent.withAlpha (0.35f);
    g.setColour (fillColour);
    g.fillRoundedRectangle (fill, trackH * 0.5f);

    // Thumb
    const float thumbR = 7.0f;
    auto thumb = juce::Rectangle<float> (sliderPos - thumbR, midY - thumbR, thumbR * 2.0f, thumbR * 2.0f);
    g.setColour (slider.isEnabled() ? Brand::text : Brand::textDim);
    g.fillEllipse (thumb);

    // Subtle ring
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (thumb, 1.0f);
}

void HeatTheme::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                  bool /*hi*/, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat();
    const float pillH = juce::jmin (18.0f, bounds.getHeight());
    auto pill = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - pillH * 0.5f,
                                        32.0f, pillH);
    const bool on = b.getToggleState();

    // Pill background
    g.setColour (on ? Brand::accent.withAlpha (0.85f) : Brand::panelEdge);
    g.fillRoundedRectangle (pill, pillH * 0.5f);

    // Knob
    const float knobR = pillH * 0.5f - 2.0f;
    const float knobX = on ? pill.getRight() - knobR * 2.0f - 2.0f : pill.getX() + 2.0f;
    g.setColour (Brand::text);
    g.fillEllipse (knobX, pill.getY() + 2.0f, knobR * 2.0f, knobR * 2.0f);

    // Label to the right of the pill
    auto labelBounds = bounds.withTrimmedLeft (pill.getWidth() + 8.0f);
    g.setColour (b.isEnabled() ? Brand::text : Brand::textDim);
    g.setFont (baseFont (13.0f));
    g.drawFittedText (b.getButtonText(),
                      labelBounds.toNearestInt(),
                      juce::Justification::centredLeft, 1);
}

void HeatTheme::drawGroupComponentOutline (juce::Graphics& g, int w, int h,
                                           const juce::String& text,
                                           const juce::Justification& /*pos*/,
                                           juce::GroupComponent& /*group*/)
{
    auto bounds = juce::Rectangle<float> (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f);
    const float radius = 8.0f;

    // Panel fill (slightly raised vs window bg)
    g.setColour (Brand::panel);
    g.fillRoundedRectangle (bounds, radius);

    // Hairline border
    g.setColour (Brand::panelEdge);
    g.drawRoundedRectangle (bounds, radius, 1.0f);

    // Title in the top-left corner
    g.setColour (Brand::textDim);
    g.setFont (baseFont (11.0f, true));
    g.drawText (text.toUpperCase(),
                juce::Rectangle<int> (14, 6, w - 28, 16),
                juce::Justification::centredLeft, false);
}

} // namespace eq
