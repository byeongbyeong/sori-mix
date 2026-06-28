#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class SoriMixAudioProcessor final : public juce::AudioProcessor
{
public:
    SoriMixAudioProcessor();
    ~SoriMixAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return parameters; }
    const juce::AudioProcessorValueTreeState& getState() const noexcept { return parameters; }

    void applyAssistantCommand(const juce::String& command);

    std::atomic<float> inputLevelDb { -90.0f };
    std::atomic<float> outputLevelDb { -90.0f };
    std::atomic<float> gainReductionDb { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float readParameter(const juce::AudioProcessorValueTreeState& state, const juce::String& parameterID);
    void updateFilters();
    void setParameterValue(const juce::String& parameterID, float value);

    juce::AudioProcessorValueTreeState parameters;

    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    juce::dsp::ProcessorChain<Filter, Filter, Filter> toneChain;
    juce::dsp::Gain<float> outputGain;

    double currentSampleRate = 44100.0;
    float compressorEnvelope = 0.0f;
    float lastLowGain = 0.0f;
    float lastMidGain = 0.0f;
    float lastHighGain = 0.0f;
    float lastMidFreq = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoriMixAudioProcessor)
};
