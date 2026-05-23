#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <atomic>

class EQHeatmapAudioProcessor : public juce::AudioProcessor
{
public:
    // --- Heatmap resolution ---
    static constexpr int kPanBins  = 128;  // horizontal (L..R)
    static constexpr int kFreqBins = 192;  // vertical  (20 Hz .. 20 kHz)

    // Frequency range
    static constexpr float kFreqMinHz = 20.0f;
    static constexpr float kFreqMaxHz = 20000.0f;

    // FFT / latency (snappy)
    static constexpr int kFftOrder = 11;   // 2048
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kHopSize  = 256;

    EQHeatmapAudioProcessor();
    ~EQHeatmapAudioProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "EQHeatmap"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // UI accessors
    float getCellValue (int freqIdx, int panIdx) const noexcept;
    float getFreqMinHz() const noexcept { return kFreqMinHz; }
    float getFreqMaxHz() const noexcept { return kFreqMaxHz; }
    const std::vector<float>& getRowCentersHz() const noexcept { return rowCenterHz; }

    // Parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // Analysis
    double sampleRate = 44100.0;

    juce::dsp::FFT fft { kFftOrder };
    juce::HeapBlock<float> fftTimeL, fftTimeR, fftFreqL, fftFreqR, window;

    std::vector<float> fifoL, fifoR;
    int fifoWritePos = 0;

    // FFT bin -> heatmap row mapping (log scale)
    std::vector<int>   binToRow;        // size kFftSize/2+1
    std::vector<float> rowCenterHz;     // for bass-tilt weighting

    // Heatmap data
    float cellValues[kFreqBins][kPanBins] = {};
    float frameAccum[kFreqBins][kPanBins] = {};
    int   frameCount[kFreqBins][kPanBins] = {};

    // Per-frame linear intensities (0..1 before gamma/smoothing), plus scratch for bleed
    float vFrame[kFreqBins][kPanBins] = {};
    float scratch[kFreqBins][kPanBins] = {};

    // --- Visual tuning ---
    static constexpr float kBassTilt = -0.25f;  // (f/1k)^kBassTilt ; negative boosts bass
    float emaAlpha = 0.40f;                     // display smoothing

    // Cached raw parameter pointers (lock-free reads)
    std::atomic<float>* sensitivityDbParam = nullptr;  // 6..120 dB (when linked)
    std::atomic<float>* rangeLowerDbParam  = nullptr;  // -200..0 dB
    std::atomic<float>* rangeUpperDbParam  = nullptr;  // -30..+6 dB
    std::atomic<float>* linkToSensParam    = nullptr;  // 0/1
    std::atomic<float>* hotRefPctParam     = nullptr;  // 5..120 %
    std::atomic<float>* gammaParam         = nullptr;  // 0.3..2.5
    std::atomic<float>* trailMsParam       = nullptr;  // 0..2000 ms

    // Cell Bleed controls
    std::atomic<float>* bleedEnableParam   = nullptr;  // 0/1
    std::atomic<float>* bleedFreqWidthParam= nullptr;  // 0..48 bins
    std::atomic<float>* bleedPanWidthParam = nullptr;  // 0..32 bins
    std::atomic<float>* bleedDecayPctParam = nullptr;  // 0..95 %

    // Readout / HUD
    std::atomic<float>* readoutFollowCursorParam = nullptr;  // 0/1 (bool)

    void resetAnalysisBuffers();
    void buildBinToRowMap();
    void pushBlockToFifo (const juce::AudioBuffer<float>& buffer);
    void performFFTAndAccumulate();

    void applyBleed(); // uses vFrame -> scratch -> vFrame (separable exponential)

    static float clamp01 (float v) { return juce::jlimit (0.0f, 1.0f, v); }
};
