#include "PluginEditor.h"

#include <cmath>
#include <thread>

namespace
{
constexpr int margin = 24;
constexpr auto compareBeforeID = "compareBefore";
constexpr auto lowGainID = "lowGain";
constexpr auto midGainID = "midGain";
constexpr auto midFreqID = "midFreq";
constexpr auto highGainID = "highGain";
constexpr auto compAmountID = "compAmount";
constexpr auto compMakeupID = "compMakeup";
constexpr auto compAttackID = "compAttack";
constexpr auto compReleaseID = "compRelease";
constexpr auto compKneeID = "compKnee";
constexpr auto compRangeID = "compRange";
constexpr auto deEssAmountID = "deEssAmount";
constexpr auto resonanceAmountID = "resonanceAmount";
constexpr auto resonanceFreqID = "resonanceFreq";
constexpr auto satDriveID = "satDrive";
constexpr auto widthID = "width";
constexpr auto outputGainID = "outputGain";
constexpr auto mixID = "mix";

constexpr std::array<const char*, VocalChain::stageCount> stageEnabledIDs {{
    "deEsserEnabled",
    "resonanceEqEnabled",
    "compressorEnabled",
    "musicalEqEnabled",
    "saturationEnabled",
    "inflatorEnabled",
}};

constexpr std::array<const char*, VocalChain::stageCount> stageSlotIDs {{
    "stageSlot1",
    "stageSlot2",
    "stageSlot3",
    "stageSlot4",
    "stageSlot5",
    "stageSlot6",
}};

juce::Colour backgroundColour() { return juce::Colour(0xfffbf4ec); }
juce::Colour panelColour() { return juce::Colour(0xfffffbf6); }
juce::Colour controlColour() { return juce::Colour(0xffffefe4); }
juce::Colour accentColour() { return juce::Colour(0xffffa88f); }
juce::Colour textColour() { return juce::Colour(0xff5b463d); }
juce::Colour mutedTextColour() { return juce::Colour(0xff9f8175); }
juce::Colour outlineColour() { return juce::Colour(0xffecd8cc); }

const std::array<const char*, 6> quickCommandLabels {
    "Warm", "Bright", "Punch", "Wide", "Clean", "Reset vibe"
};

struct StageControlSpec
{
    const char* parameterID = nullptr;
    const char* label = nullptr;
};

const std::array<std::array<StageControlSpec, 8>, VocalChain::stageCount> stageControlSpecs {{
    std::array<StageControlSpec, 8> {{
        { deEssAmountID, "Sibilance" },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { mixID, "Global Mix" },
    }},
    std::array<StageControlSpec, 8> {{
        { resonanceAmountID, "Suppress" },
        { resonanceFreqID, "Target" },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { mixID, "Global Mix" },
    }},
    std::array<StageControlSpec, 8> {{
        { compAmountID, "Leveling" },
        { compAttackID, "Attack" },
        { compReleaseID, "Release" },
        { compKneeID, "Knee" },
        { compRangeID, "Range" },
        { compMakeupID, "Makeup" },
        { nullptr, nullptr },
        { mixID, "Global Mix" },
    }},
    std::array<StageControlSpec, 8> {{
        { lowGainID, "Body" },
        { midGainID, "Shape" },
        { midFreqID, "Focus" },
        { highGainID, "Air" },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { mixID, "Global Mix" },
    }},
    std::array<StageControlSpec, 8> {{
        { satDriveID, "Drive" },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { mixID, "Global Mix" },
    }},
    std::array<StageControlSpec, 8> {{
        { widthID, "Size" },
        { outputGainID, "Output" },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { nullptr, nullptr },
        { mixID, "Global Mix" },
    }},
}};
}

SoriMixAudioProcessorEditor::SoriMixAudioProcessorEditor(SoriMixAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(920, 640);
    setResizable(true, true);
    setResizeLimits(820, 620, 1320, 860);

    for (size_t i = 0; i < controls.size(); ++i)
    {
        const auto& spec = stageControlSpecs[selectedStageIndex][i];
        configureControl(controls[i],
                         spec.parameterID != nullptr ? spec.parameterID : mixID,
                         spec.label != nullptr ? spec.label : "");
    }

    for (size_t i = 0; i < stageButtons.size(); ++i)
    {
        const auto& stage = VocalChain::stageInfo(i);
        stageButtons[i].setButtonText(stage.shortName);
        stageButtons[i].setClickingTogglesState(false);
        stageButtons[i].setColour(juce::TextButton::buttonColourId, controlColour());
        stageButtons[i].setColour(juce::TextButton::buttonOnColourId, stageColour(i));
        stageButtons[i].setColour(juce::TextButton::textColourOffId, textColour());
        stageButtons[i].setColour(juce::TextButton::textColourOnId, textColour());
        stageButtons[i].onClick = [this, i] { selectStage(i); };
        addAndMakeVisible(stageButtons[i]);
    }

    stageLabel.setColour(juce::Label::textColourId, textColour());
    stageLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(stageLabel);

    promptBox.setMultiLine(false);
    promptBox.setReturnKeyStartsNewLine(false);
    promptBox.setTextToShowWhenEmpty("Ask the selected vocal stage...", mutedTextColour());
    promptBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xfffffbf6));
    promptBox.setColour(juce::TextEditor::textColourId, textColour());
    promptBox.setColour(juce::TextEditor::outlineColourId, outlineColour());
    promptBox.onReturnKey = [this] { runAssistantCommand(promptBox.getText()); };
    addAndMakeVisible(promptBox);

    applyButton.setColour(juce::TextButton::buttonColourId, accentColour());
    applyButton.setColour(juce::TextButton::textColourOffId, textColour());
    applyButton.onClick = [this] { runAssistantCommand(promptBox.getText()); };
    addAndMakeVisible(applyButton);

    providerBox.addItem("OpenAI", 1);
    providerBox.addItem("Groq", 2);
    providerBox.setSelectedId(1, juce::dontSendNotification);
    providerBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xfffffbf6));
    providerBox.setColour(juce::ComboBox::textColourId, textColour());
    providerBox.setColour(juce::ComboBox::outlineColourId, outlineColour());
    providerBox.onChange = [this] { refreshCredentialStatus(); };
    addAndMakeVisible(providerBox);

    apiKeyBox.setMultiLine(false);
    apiKeyBox.setPasswordCharacter(0x2022);
    apiKeyBox.setTextToShowWhenEmpty("API key", mutedTextColour());
    apiKeyBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xfffffbf6));
    apiKeyBox.setColour(juce::TextEditor::textColourId, textColour());
    apiKeyBox.setColour(juce::TextEditor::outlineColourId, outlineColour());
    addAndMakeVisible(apiKeyBox);

    saveKeyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffd9efd9));
    saveKeyButton.setColour(juce::TextButton::textColourOffId, textColour());
    saveKeyButton.onClick = [this] { saveApiKey(); };
    addAndMakeVisible(saveKeyButton);

    deleteKeyButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffffddd6));
    deleteKeyButton.setColour(juce::TextButton::textColourOffId, textColour());
    deleteKeyButton.onClick = [this] { deleteApiKey(); };
    addAndMakeVisible(deleteKeyButton);

    for (size_t i = 0; i < quickButtons.size(); ++i)
    {
        quickButtons[i].setButtonText(quickCommandLabels[i]);
        quickButtons[i].setColour(juce::TextButton::buttonColourId, controlColour());
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

    stageEnableButton.setClickingTogglesState(true);
    stageEnableButton.setColour(juce::TextButton::buttonOnColourId, accentColour());
    stageEnableButton.setColour(juce::TextButton::buttonColourId, controlColour());
    stageEnableButton.setColour(juce::TextButton::textColourOnId, textColour());
    stageEnableButton.setColour(juce::TextButton::textColourOffId, textColour());
    addAndMakeVisible(stageEnableButton);

    compareButton.setClickingTogglesState(true);
    compareButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffffc7b8));
    compareButton.setColour(juce::TextButton::buttonColourId, controlColour());
    compareButton.setColour(juce::TextButton::textColourOnId, textColour());
    compareButton.setColour(juce::TextButton::textColourOffId, textColour());
    addAndMakeVisible(compareButton);
    compareAttachment = std::make_unique<ButtonAttachment>(audioProcessor.getState(), compareBeforeID, compareButton);

    const auto stageChoices = ChainEngine::getStageChoices();
    for (size_t i = 0; i < chainSlotBoxes.size(); ++i)
    {
        chainSlotBoxes[i].addItemList(stageChoices, 1);
        chainSlotBoxes[i].setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xfffffbf6));
        chainSlotBoxes[i].setColour(juce::ComboBox::textColourId, textColour());
        chainSlotBoxes[i].setColour(juce::ComboBox::outlineColourId, outlineColour());
        addAndMakeVisible(chainSlotBoxes[i]);
        chainSlotAttachments[i] = std::make_unique<ComboBoxAttachment>(audioProcessor.getState(), stageSlotIDs[i], chainSlotBoxes[i]);
    }

    statusLabel.setText("Assistant ready", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, mutedTextColour());
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    credentialLabel.setColour(juce::Label::textColourId, mutedTextColour());
    credentialLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(credentialLabel);
    refreshCredentialStatus();

    inputMeterLabel.setText("IN", juce::dontSendNotification);
    outputMeterLabel.setText("OUT", juce::dontSendNotification);
    reductionMeterLabel.setText("GR", juce::dontSendNotification);
    for (auto* label : { &inputMeterLabel, &outputMeterLabel, &reductionMeterLabel })
    {
        label->setColour(juce::Label::textColourId, mutedTextColour());
        label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*label);
    }

    startTimerHz(30);
    updateStagePage();
}

void SoriMixAudioProcessorEditor::configureControl(Control& control,
                                                   const juce::String& parameterID,
                                                   const juce::String& title)
{
    control.label.setText(title, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setColour(juce::Label::textColourId, textColour());
    addAndMakeVisible(control.label);

    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 20);
    control.slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour());
    control.slider.setColour(juce::Slider::rotarySliderOutlineColourId, outlineColour());
    control.slider.setColour(juce::Slider::thumbColourId, textColour());
    control.slider.setColour(juce::Slider::textBoxTextColourId, textColour());
    control.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xfffffbf6));
    control.slider.setColour(juce::Slider::textBoxOutlineColourId, outlineColour());
    addAndMakeVisible(control.slider);

    attachControl(control, parameterID, title);
}

void SoriMixAudioProcessorEditor::attachControl(Control& control,
                                                const juce::String& parameterID,
                                                const juce::String& title)
{
    if (control.parameterID == parameterID && control.title == title && control.attachment != nullptr)
        return;

    control.attachment.reset();
    control.parameterID = parameterID;
    control.title = title;
    control.label.setText(title, juce::dontSendNotification);

    if (parameterID.isNotEmpty())
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
    g.setColour(mutedTextColour());
    g.drawText("Vocal chain shaping, monitoring, and stage-aware assistant", margin + 20, margin + 48, 500, 24,
               juce::Justification::centredLeft);

    auto meterArea = juce::Rectangle<float>(static_cast<float>(getWidth() - margin - 174),
                                            static_cast<float>(margin + 14), 130.0f, 88.0f);
    const auto reductionForMeter = juce::jmap(juce::jlimit(0.0f, 18.0f, std::abs(reductionMeter)), 0.0f, 18.0f, -60.0f, 0.0f);
    drawMeter(g, meterArea.removeFromTop(20), inputMeter, juce::Colour(0xffffb48f));
    drawMeter(g, meterArea.removeFromTop(20).translated(0.0f, 10.0f), outputMeter, juce::Colour(0xffb6d9ff));
    drawMeter(g, meterArea.removeFromTop(20).translated(0.0f, 20.0f), reductionForMeter, juce::Colour(0xffd8c4ff));

    drawStageInsight(g, stageInsightBounds);

    g.setColour(outlineColour());
    g.drawRoundedRectangle(area, 8.0f, 1.0f);
}

void SoriMixAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(margin + 18);
    area.removeFromTop(82);

    auto stageRow = area.removeFromTop(34);
    stageLabel.setBounds(stageRow.removeFromLeft(112).reduced(0, 1));
    const auto stageButtonWidth = stageRow.getWidth() / static_cast<int>(stageButtons.size());
    for (auto& button : stageButtons)
        button.setBounds(stageRow.removeFromLeft(stageButtonWidth).reduced(3, 1));

    area.removeFromTop(8);

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

    promptArea.removeFromTop(8);
    auto moduleRow = promptArea.removeFromTop(30);
    stageEnableButton.setBounds(moduleRow.removeFromLeft(112).reduced(3, 0));
    compareButton.setBounds(moduleRow.removeFromLeft(92).reduced(3, 0));
    const auto slotWidth = moduleRow.getWidth() / static_cast<int>(chainSlotBoxes.size());
    for (auto& slotBox : chainSlotBoxes)
        slotBox.setBounds(moduleRow.removeFromLeft(slotWidth).reduced(2, 0));

    stageInsightBounds = area.removeFromTop(96).reduced(8, 8);

    auto controlArea = area.reduced(0, 12);
    auto topRow = controlArea.removeFromTop(controlArea.getHeight() / 2);
    auto bottomRow = controlArea;

    for (int i = 0; i < 4; ++i)
        layoutControl(controls[static_cast<size_t>(i)], topRow.removeFromLeft(topRow.getWidth() / (4 - i)).reduced(8));

    for (int i = 4; i < 8; ++i)
        layoutControl(controls[static_cast<size_t>(i)], bottomRow.removeFromLeft(bottomRow.getWidth() / (8 - i)).reduced(8));

    inputMeterLabel.setBounds(getWidth() - margin - 190, margin + 18, 38, 20);
    outputMeterLabel.setBounds(getWidth() - margin - 190, margin + 48, 38, 20);
    reductionMeterLabel.setBounds(getWidth() - margin - 190, margin + 78, 38, 20);
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

    if (assistantRequestInFlight.load())
        return;

    const auto provider = providerBox.getText();
    const auto key = SecureCredentialStore::instance().loadApiKey(provider);
    const auto& stage = VocalChain::stageInfo(selectedStageIndex);
    const auto assistantPrompt = juce::String("Current vocal chain stage: ")
        + stage.displayName + "\n" + stage.assistantContext + "\nUser request: " + trimmed;

    if (key.result != SecureCredentialStore::Result::ok)
    {
        audioProcessor.applyAssistantCommand(trimmed);
        statusLabel.setText("Local preset applied: " + trimmed, juce::dontSendNotification);
        promptBox.clear();
        return;
    }

    assistantRequestInFlight.store(true);
    setAssistantControlsEnabled(false);
    statusLabel.setText("Asking " + provider + "...", juce::dontSendNotification);
    promptBox.clear();

    juce::Component::SafePointer<SoriMixAudioProcessorEditor> safeThis(this);
    std::thread([safeThis, provider, apiKey = key.value, trimmed, assistantPrompt] {
        auto assistantResult = AssistantClient::requestPlan(provider, apiKey, assistantPrompt);

        juce::MessageManager::callAsync([safeThis, result = std::move(assistantResult), trimmed] {
            if (safeThis == nullptr)
                return;

            safeThis->assistantRequestInFlight.store(false);
            safeThis->setAssistantControlsEnabled(true);

            if (result.ok)
            {
                safeThis->audioProcessor.applyAssistantPlan(result.plan);
                safeThis->statusLabel.setText(result.message, juce::dontSendNotification);
            }
            else
            {
                safeThis->audioProcessor.applyAssistantCommand(trimmed);
                safeThis->statusLabel.setText(result.message + " Local fallback applied.", juce::dontSendNotification);
            }
        });
    }).detach();
}

void SoriMixAudioProcessorEditor::selectStage(size_t index)
{
    selectedStageIndex = juce::jmin(index, stageButtons.size() - 1);
    updateStagePage();
}

void SoriMixAudioProcessorEditor::updateStageColours()
{
    const auto colour = stageColour(selectedStageIndex);

    stageEnableButton.setColour(juce::TextButton::buttonOnColourId, colour);
    applyButton.setColour(juce::TextButton::buttonColourId, colour);

    for (auto& control : controls)
    {
        control.slider.setColour(juce::Slider::rotarySliderFillColourId, colour);
        control.label.setColour(juce::Label::textColourId, textColour());
    }
}

void SoriMixAudioProcessorEditor::updateStagePage()
{
    const auto& stage = VocalChain::stageInfo(selectedStageIndex);
    stageLabel.setText(stage.displayName, juce::dontSendNotification);
    stageEnableButton.setButtonText(juce::String(stage.shortName) + " On");
    stageEnableAttachment = std::make_unique<ButtonAttachment>(
        audioProcessor.getState(), stageEnabledIDs[selectedStageIndex], stageEnableButton);

    for (size_t i = 0; i < stageButtons.size(); ++i)
        stageButtons[i].setToggleState(i == selectedStageIndex, juce::dontSendNotification);

    updateStageColours();

    auto setControlVisible = [this](size_t index, bool shouldBeVisible) {
        controls[index].label.setVisible(shouldBeVisible);
        controls[index].slider.setVisible(shouldBeVisible);
    };

    const auto& specs = stageControlSpecs[selectedStageIndex];
    for (size_t i = 0; i < controls.size(); ++i)
    {
        const auto& spec = specs[i];
        const auto isVisible = spec.parameterID != nullptr && spec.label != nullptr;
        attachControl(controls[i],
                      isVisible ? spec.parameterID : juce::String(),
                      isVisible ? spec.label : juce::String());
        setControlVisible(i, isVisible);
    }

    promptBox.setTextToShowWhenEmpty("Ask " + juce::String(stage.shortName) + " assistant...", juce::Colour(0xff7f8c86));
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

void SoriMixAudioProcessorEditor::setAssistantControlsEnabled(bool shouldBeEnabled)
{
    promptBox.setEnabled(shouldBeEnabled);
    applyButton.setEnabled(shouldBeEnabled);
    providerBox.setEnabled(shouldBeEnabled);

    for (auto& button : quickButtons)
        button.setEnabled(shouldBeEnabled);
}

float SoriMixAudioProcessorEditor::readParameterValue(const juce::String& parameterID, float fallback) const
{
    if (auto* value = audioProcessor.getState().getRawParameterValue(parameterID))
        return value->load();

    return fallback;
}

float SoriMixAudioProcessorEditor::compressorCurveReduction(float inputDb,
                                                            float amount,
                                                            float kneeDb,
                                                            float rangeDb)
{
    const auto thresholdDb = juce::jmap(amount, 0.0f, 1.0f, -8.0f, -32.0f);
    const auto ratio = juce::jmap(amount, 0.0f, 1.0f, 1.0f, 4.8f);
    const auto overDb = inputDb - thresholdDb;
    const auto ratioCurve = (1.0f / ratio) - 1.0f;

    auto reduction = 0.0f;
    if (kneeDb <= 0.0f)
    {
        reduction = overDb > 0.0f ? ratioCurve * overDb : 0.0f;
    }
    else
    {
        const auto halfKnee = kneeDb * 0.5f;
        if (overDb >= halfKnee)
            reduction = ratioCurve * overDb;
        else if (overDb > -halfKnee)
        {
            const auto kneePosition = overDb + halfKnee;
            reduction = ratioCurve * kneePosition * kneePosition / (2.0f * kneeDb);
        }
    }

    return juce::jmax(reduction, -rangeDb);
}

void SoriMixAudioProcessorEditor::drawStageInsight(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    if (VocalChain::stageInfo(selectedStageIndex).stage == VocalStage::compressor)
        drawCompressorInsight(g, bounds.toFloat());
}

void SoriMixAudioProcessorEditor::drawCompressorInsight(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto colour = stageColour(selectedStageIndex);
    const auto amount = readParameterValue(compAmountID, 0.15f);
    const auto attack = readParameterValue(compAttackID, 12.0f);
    const auto release = readParameterValue(compReleaseID, 180.0f);
    const auto knee = readParameterValue(compKneeID, 8.0f);
    const auto range = readParameterValue(compRangeID, 10.0f);
    const auto reduction = juce::jlimit(0.0f, 18.0f, std::abs(reductionMeter));

    g.setColour(juce::Colour(0xfffff7f1));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(outlineColour());
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    auto content = bounds.reduced(14.0f, 10.0f);
    auto graph = content.removeFromLeft(250.0f);
    content.removeFromLeft(18.0f);

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(textColour());
    g.drawText("Compression curve", graph.removeFromTop(18.0f), juce::Justification::centredLeft);

    auto graphBox = graph.reduced(0.0f, 2.0f);
    g.setColour(juce::Colour(0xfffffbf6));
    g.fillRoundedRectangle(graphBox, 6.0f);
    g.setColour(outlineColour().withAlpha(0.72f));
    g.drawRoundedRectangle(graphBox, 6.0f, 1.0f);

    const auto plot = graphBox.reduced(12.0f, 9.0f);
    auto mapX = [plot](float db) {
        return juce::jmap(db, -48.0f, 0.0f, plot.getX(), plot.getRight());
    };
    auto mapY = [plot](float db) {
        return juce::jmap(db, -48.0f, 0.0f, plot.getBottom(), plot.getY());
    };

    juce::Path dryLine;
    dryLine.startNewSubPath(mapX(-48.0f), mapY(-48.0f));
    dryLine.lineTo(mapX(0.0f), mapY(0.0f));
    g.setColour(outlineColour());
    g.strokePath(dryLine, juce::PathStrokeType(1.0f));

    juce::Path curve;
    for (int i = 0; i <= 40; ++i)
    {
        const auto inputDb = juce::jmap(static_cast<float>(i), 0.0f, 40.0f, -48.0f, 0.0f);
        const auto outputDb = inputDb + compressorCurveReduction(inputDb, amount, knee, range);
        const auto x = mapX(inputDb);
        const auto y = mapY(outputDb);

        if (i == 0)
            curve.startNewSubPath(x, y);
        else
            curve.lineTo(x, y);
    }

    g.setColour(colour);
    g.strokePath(curve, juce::PathStrokeType(2.4f));

    auto drawBar = [&g, colour](juce::Rectangle<float> row,
                                const juce::String& label,
                                float normalised,
                                const juce::String& valueText) {
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff5b463d));
        g.drawText(label, row.removeFromLeft(70.0f), juce::Justification::centredLeft);

        auto valueArea = row.removeFromRight(72.0f);
        auto bar = row.reduced(0.0f, 3.0f);
        g.setColour(juce::Colour(0xffffeadf));
        g.fillRoundedRectangle(bar, 4.0f);
        g.setColour(colour.withAlpha(0.88f));
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, normalised)), 4.0f);
        g.setColour(juce::Colour(0xff9f8175));
        g.drawText(valueText, valueArea, juce::Justification::centredRight);
    };

    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.setColour(textColour());
    g.drawText("Motion", content.removeFromTop(16.0f), juce::Justification::centredLeft);

    drawBar(content.removeFromTop(12.0f), "Attack",
            juce::jmap(attack, 0.5f, 80.0f, 0.0f, 1.0f),
            juce::String(attack, 1) + " ms");
    drawBar(content.removeFromTop(12.0f), "Release",
            juce::jmap(release, 20.0f, 800.0f, 0.0f, 1.0f),
            juce::String(release, 0) + " ms");
    drawBar(content.removeFromTop(12.0f), "Knee",
            knee / 24.0f,
            juce::String(knee, 1) + " dB");
    drawBar(content.removeFromTop(12.0f), "Range",
            juce::jmap(range, 1.0f, 18.0f, 0.0f, 1.0f),
            juce::String(range, 1) + " dB");
    drawBar(content.removeFromTop(12.0f), "Live GR",
            reduction / 18.0f,
            juce::String(reduction, 1) + " dB");
}

void SoriMixAudioProcessorEditor::timerCallback()
{
    inputMeter = audioProcessor.inputLevelDb.load();
    outputMeter = audioProcessor.outputLevelDb.load();
    reductionMeter = audioProcessor.gainReductionDb.load();
    repaint();
}

juce::Colour SoriMixAudioProcessorEditor::stageColour(size_t index)
{
    static const std::array<juce::Colour, VocalChain::stageCount> colours {{
        juce::Colour(0xffffb7a6),
        juce::Colour(0xffffd6a5),
        juce::Colour(0xfff7c9d7),
        juce::Colour(0xffcfe8c9),
        juce::Colour(0xffffc4a3),
        juce::Colour(0xffc9ddff),
    }};

    return colours[juce::jmin(index, colours.size() - 1)];
}

void SoriMixAudioProcessorEditor::drawMeter(juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            float valueDb,
                                            juce::Colour colour)
{
    const auto normalised = juce::jlimit(0.0f, 1.0f, juce::jmap(valueDb, -60.0f, 0.0f, 0.0f, 1.0f));
    g.setColour(juce::Colour(0xffffeadf));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(colour);
    g.fillRoundedRectangle(bounds.withWidth(bounds.getWidth() * normalised), 3.0f);
}
