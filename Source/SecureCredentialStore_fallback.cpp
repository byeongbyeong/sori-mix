#include "SecureCredentialStore.h"

#if ! (JUCE_MAC || JUCE_IOS)

bool SecureCredentialStore::isAvailable() const
{
    return false;
}

SecureCredentialStore::Result SecureCredentialStore::saveApiKey(const juce::String&,
                                                                const juce::String&,
                                                                juce::String& message)
{
    message = "Secure credential storage is not available on this platform yet.";
    return Result::unavailable;
}

SecureCredentialStore::ReadResult SecureCredentialStore::loadApiKey(const juce::String&) const
{
    return { Result::unavailable, {}, "Secure credential storage is not available on this platform yet." };
}

SecureCredentialStore::Result SecureCredentialStore::deleteApiKey(const juce::String&, juce::String& message)
{
    message = "Secure credential storage is not available on this platform yet.";
    return Result::unavailable;
}

#endif
