#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

EQHeatmapAudioProcessor::EQHeatmapAudioProcessor()
: juce::AudioProcessor (
    BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    // cache raw parameter pointers (lock-free)
    sensitivityDbParam = apvts.getRawParameterValue ("sensitivityDb");
    rangeLowerDbParam  = apvts.getRawParameterValue ("rangeLowerDb");
    rangeUpperDbParam  = apvts.getRawParameterValue ("rangeUpperDb");
    linkToSensParam    = apvts.getRawParameterValue ("linkToSensitivity");
    hotRefPctParam     = apvts.getRawParameterValue ("hotRefPct");
    gammaParam         = apvts.getRawParameterValue ("gamma");
    trailMsParam       = apvts.getRawParameterValue ("trailMs");

    bleedEnableParam    = apvts.getRawParameterValue ("bleedEnable");
    bleedFreqWidthParam = apvts.getRawParameterValue ("bleedFreqWidth");
    bleedPanWidthParam  = apvts.getRawParameterValue ("bleedPanWidth");
    bleedDecayPctParam  = apvts.getRawParameterValue ("bleedDecayPct");

    readoutFollowCursorParam = apvts.getRawParameterValue ("readoutFollowCursor");

    fftTimeL.allocate (kFftSize, true);
    fftTimeR.allocate (kFftSize, true);
    fftFreqL.allocate (2 * kFftSize, true);
    fftFreqR.allocate (2 * kFftSize, true);
    window.allocate (kFftSize, true);

    for (int n = 0; n < kFftSize; ++n)
        window[n] = 0.5f * (1.0f - std::cos (2.0 * juce::MathConstants<double>::pi * n / (kFftSize - 1)));
}

juce::AudioProcessorValueTreeState::ParameterLayout EQHeatmapAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Core visual scaling
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "sensitivityDb", "Sensitivity (dB)",
        juce::NormalisableRange<float> (6.0f, 120.0f, 1.0f), 48.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        "linkToSensitivity", "Link to Sensitivity", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "rangeLowerDb", "Lower (dB)",
        juce::NormalisableRange<float> (-200.0f, 0.0f, 0.5f), -60.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "rangeUpperDb", "Upper (dB)",
        juce::NormalisableRange<float> (-30.0f, 6.0f, 0.5f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "hotRefPct", "Hot Ref (%)",
        juce::NormalisableRange<float> (5.0f, 120.0f, 1.0f), 80.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "gamma", "Gamma",
        juce::NormalisableRange<float> (0.3f, 2.5f, 0.01f), 1.4f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "trailMs", "Trail (ms)",
        juce::NormalisableRange<float> (0.0f, 2000.0f, 1.0f), 350.0f));

    // Cell Bleed
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        "bleedEnable", "Enable Bleed", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "bleedFreqWidth", "Freq Width (bins)",
        juce::NormalisableRange<float> (0.0f, 48.0f, 1.0f), 8.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "bleedPanWidth", "Pan Width (bins)",
        juce::NormalisableRange<float> (0.0f, 32.0f, 1.0f), 6.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "bleedDecayPct", "Bleed Decay (%)",
        juce::NormalisableRange<float> (0.0f, 95.0f, 1.0f), 50.0f));

    // Display-side filters
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "noiseGate", "Noise Gate (%)",
        juce::NormalisableRange<float> (0.0f, 50.0f, 0.5f), 5.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "peakFocus", "Peak Focus (%)",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));

    // Readout / HUD
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        "readoutFollowCursor", "Readout: Follow Cursor", true));

    return { params.begin(), params.end() };
}

bool EQHeatmapAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void EQHeatmapAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    fifoL.assign (kFftSize, 0.0f);
    fifoR.assign (kFftSize, 0.0f);
    fifoWritePos = 0;

    buildBinToRowMap();
    resetAnalysisBuffers();
}

void EQHeatmapAudioProcessor::resetAnalysisBuffers()
{
    std::fill (&cellValues[0][0], &cellValues[0][0] + kFreqBins * kPanBins, 0.0f);
}

void EQHeatmapAudioProcessor::buildBinToRowMap()
{
    const int nBins = kFftSize / 2 + 1;
    binToRow.assign (nBins, -1);
    rowCenterHz.resize (kFreqBins);

    const float fMin = kFreqMinHz;
    const float fMax = kFreqMaxHz;
    const float logSpan = std::log (fMax / fMin);

    for (int r = 0; r < kFreqBins; ++r)
    {
        const float t = (r + 0.5f) / (float) kFreqBins;
        rowCenterHz[(size_t) r] = fMin * std::exp (t * logSpan);
    }

    for (int k = 0; k < nBins; ++k)
    {
        const float freq = (sampleRate * 0.5f) * (static_cast<float>(k) / (nBins - 1));
        if (freq < fMin || freq > fMax) { binToRow[(size_t) k] = -1; continue; }

        const float t  = std::log (freq / fMin) / logSpan; // 0..1
        int row        = (int) std::floor (t * kFreqBins);
        binToRow[(size_t) k] = juce::jlimit (0, kFreqBins - 1, row);
    }
}

void EQHeatmapAudioProcessor::pushBlockToFifo (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const float* left  = buffer.getReadPointer (0);
    const float* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : left;

    int i = 0;
    while (i < numSamples)
    {
        const int space  = kFftSize - fifoWritePos;
        const int toCopy = std::min (space, numSamples - i);

        std::memcpy (fifoL.data() + fifoWritePos, left  + i, sizeof(float) * toCopy);
        std::memcpy (fifoR.data() + fifoWritePos, right + i, sizeof(float) * toCopy);

        fifoWritePos += toCopy;
        i            += toCopy;

        if (fifoWritePos >= kFftSize)
        {
            for (int n = 0; n < kFftSize; ++n)
            {
                fftTimeL[n] = fifoL[n] * window[n];
                fftTimeR[n] = fifoR[n] * window[n];
            }

            performFFTAndAccumulate();

            const int remain = kFftSize - kHopSize;
            std::memmove (fifoL.data(), fifoL.data() + kHopSize, sizeof(float) * remain);
            std::memmove (fifoR.data(), fifoR.data() + kHopSize, sizeof(float) * remain);
            fifoWritePos = remain;
        }
    }
}

void EQHeatmapAudioProcessor::applyBleed()
{
    const int panW  = (int) juce::jlimit (0.0f, 32.0f,  bleedPanWidthParam  ? bleedPanWidthParam->load()  : 0.0f);
    const int freqW = (int) juce::jlimit (0.0f, 48.0f,  bleedFreqWidthParam ? bleedFreqWidthParam->load() : 0.0f);
    const float decayPct = juce::jlimit (0.0f, 95.0f,   bleedDecayPctParam  ? bleedDecayPctParam->load()  : 50.0f);
    const float r = decayPct / 100.0f; // 0..0.95

    // --- Horizontal (pan) pass: vFrame -> scratch ---
    for (int fy = 0; fy < kFreqBins; ++fy)
    {
        for (int px = 0; px < kPanBins; ++px)
        {
            float sum = vFrame[fy][px]; // center
            // left/right neighbors
            float gain = r;
            for (int d = 1; d <= panW; ++d)
            {
                const int L = px - d;
                const int R = px + d;
                const float w = std::pow (r, (float) d); // geometric falloff
                if (L >= 0)         sum += vFrame[fy][L] * w;
                if (R < kPanBins)   sum += vFrame[fy][R] * w;
            }
            scratch[fy][px] = sum;
        }
    }

    // --- Vertical (frequency) pass: scratch -> vFrame ---
    for (int fy = 0; fy < kFreqBins; ++fy)
    {
        for (int px = 0; px < kPanBins; ++px)
        {
            float sum = scratch[fy][px]; // center
            for (int d = 1; d <= freqW; ++d)
            {
                const int Dn = fy - d;
                const int Up = fy + d;
                const float w = std::pow (r, (float) d);
                if (Dn >= 0)         sum += scratch[Dn][px] * w;
                if (Up < kFreqBins)  sum += scratch[Up][px] * w;
            }
            vFrame[fy][px] = clamp01 (sum); // clamp to 0..1 (avoid runaway)
        }
    }
}

void EQHeatmapAudioProcessor::performFFTAndAccumulate()
{
    std::memcpy (fftFreqL.getData(), fftTimeL.getData(), sizeof(float) * kFftSize);
    std::memcpy (fftFreqR.getData(), fftTimeR.getData(), sizeof(float) * kFftSize);

    fft.performRealOnlyForwardTransform (fftFreqL.getData());
    fft.performRealOnlyForwardTransform (fftFreqR.getData());

    // clear frame accumulators & vFrame
    for (int fy = 0; fy < kFreqBins; ++fy)
    {
        std::fill (frameAccum[fy], frameAccum[fy] + kPanBins, 0.0f);
        std::fill (frameCount[fy], frameCount[fy] + kPanBins, 0);
        std::fill (vFrame[fy],     vFrame[fy]     + kPanBins, 0.0f);
    }

    const int nBins = kFftSize / 2 + 1;

    // accumulate power -> (row, pan)
    for (int k = 1; k < nBins; ++k) // skip DC
    {
        const int row = binToRow[(size_t) k];
        if (row < 0) continue;

        const float reL = fftFreqL[2 * k];
        const float imL = fftFreqL[2 * k + 1];
        const float reR = fftFreqR[2 * k];
        const float imR = fftFreqR[2 * k + 1];

        const float eL = reL * reL + imL * imL;
        const float eR = reR * reR + imR * imR;
        const float eT = eL + eR;
        if (eT <= 1e-20f) continue;

        const float pan = juce::jlimit (-1.0f, 1.0f, (eR - eL) / (eT + 1e-20f));
        int panIdx = (int) std::floor (0.5f * (pan + 1.0f) * kPanBins);
        panIdx = juce::jlimit (0, kPanBins - 1, panIdx);

        const float weight = std::pow (rowCenterHz[(size_t) row] / 1000.0f, kBassTilt);
        frameAccum[row][panIdx] += eT * weight;
        frameCount[row][panIdx] += 1;
    }

    // per-frame peak
    float peak = 0.0f;
    for (int fy = 0; fy < kFreqBins; ++fy)
        for (int px = 0; px < kPanBins; ++px)
            if (frameCount[fy][px] > 0)
                peak = std::max (peak, frameAccum[fy][px] / (float) frameCount[fy][px]);
    if (peak < 1e-30f) peak = 1e-30f;

    // reference below/above peak
    float hotPct = hotRefPctParam ? hotRefPctParam->load() : 80.0f; // %
    hotPct = juce::jlimit (5.0f, 120.0f, hotPct);
    float ref = peak * (hotPct / 100.0f);
    if (ref < 1e-30f) ref = 1e-30f;

    // effective dB mapping range
    const bool linked  = linkToSensParam ? (linkToSensParam->load() >= 0.5f) : true;
    const float sensDb = sensitivityDbParam ? sensitivityDbParam->load() : 48.0f;

    float lowerDb = rangeLowerDbParam ? rangeLowerDbParam->load() : -60.0f;
    float upperDb = rangeUpperDbParam ? rangeUpperDbParam->load() : 0.0f;

    if (linked) { lowerDb = -sensDb; upperDb = 0.0f; }

    // bounds & ordering
    upperDb = juce::jlimit (-30.0f, 6.0f, upperDb);
    lowerDb = juce::jlimit (-200.0f, upperDb - 1.0f, lowerDb);

    const float spanDb = juce::jmax (1.0f, upperDb - lowerDb); // >= 1 dB

    // Map to linear 0..1 **without** gamma, store in vFrame
    for (int fy = 0; fy < kFreqBins; ++fy)
        for (int px = 0; px < kPanBins; ++px)
        {
            if (frameCount[fy][px] == 0) { vFrame[fy][px] = 0.0f; continue; }

            const float avgPower = frameAccum[fy][px] / (float) frameCount[fy][px];
            const float rel = avgPower / ref;                  // 0..∞
            float dbRel = 10.0f * std::log10 (rel + 1e-30f);   // may be > 0 if above ref
            dbRel = juce::jlimit (lowerDb, upperDb, dbRel);
            vFrame[fy][px] = clamp01 ((dbRel - lowerDb) / spanDb);
        }

    // Optional Cell Bleed (separable exponential)
    const bool doBleed = bleedEnableParam ? (bleedEnableParam->load() >= 0.5f) : false;
    if (doBleed) applyBleed();

    // gamma, trail, EMA smoothing -> cellValues
    const float gamma = juce::jlimit (0.3f, 2.5f, gammaParam ? gammaParam->load() : 0.8f);
    const float hMs = trailMsParam ? trailMsParam->load() : 350.0f;
    const float frameSec = (float) kHopSize / (float) sampleRate;
    float decay = 0.0f;
    if (hMs <= 1.0f) decay = 0.0f;
    else             decay = std::pow (0.5f, frameSec / (hMs / 1000.0f));

    for (int fy = 0; fy < kFreqBins; ++fy)
    {
        for (int px = 0; px < kPanBins; ++px)
        {
            float v = vFrame[fy][px];
            if (v > 0.0f)
            {
                v = std::pow (v, gamma); // perceptual shaping
                cellValues[fy][px] = (1.0f - emaAlpha) * cellValues[fy][px] + emaAlpha * v;
            }
            else
            {
                cellValues[fy][px] *= decay;
            }
        }
    }
}

void EQHeatmapAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) return;
    pushBlockToFifo (buffer);
}

float EQHeatmapAudioProcessor::getCellValue (int fy, int px) const noexcept
{
    if (fy < 0 || fy >= kFreqBins || px < 0 || px >= kPanBins) return 0.0f;
    return cellValues[fy][px];
}

void EQHeatmapAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::MemoryOutputStream mos (destData, true);
    state.writeToStream (mos);
}

void EQHeatmapAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid()) apvts.replaceState (tree);
}

juce::AudioProcessorEditor* EQHeatmapAudioProcessor::createEditor()
{
    return new EQHeatmapAudioProcessorEditor (*this);
}

// JUCE factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQHeatmapAudioProcessor();
}
