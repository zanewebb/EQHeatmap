#pragma once
#include <JuceHeader.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "Theme.h"

class EQHeatmapAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit EQHeatmapAudioProcessorEditor (EQHeatmapAudioProcessor&);
    ~EQHeatmapAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    EQHeatmapAudioProcessor& processor;
    eq::HeatTheme theme;

    // GPU-accelerate JUCE's Graphics calls (frees the CPU to spend on FFT).
    juce::OpenGLContext openGLContext;

    // Tiny source image (cols x rows). Repainted each tick from processor cell
    // values, then stretched to the plot area with high-quality bilinear
    // resampling — gives smooth cells instead of pixelated rectangles.
    juce::Image heatmapImage;

    // Cached layout rectangles, populated by resized() and consumed by paint().
    juce::Rectangle<int> plotArea, controlsPanel;

    // Hover state for HUD readout. (-1, -1) = cursor not over the plot.
    juce::Point<int> hoverPos { -1, -1 };

    // Controls
    juce::GroupComponent controlsGroup { "controlsGroup", "Visualizer Controls" };
    juce::GroupComponent bleedGroup    { "bleedGroup",    "Cell Bleed" };

    // Top panel
    juce::ToggleButton linkToSensitivity { "Link to Sensitivity" };
    juce::Slider sensitivity, lowerDb, upperDb, hotRefPct, gamma, trailMs, noiseGate, peakFocus;
    juce::Label  lblSensitivity, lblLower, lblUpper, lblHotRef, lblGamma, lblTrail, lblNoiseGate, lblPeakFocus;

    // Bleed panel (4 controls incl. toggle)
    juce::ToggleButton bleedEnable { "Enable Bleed" };
    juce::Slider bleedFreqWidth, bleedPanWidth, bleedDecayPct;
    juce::Label  lblBleedFreq, lblBleedPan, lblBleedDecay;

    juce::ToggleButton readoutFollow { "Follow Cursor" };

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> upperAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hotRefAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gammaAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trailAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseGateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> peakFocusAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bleedEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bleedFreqWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bleedPanWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bleedDecayPctAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> readoutFollowAttachment;

    // Owns the tooltip pop-up window. Must outlive any control that calls setTooltip.
    juce::TooltipWindow tooltipWindow { this, 700 };

    void timerCallback() override { repaint(); }  // 60 Hz

    void updateRangeEnablement();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQHeatmapAudioProcessorEditor)
};
