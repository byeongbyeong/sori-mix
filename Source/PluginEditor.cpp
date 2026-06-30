#include "PluginEditor.h"

namespace
{
constexpr int margin = 24;

juce::Colour backgroundColour() { return juce::Colour(0xff101416); }
juce::Colour panelColour() { return juce::Colour(0xff182022); }
juce::Colour accentColour() { return juce::Colour(0xff5ed6a0); }
juce::Colour textColour() { return juce::Colour(0xffe8f0eb); }

const std::array<const char*, 6> quickCommandLabels {
    "Warm", "Bright", "Punch", "Wide", "Clean", "Reset vibe"
};
}

SoriMixAudioProcessorEditor::SoriMixAudioProcessorEditor(SoriMixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(820, 520);
    setResizable(true, true);
    setResizeLimits(680, 440, 1280, 820);

    const std::array<std::pair<const char*, const char*>, 8> controlSpecs {{
        { "lowGain", "Low" },
        { "midGain", "Mid" },
        { "midFreq", "Focus" },
        { "highGain", "High" },
        { "compAmount", "Glue" },
        { "width", "Width" },
        { "outputGain", "Output" },
        { "mix", "Mix" },
    }};

    for (size_t i = 0; i < controls.size(); ++i)
        configureControl(controls[i], controlSpecs[i].first, controlSpecs[i].second);

    promptBox.setMultiLine(false);
    promptBox.setReturnKeyStartsNewLine(false);
    promptBox.setTextToShowWhenEmpty("Try: warmer vocal, brighter synth, wider chorus...", juce::Colour(0xff7f8c86));
    promptBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1112));
    promptBox.setColour(juce::TextEditor::textColourId, textColour());
    promptBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2b3839));
    promptBox.onReturnKey = [this] { runAssistantCommand(promptBox.getText()); };
    addAndMakeVisible(promptBox);

    applyButton.setColour(juce::TextButton::buttonColourId, accentColour());
    applyButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff06100c));
    applyButton.onClick = [this] { runAssistantCommand(promptBox.getText()); };
    addAndMakeVisible(applyButton);

    providerBox.addItem("OpenAI", 1);
    providerBox.addItem("Groq", 2);
    providerBox.setSelectedId(1, juce::dontSendNotification);
    providerBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1112));
    providerBox.setColour(juce::ComboBox::textColourId, textColour());
    providerBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff2b3839));
    providerBox.onChange = [this] { refreshCredentialStatus(); };
    addAndMakeVisible(providerBox);

    apiKeyBox.setMultiLine(false);
    apiKeyBox.setPasswordCharacter(0x2022);
    apiKeyBox.setTextToShowWhenEmpty("API key", juce::Colour(0xff7f8c86));
    apiKeyBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1112));
    apiKeyBox.setColour(juce::TextEditor::textColourId, textColour());
    apiKeyBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2b3839));
    addAndMakeVisible(apiKeyBox);

    saveKeyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff355746));
    saveKeyButton.setColour(juce::TextButton::textColourOffId, textColour());
    saveKeyButton.onClick = [this] { saveApiKey(); };
    addAndMakeVisible(saveKeyButton);

    deleteKeyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff463033));
    deleteKeyButton.setColour(juce::TextButton::textColourOffId, textColour());
    deleteKeyButton.onClick = [this] { deleteApiKey(); };
    addAndMakeVisible(deleteKeyButton);

    for (size_t i = 0; i < quickButtons.size(); ++i)
    {
        quickButtons[i].setButtonText(quickCommandLabels[i]);
        quickButtons[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff243032));
        quickButtons[i].setColour(juce::TextButton::textColourOffId, textColour());
        quickButtons[i].onClick = [this, i] {
            const auto label = quickButtons[i].getButtonText();
            if (label == "Reset vibe")
                runAssistantCommand("clean");
            else
                runAssistantCommand(label);
        };
        addAndMakeVisible(quickButtons[i]);
    }

    statusLabel.setText("Assistant ready", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb4ac));
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    credentialLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb4ac));
    credentialLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(credentialLabel);
    refreshCredentialStatus();

    inputMeterLabel.setText("IN", juce::dontSendNotification);
    outputMeterLabel.setText("OUT", juce::dontSendNotification);
    for (auto* label : { &inputMeterLabel, &outputMeterLabel })
    {
        label->setColour(juce::Label::textColourId, juce::Colour(0xff9fb4ac));
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*label);
    }

    startTimerHz(30);
}

void SoriMixAudioProcessorEditor::configureControl(Control& control,
                                                   const juce::String& parameterID,
                                                   const juce::String& title)
{
    control.parameterID = parameterID;
    control.title = title;

    control.label.setText(title, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setColour(juce::Label::textColourId, textColour());
    addAndMakeVisible(control.label);

    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 20);
    control.slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour());
    control.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff344143));
    control.slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xfff4fff9));
    control.slider.setColour(juce::Slider::textBoxTextColourId, textColour());
    control.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00000000));
    control.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00000000));
    addAndMakeVisible(control.slider);

    control.attachment = std::make_unique<SliderAttachment>(audioProcessor.getState(), parameterID, control.slider);
}

void SoriMixAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour());

    auto area = getLocalBounds().reduced(margin).toFloat();
    g.setColour(panelColour());
    g.fillRoundedRectangle(area, 8.0f);

    g.setColour(textColour());
    g.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    g.drawText("SoriMix", margin + 18, margin + 14, 180, 38, juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(13.0f));
    g.setColour(juce::Colour(0xff9fb4ac));
    g.drawText("Prompt-shaped tone, glue, width, and level control", margin + 20, margin + 48, 440, 24,
               juce::Justification::centredLeft);

    auto meterArea = juce::Rectangle<float>(static_cast<float>(getWidth() - margin - 174),
                                            static_cast<float>(margin + 20), 130.0f, 72.0f);
    drawMeter(g, meterArea.removeFromTop(24), inputMeter, accentColour());
    drawMeter(g, meterArea.removeFromTop(24).translated(0.0f, 14.0f), outputMeter, juce::Colour(0xff74a7ff));

    g.setColour(juce::Colour(0xff2b3839));
    g.drawRoundedRectangle(area, 8.0f, 1.0f);
}

void SoriMixAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(margin + 18);
    area.removeFromTop(82);

    auto promptArea = area.removeFromBottom(158);
    auto statusArea = promptArea.removeFromBottom(26);
    statusLabel.setBounds(statusArea);

    auto credentialRow = promptArea.removeFromBottom(36);
    deleteKeyButton.setBounds(credentialRow.removeFromRight(82).reduced(3, 3));
    saveKeyButton.setBounds(credentialRow.removeFromRight(102).reduced(3, 3));
    apiKeyBox.setBounds(credentialRow.removeFromRight(240).reduced(3, 3));
    providerBox.setBounds(credentialRow.removeFromLeft(112).reduced(3, 3));
    credentialLabel.setBounds(credentialRow.reduced(8, 0));

    promptArea.removeFromBottom(8);
    auto commandRow = promptArea.removeFromTop(38);
    applyButton.setBounds(commandRow.removeFromRight(92));
    commandRow.removeFromRight(10);
    promptBox.setBounds(commandRow);

    promptArea.removeFromTop(14);
    auto quickRow = promptArea.removeFromTop(32);
    const auto quickWidth = quickRow.getWidth() / static_cast<int>(quickButtons.size());
    for (auto& button : quickButtons)
        button.setBounds(quickRow.removeFromLeft(quickWidth).reduced(3, 0));

    auto controlArea = area.reduced(0, 12);
    auto topRow = controlArea.removeFromTop(controlArea.getHeight() / 2);
    auto bottomRow = controlArea;

    for (int i = 0; i < 4; ++i)
        layoutControl(controls[static_cast<size_t>(i)], topRow.removeFromLeft(topRow.getWidth() / (4 - i)).reduced(8));

    for (int i = 4; i < 8; ++i)
        layoutControl(controls[static_cast<size_t>(i)], bottomRow.removeFromLeft(bottomRow.getWidth() / (8 - i)).reduced(8));

    inputMeterLabel.setBounds(getWidth() - margin - 190, margin + 18, 38, 20);
    outputMeterLabel.setBounds(getWidth() - margin - 190, margin + 56, 38, 20);
}

void SoriMixAudioProcessorEditor::layoutControl(Control& control, juce::Rectangle<int> bounds)
{
    control.label.setBounds(bounds.removeFromTop(24));
    control.slider.setBounds(bounds);
}

void SoriMixAudioProcessorEditor::runAssistantCommand(const juce::String& command)
{
    const auto trimmed = command.trim();
    if (trimmed.isEmpty())
        return;

    audioProcessor.applyAssistantCommand(trimmed);
    statusLabel.setText("Applied: " + trimmed, juce::dontSendNotification);
    promptBox.clear();
}

void SoriMixAudioProcessorEditor::saveApiKey()
{
    juce::String message;
    const auto provider = providerBox.getText();
    const auto result = SecureCredentialStore::instance().saveApiKey(provider, apiKeyBox.getText(), message);

    apiKeyBox.clear();
    statusLabel.setText(message, juce::dontSendNotification);
    refreshCredentialStatus();

    if (result != SecureCredentialStore::Result::ok)
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "SoriMix", message);
}

void SoriMixAudioProcessorEditor::deleteApiKey()
{
    juce::String message;
    const auto result = SecureCredentialStore::instance().deleteApiKey(providerBox.getText(), message);

    statusLabel.setText(message, juce::dontSendNotification);
    refreshCredentialStatus();

    if (result != SecureCredentialStore::Result::ok)
        juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "SoriMix", message);
}

void SoriMixAudioProcessorEditor::refreshCredentialStatus()
{
    const auto provider = providerBox.getText();
    const auto result = SecureCredentialStore::instance().loadApiKey(provider);

    if (result.result == SecureCredentialStore::Result::ok)
        credentialLabel.setText(provider + " key saved in Keychain", juce::dontSendNotification);
    else if (result.result == SecureCredentialStore::Result::notFound)
        credentialLabel.setText(provider + " key not saved", juce::dontSendNotification);
    else
        credentialLabel.setText("Secure key storage unavailable", juce::dontSendNotification);
}

void SoriMixAudioProcessorEditor::timerCallback()
{
    inputMeter = audioProcessor.inputLevelDb.load();
    outputMeter = audioProcessor.outputLevelDb.load();
    reductionMeter = audioProcessor.gainReductionDb.load();
    repaint();
}

void SoriMixAudioProcessorEditor::drawMeter(juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            float valueDb,
                                            juce::Colour colour)
{
    const auto normalised = juce::jlimit(0.0f, 1.0f, juce::jmap(valueDb, -60.0f, 0.0f, 0.0f, 1.0f));
    g.setColour(juce::Colour(0xff253032));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(colour);
    g.fillRoundedRectangle(bounds.withWidth(bounds.getWidth() * normalised), 3.0f);
}
