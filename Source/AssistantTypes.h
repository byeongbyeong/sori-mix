#pragma once

#include <juce_core/juce_core.h>

#include <optional>

struct AssistantParameterPlan
{
    std::optional<float> lowGain;
    std::optional<float> midGain;
    std::optional<float> midFreq;
    std::optional<float> highGain;
    std::optional<float> deEssAmount;
    std::optional<float> resonanceAmount;
    std::optional<float> resonanceFreq;
    std::optional<float> compAmount;
    std::optional<float> compMakeup;
    std::optional<float> satDrive;
    std::optional<float> width;
    std::optional<float> outputGain;
    std::optional<float> mix;
    std::optional<bool> toneEnabled;
    std::optional<bool> glueEnabled;
    std::optional<bool> widthEnabled;
    std::optional<int> chainOrder;
    juce::String summary;
};
