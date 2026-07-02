#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
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
constexpr auto compareBeforeID = "compareBefore";

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

float levelToDb(float value)
{
    return juce::Decibels::gainToDecibels(juce::jmax(value, 0.00003f));
}

juce::ParameterID parameterID(const char* id, int versionHint)
{
    return { id, versionHint };
}

bool shouldProcessSmoothedWet(const juce::SmoothedValue<float>& wet)
{
    return wet.isSmoothing()
        || wet.getCurrentValue() > 0.0001f
        || wet.getTargetValue() > 0.0001f;
}

void copyBuffer(juce::AudioBuffer<float>& destination, const juce::AudioBuffer<float>& source)
{
    destination.setSize(source.getNumChannels(), source.getNumSamples(), false, false, true);
    for (int channel = 0; channel < source.getNumChannels(); ++channel)
        destination.copyFrom(channel, 0, source, channel, 0, source.getNumSamples());
}

void blendFromDryBuffer(juce::AudioBuffer<float>& wetBuffer,
                        const juce::AudioBuffer<float>& dryBufferToBlend,
                        juce::SmoothedValue<float>& wetAmount)
{
    if (! wetAmount.isSmoothing() && juce::approximatelyEqual(wetAmount.getCurrentValue(), 1.0f))
        return;

    for (int sample = 0; sample < wetBuffer.getNumSamples(); ++sample)
    {
        const auto wetMix = wetAmount.getNextValue();
        for (int channel = 0; channel < wetBuffer.getNumChannels(); ++channel)
        {
            auto* wet = wetBuffer.getWritePointer(channel);
            const auto* dry = dryBufferToBlend.getReadPointer(channel);
            wet[sample] = dry[sample] + (wet[sample] - dry[sample]) * wetMix;
        }
    }
}

size_t stageIndex(VocalStage stage)
{
    return static_cast<size_t>(stage);
}

bool ordersEqual(const ChainEngine::Order& first, const ChainEngine::Order& second)
{
    for (size_t i = 0; i < first.size(); ++i)
        if (first[i] != second[i])
            return false;

    return true;
}
}

SoriMixAudioProcessor::SoriMixAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SoriMixAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(lowGainID, 1), "Low Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(midGainID, 2), "Mid Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(midFreqID, 3), "Mid Focus",
        juce::NormalisableRange<float>(250.0f, 4500.0f, 1.0f, 0.45f), 900.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(highGainID, 4), "High Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(compAmountID, 5), "Glue",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(widthID, 6), "Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(outputGainID, 7), "Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(mixID, 8), "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(parameterID(compareBeforeID, 9), "Compare Before", false));

    int versionHint = 10;
    for (size_t i = 0; i < VocalChain::stageCount; ++i)
    {
        const auto& stage = VocalChain::stageInfo(i);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            parameterID(stageEnabledIDs[i], versionHint++), juce::String(stage.displayName) + " Enabled", true));
    }

    const auto stageChoices = ChainEngine::getStageChoices();
    for (size_t i = 0; i < VocalChain::stageCount; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            parameterID(stageSlotIDs[i], versionHint++), "Chain Slot " + juce::String(static_cast<int>(i + 1)),
            stageChoices, static_cast<int>(i)));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(deEssAmountID, versionHint++), "DeEss Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.22f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(resonanceAmountID, versionHint++), "Res EQ Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(resonanceFreqID, versionHint++), "Res EQ Target",
        juce::NormalisableRange<float>(250.0f, 4500.0f, 1.0f, 0.45f), 900.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(compMakeupID, versionHint++), "Comp Makeup",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(compAttackID, versionHint++), "Comp Attack",
        juce::NormalisableRange<float>(0.5f, 80.0f, 0.1f, 0.45f), 12.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(compReleaseID, versionHint++), "Comp Release",
        juce::NormalisableRange<float>(20.0f, 800.0f, 1.0f, 0.42f), 180.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(compKneeID, versionHint++), "Comp Knee",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 8.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(compRangeID, versionHint++), "Comp Range",
        juce::NormalisableRange<float>(1.0f, 18.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(parameterID(satDriveID, versionHint++), "Sat Drive",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.18f));

    return { params.begin(), params.end() };
}

void SoriMixAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    toneModule.prepare(spec);
    toneModule.updateImmediate(currentSampleRate,
                               readParameter(parameters, lowGainID),
                               readParameter(parameters, midGainID),
                               readParameter(parameters, midFreqID),
                               readParameter(parameters, highGainID));
    resonanceEqModule.prepare(spec);
    resonanceEqModule.update(currentSampleRate,
                             -12.0f * readParameter(parameters, resonanceAmountID),
                             readParameter(parameters, resonanceFreqID));
    deEsserModule.prepare(sampleRate, getTotalNumOutputChannels());
    deEsserModule.setAmountImmediate(readParameter(parameters, deEssAmountID));
    glueModule.prepare(sampleRate);
    glueModule.setAmountImmediate(readParameter(parameters, compAmountID));
    glueModule.setMakeupImmediate(readParameter(parameters, compMakeupID));
    glueModule.setTimingImmediate(readParameter(parameters, compAttackID),
                                  readParameter(parameters, compReleaseID));
    glueModule.setCurveImmediate(readParameter(parameters, compKneeID),
                                 readParameter(parameters, compRangeID));
    saturationModule.prepare(sampleRate);
    saturationModule.setAmountImmediate(readParameter(parameters, satDriveID));
    widthModule.prepare(sampleRate);
    widthModule.setWidthImmediate(readParameter(parameters, widthID));
    outputGain.prepare(spec);
    outputGain.setRampDurationSeconds(0.02);

    for (size_t i = 0; i < stageWetSmoothed.size(); ++i)
    {
        stageWetSmoothed[i].reset(sampleRate, 0.012);
        stageWetSmoothed[i].setCurrentAndTargetValue(readParameter(parameters, stageEnabledIDs[i]) > 0.5f ? 1.0f : 0.0f);
    }

    mixSmoothed.reset(sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue(readParameter(parameters, mixID));
    compareWetSmoothed.reset(sampleRate, 0.012);
    compareWetSmoothed.setCurrentAndTargetValue(readParameter(parameters, compareBeforeID) > 0.5f ? 0.0f : 1.0f);
    chainTransitionGain.reset(sampleRate, 0.012);
    chainTransitionGain.setCurrentAndTargetValue(1.0f);
    activeStageOrder = readStageOrder();
    pendingStageOrder = activeStageOrder;
    chainTransitionState = ChainTransitionState::stable;

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, false, true);
    moduleDryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, false, true);
}

void SoriMixAudioProcessor::releaseResources()
{
    dryBuffer.setSize(0, 0);
    moduleDryBuffer.setSize(0, 0);
}

bool SoriMixAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

float SoriMixAudioProcessor::readParameter(const juce::AudioProcessorValueTreeState& state,
                                           const juce::String& parameterID)
{
    if (auto* value = state.getRawParameterValue(parameterID))
        return value->load();

    jassertfalse;
    return 0.0f;
}

ChainEngine::Order SoriMixAudioProcessor::readStageOrder() const
{
    std::array<int, VocalChain::stageCount> slots {};
    for (size_t i = 0; i < slots.size(); ++i)
        slots[i] = static_cast<int>(readParameter(parameters, stageSlotIDs[i]));

    return ChainEngine::orderFromSlots(slots);
}

void SoriMixAudioProcessor::updateFilters()
{
    toneModule.update(currentSampleRate,
                      readParameter(parameters, lowGainID),
                      readParameter(parameters, midGainID),
                      readParameter(parameters, midFreqID),
                      readParameter(parameters, highGainID));
}

void SoriMixAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();

    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const auto inputRms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    inputLevelDb.store(levelToDb(inputRms));

    deEsserModule.setAmount(readParameter(parameters, deEssAmountID));
    resonanceEqModule.update(currentSampleRate,
                             -12.0f * readParameter(parameters, resonanceAmountID),
                             readParameter(parameters, resonanceFreqID));
    glueModule.setAmount(readParameter(parameters, compAmountID));
    glueModule.setMakeup(readParameter(parameters, compMakeupID));
    glueModule.setTiming(readParameter(parameters, compAttackID),
                         readParameter(parameters, compReleaseID));
    glueModule.setCurve(readParameter(parameters, compKneeID),
                        readParameter(parameters, compRangeID));
    saturationModule.setAmount(readParameter(parameters, satDriveID));
    widthModule.setWidth(readParameter(parameters, widthID));
    mixSmoothed.setTargetValue(readParameter(parameters, mixID));
    compareWetSmoothed.setTargetValue(readParameter(parameters, compareBeforeID) > 0.5f ? 0.0f : 1.0f);

    const auto needsDryBlend = mixSmoothed.isSmoothing()
        || ! juce::approximatelyEqual(mixSmoothed.getTargetValue(), 1.0f)
        || compareWetSmoothed.isSmoothing()
        || ! juce::approximatelyEqual(compareWetSmoothed.getTargetValue(), 1.0f);
    if (needsDryBlend)
    {
        dryBuffer.setSize(totalOutputChannels, buffer.getNumSamples(), false, false, true);
        for (int channel = 0; channel < totalOutputChannels; ++channel)
            dryBuffer.copyFrom(channel, 0, buffer, channel, 0, buffer.getNumSamples());
    }

    for (size_t i = 0; i < stageWetSmoothed.size(); ++i)
        stageWetSmoothed[i].setTargetValue(readParameter(parameters, stageEnabledIDs[i]) > 0.5f ? 1.0f : 0.0f);

    const auto requestedStageOrder = readStageOrder();
    if (! ordersEqual(requestedStageOrder, activeStageOrder))
    {
        pendingStageOrder = requestedStageOrder;
        if (chainTransitionState == ChainTransitionState::stable)
        {
            chainTransitionState = ChainTransitionState::fadingOut;
            chainTransitionGain.setTargetValue(0.0f);
        }
    }

    if (chainTransitionState == ChainTransitionState::fadingOut && chainTransitionGain.getCurrentValue() <= 0.001f)
    {
        activeStageOrder = pendingStageOrder;
        chainTransitionState = ChainTransitionState::fadingIn;
        chainTransitionGain.setCurrentAndTargetValue(0.0f);
        chainTransitionGain.setTargetValue(1.0f);
    }

    auto maxReduction = 0.0f;

    for (const auto stage : activeStageOrder)
    {
        auto& stageWet = stageWetSmoothed[stageIndex(stage)];

        switch (stage)
        {
            case VocalStage::deEsser:
                if (shouldProcessSmoothedWet(stageWet))
                {
                    copyBuffer(moduleDryBuffer, buffer);
                    maxReduction = juce::jmin(maxReduction, deEsserModule.process(buffer));
                    blendFromDryBuffer(buffer, moduleDryBuffer, stageWet);
                }
                else
                {
                    deEsserModule.skip(buffer.getNumSamples());
                }
                break;

            case VocalStage::resonanceEq:
                if (shouldProcessSmoothedWet(stageWet))
                {
                    copyBuffer(moduleDryBuffer, buffer);
                    resonanceEqModule.process(buffer);
                    blendFromDryBuffer(buffer, moduleDryBuffer, stageWet);
                }
                break;

            case VocalStage::compressor:
                if (shouldProcessSmoothedWet(stageWet))
                {
                    copyBuffer(moduleDryBuffer, buffer);
                    maxReduction = juce::jmin(maxReduction, glueModule.process(buffer));
                    blendFromDryBuffer(buffer, moduleDryBuffer, stageWet);
                }
                else
                {
                    glueModule.skip(buffer.getNumSamples());
                }
                break;

            case VocalStage::musicalEq:
                if (shouldProcessSmoothedWet(stageWet))
                {
                    copyBuffer(moduleDryBuffer, buffer);
                    updateFilters();
                    toneModule.process(buffer);
                    blendFromDryBuffer(buffer, moduleDryBuffer, stageWet);
                }
                break;

            case VocalStage::saturation:
                if (shouldProcessSmoothedWet(stageWet))
                {
                    copyBuffer(moduleDryBuffer, buffer);
                    saturationModule.process(buffer);
                    blendFromDryBuffer(buffer, moduleDryBuffer, stageWet);
                }
                else
                {
                    saturationModule.skip(buffer.getNumSamples());
                }
                break;

            case VocalStage::inflator:
                if (totalOutputChannels >= 2 && shouldProcessSmoothedWet(stageWet))
                {
                    copyBuffer(moduleDryBuffer, buffer);
                    widthModule.process(buffer);
                    blendFromDryBuffer(buffer, moduleDryBuffer, stageWet);
                }
                else
                {
                    widthModule.skip(buffer.getNumSamples());
                }
                break;
        }
    }

    juce::dsp::AudioBlock<float> block(buffer);
    outputGain.setGainDecibels(readParameter(parameters, outputGainID));
    outputGain.process(juce::dsp::ProcessContextReplacing<float>(block));

    if (needsDryBlend)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto wetMix = mixSmoothed.getNextValue();
            for (int channel = 0; channel < totalOutputChannels; ++channel)
            {
                auto* wet = buffer.getWritePointer(channel);
                const auto* dry = dryBuffer.getReadPointer(channel);
                wet[sample] = dry[sample] + (wet[sample] - dry[sample]) * wetMix;
            }
        }
    }
    else
    {
        mixSmoothed.skip(buffer.getNumSamples());
    }

    if (compareWetSmoothed.isSmoothing()
        || ! juce::approximatelyEqual(compareWetSmoothed.getCurrentValue(), 1.0f)
        || ! juce::approximatelyEqual(compareWetSmoothed.getTargetValue(), 1.0f))
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto wetAmount = compareWetSmoothed.getNextValue();
            for (int channel = 0; channel < totalOutputChannels; ++channel)
            {
                auto* wet = buffer.getWritePointer(channel);
                const auto* dry = dryBuffer.getReadPointer(channel);
                wet[sample] = dry[sample] + (wet[sample] - dry[sample]) * wetAmount;
            }
        }
    }
    else
    {
        compareWetSmoothed.skip(buffer.getNumSamples());
    }

    if (chainTransitionGain.isSmoothing()
        || ! juce::approximatelyEqual(chainTransitionGain.getCurrentValue(), 1.0f)
        || ! juce::approximatelyEqual(chainTransitionGain.getTargetValue(), 1.0f))
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto gain = chainTransitionGain.getNextValue();
            for (int channel = 0; channel < totalOutputChannels; ++channel)
                buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);
        }

        if (chainTransitionState == ChainTransitionState::fadingIn
            && ! chainTransitionGain.isSmoothing()
            && juce::approximatelyEqual(chainTransitionGain.getCurrentValue(), 1.0f))
        {
            chainTransitionState = ChainTransitionState::stable;
        }
    }

    const auto outputRms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    outputLevelDb.store(levelToDb(outputRms));
    gainReductionDb.store(maxReduction);
}

void SoriMixAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void SoriMixAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

void SoriMixAudioProcessor::setParameterValue(const juce::String& parameterID, float value)
{
    if (auto* parameter = parameters.getParameter(parameterID))
    {
        const auto range = parameter->getNormalisableRange();
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(range.convertTo0to1(juce::jlimit(range.start, range.end, value)));
        parameter->endChangeGesture();
    }
}

void SoriMixAudioProcessor::applyAssistantCommand(const juce::String& command)
{
    const auto text = command.toLowerCase();

    if (text.contains("warm") || text.contains("따뜻") || text.contains("warmth"))
    {
        setParameterValue(lowGainID, 2.2f);
        setParameterValue(midGainID, -0.8f);
        setParameterValue(highGainID, -0.5f);
        setParameterValue(compAmountID, 0.22f);
        setParameterValue(compAttackID, 18.0f);
        setParameterValue(compReleaseID, 240.0f);
        setParameterValue(compKneeID, 13.0f);
        setParameterValue(compRangeID, 7.5f);
        setParameterValue(satDriveID, 0.22f);
    }
    else if (text.contains("bright") || text.contains("air") || text.contains("선명"))
    {
        setParameterValue(lowGainID, -0.8f);
        setParameterValue(midGainID, 0.4f);
        setParameterValue(highGainID, 2.8f);
        setParameterValue(deEssAmountID, 0.28f);
        setParameterValue(widthID, 1.18f);
    }
    else if (text.contains("punch") || text.contains("펀치"))
    {
        setParameterValue(lowGainID, 1.4f);
        setParameterValue(midGainID, 1.2f);
        setParameterValue(midFreqID, 1100.0f);
        setParameterValue(compAmountID, 0.45f);
        setParameterValue(compAttackID, 4.5f);
        setParameterValue(compReleaseID, 115.0f);
        setParameterValue(compKneeID, 6.0f);
        setParameterValue(compRangeID, 11.0f);
        setParameterValue(compMakeupID, 0.8f);
    }
    else if (text.contains("wide") || text.contains("space") || text.contains("넓"))
    {
        setParameterValue(highGainID, 1.2f);
        setParameterValue(widthID, 1.55f);
        setParameterValue(mixID, 0.92f);
    }
    else if (text.contains("clean") || text.contains("clear") || text.contains("깨끗"))
    {
        setParameterValue(lowGainID, -1.0f);
        setParameterValue(midGainID, -1.2f);
        setParameterValue(highGainID, 1.0f);
        setParameterValue(deEssAmountID, 0.18f);
        setParameterValue(resonanceAmountID, 0.22f);
        setParameterValue(compAmountID, 0.12f);
        setParameterValue(compAttackID, 24.0f);
        setParameterValue(compReleaseID, 260.0f);
        setParameterValue(compKneeID, 14.0f);
        setParameterValue(compRangeID, 5.0f);
        setParameterValue(satDriveID, 0.08f);
        setParameterValue(widthID, 1.0f);
    }
}

void SoriMixAudioProcessor::applyAssistantPlan(const AssistantParameterPlan& plan)
{
    if (plan.lowGain.has_value())
        setParameterValue(lowGainID, *plan.lowGain);
    if (plan.midGain.has_value())
        setParameterValue(midGainID, *plan.midGain);
    if (plan.midFreq.has_value())
        setParameterValue(midFreqID, *plan.midFreq);
    if (plan.highGain.has_value())
        setParameterValue(highGainID, *plan.highGain);
    if (plan.deEssAmount.has_value())
        setParameterValue(deEssAmountID, *plan.deEssAmount);
    if (plan.resonanceAmount.has_value())
        setParameterValue(resonanceAmountID, *plan.resonanceAmount);
    if (plan.resonanceFreq.has_value())
        setParameterValue(resonanceFreqID, *plan.resonanceFreq);
    if (plan.compAmount.has_value())
        setParameterValue(compAmountID, *plan.compAmount);
    if (plan.compMakeup.has_value())
        setParameterValue(compMakeupID, *plan.compMakeup);
    if (plan.compAttack.has_value())
        setParameterValue(compAttackID, *plan.compAttack);
    if (plan.compRelease.has_value())
        setParameterValue(compReleaseID, *plan.compRelease);
    if (plan.compKnee.has_value())
        setParameterValue(compKneeID, *plan.compKnee);
    if (plan.compRange.has_value())
        setParameterValue(compRangeID, *plan.compRange);
    if (plan.satDrive.has_value())
        setParameterValue(satDriveID, *plan.satDrive);
    if (plan.width.has_value())
        setParameterValue(widthID, *plan.width);
    if (plan.outputGain.has_value())
        setParameterValue(outputGainID, *plan.outputGain);
    if (plan.mix.has_value())
        setParameterValue(mixID, *plan.mix);
    if (plan.toneEnabled.has_value())
        setParameterValue(stageEnabledIDs[stageIndex(VocalStage::musicalEq)], *plan.toneEnabled ? 1.0f : 0.0f);
    if (plan.glueEnabled.has_value())
        setParameterValue(stageEnabledIDs[stageIndex(VocalStage::compressor)], *plan.glueEnabled ? 1.0f : 0.0f);
    if (plan.widthEnabled.has_value())
        setParameterValue(stageEnabledIDs[stageIndex(VocalStage::inflator)], *plan.widthEnabled ? 1.0f : 0.0f);
}

juce::AudioProcessorEditor* SoriMixAudioProcessor::createEditor()
{
    return new SoriMixAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoriMixAudioProcessor();
}
