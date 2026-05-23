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

    // Pre-allocate the source image at native cell resolution. Bilinear
    // upsampling onto the plot does the visual smoothing.
    heatmapImage = juce::Image (juce::Image::ARGB,
                                EQHeatmapAudioProcessor::kPanBins,
                                EQHeatmapAudioProcessor::kFreqBins,
                                true);

    // GPU-accelerate paint(). Safe to attach late; JUCE wires the GL render
    // pipeline transparently behind the existing Graphics API.
    openGLContext.setContinuousRepainting (false); // we repaint on our timer
    openGLContext.attachTo (*this);

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
    openGLContext.detach();
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
    // Layout: heatmap is the hero on the left, controls live in a right-side drawer.
    auto r = getLocalBounds().reduced (16);

    const int rightPanelW = 296;
    controlsPanel = r.removeFromRight (rightPanelW);
    r.removeFromRight (12); // gap between heatmap and panel
    plotArea = r;

    // Drawer column.
    auto col = controlsPanel;

    // Visualizer Controls — toggle + 6 stacked rows
    auto ctrlBox = col.removeFromTop (300);
    controlsGroup.setBounds (ctrlBox);
    auto ctrlInner = ctrlBox.reduced (14, 30);

    linkToSensitivity.setBounds (ctrlInner.removeFromTop (22));
    ctrlInner.removeFromTop (8);

    auto placeRow = [] (juce::Rectangle<int>& area, juce::Label& L, juce::Component& C)
    {
        auto row = area.removeFromTop (32);
        L.setBounds (row.removeFromLeft (110));
        C.setBounds (row.reduced (0, 4));
        area.removeFromTop (4);
    };

    placeRow (ctrlInner, lblSensitivity, sensitivity);
    placeRow (ctrlInner, lblLower,       lowerDb);
    placeRow (ctrlInner, lblUpper,       upperDb);
    placeRow (ctrlInner, lblHotRef,      hotRefPct);
    placeRow (ctrlInner, lblGamma,       gamma);
    placeRow (ctrlInner, lblTrail,       trailMs);

    col.removeFromTop (14);

    // Cell Bleed — toggle + 3 stacked rows
    auto bleedBox = col.removeFromTop (190);
    bleedGroup.setBounds (bleedBox);
    auto bInner = bleedBox.reduced (14, 30);

    bleedEnable.setBounds (bInner.removeFromTop (22));
    bInner.removeFromTop (8);

    placeRow (bInner, lblBleedFreq,  bleedFreqWidth);
    placeRow (bInner, lblBleedPan,   bleedPanWidth);
    placeRow (bInner, lblBleedDecay, bleedDecayPct);
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

    // Heatmap canvas inset: gutter on the left for frequency labels,
    // strip on the bottom for pan labels.
    const int leftLabelW = 64;
    const int bottomH    = 30;
    auto plot = plotArea.withTrimmedLeft (leftLabelW).withTrimmedBottom (bottomH);

    // Plot background — slightly darker than panels so the heat colors pop.
    g.setColour (juce::Colour { 0xFF05060A });
    g.fillRoundedRectangle (plot.toFloat(), 8.0f);

    const int cols = P::kPanBins;
    const int rows = P::kFreqBins;

    // --- Upload current cell values into the source image (rows flipped so
    //     low freqs are at the bottom). Magma colormap applied per pixel.
    {
        juce::Image::BitmapData bd (heatmapImage, juce::Image::BitmapData::writeOnly);
        for (int fy = 0; fy < rows; ++fy)
        {
            const int yDst = rows - 1 - fy;
            for (int px = 0; px < cols; ++px)
            {
                const float v = processor.getCellValue (fy, px);
                if (v <= 0.05f) {
                    bd.setPixelColour (px, yDst, juce::Colour (juce::uint32 (0)));
                    continue;
                }
                bd.setPixelColour (px, yDst, eq::magma (eq::applyDisplayCurve (v)));
            }
        }
    }

    // --- Draw the heatmap with bilinear interpolation (GPU-accelerated via the
    //     attached OpenGL context). This is the single biggest visual upgrade:
    //     the 192x128 source gets smoothly resampled instead of rendered as
    //     discrete rectangles.
    {
        juce::Graphics::ScopedSaveState save (g);
        // Clip to rounded plot rect so the heatmap respects the corner radius.
        juce::Path clip;
        clip.addRoundedRectangle (plot.toFloat(), 8.0f);
        g.reduceClipRegion (clip);

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (heatmapImage, plot.toFloat(),
                     juce::RectanglePlacement::stretchToFit, false);
    }

    // Plot border
    g.setColour (eq::Brand::panelEdge);
    g.drawRoundedRectangle (plot.toFloat(), 8.0f, 1.0f);

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

    g.setColour (eq::Brand::grid.withAlpha (0.45f));
    for (float f : kTickHz)
        g.drawLine ((float) plot.getX(), freqToY (f), (float) plot.getRight(), freqToY (f), 1.0f);

    g.setColour (eq::Brand::textDim);
    g.setFont (juce::Font (juce::FontOptions ("Inter", 10.5f, juce::Font::plain)));
    for (float f : kTickHz)
    {
        const float y = freqToY (f) - 7.0f;
        g.drawFittedText (hzLabel (f),
            juce::Rectangle<int> (plotArea.getX(), (int) y, leftLabelW - 6, 14),
            juce::Justification::centredRight, 1);
    }

    // --- pan labels ---
    const float plotW = (float) plot.getWidth();
    g.setColour (eq::Brand::textDim);
    g.setFont (juce::Font (juce::FontOptions ("Inter", 11.0f, juce::Font::plain)));
    auto drawPan = [&] (const juce::String& s, float xRatio)
    {
        const int x = (int) ((float) plot.getX() + xRatio * plotW) - 30;
        g.drawFittedText (s,
            juce::Rectangle<int> (x, plot.getBottom() + 6, 60, 14),
            juce::Justification::centred, 1);
    };
    drawPan ("L",    0.0f);
    drawPan ("-0.5", 0.25f);
    drawPan ("C",    0.5f);
    drawPan ("+0.5", 0.75f);
    drawPan ("R",    1.0f);

    // --- axis title (pan only — frequency title removed; gutter is self-explanatory) ---
    g.setColour (eq::Brand::text);
    g.setFont (juce::Font (juce::FontOptions ("Inter", 10.5f, juce::Font::bold)));
    g.drawText ("PAN",
                juce::Rectangle<int> (plot.getX(), plot.getBottom() + 20, plot.getWidth(), 12),
                juce::Justification::centred, false);
}

void EQHeatmapAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();
    const bool inPlot = plotArea.contains (p);
    const juce::Point<int> next = inPlot ? p : juce::Point<int> { -1, -1 };
    if (next != hoverPos)
    {
        hoverPos = next;
        repaint (plotArea); // cheap: only the plot rect, not the whole editor
    }
}

void EQHeatmapAudioProcessorEditor::mouseExit (const juce::MouseEvent&)
{
    if (hoverPos != juce::Point<int> { -1, -1 })
    {
        hoverPos = { -1, -1 };
        repaint (plotArea);
    }
}
