#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "SecureCredentialStore.h"

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
    void layoutControl(Control& control, juce::Rectangle<int> bounds);
    void runAssistantCommand(const juce::String& command);
    void saveApiKey();
    void deleteApiKey();
    void refreshCredentialStatus();
    static void drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float valueDb, juce::Colour colour);

    SoriMixAudioProcessor& audioProcessor;

    std::array<Control, 8> controls;
    juce::TextEditor promptBox;
    juce::TextButton applyButton { "Apply" };
    juce::ComboBox providerBox;
    juce::TextEditor apiKeyBox;
    juce::TextButton saveKeyButton { "Save key" };
    juce::TextButton deleteKeyButton { "Delete" };
    std::array<juce::TextButton, 6> quickButtons;
    juce::Label statusLabel;
    juce::Label credentialLabel;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;

    float inputMeter = -90.0f;
    float outputMeter = -90.0f;
    float reductionMeter = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SoriMixAudioProcessorEditor)
};
