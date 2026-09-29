#include "WavUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace amen::core
{

namespace
{

constexpr std::uint16_t kWavFormatPcm = 0x0001;
constexpr std::uint16_t kWavFormatIeeeFloat = 0x0003;
constexpr std::uint16_t kWavFormatExtensible = 0xFFFE;

std::uint16_t readLe16(const std::uint8_t* p)
{
    return static_cast<std::uint16_t>(p[0]) |
           (static_cast<std::uint16_t>(p[1]) << 8);
}

std::uint32_t readLe32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

std::uint16_t readLe16(std::istream& is)
{
    std::uint8_t buf[2] = {};
    is.read(reinterpret_cast<char*>(buf), 2);
    return readLe16(buf);
}

std::uint32_t readLe32(std::istream& is)
{
    std::uint8_t buf[4] = {};
    is.read(reinterpret_cast<char*>(buf), 4);
    return readLe32(buf);
}

void writeLe16(std::ostream& os, std::uint16_t v)
{
    const std::uint8_t buf[2] = {
        static_cast<std::uint8_t>(v & 0xFFu),
        static_cast<std::uint8_t>((v >> 8) & 0xFFu)
    };
    os.write(reinterpret_cast<const char*>(buf), 2);
}

void writeLe32(std::ostream& os, std::uint32_t v)
{
    const std::uint8_t buf[4] = {
        static_cast<std::uint8_t>(v & 0xFFu),
        static_cast<std::uint8_t>((v >> 8) & 0xFFu),
        static_cast<std::uint8_t>((v >> 16) & 0xFFu),
        static_cast<std::uint8_t>((v >> 24) & 0xFFu)
    };
    os.write(reinterpret_cast<const char*>(buf), 4);
}

float clampToUnit(float v)
{
    return std::max(-1.0f, std::min(1.0f, v));
}

} // namespace

Result<AudioData, WavIoError> readWavFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return Result<AudioData, WavIoError>::err({WavIoErrorCode::FileOpenFailed,
                                                   "Could not open WAV file: " + path.string()});
    }

    char riff[4] = {};
    char wave[4] = {};
    in.read(riff, 4);
    (void)readLe32(in); // file size
    in.read(wave, 4);

    if (!in || std::string(riff, 4) != "RIFF" || std::string(wave, 4) != "WAVE")
    {
        return Result<AudioData, WavIoError>::err({WavIoErrorCode::InvalidFormat,
                                                   "File is not a RIFF/WAVE file: " + path.string()});
    }

    std::vector<std::uint8_t> fmtChunk;
    std::vector<std::uint8_t> dataChunk;

    while (in && (!fmtChunk.size() || !dataChunk.size()))
    {
        char chunkIdChars[4] = {};
        in.read(chunkIdChars, 4);
        if (!in)
            break;

        const std::uint32_t chunkSize = readLe32(in);
        if (!in)
            break;

        const std::string chunkId(chunkIdChars, 4);

        if (chunkId == "fmt ")
        {
            fmtChunk.resize(chunkSize);
            if (chunkSize > 0)
                in.read(reinterpret_cast<char*>(fmtChunk.data()), static_cast<std::streamsize>(chunkSize));
        }
        else if (chunkId == "data")
        {
            dataChunk.resize(chunkSize);
            if (chunkSize > 0)
                in.read(reinterpret_cast<char*>(dataChunk.data()), static_cast<std::streamsize>(chunkSize));
        }
        else
        {
            in.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
        }

        if (chunkSize % 2u == 1u)
            in.seekg(1, std::ios::cur);
    }

    if (fmtChunk.size() < 16 || dataChunk.empty())
    {
        return Result<AudioData, WavIoError>::err({WavIoErrorCode::InvalidFormat,
                                                   "WAV missing fmt or data chunk: " + path.string()});
    }

    std::uint16_t formatTag = readLe16(fmtChunk.data() + 0);
    const int channels = static_cast<int>(readLe16(fmtChunk.data() + 2));
    const int sampleRate = static_cast<int>(readLe32(fmtChunk.data() + 4));
    std::uint16_t bitsPerSample = readLe16(fmtChunk.data() + 14);

    if (formatTag == kWavFormatExtensible)
    {
        if (fmtChunk.size() < 40)
        {
            return Result<AudioData, WavIoError>::err({WavIoErrorCode::UnsupportedFormat,
                                                       "Unsupported WAVE_FORMAT_EXTENSIBLE fmt size"});
        }

        bitsPerSample = readLe16(fmtChunk.data() + 18);
        formatTag = readLe16(fmtChunk.data() + 24);
    }

    if (channels <= 0 || sampleRate <= 0)
    {
        return Result<AudioData, WavIoError>::err({WavIoErrorCode::InvalidFormat,
                                                   "Invalid channels or sample rate in WAV"});
    }

    int bytesPerSample = 0;
    bool isFloat = false;

    if (formatTag == kWavFormatPcm)
    {
        if (bitsPerSample == 8)
            bytesPerSample = 1;
        else if (bitsPerSample == 16)
            bytesPerSample = 2;
        else if (bitsPerSample == 24)
            bytesPerSample = 3;
        else if (bitsPerSample == 32)
            bytesPerSample = 4;
        else
        {
            return Result<AudioData, WavIoError>::err({WavIoErrorCode::UnsupportedFormat,
                                                       "Unsupported PCM bit depth: " + std::to_string(bitsPerSample)});
        }
    }
    else if (formatTag == kWavFormatIeeeFloat)
    {
        if (bitsPerSample != 32)
        {
            return Result<AudioData, WavIoError>::err({WavIoErrorCode::UnsupportedFormat,
                                                       "Only 32-bit float WAV is supported"});
        }
        bytesPerSample = 4;
        isFloat = true;
    }
    else
    {
        return Result<AudioData, WavIoError>::err({WavIoErrorCode::UnsupportedFormat,
                                                   "Unsupported WAV format tag: " + std::to_string(formatTag)});
    }

    const std::size_t bytesPerFrame = static_cast<std::size_t>(channels * bytesPerSample);
    if (bytesPerFrame == 0)
    {
        return Result<AudioData, WavIoError>::err({WavIoErrorCode::DecodeFailed,
                                                   "Invalid bytes per frame"});
    }

    const std::size_t frames = dataChunk.size() / bytesPerFrame;
    if (frames == 0)
    {
        return Result<AudioData, WavIoError>::err({WavIoErrorCode::DecodeFailed,
                                                   "No audio frames in WAV data"});
    }

    AudioData audio;
    audio.sampleRate = sampleRate;
    audio.channels = channels;
    audio.interleavedSamples.resize(frames * static_cast<std::size_t>(channels));

    std::size_t src = 0;
    for (std::size_t i = 0; i < frames * static_cast<std::size_t>(channels); ++i)
    {
        float s = 0.0f;
        if (isFloat)
        {
            union
            {
                std::uint32_t u;
                float f;
            } conv {};
            conv.u = readLe32(&dataChunk[src]);
            s = conv.f;
            src += 4;
        }
        else if (bitsPerSample == 8)
        {
            s = (static_cast<int>(dataChunk[src]) - 128) / 128.0f;
            src += 1;
        }
        else if (bitsPerSample == 16)
        {
            const std::int16_t v = static_cast<std::int16_t>(readLe16(&dataChunk[src]));
            s = static_cast<float>(v) / 32768.0f;
            src += 2;
        }
        else if (bitsPerSample == 24)
        {
            std::int32_t v = static_cast<std::int32_t>(dataChunk[src]) |
                             (static_cast<std::int32_t>(dataChunk[src + 1]) << 8) |
                             (static_cast<std::int32_t>(dataChunk[src + 2]) << 16);
            if ((v & 0x00800000) != 0)
                v |= static_cast<std::int32_t>(0xFF000000);

            s = static_cast<float>(v) / 8388608.0f;
            src += 3;
        }
        else if (bitsPerSample == 32)
        {
            const std::int32_t v = static_cast<std::int32_t>(readLe32(&dataChunk[src]));
            s = static_cast<float>(v) / 2147483648.0f;
            src += 4;
        }

        audio.interleavedSamples[i] = clampToUnit(s);
    }

    return Result<AudioData, WavIoError>::ok(std::move(audio));
}

Result<std::monostate, WavIoError> writeWavFile16(const std::filesystem::path& path, const AudioData& audio)
{
    if (audio.channels <= 0 || audio.sampleRate <= 0)
    {
        return Result<std::monostate, WavIoError>::err({WavIoErrorCode::EncodeFailed,
                                                        "AudioData has invalid channels or sample rate"});
    }

    if (audio.interleavedSamples.size() % static_cast<std::size_t>(audio.channels) != 0)
    {
        return Result<std::monostate, WavIoError>::err({WavIoErrorCode::EncodeFailed,
                                                        "AudioData sample count is not divisible by channels"});
    }

    const std::size_t frames = audio.frameCount();
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(frames * static_cast<std::size_t>(audio.channels) * 2u);
    const std::uint32_t riffSize = 36u + dataBytes;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return Result<std::monostate, WavIoError>::err({WavIoErrorCode::FileOpenFailed,
                                                        "Could not open output WAV path: " + path.string()});
    }

    out.write("RIFF", 4);
    writeLe32(out, riffSize);
    out.write("WAVE", 4);

    out.write("fmt ", 4);
    writeLe32(out, 16);
    writeLe16(out, kWavFormatPcm);
    writeLe16(out, static_cast<std::uint16_t>(audio.channels));
    writeLe32(out, static_cast<std::uint32_t>(audio.sampleRate));
    writeLe32(out, static_cast<std::uint32_t>(audio.sampleRate * audio.channels * 2));
    writeLe16(out, static_cast<std::uint16_t>(audio.channels * 2));
    writeLe16(out, 16);

    out.write("data", 4);
    writeLe32(out, dataBytes);

    for (float sample : audio.interleavedSamples)
    {
        const float clamped = clampToUnit(sample);
        const std::int16_t pcm16 = (clamped <= -1.0f)
            ? std::numeric_limits<std::int16_t>::min()
            : static_cast<std::int16_t>(std::lround(clamped * 32767.0f));

        writeLe16(out, static_cast<std::uint16_t>(pcm16));
    }

    if (!out)
    {
        return Result<std::monostate, WavIoError>::err({WavIoErrorCode::EncodeFailed,
                                                        "Failed while writing WAV file: " + path.string()});
    }

    return Result<std::monostate, WavIoError>::ok(std::monostate{});
}

AudioData convertChannels(const AudioData& source, int targetChannels)
{
    if (source.channels == targetChannels)
        return source;

    AudioData converted;
    converted.sampleRate = source.sampleRate;
    converted.channels = targetChannels;

    const std::size_t frames = source.frameCount();
    converted.interleavedSamples.resize(frames * static_cast<std::size_t>(targetChannels), 0.0f);

    if (source.channels <= 0 || targetChannels <= 0)
        return converted;

    if (targetChannels == 1)
    {
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            double sum = 0.0;
            for (int ch = 0; ch < source.channels; ++ch)
                sum += source.interleavedSamples[frame * static_cast<std::size_t>(source.channels) + static_cast<std::size_t>(ch)];

            converted.interleavedSamples[frame] = static_cast<float>(sum / source.channels);
        }

        return converted;
    }

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        for (int ch = 0; ch < targetChannels; ++ch)
        {
            const int srcChannel = std::min(ch, source.channels - 1);
            converted.interleavedSamples[frame * static_cast<std::size_t>(targetChannels) + static_cast<std::size_t>(ch)] =
                source.interleavedSamples[frame * static_cast<std::size_t>(source.channels) + static_cast<std::size_t>(srcChannel)];
        }
    }

    return converted;
}

AudioData resampleLinear(const AudioData& source, int targetSampleRate)
{
    if (source.sampleRate == targetSampleRate || source.channels <= 0 || source.frameCount() == 0)
        return source;

    AudioData out;
    out.sampleRate = targetSampleRate;
    out.channels = source.channels;

    const std::size_t srcFrames = source.frameCount();
    const double ratio = static_cast<double>(targetSampleRate) / static_cast<double>(source.sampleRate);
    const std::size_t dstFrames = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(srcFrames * ratio)));

    out.interleavedSamples.resize(dstFrames * static_cast<std::size_t>(source.channels), 0.0f);

    for (int ch = 0; ch < source.channels; ++ch)
    {
        for (std::size_t i = 0; i < dstFrames; ++i)
        {
            const double srcPos = static_cast<double>(i) / ratio;
            const std::size_t idx0 = static_cast<std::size_t>(std::floor(srcPos));
            const std::size_t idx1 = std::min<std::size_t>(idx0 + 1, srcFrames - 1);
            const double frac = srcPos - static_cast<double>(idx0);

            const float s0 = source.interleavedSamples[idx0 * static_cast<std::size_t>(source.channels) + static_cast<std::size_t>(ch)];
            const float s1 = source.interleavedSamples[idx1 * static_cast<std::size_t>(source.channels) + static_cast<std::size_t>(ch)];
            const float y = static_cast<float>((1.0 - frac) * s0 + frac * s1);

            out.interleavedSamples[i * static_cast<std::size_t>(source.channels) + static_cast<std::size_t>(ch)] = y;
        }
    }

    return out;
}

} // namespace amen::core
