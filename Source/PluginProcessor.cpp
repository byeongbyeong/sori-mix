#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto lowGainID = "lowGain";
constexpr auto midGainID = "midGain";
constexpr auto midFreqID = "midFreq";
constexpr auto highGainID = "highGain";
constexpr auto compAmountID = "compAmount";
constexpr auto widthID = "width";
constexpr auto outputGainID = "outputGain";
constexpr auto mixID = "mix";

float dbToLinear(float db)
{
    return juce::Decibels::decibelsToGain(db);
}

float levelToDb(float value)
{
    return juce::Decibels::gainToDecibels(juce::jmax(value, 0.00003f));
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

    params.push_back(std::make_unique<juce::AudioParameterFloat>(lowGainID, "Low Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(midGainID, "Mid Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(midFreqID, "Mid Focus",
        juce::NormalisableRange<float>(250.0f, 4500.0f, 1.0f, 0.45f), 900.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(highGainID, "High Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(compAmountID, "Glue",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(widthID, "Width",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(outputGainID, "Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(mixID, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    return { params.begin(), params.end() };
}

void SoriMixAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    toneChain.prepare(spec);
    outputGain.prepare(spec);
    outputGain.setRampDurationSeconds(0.02);
    compressorEnvelope = 0.0f;
    updateFilters();
}

void SoriMixAudioProcessor::releaseResources()
{
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

void SoriMixAudioProcessor::updateFilters()
{
    const auto lowGain = readParameter(parameters, lowGainID);
    const auto midGain = readParameter(parameters, midGainID);
    const auto midFreq = readParameter(parameters, midFreqID);
    const auto highGain = readParameter(parameters, highGainID);

    if (juce::approximatelyEqual(lowGain, lastLowGain)
        && juce::approximatelyEqual(midGain, lastMidGain)
        && juce::approximatelyEqual(midFreq, lastMidFreq)
        && juce::approximatelyEqual(highGain, lastHighGain))
        return;

    lastLowGain = lowGain;
    lastMidGain = midGain;
    lastMidFreq = midFreq;
    lastHighGain = highGain;

    *toneChain.get<0>().state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        currentSampleRate, 160.0f, 0.707f, dbToLinear(lowGain));
    *toneChain.get<1>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        currentSampleRate, midFreq, 0.85f, dbToLinear(midGain));
    *toneChain.get<2>().state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        currentSampleRate, 7200.0f, 0.707f, dbToLinear(highGain));
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

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer, true);

    updateFilters();

    juce::dsp::AudioBlock<float> block(buffer);
    toneChain.process(juce::dsp::ProcessContextReplacing<float>(block));

    const auto compAmount = readParameter(parameters, compAmountID);
    const auto threshold = juce::jmap(compAmount, 0.0f, 1.0f, -3.0f, -28.0f);
    const auto ratio = juce::jmap(compAmount, 0.0f, 1.0f, 1.0f, 5.0f);
    const auto attack = 0.08f;
    const auto release = 0.985f;
    auto maxReduction = 0.0f;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        auto peak = 0.0f;
        for (int channel = 0; channel < totalOutputChannels; ++channel)
            peak = juce::jmax(peak, std::abs(buffer.getSample(channel, sample)));

        const auto detectorDb = levelToDb(peak);
        const auto overDb = juce::jmax(0.0f, detectorDb - threshold);
        const auto targetReductionDb = -overDb * (1.0f - (1.0f / ratio));
        compressorEnvelope = targetReductionDb < compressorEnvelope
            ? juce::jmap(attack, 0.0f, 1.0f, targetReductionDb, compressorEnvelope)
            : compressorEnvelope * release;

        const auto gain = dbToLinear(compressorEnvelope);
        maxReduction = juce::jmin(maxReduction, compressorEnvelope);

        for (int channel = 0; channel < totalOutputChannels; ++channel)
            buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);
    }

    if (totalOutputChannels >= 2)
    {
        const auto width = readParameter(parameters, widthID);
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto mid = 0.5f * (left[sample] + right[sample]);
            const auto side = 0.5f * (left[sample] - right[sample]) * width;
            left[sample] = mid + side;
            right[sample] = mid - side;
        }
    }

    outputGain.setGainDecibels(readParameter(parameters, outputGainID));
    outputGain.process(juce::dsp::ProcessContextReplacing<float>(block));

    const auto wetMix = readParameter(parameters, mixID);
    if (wetMix < 1.0f)
    {
        for (int channel = 0; channel < totalOutputChannels; ++channel)
        {
            auto* wet = buffer.getWritePointer(channel);
            const auto* dry = dryBuffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                wet[sample] = dry[sample] + (wet[sample] - dry[sample]) * wetMix;
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
    }
    else if (text.contains("bright") || text.contains("air") || text.contains("선명"))
    {
        setParameterValue(lowGainID, -0.8f);
        setParameterValue(midGainID, 0.4f);
        setParameterValue(highGainID, 2.8f);
        setParameterValue(widthID, 1.18f);
    }
    else if (text.contains("punch") || text.contains("펀치"))
    {
        setParameterValue(lowGainID, 1.4f);
        setParameterValue(midGainID, 1.2f);
        setParameterValue(midFreqID, 1100.0f);
        setParameterValue(compAmountID, 0.45f);
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
        setParameterValue(compAmountID, 0.12f);
        setParameterValue(widthID, 1.0f);
    }
}

juce::AudioProcessorEditor* SoriMixAudioProcessor::createEditor()
{
    return new SoriMixAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoriMixAudioProcessor();
}
