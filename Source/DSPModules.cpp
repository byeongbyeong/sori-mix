#include "DSPModules.h"

#include <cmath>

namespace
{
float dbToLinear(float db)
{
    return juce::Decibels::decibelsToGain(db);
}

float levelToDb(float value)
{
    return juce::Decibels::gainToDecibels(juce::jmax(value, 0.00003f));
}

float timeConstantCoefficient(double sampleRate, float milliseconds)
{
    const auto samples = juce::jmax(1.0, sampleRate * milliseconds * 0.001);
    return static_cast<float>(std::exp(-1.0 / samples));
}

float clampToneFrequency(double sampleRate, float frequency)
{
    const auto upperFrequency = static_cast<float>(sampleRate * 0.45);
    return juce::jlimit(20.0f, juce::jmax(20.0f, upperFrequency), frequency);
}

float onePoleLowpassCoefficient(double sampleRate, float cutoff)
{
    const auto safeCutoff = clampToneFrequency(sampleRate, cutoff);
    return std::exp(-2.0f * juce::MathConstants<float>::pi * safeCutoff / static_cast<float>(sampleRate));
}

float mapHighGainToPresence(float highGain)
{
    return highGain >= 0.0f ? highGain * 0.35f : highGain * 0.2f;
}

float calculateSoftKneeGainReductionDb(float inputDb, float thresholdDb, float ratio, float kneeDb)
{
    const auto overDb = inputDb - thresholdDb;
    const auto ratioCurve = (1.0f / ratio) - 1.0f;

    if (kneeDb <= 0.0f)
        return overDb > 0.0f ? ratioCurve * overDb : 0.0f;

    const auto halfKnee = kneeDb * 0.5f;
    if (overDb <= -halfKnee)
        return 0.0f;

    if (overDb >= halfKnee)
        return ratioCurve * overDb;

    const auto kneePosition = overDb + halfKnee;
    return ratioCurve * kneePosition * kneePosition / (2.0f * kneeDb);
}
}

void ToneModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    for (auto& chain : chains)
        chain.prepare(spec);

    transition.reset(spec.sampleRate, 0.02);
    transition.setCurrentAndTargetValue(1.0f);
    transitionBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize), false, false, true);
}

void ToneModule::updateChain(ToneChain& chainToUpdate,
                             double sampleRate,
                             float lowGain,
                             float midGain,
                             float midFreq,
                             float highGain)
{
    const auto rumbleFrequency = clampToneFrequency(sampleRate, 68.0f);
    const auto lowFrequency = clampToneFrequency(sampleRate, lowGain >= 0.0f ? 165.0f : 220.0f);
    const auto safeMidFrequency = clampToneFrequency(sampleRate, midFreq);
    const auto presenceFrequency = clampToneFrequency(sampleRate, 3600.0f);
    const auto highFrequency = clampToneFrequency(sampleRate, 9200.0f);
    const auto midQ = midGain < 0.0f ? 0.72f : 0.95f;
    const auto presenceGain = mapHighGainToPresence(highGain);

    *chainToUpdate.get<0>().state = *juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sampleRate, rumbleFrequency, 0.707f);
    *chainToUpdate.get<1>().state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate, lowFrequency, 0.72f, dbToLinear(lowGain));
    *chainToUpdate.get<2>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, safeMidFrequency, midQ, dbToLinear(midGain));
    *chainToUpdate.get<3>().state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, presenceFrequency, 0.8f, dbToLinear(presenceGain));
    *chainToUpdate.get<4>().state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate, highFrequency, 0.65f, dbToLinear(highGain));
}

void ToneModule::updateImmediate(double sampleRate, float lowGain, float midGain, float midFreq, float highGain)
{
    lastLowGain = lowGain;
    lastMidGain = midGain;
    lastMidFreq = midFreq;
    lastHighGain = highGain;

    for (auto& chain : chains)
    {
        chain.reset();
        updateChain(chain, sampleRate, lowGain, midGain, midFreq, highGain);
    }

    activeChainIndex = 0;
    pendingChainIndex = 1;
    transition.setCurrentAndTargetValue(1.0f);
}

void ToneModule::update(double sampleRate, float lowGain, float midGain, float midFreq, float highGain)
{
    if (juce::approximatelyEqual(lowGain, lastLowGain)
        && juce::approximatelyEqual(midGain, lastMidGain)
        && juce::approximatelyEqual(midFreq, lastMidFreq)
        && juce::approximatelyEqual(highGain, lastHighGain))
        return;

    lastLowGain = lowGain;
    lastMidGain = midGain;
    lastMidFreq = midFreq;
    lastHighGain = highGain;

    pendingChainIndex = 1 - activeChainIndex;
    chains[pendingChainIndex].reset();
    updateChain(chains[pendingChainIndex], sampleRate, lowGain, midGain, midFreq, highGain);
    transition.setCurrentAndTargetValue(0.0f);
    transition.setTargetValue(1.0f);
}

void ToneModule::process(juce::AudioBuffer<float>& buffer)
{
    if (! transition.isSmoothing() && transition.getCurrentValue() >= 1.0f)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        chains[activeChainIndex].process(juce::dsp::ProcessContextReplacing<float>(block));
        return;
    }

    transitionBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        transitionBuffer.copyFrom(channel, 0, buffer, channel, 0, buffer.getNumSamples());

    juce::dsp::AudioBlock<float> oldBlock(buffer);
    chains[activeChainIndex].process(juce::dsp::ProcessContextReplacing<float>(oldBlock));

    juce::dsp::AudioBlock<float> newBlock(transitionBuffer);
    chains[pendingChainIndex].process(juce::dsp::ProcessContextReplacing<float>(newBlock));

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto newAmount = transition.getNextValue();
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto oldSample = buffer.getSample(channel, sample);
            const auto newSample = transitionBuffer.getSample(channel, sample);
            buffer.setSample(channel, sample, oldSample + (newSample - oldSample) * newAmount);
        }
    }

    if (! transition.isSmoothing())
        activeChainIndex = pendingChainIndex;
}

void ResonanceEqModule::prepare(const juce::dsp::ProcessSpec& spec)
{
    currentSampleRate = spec.sampleRate;

    const std::array<float, numBands> ratios { 0.5f, 0.78f, 1.0f, 1.55f, 2.35f };
    const std::array<float, numBands> qValues { 3.0f, 4.2f, 5.0f, 4.5f, 3.4f };

    for (size_t i = 0; i < bands.size(); ++i)
    {
        bands[i].frequencyRatio = ratios[i];
        bands[i].q = qValues[i];
        bands[i].reductionDb = 0.0f;
        bands[i].filter.prepare(spec);
    }

    update(spec.sampleRate, 0.0f, 900.0f);
}

void ResonanceEqModule::update(double sampleRate, float amount, float frequency)
{
    currentSampleRate = sampleRate;
    lastAmount = juce::jlimit(-12.0f, 0.0f, amount);
    lastFrequency = clampToneFrequency(sampleRate, frequency);
}

void ResonanceEqModule::process(juce::AudioBuffer<float>& buffer)
{
    const auto amountNorm = juce::jlimit(0.0f, 1.0f, std::abs(lastAmount) / 12.0f);
    if (amountNorm <= 0.0001f || buffer.getNumSamples() == 0)
        return;

    auto broadbandPower = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getSample(channel, sample);
            broadbandPower += value * value;
        }

    const auto broadbandDb = levelToDb(std::sqrt(broadbandPower / static_cast<float>(juce::jmax(1, buffer.getNumChannels() * buffer.getNumSamples()))));
    const auto smoothingSamples = juce::jmax(1.0, currentSampleRate * 0.025);
    const auto smoothingCoefficient = static_cast<float>(std::exp(-static_cast<double>(buffer.getNumSamples()) / smoothingSamples));

    for (auto& band : bands)
    {
        const auto bandFrequency = clampToneFrequency(currentSampleRate, lastFrequency * band.frequencyRatio);
        const auto omega = 2.0f * juce::MathConstants<float>::pi * bandFrequency / static_cast<float>(currentSampleRate);
        const auto coefficient = 2.0f * std::cos(omega);
        auto bandPower = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto q1 = 0.0f;
            auto q2 = 0.0f;

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto q0 = buffer.getSample(channel, sample) + coefficient * q1 - q2;
                q2 = q1;
                q1 = q0;
            }

            bandPower += q1 * q1 + q2 * q2 - coefficient * q1 * q2;
        }

        const auto normalisedBandPower = juce::jmax(0.0f, bandPower / static_cast<float>(juce::jmax(1, buffer.getNumChannels() * buffer.getNumSamples())));
        const auto bandDb = levelToDb(std::sqrt(normalisedBandPower));
        const auto resonanceExcessDb = bandDb - broadbandDb;
        const auto thresholdDb = juce::jmap(amountNorm, 0.0f, 1.0f, 7.0f, 1.5f);
        const auto targetReductionDb = -juce::jlimit(0.0f, 8.5f, (resonanceExcessDb - thresholdDb) * 0.75f * amountNorm);

        band.reductionDb = targetReductionDb + smoothingCoefficient * (band.reductionDb - targetReductionDb);

        *band.filter.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
            currentSampleRate, bandFrequency, band.q, dbToLinear(band.reductionDb));
    }

    juce::dsp::AudioBlock<float> block(buffer);
    for (auto& band : bands)
        band.filter.process(juce::dsp::ProcessContextReplacing<float>(block));
}

void DeEsserModule::prepare(double sampleRate, int maxChannels)
{
    currentSampleRate = sampleRate;
    amountSmoothed.reset(sampleRate, 0.015);
    sibilanceLowpassState.assign(static_cast<size_t>(juce::jmax(1, maxChannels)), 0.0f);
    airLowpassState.assign(static_cast<size_t>(juce::jmax(1, maxChannels)), 0.0f);
    sibilanceDetectorLevel = 0.0f;
    broadbandDetectorLevel = 0.0f;
    gainReductionEnvelope = 0.0f;
}

void DeEsserModule::setAmount(float amount)
{
    amountSmoothed.setTargetValue(amount);
}

void DeEsserModule::setAmountImmediate(float amount)
{
    amountSmoothed.setCurrentAndTargetValue(amount);
}

float DeEsserModule::process(juce::AudioBuffer<float>& buffer)
{
    auto maxReduction = 0.0f;
    const auto totalChannels = buffer.getNumChannels();
    const auto sibilanceCoefficient = onePoleLowpassCoefficient(currentSampleRate, 5400.0f);
    const auto airCoefficient = onePoleLowpassCoefficient(currentSampleRate, 10300.0f);
    const auto detectorAttackCoefficient = timeConstantCoefficient(currentSampleRate, 0.8f);
    const auto detectorReleaseCoefficient = timeConstantCoefficient(currentSampleRate, 58.0f);
    const auto gainAttackCoefficient = timeConstantCoefficient(currentSampleRate, 1.4f);
    const auto gainReleaseCoefficient = timeConstantCoefficient(currentSampleRate, 92.0f);

    if (static_cast<int>(sibilanceLowpassState.size()) < totalChannels)
    {
        sibilanceLowpassState.resize(static_cast<size_t>(totalChannels), 0.0f);
        airLowpassState.resize(static_cast<size_t>(totalChannels), 0.0f);
    }

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto amount = amountSmoothed.getNextValue();
        const auto thresholdDb = juce::jmap(amount, 0.0f, 1.0f, -15.0f, -34.0f);
        const auto excessThresholdDb = juce::jmap(amount, 0.0f, 1.0f, 12.0f, 3.5f);
        const auto maxReductionDb = juce::jmap(amount, 0.0f, 1.0f, 1.5f, 12.0f);

        auto sibilanceEnergy = 0.0f;
        auto broadbandEnergy = 0.0f;
        for (int channel = 0; channel < totalChannels; ++channel)
        {
            const auto input = buffer.getSample(channel, sample);
            auto& sibilanceLow = sibilanceLowpassState[static_cast<size_t>(channel)];
            auto& airLow = airLowpassState[static_cast<size_t>(channel)];
            sibilanceLow = input + sibilanceCoefficient * (sibilanceLow - input);
            airLow = input + airCoefficient * (airLow - input);

            const auto sibilance = airLow - sibilanceLow;
            const auto air = input - airLow;
            sibilanceEnergy += sibilance * sibilance + air * air * 0.35f;
            broadbandEnergy += input * input;
        }

        const auto sibilanceInput = std::sqrt(sibilanceEnergy / static_cast<float>(juce::jmax(1, totalChannels)));
        const auto broadbandInput = std::sqrt(broadbandEnergy / static_cast<float>(juce::jmax(1, totalChannels)));
        const auto sibilanceCoefficientForDetector = sibilanceInput > sibilanceDetectorLevel ? detectorAttackCoefficient : detectorReleaseCoefficient;
        const auto broadbandCoefficientForDetector = broadbandInput > broadbandDetectorLevel ? detectorAttackCoefficient : detectorReleaseCoefficient;
        sibilanceDetectorLevel = sibilanceInput + sibilanceCoefficientForDetector * (sibilanceDetectorLevel - sibilanceInput);
        broadbandDetectorLevel = broadbandInput + broadbandCoefficientForDetector * (broadbandDetectorLevel - broadbandInput);

        const auto sibilanceDb = levelToDb(sibilanceDetectorLevel);
        const auto broadbandDb = levelToDb(broadbandDetectorLevel);
        const auto excessDb = sibilanceDb - broadbandDb;
        auto targetReductionDb = calculateSoftKneeGainReductionDb(sibilanceDb, thresholdDb, 4.5f, 6.0f);
        targetReductionDb -= juce::jmax(0.0f, excessDb - excessThresholdDb) * amount * 0.7f;
        targetReductionDb = juce::jmax(targetReductionDb, -maxReductionDb);

        const auto gainCoefficient = targetReductionDb < gainReductionEnvelope ? gainAttackCoefficient : gainReleaseCoefficient;
        gainReductionEnvelope = targetReductionDb + gainCoefficient * (gainReductionEnvelope - targetReductionDb);

        const auto sibilanceGain = dbToLinear(gainReductionEnvelope);
        const auto airGain = dbToLinear(gainReductionEnvelope * 0.38f);
        maxReduction = juce::jmin(maxReduction, gainReductionEnvelope);

        for (int channel = 0; channel < totalChannels; ++channel)
        {
            const auto low = sibilanceLowpassState[static_cast<size_t>(channel)];
            const auto airLow = airLowpassState[static_cast<size_t>(channel)];
            const auto input = buffer.getSample(channel, sample);
            const auto sibilance = airLow - low;
            const auto air = input - airLow;
            buffer.setSample(channel, sample, low + sibilance * sibilanceGain + air * airGain);
        }
    }

    return maxReduction;
}

void DeEsserModule::skip(int numSamples)
{
    amountSmoothed.skip(numSamples);
    sibilanceDetectorLevel = 0.0f;
    broadbandDetectorLevel = 0.0f;
    gainReductionEnvelope = 0.0f;
}

void GlueModule::prepare(double sampleRate)
{
    currentSampleRate = sampleRate;
    amountSmoothed.reset(sampleRate, 0.02);
    peakDetectorLevel = 0.0f;
    rmsDetectorLevel = 0.0f;
    gainReductionEnvelope = 0.0f;
}

void GlueModule::setAmount(float amount)
{
    amountSmoothed.setTargetValue(amount);
}

void GlueModule::setAmountImmediate(float amount)
{
    amountSmoothed.setCurrentAndTargetValue(amount);
}

float GlueModule::process(juce::AudioBuffer<float>& buffer)
{
    auto maxReduction = 0.0f;
    const auto totalChannels = buffer.getNumChannels();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto compAmount = amountSmoothed.getNextValue();
        const auto threshold = juce::jmap(compAmount, 0.0f, 1.0f, -8.0f, -32.0f);
        const auto ratio = juce::jmap(compAmount, 0.0f, 1.0f, 1.0f, 4.8f);
        const auto kneeDb = juce::jmap(compAmount, 0.0f, 1.0f, 5.0f, 13.0f);
        const auto rangeDb = juce::jmap(compAmount, 0.0f, 1.0f, 2.0f, 12.0f);
        const auto peakAttackMs = juce::jmap(compAmount, 0.0f, 1.0f, 8.0f, 1.5f);
        const auto peakReleaseMs = juce::jmap(compAmount, 0.0f, 1.0f, 70.0f, 115.0f);
        const auto rmsAttackMs = juce::jmap(compAmount, 0.0f, 1.0f, 32.0f, 14.0f);
        const auto rmsReleaseMs = juce::jmap(compAmount, 0.0f, 1.0f, 180.0f, 360.0f);
        const auto peakAttack = timeConstantCoefficient(currentSampleRate, peakAttackMs);
        const auto peakRelease = timeConstantCoefficient(currentSampleRate, peakReleaseMs);
        const auto rmsAttack = timeConstantCoefficient(currentSampleRate, rmsAttackMs);
        const auto rmsRelease = timeConstantCoefficient(currentSampleRate, rmsReleaseMs);
        const auto gainAttack = timeConstantCoefficient(currentSampleRate, juce::jmap(compAmount, 0.0f, 1.0f, 18.0f, 4.5f));

        auto peak = 0.0f;
        auto sumSquares = 0.0f;
        for (int channel = 0; channel < totalChannels; ++channel)
        {
            const auto value = buffer.getSample(channel, sample);
            const auto magnitude = std::abs(value);
            peak = juce::jmax(peak, magnitude);
            sumSquares += value * value;
        }

        const auto rms = std::sqrt(sumSquares / static_cast<float>(juce::jmax(1, totalChannels)));
        const auto peakCoefficient = peak > peakDetectorLevel ? peakAttack : peakRelease;
        const auto rmsCoefficient = rms > rmsDetectorLevel ? rmsAttack : rmsRelease;
        peakDetectorLevel = peak + peakCoefficient * (peakDetectorLevel - peak);
        rmsDetectorLevel = rms + rmsCoefficient * (rmsDetectorLevel - rms);

        const auto rmsDb = levelToDb(rmsDetectorLevel);
        const auto peakDb = levelToDb(peakDetectorLevel);
        const auto transientAllowanceDb = juce::jmap(compAmount, 0.0f, 1.0f, 9.0f, 3.0f);
        const auto detectorDb = juce::jmax(rmsDb, peakDb - transientAllowanceDb);
        auto targetReductionDb = calculateSoftKneeGainReductionDb(detectorDb, threshold, ratio, kneeDb);
        targetReductionDb = juce::jmax(targetReductionDb, -rangeDb);

        const auto releaseDepth = juce::jlimit(0.0f, 1.0f, std::abs(gainReductionEnvelope) / juce::jmax(1.0f, rangeDb));
        const auto programReleaseMs = juce::jmap(releaseDepth, 0.0f, 1.0f, 90.0f, 520.0f);
        const auto gainRelease = timeConstantCoefficient(currentSampleRate, programReleaseMs);
        const auto gainCoefficient = targetReductionDb < gainReductionEnvelope ? gainAttack : gainRelease;
        gainReductionEnvelope = targetReductionDb + gainCoefficient * (gainReductionEnvelope - targetReductionDb);

        const auto autoMakeupDb = compAmount * 3.0f;
        const auto gain = dbToLinear(gainReductionEnvelope + autoMakeupDb);
        maxReduction = juce::jmin(maxReduction, gainReductionEnvelope);

        for (int channel = 0; channel < totalChannels; ++channel)
            buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);
    }

    return maxReduction;
}

void GlueModule::skip(int numSamples)
{
    amountSmoothed.skip(numSamples);
    peakDetectorLevel = 0.0f;
    rmsDetectorLevel = 0.0f;
    gainReductionEnvelope = 0.0f;
}

void SaturationModule::prepare(double sampleRate)
{
    amountSmoothed.reset(sampleRate, 0.02);
}

void SaturationModule::setAmount(float amount)
{
    amountSmoothed.setTargetValue(amount);
}

void SaturationModule::setAmountImmediate(float amount)
{
    amountSmoothed.setCurrentAndTargetValue(amount);
}

void SaturationModule::process(juce::AudioBuffer<float>& buffer)
{
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto amount = amountSmoothed.getNextValue();
        const auto drive = juce::jmap(amount, 0.0f, 1.0f, 1.0f, 2.4f);
        const auto wet = juce::jmap(amount, 0.0f, 1.0f, 0.0f, 0.42f);
        const auto trim = 1.0f / juce::jmap(amount, 0.0f, 1.0f, 1.0f, 1.45f);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto dry = buffer.getSample(channel, sample);
            const auto shaped = std::tanh(dry * drive) * trim;
            buffer.setSample(channel, sample, dry + (shaped - dry) * wet);
        }
    }
}

void SaturationModule::skip(int numSamples)
{
    amountSmoothed.skip(numSamples);
}

void WidthModule::prepare(double sampleRate)
{
    widthSmoothed.reset(sampleRate, 0.02);
}

void WidthModule::setWidth(float width)
{
    widthSmoothed.setTargetValue(width);
}

void WidthModule::setWidthImmediate(float width)
{
    widthSmoothed.setCurrentAndTargetValue(width);
}

void WidthModule::process(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
    {
        skip(buffer.getNumSamples());
        return;
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto width = widthSmoothed.getNextValue();
        const auto mid = 0.5f * (left[sample] + right[sample]);
        const auto side = 0.5f * (left[sample] - right[sample]) * width;
        left[sample] = mid + side;
        right[sample] = mid - side;
    }
}

void WidthModule::skip(int numSamples)
{
    widthSmoothed.skip(numSamples);
}
