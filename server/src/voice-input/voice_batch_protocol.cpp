#include "voice_batch_protocol.h"
#include "voice_providers.h"
#include <msime/voice/wav_writer.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string_view>

namespace VoiceInput
{
namespace
{
// MiMo accepts at most 10 MB of base64 inside input_audio.data. A WAV expands to 4/3 of its size
// in base64, so the raw upload cap is 3/4 of that - roughly 245 s of 16 kHz mono 16-bit PCM.
constexpr std::size_t kMimoMaximumBase64Bytes = 10 * 1024 * 1024;
constexpr std::size_t kMimoMaximumWavBytes = kMimoMaximumBase64Bytes / 4 * 3;
constexpr std::size_t kMimoUploadSampleLimit = (kMimoMaximumWavBytes - 44) / 2;

std::string Base64Encode(const unsigned char *data, std::size_t size)
{
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve((size + 2) / 3 * 4);
    std::size_t offset = 0;
    for (; offset + 3 <= size; offset += 3)
    {
        const std::uint32_t triple = (static_cast<std::uint32_t>(data[offset]) << 16) |
                                     (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
                                     static_cast<std::uint32_t>(data[offset + 2]);
        encoded.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        encoded.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        encoded.push_back(kAlphabet[(triple >> 6) & 0x3F]);
        encoded.push_back(kAlphabet[triple & 0x3F]);
    }
    const std::size_t remaining = size - offset;
    if (remaining != 0)
    {
        const std::uint32_t triple =
            (static_cast<std::uint32_t>(data[offset]) << 16) |
            (remaining == 2 ? static_cast<std::uint32_t>(data[offset + 1]) << 8 : std::uint32_t{0});
        encoded.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        encoded.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        encoded.push_back(remaining == 2 ? kAlphabet[(triple >> 6) & 0x3F] : '=');
        encoded.push_back('=');
    }
    return encoded;
}

// auto | zh | en. The Windows language picker says zh-cn; an unknown value falls back to auto
// so the service detects the language itself instead of rejecting the request.
std::string MimoLanguage(std::string_view language)
{
    if (language == "zh-cn" || language == "zh")
        return "zh";
    if (language == "en")
        return "en";
    return "auto";
}
} // namespace

std::string BuildMimoTranscriptionBody(std::string_view wav, std::string_view model, std::string_view language)
{
    if (wav.empty() || wav.size() > kMimoMaximumWavBytes || model.empty())
        throw metasequoia::voice::VoiceError("Audio and transcription model are required within the upload limit");
    nlohmann::json audio;
    audio["type"] = "input_audio";
    audio["input_audio"]["data"] =
        "data:audio/wav;base64," + Base64Encode(reinterpret_cast<const unsigned char *>(wav.data()), wav.size());
    nlohmann::json message;
    message["role"] = "user";
    message["content"] = nlohmann::json::array({audio});
    nlohmann::json body;
    body["model"] = std::string(model);
    body["stream"] = false;
    body["messages"] = nlohmann::json::array({message});
    body["asr_options"] = {{"language", MimoLanguage(language)}};
    return body.dump();
}

std::string BuildMimoTranscription(const std::vector<float> &samples, const VoiceInputConfig &config)
{
    const auto wav =
        metasequoia::voice::WavWriter::create_wav(samples, metasequoia::voice::sample_rate, kMimoUploadSampleLimit);
    return BuildMimoTranscriptionBody(std::string_view(reinterpret_cast<const char *>(wav.data()), wav.size()),
                                      ResolveAsrModel(config), config.language);
}

bool ParseMimoTranscription(std::string_view response, std::string &text)
{
    text.clear();
    nlohmann::json json;
    try
    {
        json = nlohmann::json::parse(response.begin(), response.end());
    }
    catch (const nlohmann::json::exception &)
    {
        return false;
    }
    if (!json.is_object())
        return false;
    if (json.contains("choices") && json["choices"].is_array() && !json["choices"].empty())
    {
        const nlohmann::json &choice = json["choices"].front();
        if (choice.is_object() && choice.contains("message") && choice["message"].is_object() &&
            choice["message"].contains("content"))
        {
            // The documented shape is a plain string; a few gateways wrap it in content parts.
            const nlohmann::json &content = choice["message"]["content"];
            if (content.is_string())
                text = content.get<std::string>();
            else if (content.is_array())
            {
                for (const auto &part : content)
                {
                    if (part.is_object() && part.contains("text") && part["text"].is_string())
                        text += part["text"].get<std::string>();
                }
            }
        }
    }
    if (text.empty() && json.contains("text") && json["text"].is_string())
        text = json["text"].get<std::string>();
    return !text.empty();
}

metasequoia::voice::MultipartRequest BuildBatchTranscription(const std::vector<float> &samples,
                                                             const VoiceInputConfig &config)
{
    const bool siliconflow = NormalizeProviderId(config.asr_provider) == "siliconflow";
    std::vector<float> padded;
    if (siliconflow)
    {
        constexpr std::size_t pad_frames = metasequoia::voice::sample_rate / 5;
        padded.reserve(samples.size() + pad_frames * 2);
        padded.insert(padded.end(), pad_frames, 0.0f);
        padded.insert(padded.end(), samples.begin(), samples.end());
        padded.insert(padded.end(), pad_frames, 0.0f);
        if (padded.size() < metasequoia::voice::sample_rate)
            padded.resize(metasequoia::voice::sample_rate, 0.0f);
    }
    constexpr std::size_t upload_sample_limit = (metasequoia::voice::maximum_encoded_audio_bytes - 44) / 2;
    const auto wav = metasequoia::voice::WavWriter::create_wav(siliconflow ? padded : samples,
                                                               metasequoia::voice::sample_rate, upload_sample_limit);
    // SiliconFlow rejects the optional language field. Keep the existing Windows mapping elsewhere.
    const std::string language = !siliconflow && config.language == "zh-cn" ? "zh"
                                 : !siliconflow && config.language == "en"  ? "en"
                                                                            : "";
    return metasequoia::voice::make_transcription_request(
        std::string_view(reinterpret_cast<const char *>(wav.data()), wav.size()), ResolveAsrModel(config), language);
}

std::string BuildBatchPolish(const std::string &text, const VoiceInputConfig &config)
{
    auto body = nlohmann::json::parse(metasequoia::voice::make_polish_request(
        ResolvePolishModel(config), ResolvePolishSystemPrompt(config), WrapAsrUserMessage(text)));
    // Same rule as the AI assistant: vendor fields follow the endpoint that will be called, so a
    // slot labelled "siliconflow" but pointing at another service is not sent enable_thinking.
    const auto vendor = VendorForRequest(config.polish_provider, config.polish_endpoint);
    if (vendor == ChatVendor::SiliconFlow)
    {
        body["enable_thinking"] = false;
    }
    else if (UsesDeepSeekThinking(config.polish_provider, config.polish_endpoint, config.polish_thinking))
    {
        body["thinking"] = {{"type", "enabled"}};
        body["reasoning_effort"] = config.polish_thinking;
    }
    else if (vendor == ChatVendor::DeepSeek)
    {
        body["thinking"] = {{"type", "disabled"}};
    }
    return body.dump();
}
} // namespace VoiceInput
