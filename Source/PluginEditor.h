#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "AssistantClient.h"
#include "PluginProcessor.h"
#include "SecureCredentialStore.h"
#include "VocalChain.h"

class SoriMixAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit SoriMixAudioProcessorEditor(SoriMixAudioProcessor&);
    ~SoriMixAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Control
    {
        juce::String parameterID;
        juce::String title;
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void timerCallback() override;
    void configureControl(Control& control, const juce::String& parameterID, const juce::String& title);
    void attachControl(Control& control, const juce::String& parameterID, const juce::String& title);
    void layoutControl(Control& control, juce::Rectangle<int> bounds);
    void runAssistantCommand(const juce::String& command);
    void updateStagePage();
    void selectStage(size_t index);
    void updateStageColours();
    void saveApiKey();
    void deleteApiKey();
    void refreshCredentialStatus();
    void setAssistantControlsEnabled(bool shouldBeEnabled);
    float readParameterValue(const juce::String& parameterID, float fallback = 0.0f) const;
    void drawStageInsight(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawCompressorInsight(juce::Graphics& g, juce::Rectangle<float> bounds);
    static float compressorCurveReduction(float inputDb, float amount, float kneeDb, float rangeDb);
    static void drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float valueDb, juce::Colour colour);
    static juce::Colour stageColour(size_t index);

    SoriMixAudioProcessor& audioProcessor;

    std::array<Control, 8> controls;
    std::array<juce::TextButton, VocalChain::stageCount> stageButtons;
    juce::Label stageLabel;
    juce::TextEditor promptBox;
    juce::TextButton applyButton { "Apply" };
    juce::ComboBox providerBox;
    juce::TextEditor apiKeyBox;
    juce::TextButton saveKeyButton { "Save key" };
    juce::TextButton deleteKeyButton { "Delete" };
    std::array<juce::TextButton, 6> quickButtons;
    juce::TextButton stageEnableButton { "Stage On" };
    juce::TextButton compareButton { "Before" };
    std::array<juce::ComboBox, VocalChain::stageCount> chainSlotBoxes;
    std::unique_ptr<ButtonAttachment> stageEnableAttachment;
    std::unique_ptr<ButtonAttachment> compareAttachment;
    std::array<std::unique_ptr<ComboBoxAttachment>, VocalChain::stageCount> chainSlotAttachments;
    juce::Label statusLabel;
    juce::Label credentialLabel;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label reductionMeterLabel;
    juce::Rectangle<int> stageInsightBounds;

    float inputMeter = -90.0f;
    float outputMeter = -90.0f;
    float reductionMeter = 0.0f;
    size_t selectedStageIndex = 2;
    std::atomic<bool> assistantRequestInFlight { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoriMixAudioProcessorEditor)
};
