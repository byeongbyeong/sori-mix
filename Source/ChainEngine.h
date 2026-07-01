#pragma once

#include <juce_core/juce_core.h>

#include "VocalChain.h"

#include <array>

class ChainEngine final
{
public:
    using Order = std::array<VocalStage, VocalChain::stageCount>;

    static juce::StringArray getStageChoices();
    static Order defaultOrder();
    static Order orderFromSlots(const std::array<int, VocalChain::stageCount>& slots);

private:
    ChainEngine() = delete;
};
