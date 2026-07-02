#include "AssistantClient.h"

namespace
{
constexpr auto openAIProvider = "OpenAI";
constexpr auto groqProvider = "Groq";

juce::String stripToJsonObject(juce::String text)
{
    text = text.trim();

    if (text.startsWith("```"))
    {
        const auto firstNewline = text.indexOfChar('\n');
        if (firstNewline >= 0)
            text = text.substring(firstNewline + 1);

        if (text.endsWith("```"))
            text = text.dropLastCharacters(3);
    }

    const auto start = text.indexOfChar('{');
    const auto end = text.lastIndexOfChar('}');

    if (start >= 0 && end > start)
        return text.substring(start, end + 1);

    return text;
}

juce::String systemPrompt()
{
    return "You are SoriMix, a vocal mixing channel-strip assistant. Return only a JSON object. "
           "The product follows this preferred vocal chain: De-Esser, Resonance EQ, "
           "Compressor, Musical EQ, Saturation, Inflator. Pitch/modulation and auto-tune are handled outside this plugin. "
           "The user prompt may include "
           "the currently selected stage; make decisions mainly for that stage while keeping the full vocal chain in mind. "
           "Use these optional numeric fields only when helpful: deEssAmount, resonanceAmount, resonanceFreq, "
           "compAmount, compAttack, compRelease, compKnee, compRange, compMakeup, "
           "lowGain, midGain, midFreq, highGain, satDrive, width, outputGain, mix. "
           "Ranges: deEssAmount/resonanceAmount/compAmount/satDrive/mix 0..1, resonanceFreq/midFreq 250..4500 Hz, "
           "compAttack 0.5..80 ms, compRelease 20..800 ms, compKnee 0..24 dB, compRange 1..18 dB, "
           "compMakeup/lowGain/midGain/highGain -12..12 dB, width 0..2, outputGain -24..12 dB. "
           "Stage mapping: De-Esser uses deEssAmount; Resonance EQ uses resonanceAmount and resonanceFreq; "
           "Compressor uses compAmount, compAttack, compRelease, compKnee, compRange, and compMakeup; "
           "Musical EQ uses lowGain, midGain, midFreq, and highGain; "
           "Saturation uses satDrive; Inflator uses width and outputGain. "
           "You may also use optional boolean fields toneEnabled, glueEnabled, widthEnabled; these map to "
           "Musical EQ, Compressor, and Inflator compatibility controls. "
           "Also include a short summary string. Prefer subtle, musical moves.";
}

juce::var makeMessage(const juce::String& role, const juce::String& content)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("role", role);
    object->setProperty("content", content);
    return juce::var(object);
}

juce::String makeRequestBody(const juce::String& provider, const juce::String& prompt)
{
    auto* payload = new juce::DynamicObject();
    payload->setProperty("model", provider == groqProvider ? "llama-3.1-8b-instant" : "gpt-4o-mini");
    payload->setProperty("temperature", 0.25);

    juce::Array<juce::var> messages;
    messages.add(makeMessage("system", systemPrompt()));
    messages.add(makeMessage("user", prompt));
    payload->setProperty("messages", messages);

    if (provider == openAIProvider)
    {
        auto* responseFormat = new juce::DynamicObject();
        responseFormat->setProperty("type", "json_object");
        payload->setProperty("response_format", juce::var(responseFormat));
    }

    return juce::JSON::toString(juce::var(payload), true);
}

juce::String endpointForProvider(const juce::String& provider)
{
    if (provider == groqProvider)
        return "https://api.groq.com/openai/v1/chat/completions";

    return "https://api.openai.com/v1/chat/completions";
}

juce::String headersForKey(const juce::String& apiKey)
{
    return "Content-Type: application/json\r\nAuthorization: Bearer " + apiKey.trim() + "\r\n";
}

std::optional<float> readFloatProperty(const juce::DynamicObject& object, const juce::Identifier& name)
{
    const auto value = object.getProperty(name);
    if (value.isDouble() || value.isInt() || value.isInt64())
        return static_cast<float>(value);

    return std::nullopt;
}

std::optional<float> readRangedFloatProperty(const juce::DynamicObject& object,
                                             const juce::Identifier& name,
                                             float minimum,
                                             float maximum)
{
    if (const auto value = readFloatProperty(object, name))
        return juce::jlimit(minimum, maximum, *value);

    return std::nullopt;
}

std::optional<bool> readBoolProperty(const juce::DynamicObject& object, const juce::Identifier& name)
{
    const auto value = object.getProperty(name);

    if (value.isBool())
        return static_cast<bool>(value);

    if (value.isInt() || value.isInt64())
        return static_cast<int>(value) != 0;

    if (value.isString())
    {
        const auto text = value.toString().trim().toLowerCase();
        if (text == "true" || text == "on" || text == "enabled")
            return true;
        if (text == "false" || text == "off" || text == "disabled")
            return false;
    }

    return std::nullopt;
}

bool hasParameterChange(const AssistantParameterPlan& plan)
{
    return plan.lowGain.has_value()
        || plan.midGain.has_value()
        || plan.midFreq.has_value()
        || plan.highGain.has_value()
        || plan.deEssAmount.has_value()
        || plan.resonanceAmount.has_value()
        || plan.resonanceFreq.has_value()
        || plan.compAmount.has_value()
        || plan.compMakeup.has_value()
        || plan.compAttack.has_value()
        || plan.compRelease.has_value()
        || plan.compKnee.has_value()
        || plan.compRange.has_value()
        || plan.satDrive.has_value()
        || plan.width.has_value()
        || plan.outputGain.has_value()
        || plan.mix.has_value()
        || plan.toneEnabled.has_value()
        || plan.glueEnabled.has_value()
        || plan.widthEnabled.has_value();
}

juce::String extractAssistantContent(const juce::String& responseText, juce::String& error)
{
    juce::var parsed;
    const auto parseResult = juce::JSON::parse(responseText, parsed);
    if (parseResult.failed())
    {
        error = parseResult.getErrorMessage();
        return {};
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        error = "Provider returned an unexpected response.";
        return {};
    }

    if (auto* choices = root->getProperty("choices").getArray())
    {
        if (! choices->isEmpty())
        {
            if (auto* choice = choices->getReference(0).getDynamicObject())
            {
                if (auto* message = choice->getProperty("message").getDynamicObject())
                    return message->getProperty("content").toString();
            }
        }
    }

    error = root->getProperty("error").toString();
    if (error.isEmpty())
        error = "Provider response did not contain assistant content.";

    return {};
}

AssistantClient::Result parsePlan(const juce::String& content)
{
    juce::var parsed;
    const auto parseResult = juce::JSON::parse(stripToJsonObject(content), parsed);
    if (parseResult.failed())
        return { false, {}, "Assistant returned invalid JSON: " + parseResult.getErrorMessage() };

    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
        return { false, {}, "Assistant returned an unexpected plan." };

    AssistantParameterPlan plan;
    plan.lowGain = readRangedFloatProperty(*object, "lowGain", -12.0f, 12.0f);
    plan.midGain = readRangedFloatProperty(*object, "midGain", -12.0f, 12.0f);
    plan.midFreq = readRangedFloatProperty(*object, "midFreq", 250.0f, 4500.0f);
    plan.highGain = readRangedFloatProperty(*object, "highGain", -12.0f, 12.0f);
    plan.deEssAmount = readRangedFloatProperty(*object, "deEssAmount", 0.0f, 1.0f);
    plan.resonanceAmount = readRangedFloatProperty(*object, "resonanceAmount", 0.0f, 1.0f);
    plan.resonanceFreq = readRangedFloatProperty(*object, "resonanceFreq", 250.0f, 4500.0f);
    plan.compAmount = readRangedFloatProperty(*object, "compAmount", 0.0f, 1.0f);
    plan.compMakeup = readRangedFloatProperty(*object, "compMakeup", -12.0f, 12.0f);
    plan.compAttack = readRangedFloatProperty(*object, "compAttack", 0.5f, 80.0f);
    plan.compRelease = readRangedFloatProperty(*object, "compRelease", 20.0f, 800.0f);
    plan.compKnee = readRangedFloatProperty(*object, "compKnee", 0.0f, 24.0f);
    plan.compRange = readRangedFloatProperty(*object, "compRange", 1.0f, 18.0f);
    plan.satDrive = readRangedFloatProperty(*object, "satDrive", 0.0f, 1.0f);
    plan.width = readRangedFloatProperty(*object, "width", 0.0f, 2.0f);
    plan.outputGain = readRangedFloatProperty(*object, "outputGain", -24.0f, 12.0f);
    plan.mix = readRangedFloatProperty(*object, "mix", 0.0f, 1.0f);
    plan.toneEnabled = readBoolProperty(*object, "toneEnabled");
    plan.glueEnabled = readBoolProperty(*object, "glueEnabled");
    plan.widthEnabled = readBoolProperty(*object, "widthEnabled");
    plan.summary = object->getProperty("summary").toString().trim();

    if (! hasParameterChange(plan))
        return { false, {}, "Assistant returned no usable parameter changes." };

    if (plan.summary.isEmpty())
        plan.summary = "Assistant mix plan applied.";

    return { true, plan, plan.summary };
}
}

AssistantClient::Result AssistantClient::requestPlan(const juce::String& provider,
                                                     const juce::String& apiKey,
                                                     const juce::String& prompt)
{
    const auto trimmedProvider = provider.trim();
    const auto trimmedKey = apiKey.trim();
    const auto trimmedPrompt = prompt.trim();

    if (trimmedKey.isEmpty())
        return { false, {}, "Save an API key before using the AI assistant." };

    if (trimmedPrompt.isEmpty())
        return { false, {}, "Enter a mix request first." };

    if (trimmedProvider != openAIProvider && trimmedProvider != groqProvider)
        return { false, {}, "Unsupported assistant provider: " + trimmedProvider };

    const auto url = juce::URL(endpointForProvider(trimmedProvider)).withPOSTData(makeRequestBody(trimmedProvider, trimmedPrompt));
    int statusCode = 0;
    juce::StringPairArray responseHeaders;

    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withExtraHeaders(headersForKey(trimmedKey))
                                            .withConnectionTimeoutMs(15000)
                                            .withNumRedirectsToFollow(3)
                                            .withStatusCode(&statusCode)
                                            .withResponseHeaders(&responseHeaders));

    if (stream == nullptr)
        return { false, {}, "Could not connect to the assistant provider." };

    const auto responseText = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300)
        return { false, {}, "Assistant provider returned HTTP " + juce::String(statusCode) + "." };

    juce::String error;
    const auto content = extractAssistantContent(responseText, error);
    if (content.isEmpty())
        return { false, {}, error };

    return parsePlan(content);
}
