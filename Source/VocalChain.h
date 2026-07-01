#pragma once

#include <juce_core/juce_core.h>

#include <array>

enum class VocalStage
{
    deEsser,
    resonanceEq,
    compressor,
    musicalEq,
    saturation,
    inflator
};

struct VocalStageInfo
{
    VocalStage stage;
    const char* shortName;
    const char* displayName;
    const char* assistantContext;
};

class VocalChain final
{
public:
    static constexpr size_t stageCount = 6;
    using StageList = std::array<VocalStageInfo, stageCount>;

    static const StageList& stages();
    static const VocalStageInfo& stageInfo(size_t index);
    static juce::String defaultChainText();

private:
    VocalChain() = delete;
};
