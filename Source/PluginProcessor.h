#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "AssistantTypes.h"
#include "ChainEngine.h"
#include "DSPModules.h"

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
    void applyAssistantPlan(const AssistantParameterPlan& plan);

    std::atomic<float> inputLevelDb { -90.0f };
    std::atomic<float> outputLevelDb { -90.0f };
    std::atomic<float> gainReductionDb { 0.0f };

private:
    enum class ChainTransitionState
    {
        stable,
        fadingOut,
        fadingIn
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float readParameter(const juce::AudioProcessorValueTreeState& state, const juce::String& parameterID);
    ChainEngine::Order readStageOrder() const;
    void updateFilters();
    void setParameterValue(const juce::String& parameterID, float value);

    juce::AudioProcessorValueTreeState parameters;

    ToneModule toneModule;
    ResonanceEqModule resonanceEqModule;
    DeEsserModule deEsserModule;
    GlueModule glueModule;
    SaturationModule saturationModule;
    WidthModule widthModule;
    juce::dsp::Gain<float> outputGain;
    std::array<juce::SmoothedValue<float>, VocalChain::stageCount> stageWetSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> compareWetSmoothed;
    juce::SmoothedValue<float> chainTransitionGain;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> moduleDryBuffer;

    double currentSampleRate = 44100.0;
    ChainEngine::Order activeStageOrder = ChainEngine::defaultOrder();
    ChainEngine::Order pendingStageOrder = ChainEngine::defaultOrder();
    ChainTransitionState chainTransitionState = ChainTransitionState::stable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoriMixAudioProcessor)
};
