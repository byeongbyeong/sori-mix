#include "ChainEngine.h"

namespace
{
VocalStage stageForChoiceIndex(int index)
{
    const auto safeIndex = juce::jlimit(0, static_cast<int>(VocalChain::stageCount) - 1, index);
    return VocalChain::stageInfo(static_cast<size_t>(safeIndex)).stage;
}

bool containsStage(const ChainEngine::Order& order, size_t filledCount, VocalStage stage)
{
    for (size_t i = 0; i < filledCount; ++i)
        if (order[i] == stage)
            return true;

    return false;
}
}

juce::StringArray ChainEngine::getStageChoices()
{
    juce::StringArray choices;
    for (const auto& stage : VocalChain::stages())
        choices.add(stage.shortName);

    return choices;
}

ChainEngine::Order ChainEngine::defaultOrder()
{
    Order order {};
    for (size_t i = 0; i < VocalChain::stageCount; ++i)
        order[i] = VocalChain::stageInfo(i).stage;

    return order;
}

ChainEngine::Order ChainEngine::orderFromSlots(const std::array<int, VocalChain::stageCount>& slots)
{
    auto order = defaultOrder();
    size_t filledCount = 0;

    for (const auto slot : slots)
    {
        const auto stage = stageForChoiceIndex(slot);
        if (! containsStage(order, filledCount, stage))
            order[filledCount++] = stage;
    }

    for (const auto& stageInfo : VocalChain::stages())
        if (! containsStage(order, filledCount, stageInfo.stage))
            order[filledCount++] = stageInfo.stage;

    return order;
}
