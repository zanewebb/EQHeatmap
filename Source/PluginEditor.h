#pragma once
#include <JuceHeader.h>
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

private:
    EQHeatmapAudioProcessor& processor;
    eq::HeatTheme theme;

    // Controls
    juce::GroupComponent controlsGroup { "controlsGroup", "Visualizer Controls" };
    juce::GroupComponent bleedGroup    { "bleedGroup",    "Cell Bleed" };

    // Top panel
    juce::ToggleButton linkToSensitivity { "Link to Sensitivity" };
    juce::Slider sensitivity, lowerDb, upperDb, hotRefPct, gamma, trailMs;
    juce::Label  lblSensitivity, lblLower, lblUpper, lblHotRef, lblGamma, lblTrail;

    // Bleed panel (4 controls incl. toggle)
    juce::ToggleButton bleedEnable { "Enable Bleed" };
    juce::Slider bleedFreqWidth, bleedPanWidth, bleedDecayPct;
    juce::Label  lblBleedFreq, lblBleedPan, lblBleedDecay;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> linkAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> upperAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hotRefAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gammaAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trailAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bleedEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bleedFreqWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bleedPanWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bleedDecayPctAttachment;

    void timerCallback() override { repaint(); }  // 60 Hz

    void updateRangeEnablement();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQHeatmapAudioProcessorEditor)
};
