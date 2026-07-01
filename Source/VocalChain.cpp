#include "VocalChain.h"

namespace
{
const VocalChain::StageList stageDefinitions {{
    { VocalStage::deEsser, "DeEss", "De-Esser", "Focus on sibilance, harsh consonants, and high-frequency vocal control." },
    { VocalStage::resonanceEq, "Res EQ", "Resonance EQ", "Focus on resonant buildup, boxiness, nasal tones, mud, and unpleasant ringing." },
    { VocalStage::compressor, "Comp", "Compressor", "Focus on vocal leveling, peak control, density, frontness, and transparent gain reduction." },
    { VocalStage::musicalEq, "EQ", "Musical EQ", "Focus on body, clarity, presence, air, brightness, and mix placement." },
    { VocalStage::saturation, "Sat", "Saturation", "Focus on harmonic density, edge, warmth, and controlled vocal color." },
    { VocalStage::inflator, "Inflate", "Inflator", "Focus on perceived loudness, vocal size, final energy, and output headroom." },
}};
}

const VocalChain::StageList& VocalChain::stages()
{
    return stageDefinitions;
}

const VocalStageInfo& VocalChain::stageInfo(size_t index)
{
    return stageDefinitions[juce::jmin(index, stageDefinitions.size() - 1)];
}

juce::String VocalChain::defaultChainText()
{
    juce::StringArray names;
    for (const auto& stage : stageDefinitions)
        names.add(stage.shortName);

    return names.joinIntoString(" > ");
}
