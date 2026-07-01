#pragma once

#include <juce_core/juce_core.h>

#include "AssistantTypes.h"

class AssistantClient final
{
public:
    struct Result
    {
        bool ok = false;
        AssistantParameterPlan plan;
        juce::String message;
    };

    static Result requestPlan(const juce::String& provider,
                              const juce::String& apiKey,
                              const juce::String& prompt);

private:
    AssistantClient() = delete;
};
