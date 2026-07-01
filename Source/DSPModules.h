#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <vector>

class ToneModule final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void updateImmediate(double sampleRate, float lowGain, float midGain, float midFreq, float highGain);
    void update(double sampleRate, float lowGain, float midGain, float midFreq, float highGain);
    void process(juce::AudioBuffer<float>& buffer);

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    using ToneChain = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter, Filter>;

    void updateChain(ToneChain& chainToUpdate,
                     double sampleRate,
                     float lowGain,
                     float midGain,
                     float midFreq,
                     float highGain);

    std::array<ToneChain, 2> chains;
    juce::SmoothedValue<float> transition;
    juce::AudioBuffer<float> transitionBuffer;
    size_t activeChainIndex = 0;
    size_t pendingChainIndex = 1;
    float lastLowGain = 0.0f;
    float lastMidGain = 0.0f;
    float lastHighGain = 0.0f;
    float lastMidFreq = 0.0f;
};

class ResonanceEqModule final
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void update(double sampleRate, float amount, float frequency);
    void process(juce::AudioBuffer<float>& buffer);

private:
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    static constexpr size_t numBands = 5;

    struct Band
    {
        Filter filter;
        float frequencyRatio = 1.0f;
        float q = 4.0f;
        float reductionDb = 0.0f;
    };

    std::array<Band, numBands> bands;
    float lastAmount = 0.0f;
    float lastFrequency = 900.0f;
    double currentSampleRate = 44100.0;
};

class DeEsserModule final
{
public:
    void prepare(double sampleRate, int maxChannels);
    void setAmount(float amount);
    void setAmountImmediate(float amount);
    float process(juce::AudioBuffer<float>& buffer);
    void skip(int numSamples);

private:
    juce::SmoothedValue<float> amountSmoothed;
    std::vector<float> sibilanceLowpassState;
    std::vector<float> airLowpassState;
    double currentSampleRate = 44100.0;
    float sibilanceDetectorLevel = 0.0f;
    float broadbandDetectorLevel = 0.0f;
    float gainReductionEnvelope = 0.0f;
};

class GlueModule final
{
public:
    void prepare(double sampleRate);
    void setAmountImmediate(float amount);
    void setAmount(float amount);
    float process(juce::AudioBuffer<float>& buffer);
    void skip(int numSamples);

private:
    juce::SmoothedValue<float> amountSmoothed;
    double currentSampleRate = 44100.0;
    float peakDetectorLevel = 0.0f;
    float rmsDetectorLevel = 0.0f;
    float gainReductionEnvelope = 0.0f;
};

class SaturationModule final
{
public:
    void prepare(double sampleRate);
    void setAmount(float amount);
    void setAmountImmediate(float amount);
    void process(juce::AudioBuffer<float>& buffer);
    void skip(int numSamples);

private:
    juce::SmoothedValue<float> amountSmoothed;
};

class WidthModule final
{
public:
    void prepare(double sampleRate);
    void setWidthImmediate(float width);
    void setWidth(float width);
    void process(juce::AudioBuffer<float>& buffer);
    void skip(int numSamples);

private:
    juce::SmoothedValue<float> widthSmoothed;
};
