#pragma once
#include "config/ime_config.h"
#include <msime/voice/provider_protocol.h>
#include <string>
#include <string_view>
#include <vector>

namespace VoiceInput
{
metasequoia::voice::MultipartRequest BuildBatchTranscription(const std::vector<float> &samples,
                                                             const VoiceInputConfig &config);
// MiMo serves ASR through Chat Completions instead of multipart /audio/transcriptions: the WAV
// travels as a base64 data URL in messages[0].content[0].input_audio, and the language is an
// asr_options field. The body builder is separate so the settings credential probe can reuse it.
std::string BuildMimoTranscription(const std::vector<float> &samples, const VoiceInputConfig &config);
std::string BuildMimoTranscriptionBody(std::string_view wav, std::string_view model, std::string_view language);
// Reads choices[0].message.content from the non-streaming response. Returns false and clears
// `text` when the body is not JSON or carries no transcript.
bool ParseMimoTranscription(std::string_view response, std::string &text);
std::string BuildBatchPolish(const std::string &text, const VoiceInputConfig &config);
} // namespace VoiceInput
