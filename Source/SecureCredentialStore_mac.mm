#include "SecureCredentialStore.h"

#if JUCE_MAC || JUCE_IOS

#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace
{
constexpr auto serviceName = "com.byeongbyeong.sorimix.ai";

NSData* toData(const juce::String& text)
{
    return [NSData dataWithBytes:text.toRawUTF8() length:static_cast<NSUInteger>(text.getNumBytesAsUTF8())];
}

NSString* toNSString(const juce::String& text)
{
    return [NSString stringWithUTF8String:text.toRawUTF8()];
}

juce::String fromNSString(NSString* text)
{
    if (text == nil)
        return {};

    return juce::String([text UTF8String]);
}

juce::String statusMessage(OSStatus status)
{
    if (auto message = SecCopyErrorMessageString(status, nullptr))
    {
        auto result = fromNSString((__bridge NSString*) message);
        CFRelease(message);
        return result;
    }

    return "Keychain error " + juce::String(static_cast<int>(status));
}

NSMutableDictionary* baseQuery(const juce::String& provider)
{
    auto* query = [NSMutableDictionary dictionary];
    query[(__bridge id) kSecClass] = (__bridge id) kSecClassGenericPassword;
    query[(__bridge id) kSecAttrService] = toNSString(serviceName);
    query[(__bridge id) kSecAttrAccount] = toNSString(provider);
    return query;
}
}

bool SecureCredentialStore::isAvailable() const
{
    return true;
}

SecureCredentialStore::Result SecureCredentialStore::saveApiKey(const juce::String& provider,
                                                                const juce::String& apiKey,
                                                                juce::String& message)
{
    const auto trimmedProvider = provider.trim();
    const auto trimmedKey = apiKey.trim();

    if (trimmedProvider.isEmpty() || trimmedKey.isEmpty())
    {
        message = "Provider and API key are required.";
        return Result::failed;
    }

    auto* query = baseQuery(trimmedProvider);
    NSMutableDictionary* update = [@{ (__bridge id) kSecValueData: toData(trimmedKey) } mutableCopy];
    auto status = SecItemUpdate((__bridge CFDictionaryRef) query, (__bridge CFDictionaryRef) update);

    if (status == errSecItemNotFound)
    {
        query[(__bridge id) kSecValueData] = toData(trimmedKey);
        status = SecItemAdd((__bridge CFDictionaryRef) query, nullptr);
    }

    if (status == errSecSuccess)
    {
        message = "API key saved in macOS Keychain.";
        return Result::ok;
    }

    message = statusMessage(status);
    return Result::failed;
}

SecureCredentialStore::ReadResult SecureCredentialStore::loadApiKey(const juce::String& provider) const
{
    const auto trimmedProvider = provider.trim();
    if (trimmedProvider.isEmpty())
        return { Result::failed, {}, "Provider is required." };

    auto* query = baseQuery(trimmedProvider);
    query[(__bridge id) kSecReturnData] = @YES;
    query[(__bridge id) kSecMatchLimit] = (__bridge id) kSecMatchLimitOne;

    CFTypeRef result = nullptr;
    const auto status = SecItemCopyMatching((__bridge CFDictionaryRef) query, &result);

    if (status == errSecItemNotFound)
        return { Result::notFound, {}, "No API key saved." };

    if (status != errSecSuccess)
        return { Result::failed, {}, statusMessage(status) };

    auto* data = (__bridge NSData*) result;
    auto* text = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
    CFRelease(result);

    return { Result::ok, fromNSString(text), "API key loaded from macOS Keychain." };
}

SecureCredentialStore::Result SecureCredentialStore::deleteApiKey(const juce::String& provider,
                                                                  juce::String& message)
{
    const auto trimmedProvider = provider.trim();
    if (trimmedProvider.isEmpty())
    {
        message = "Provider is required.";
        return Result::failed;
    }

    const auto status = SecItemDelete((__bridge CFDictionaryRef) baseQuery(trimmedProvider));

    if (status == errSecSuccess || status == errSecItemNotFound)
    {
        message = "API key removed from macOS Keychain.";
        return Result::ok;
    }

    message = statusMessage(status);
    return Result::failed;
}

#endif
