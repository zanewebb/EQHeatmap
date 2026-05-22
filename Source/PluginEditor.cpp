#include "PluginEditor.h"

// PulseAudio-like tick frequencies
static const float kTickHz[] = {
    50.0f, 100.0f, 156.0f, 220.0f, 311.0f, 440.0f, 622.0f, 880.0f,
    1250.0f, 1750.0f, 2500.0f, 3500.0f, 5000.0f, 7000.0f, 10000.0f, 20000.0f
};

EQHeatmapAudioProcessorEditor::EQHeatmapAudioProcessorEditor (EQHeatmapAudioProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&theme);
    setSize (1100, 760);

    addAndMakeVisible (controlsGroup);
    addAndMakeVisible (bleedGroup);

    // --- helpers ---
    auto prepSlider = [] (juce::Slider& s, double min, double max, double step, const juce::String& suffix)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setRange (min, max, step);
        if (suffix.isNotEmpty()) s.setTextValueSuffix (" " + suffix);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    };
    auto prepLabel = [] (juce::Label& L, const juce::String& text)
    {
        L.setText (text, juce::dontSendNotification);
        L.setJustificationType (juce::Justification::centredLeft);
    };

    // --- top controls ---
    addAndMakeVisible (linkToSensitivity);
    prepSlider (sensitivity, 6.0, 120.0, 1.0, "dB");
    prepSlider (lowerDb,    -200.0,   0.0, 0.5, "dB");
    prepSlider (upperDb,     -30.0,   6.0, 0.5, "dB");
    prepSlider (hotRefPct,     5.0, 120.0, 1.0, "%");
    prepSlider (gamma,         0.3,   2.5, 0.01, "");
    prepSlider (trailMs,       0.0, 2000.0, 1.0, "ms");

    addAndMakeVisible (sensitivity);
    addAndMakeVisible (lowerDb);
    addAndMakeVisible (upperDb);
    addAndMakeVisible (hotRefPct);
    addAndMakeVisible (gamma);
    addAndMakeVisible (trailMs);

    prepLabel (lblSensitivity, "Sensitivity (dB)");
    prepLabel (lblLower,       "Lower (dB)");
    prepLabel (lblUpper,       "Upper (dB)");
    prepLabel (lblHotRef,      "Hot Ref (%)");
    prepLabel (lblGamma,       "Gamma");
    prepLabel (lblTrail,       "Trail (ms)");

    addAndMakeVisible (lblSensitivity);
    addAndMakeVisible (lblLower);
    addAndMakeVisible (lblUpper);
    addAndMakeVisible (lblHotRef);
    addAndMakeVisible (lblGamma);
    addAndMakeVisible (lblTrail);

    // --- bleed controls ---
    addAndMakeVisible (bleedEnable);
    prepSlider (bleedFreqWidth, 0.0, 48.0, 1.0, "bins");
    prepSlider (bleedPanWidth,  0.0, 32.0, 1.0, "bins");
    prepSlider (bleedDecayPct,  0.0, 95.0, 1.0, "%");

    addAndMakeVisible (bleedFreqWidth);
    addAndMakeVisible (bleedPanWidth);
    addAndMakeVisible (bleedDecayPct);

    prepLabel (lblBleedFreq,  "Freq Width");
    prepLabel (lblBleedPan,   "Pan Width");
    prepLabel (lblBleedDecay, "Bleed Decay (%)");

    addAndMakeVisible (lblBleedFreq);
    addAndMakeVisible (lblBleedPan);
    addAndMakeVisible (lblBleedDecay);

    // --- APVTS attachments ---
    linkAttachment        = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "linkToSensitivity", linkToSensitivity);
    sensitivityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "sensitivityDb", sensitivity);
    lowerAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "rangeLowerDb", lowerDb);
    upperAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "rangeUpperDb", upperDb);
    hotRefAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "hotRefPct", hotRefPct);
    gammaAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "gamma", gamma);
    trailAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "trailMs", trailMs);

    bleedEnableAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "bleedEnable", bleedEnable);
    bleedFreqWidthAttachment= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "bleedFreqWidth", bleedFreqWidth);
    bleedPanWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "bleedPanWidth",  bleedPanWidth);
    bleedDecayPctAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "bleedDecayPct",  bleedDecayPct);

    linkToSensitivity.onClick = [this] { updateRangeEnablement(); };
    updateRangeEnablement();

    startTimerHz (60);
}

EQHeatmapAudioProcessorEditor::~EQHeatmapAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void EQHeatmapAudioProcessorEditor::updateRangeEnablement()
{
    const bool linked = linkToSensitivity.getToggleState();
    lowerDb.setEnabled (!linked);
    upperDb.setEnabled (!linked);
}

void EQHeatmapAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (12);

    // --- Controls group (top) ---
    auto ctrlArea = r.removeFromTop (148);
    controlsGroup.setBounds (ctrlArea);

    auto inner = ctrlArea.reduced (10, 24); // inside the frame
    const int colW = inner.getWidth() / 2;
    auto leftCol  = inner.removeFromLeft (colW);
    auto rightCol = inner;

    auto lineH = 24;
    auto placeRow = [lineH](juce::Rectangle<int>& col, juce::Label& L, juce::Component& C)
    {
        auto row = col.removeFromTop (lineH).reduced (0, 2);
        auto lab = row.removeFromLeft (120);
        L.setBounds (lab);
        C.setBounds (row);
    };

    // link toggle at top-left
    linkToSensitivity.setBounds (ctrlArea.getX() + 14, ctrlArea.getY() + 4, 180, 18);

    // left column
    placeRow (leftCol,  lblSensitivity, sensitivity);
    placeRow (leftCol,  lblLower,       lowerDb);
    placeRow (leftCol,  lblHotRef,      hotRefPct);

    // right column
    placeRow (rightCol, lblUpper,       upperDb);
    placeRow (rightCol, lblGamma,       gamma);
    placeRow (rightCol, lblTrail,       trailMs);

    // --- Bleed group (second row) ---
    auto bleedArea = r.removeFromTop (96);
    bleedGroup.setBounds (bleedArea);

    auto bInner = bleedArea.reduced (10, 24);
    auto bLeft  = bInner.removeFromLeft (bInner.getWidth() / 2);
    auto bRight = bInner;

    bleedEnable.setBounds (bleedArea.getX() + 14, bleedArea.getY() + 4, 150, 18);

    placeRow (bLeft,  lblBleedFreq,  bleedFreqWidth);
    placeRow (bLeft,  lblBleedPan,   bleedPanWidth);
    placeRow (bRight, lblBleedDecay, bleedDecayPct);
}

static juce::String hzLabel (float f)
{
    if (f >= 1000.0f) return juce::String (f / 1000.0f, (f >= 10000.0f ? 0 : 2)) + " k";
    return juce::String (std::round (f));
}

void EQHeatmapAudioProcessorEditor::paint (juce::Graphics& g)
{
    using P = EQHeatmapAudioProcessor;
    g.fillAll (eq::Brand::bg);

    auto full = getLocalBounds().reduced (12);

    // plotting area lives below the two control panels
    const int panelsH = 148 + 96;
    auto plotBounds = full.withTrimmedTop (panelsH + 8);

    // Heatmap canvas inset: gutter on the left for frequency labels, strip on the bottom for pan labels
    const int leftLabelW = 96;
    const int bottomH    = 28;
    auto plot = plotBounds.withTrimmedLeft (leftLabelW).withTrimmedBottom (bottomH);

    // Plot background — slightly darker than panels so the heat colors pop
    g.setColour (juce::Colour { 0xFF05060A });
    g.fillRoundedRectangle (plot.toFloat(), 6.0f);

    const int cols = P::kPanBins;
    const int rows = P::kFreqBins;
    const float cw = (float) plot.getWidth()  / (float) cols;
    const float ch = (float) plot.getHeight() / (float) rows;

    // --- heatmap (magma palette) ---
    for (int fy = 0; fy < rows; ++fy)
        for (int px = 0; px < cols; ++px)
        {
            const float v = processor.getCellValue (fy, px);
            if (v <= 0.001f) continue; // leave plot background showing through
            auto cell = juce::Rectangle<float> (
                (float) plot.getX() + (float) px * cw,
                (float) plot.getY() + (float) (rows - 1 - fy) * ch,
                cw, ch);
            g.setColour (eq::magma (v));
            g.fillRect (cell);
        }

    // Plot border
    g.setColour (eq::Brand::panelEdge);
    g.drawRoundedRectangle (plot.toFloat(), 6.0f, 1.0f);

    // --- frequency gridlines + tick labels ---
    const float fMin = processor.getFreqMinHz();
    const float fMax = processor.getFreqMaxHz();
    const float logSpan = std::log (fMax / fMin);

    auto freqToY = [&] (float f) -> float
    {
        float ff = juce::jlimit (fMin, fMax, f);
        float t = std::log (ff / fMin) / logSpan;
        return (float) plot.getBottom() - t * (float) plot.getHeight();
    };

    g.setColour (eq::Brand::grid.withAlpha (0.55f));
    for (float f : kTickHz)
        g.drawLine ((float) plot.getX(), freqToY (f), (float) plot.getRight(), freqToY (f), 1.0f);

    g.setColour (eq::Brand::textDim);
    g.setFont (juce::Font (juce::FontOptions ("Inter", 11.0f, juce::Font::plain)));
    for (float f : kTickHz)
    {
        const float y = freqToY (f) - 7.0f;
        g.drawFittedText (hzLabel (f),
            juce::Rectangle<int> (plotBounds.getX() + 4, (int) y, leftLabelW - 10, 14),
            juce::Justification::centredRight, 1);
    }

    // --- pan labels ---
    g.setColour (eq::Brand::textDim);
    for (int px = 0; px <= cols; px += 32)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (float) px / (float) cols);
        const float pan = -1.0f + 2.0f * t;
        juce::String s = (px == 0) ? "L"
                                   : (px == cols / 2) ? "C"
                                                      : (px >= cols ? "R"
                                                                    : juce::String (pan, 2));
        g.drawFittedText (s,
            juce::Rectangle<int> ((int) ((float) plot.getX() + (float) px * cw) - 24,
                                  plot.getBottom() + 4, 48, bottomH - 8),
            juce::Justification::centred, 1);
    }

    // --- axis titles ---
    g.setColour (eq::Brand::text);
    g.setFont (juce::Font (juce::FontOptions ("Inter", 12.0f, juce::Font::bold)));
    g.drawText ("PAN  ( L  <-  ->  R )",
                juce::Rectangle<int> (plot.getX(), plot.getBottom() + 4, plot.getWidth(), bottomH - 4),
                juce::Justification::centredBottom, false);
    {
        juce::Graphics::ScopedSaveState save (g);
        const float cx = (float) plotBounds.getX() + 14.0f;
        const float cy = (float) plot.getCentreY();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, cx, cy));
        g.drawText ("FREQUENCY  ( log Hz )",
                    juce::Rectangle<int> ((int) cx - 80,
                                          (int) (cy - plot.getHeight() / 2),
                                          160, plot.getHeight()),
                    juce::Justification::centred, false);
    }
}
