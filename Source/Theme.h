#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace eq
{

// Brand palette. Dark, slightly warm, with an accent that ties into the magma heatmap.
namespace Brand
{
    inline const juce::Colour bg        { 0xFF0A0B10 }; // window background
    inline const juce::Colour panel     { 0xFF14161E }; // panel surface
    inline const juce::Colour panelEdge { 0xFF1F2230 }; // panel hairline
    inline const juce::Colour grid      { 0xFF2A2E3C }; // axis gridlines
    inline const juce::Colour text      { 0xFFE8EAF0 }; // primary text
    inline const juce::Colour textDim   { 0xFF7A8093 }; // axis / hint text
    inline const juce::Colour accent    { 0xFFFF6E5C }; // hot coral (magma high end)
    inline const juce::Colour accentAlt { 0xFF7B5CFF }; // cool violet (magma mid)
}

// Perceptually-uniform-ish magma lookup. t in [0,1].
// Hand-picked stops approximated from matplotlib's magma colormap.
juce::Colour magma (float t) noexcept;

// Maps [0,1] -> [0,1] but compresses low values toward 0 and expands high values.
// Use before magma() so low-energy cells collapse to black and peaks dominate the color range.
// applyDisplayCurve(0.0)  == 0.0
// applyDisplayCurve(0.05) ≈ 0.16
// applyDisplayCurve(0.5)  ≈ 0.76
// applyDisplayCurve(1.0)  == 1.0
inline float applyDisplayCurve (float v) noexcept
{
    return std::log1p (9.0f * juce::jlimit (0.0f, 1.0f, v)) / std::log (10.0f);
}

// LookAndFeel that gives the plugin a modern, dark, audio-tool aesthetic.
class HeatTheme : public juce::LookAndFeel_V4
{
public:
    HeatTheme();

    juce::Font getLabelFont        (juce::Label&)        override;
    juce::Font getSliderPopupFont  (juce::Slider&)       override;
    juce::Font getTextButtonFont   (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont     (juce::ComboBox&)     override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawGroupComponentOutline (juce::Graphics&, int w, int h,
                                    const juce::String& text,
                                    const juce::Justification&,
                                    juce::GroupComponent&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

private:
    juce::Font baseFont (float height, bool bold = false) const;
};

} // namespace eq
