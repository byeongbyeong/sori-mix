#pragma once

#include <juce_core/juce_core.h>

class SecureCredentialStore final
{
public:
    enum class Result
    {
        ok,
        notFound,
        unavailable,
        failed
    };

    struct ReadResult
    {
        Result result = Result::failed;
        juce::String value;
        juce::String message;
    };

    static SecureCredentialStore& instance();

    Result saveApiKey(const juce::String& provider, const juce::String& apiKey, juce::String& message);
    ReadResult loadApiKey(const juce::String& provider) const;
    Result deleteApiKey(const juce::String& provider, juce::String& message);
    bool isAvailable() const;

private:
    SecureCredentialStore() = default;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SecureCredentialStore)
};
