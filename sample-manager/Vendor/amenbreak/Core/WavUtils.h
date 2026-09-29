#pragma once

#include "Result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <variant>

namespace amen::core
{

struct AudioData
{
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> interleavedSamples;

    std::size_t frameCount() const
    {
        if (channels <= 0)
            return 0;
        return interleavedSamples.size() / static_cast<std::size_t>(channels);
    }

    bool empty() const
    {
        return frameCount() == 0;
    }
};

enum class WavIoErrorCode
{
    FileOpenFailed,
    InvalidFormat,
    UnsupportedFormat,
    DecodeFailed,
    EncodeFailed
};

struct WavIoError
{
    WavIoErrorCode code = WavIoErrorCode::DecodeFailed;
    std::string message;
};

Result<AudioData, WavIoError> readWavFile(const std::filesystem::path& path);
Result<std::monostate, WavIoError> writeWavFile16(const std::filesystem::path& path, const AudioData& audio);

AudioData resampleLinear(const AudioData& source, int targetSampleRate);
AudioData convertChannels(const AudioData& source, int targetChannels);

} // namespace amen::core
