#include "Exporter.h"

#include "WavUtils.h"

#include <rubberband/RubberBandStretcher.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace amen::core
{

namespace fs = std::filesystem;

namespace
{

constexpr int kOutputSampleRate = 44100;
constexpr double kPadSeconds = 0.5;
constexpr double kTimestretchTempoFactor = 0.125;
constexpr double kMinus6DbGain = 0.5011872336272722; // pow(10, -6/20)
constexpr int kMaxSlots = 16;
constexpr std::size_t kRubberBandBlockSize = 8192;
constexpr int kDefaultBpm = 170;
constexpr int kInfoBpmMin = 1;
constexpr int kInfoBpmMax = 510;

ExportError makeError(ExportErrorCode code, const std::string& message)
{
    return ExportError{code, message};
}

template <typename T>
void appendLe16(std::vector<std::uint8_t>& data, T value)
{
    const auto v = static_cast<std::uint16_t>(value);
    data.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    data.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
}

template <typename T>
void appendLe32(std::vector<std::uint8_t>& data, T value)
{
    const auto v = static_cast<std::uint32_t>(value);
    data.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    data.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    data.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    data.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}

void appendLe32Signed(std::vector<std::uint8_t>& data, std::int32_t value)
{
    appendLe32(data, static_cast<std::uint32_t>(value));
}

std::string randomSuffix()
{
    static std::mt19937_64 rng{std::random_device{}()};
    std::ostringstream ss;
    ss << std::hex << rng();
    return ss.str();
}

Result<std::monostate, ExportError> writeBinaryAtomic(const fs::path& targetPath, const std::vector<std::uint8_t>& bytes)
{
    const fs::path tmpPath = targetPath.string() + ".tmp." + randomSuffix();

    std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::FilesystemError, "Could not create temp file: " + tmpPath.string()));
    }

    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    out.close();

    if (!out)
    {
        std::error_code removeEc;
        fs::remove(tmpPath, removeEc);
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::FilesystemError, "Failed to write temp file: " + tmpPath.string()));
    }

    std::error_code ec;
    fs::remove(targetPath, ec);
    ec.clear();
    fs::rename(tmpPath, targetPath, ec);
    if (ec)
    {
        fs::remove(tmpPath, ec);
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::FilesystemError, "Failed to atomically replace " + targetPath.string()));
    }

    return Result<std::monostate, ExportError>::ok(std::monostate{});
}

Result<std::monostate, ExportError> writeWavAtomic(const fs::path& targetPath, const AudioData& audio)
{
    const fs::path tmpPath = targetPath.string() + ".tmp." + randomSuffix();
    const auto writeResult = writeWavFile16(tmpPath, audio);
    if (writeResult.isErr())
    {
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::WavIoError, writeResult.error().message));
    }

    std::error_code ec;
    fs::remove(targetPath, ec);
    ec.clear();
    fs::rename(tmpPath, targetPath, ec);
    if (ec)
    {
        fs::remove(tmpPath, ec);
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::FilesystemError, "Failed to atomically replace " + targetPath.string()));
    }

    return Result<std::monostate, ExportError>::ok(std::monostate{});
}

Result<int, ExportError> findTargetSlot(const fs::path& bankFolder)
{
    for (int slot = 0; slot < kMaxSlots; ++slot)
    {
        const fs::path marker = bankFolder / (std::to_string(slot) + ".0.wav.info");
        if (!fs::exists(marker))
            return Result<int, ExportError>::ok(slot);
    }

    // Wrap policy selected in plan.
    return Result<int, ExportError>::ok(0);
}

Result<std::monostate, ExportError> validateRequestCommon(const ExportRequest& request)
{
    if (request.outputBankFolder.empty())
    {
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::InvalidRequest, "outputBankFolder is required"));
    }

    if (request.sliceStartNorm.empty())
    {
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::SliceMismatch, "sliceStartNorm cannot be empty"));
    }

    if (request.sliceStartNorm.size() != request.sliceStopNorm.size())
    {
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::SliceMismatch, "sliceStartNorm and sliceStopNorm sizes must match"));
    }

    if (request.sliceStartNorm.size() > 255)
    {
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::SliceMismatch, "A maximum of 255 slices is supported"));
    }

    double lastStart = -1.0;
    double lastStop = -1.0;
    for (std::size_t i = 0; i < request.sliceStartNorm.size(); ++i)
    {
        const double start = request.sliceStartNorm[i];
        const double stop = request.sliceStopNorm[i];
        if (start < 0.0 || start > 1.0 || stop < 0.0 || stop > 1.0)
        {
            return Result<std::monostate, ExportError>::err(
                makeError(ExportErrorCode::SliceMismatch, "Slice positions must be normalized [0,1]"));
        }
        if (start >= stop)
        {
            return Result<std::monostate, ExportError>::err(
                makeError(ExportErrorCode::SliceMismatch, "Each slice start must be less than stop"));
        }
        if (start < lastStart || stop < lastStop)
        {
            return Result<std::monostate, ExportError>::err(
                makeError(ExportErrorCode::SliceMismatch, "Slices must be monotonic"));
        }
        lastStart = start;
        lastStop = stop;
    }

    return Result<std::monostate, ExportError>::ok(std::monostate{});
}

Result<std::monostate, ExportError> validateSourceAudio(const AudioData& source)
{
    if (source.channels < 1 || source.channels > 2)
    {
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::InvalidWav, "Only mono and stereo input is supported in v1"));
    }

    if (source.sampleRate <= 0 || source.frameCount() == 0)
    {
        return Result<std::monostate, ExportError>::err(
            makeError(ExportErrorCode::InvalidWav, "Input audio is empty or has invalid sample rate"));
    }

    return Result<std::monostate, ExportError>::ok(std::monostate{});
}

bool hasVariableSliceSpacing(const ExportRequest& request)
{
    constexpr double kEpsilon = 1.0e-5;

    const std::size_t sliceCount = request.sliceStartNorm.size();
    if (sliceCount <= 1)
        return false;

    const double totalSpan = request.sliceStopNorm.back() - request.sliceStartNorm.front();
    if (totalSpan <= 0.0)
        return true;

    const double expectedDuration = totalSpan / static_cast<double>(sliceCount);

    for (std::size_t i = 0; i < sliceCount; ++i)
    {
        const double duration = request.sliceStopNorm[i] - request.sliceStartNorm[i];
        if (std::abs(duration - expectedDuration) > kEpsilon)
            return true;

        if (i > 0)
        {
            const double gap = request.sliceStartNorm[i] - request.sliceStopNorm[i - 1];
            if (std::abs(gap) > kEpsilon)
                return true;
        }
    }

    return false;
}

AudioData prependAppendPadding(const AudioData& source)
{
    const std::size_t frames = source.frameCount();
    if (frames == 0 || source.channels <= 0)
        return source;

    const std::size_t padFramesRequested = static_cast<std::size_t>(std::llround(kPadSeconds * source.sampleRate));
    const std::size_t padFrames = std::min(frames, padFramesRequested);

    AudioData out;
    out.sampleRate = source.sampleRate;
    out.channels = source.channels;
    out.interleavedSamples.resize((frames + padFrames * 2) * static_cast<std::size_t>(source.channels), 0.0f);

    const std::size_t c = static_cast<std::size_t>(source.channels);

    // end -> front
    const std::size_t endStart = frames - padFrames;
    for (std::size_t frame = 0; frame < padFrames; ++frame)
    {
        for (std::size_t ch = 0; ch < c; ++ch)
        {
            out.interleavedSamples[(frame * c) + ch] =
                source.interleavedSamples[((endStart + frame) * c) + ch];
        }
    }

    // original
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        for (std::size_t ch = 0; ch < c; ++ch)
        {
            out.interleavedSamples[((padFrames + frame) * c) + ch] =
                source.interleavedSamples[(frame * c) + ch];
        }
    }

    // front -> end
    for (std::size_t frame = 0; frame < padFrames; ++frame)
    {
        for (std::size_t ch = 0; ch < c; ++ch)
        {
            out.interleavedSamples[((padFrames + frames + frame) * c) + ch] =
                source.interleavedSamples[(frame * c) + ch];
        }
    }

    return out;
}

void normalizeAndApplyGain(AudioData& audio)
{
    float maxAbs = 0.0f;
    for (float s : audio.interleavedSamples)
        maxAbs = std::max(maxAbs, std::abs(s));

    const float normScale = (maxAbs > 1.0e-12f) ? (1.0f / maxAbs) : 1.0f;
    const float scale = normScale * static_cast<float>(kMinus6DbGain);

    for (float& s : audio.interleavedSamples)
    {
        s *= scale;
        s = std::max(-1.0f, std::min(1.0f, s));
    }
}

Result<AudioData, ExportError> prepareProcessedVariant(const AudioData& source)
{
    AudioData padded = prependAppendPadding(source);
    AudioData resampled = resampleLinear(padded, kOutputSampleRate);
    normalizeAndApplyGain(resampled);
    return Result<AudioData, ExportError>::ok(std::move(resampled));
}

Result<AudioData, ExportError> timeStretchRubberBand(const AudioData& source, double tempoFactor)
{
    if (tempoFactor <= 0.0)
    {
        return Result<AudioData, ExportError>::err(
            makeError(ExportErrorCode::InvalidRequest, "tempoFactor must be > 0"));
    }

    const std::size_t frames = source.frameCount();
    if (frames == 0 || source.channels <= 0)
    {
        return Result<AudioData, ExportError>::err(
            makeError(ExportErrorCode::InvalidWav, "Cannot stretch empty audio"));
    }

    const double timeRatio = 1.0 / tempoFactor;
    const auto options = RubberBand::RubberBandStretcher::OptionProcessOffline |
                         RubberBand::RubberBandStretcher::OptionEngineFaster |
                         RubberBand::RubberBandStretcher::OptionPitchHighSpeed |
                         RubberBand::RubberBandStretcher::OptionThreadingAuto;

    RubberBand::RubberBandStretcher stretcher(
        static_cast<std::size_t>(source.sampleRate),
        static_cast<std::size_t>(source.channels),
        options,
        timeRatio,
        1.0);

    std::vector<std::vector<float>> inChannels(static_cast<std::size_t>(source.channels),
                                               std::vector<float>(frames, 0.0f));

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        for (int ch = 0; ch < source.channels; ++ch)
        {
            inChannels[static_cast<std::size_t>(ch)][frame] =
                source.interleavedSamples[frame * static_cast<std::size_t>(source.channels) + static_cast<std::size_t>(ch)];
        }
    }

    std::vector<const float*> inPtrs(static_cast<std::size_t>(source.channels), nullptr);

    for (std::size_t offset = 0; offset < frames; offset += kRubberBandBlockSize)
    {
        const std::size_t block = std::min(kRubberBandBlockSize, frames - offset);
        const bool finalBlock = (offset + block >= frames);

        for (int ch = 0; ch < source.channels; ++ch)
            inPtrs[static_cast<std::size_t>(ch)] = inChannels[static_cast<std::size_t>(ch)].data() + offset;

        stretcher.study(inPtrs.data(), block, finalBlock);
    }

    std::vector<std::vector<float>> outChannels(static_cast<std::size_t>(source.channels));
    const std::size_t expectedFrames = static_cast<std::size_t>(std::llround(frames * timeRatio));
    for (auto& ch : outChannels)
        ch.reserve(expectedFrames + 16384);

    std::vector<float*> outPtrs(static_cast<std::size_t>(source.channels), nullptr);
    std::vector<std::vector<float>> tempOut(static_cast<std::size_t>(source.channels));

    for (std::size_t offset = 0; offset < frames; offset += kRubberBandBlockSize)
    {
        const std::size_t block = std::min(kRubberBandBlockSize, frames - offset);
        const bool finalBlock = (offset + block >= frames);

        for (int ch = 0; ch < source.channels; ++ch)
            inPtrs[static_cast<std::size_t>(ch)] = inChannels[static_cast<std::size_t>(ch)].data() + offset;

        stretcher.process(inPtrs.data(), block, finalBlock);

        while (stretcher.available() > 0)
        {
            const auto available = static_cast<std::size_t>(stretcher.available());
            if (available == 0)
                break;

            for (int ch = 0; ch < source.channels; ++ch)
            {
                tempOut[static_cast<std::size_t>(ch)].assign(available, 0.0f);
                outPtrs[static_cast<std::size_t>(ch)] = tempOut[static_cast<std::size_t>(ch)].data();
            }

            const std::size_t retrieved = stretcher.retrieve(outPtrs.data(), available);
            if (retrieved == 0)
                break;

            for (int ch = 0; ch < source.channels; ++ch)
            {
                auto& out = outChannels[static_cast<std::size_t>(ch)];
                const auto& temp = tempOut[static_cast<std::size_t>(ch)];
                out.insert(out.end(), temp.begin(), temp.begin() + static_cast<std::ptrdiff_t>(retrieved));
            }
        }
    }

    const std::size_t outFrames = outChannels.empty() ? 0 : outChannels.front().size();
    if (outFrames == 0)
    {
        return Result<AudioData, ExportError>::err(
            makeError(ExportErrorCode::RubberBandError, "RubberBand returned no output"));
    }

    AudioData out;
    out.sampleRate = source.sampleRate;
    out.channels = source.channels;
    out.interleavedSamples.resize(outFrames * static_cast<std::size_t>(source.channels), 0.0f);

    for (std::size_t frame = 0; frame < outFrames; ++frame)
    {
        for (int ch = 0; ch < source.channels; ++ch)
        {
            out.interleavedSamples[frame * static_cast<std::size_t>(source.channels) + static_cast<std::size_t>(ch)] =
                outChannels[static_cast<std::size_t>(ch)][frame];
        }
    }

    return Result<AudioData, ExportError>::ok(std::move(out));
}

Result<AudioData, ExportError> varispeedConvert(const AudioData& source, double tempoRatio)
{
    if (tempoRatio <= 0.0)
    {
        return Result<AudioData, ExportError>::err(
            makeError(ExportErrorCode::InvalidRequest, "tempoRatio must be > 0"));
    }

    if (source.frameCount() == 0 || source.channels <= 0 || source.sampleRate <= 0)
    {
        return Result<AudioData, ExportError>::err(
            makeError(ExportErrorCode::InvalidWav, "Cannot varispeed-convert empty or invalid audio"));
    }

    const int conversionRate = std::max(1, static_cast<int>(std::llround(
                                           static_cast<double>(source.sampleRate) / tempoRatio)));
    AudioData converted = resampleLinear(source, conversionRate);
    converted.sampleRate = source.sampleRate; // Keep output sample rate stable; duration change encodes varispeed.
    return Result<AudioData, ExportError>::ok(std::move(converted));
}

int sanitizeInfoBpm(int bpm)
{
    return std::clamp(bpm, kInfoBpmMin, kInfoBpmMax);
}

std::uint32_t computeInfoSizeBytes(const AudioData& processed)
{
    const std::int64_t frames = static_cast<std::int64_t>(processed.frameCount());
    std::int64_t totalSamples = frames - static_cast<std::int64_t>(kOutputSampleRate);
    if (totalSamples < 0)
        totalSamples = 0;

    const std::int64_t fsize = totalSamples * processed.channels * 2;
    return static_cast<std::uint32_t>(std::max<std::int64_t>(0, fsize));
}

std::vector<std::uint8_t> buildInfoPayload(const ExportRequest& request,
                                           const AudioData& processed,
                                           const std::uint32_t fsize,
                                           const int infoBpm)
{
    const std::size_t sliceCount = request.sliceStartNorm.size();

    std::uint16_t oneShot = request.oneShot ? 1 : 0;
    std::uint16_t tempoMatch = request.tempoMatch ? 1 : 0;
    std::uint16_t oversampling = 0; // v1 fixed at 1x
    std::uint16_t numChannels = (processed.channels > 1) ? 1 : 0;
    std::uint16_t version = 1;
    std::uint16_t playMode = 0;
    std::uint16_t spliceVariable = hasVariableSliceSpacing(request) ? 1 : 0;

    const std::uint32_t flags = static_cast<std::uint32_t>(sanitizeInfoBpm(infoBpm) & 0x1FF) |
                                (static_cast<std::uint32_t>(playMode & 0x7) << 9) |
                                (static_cast<std::uint32_t>(oneShot & 0x1) << 12) |
                                (static_cast<std::uint32_t>(tempoMatch & 0x1) << 13) |
                                (static_cast<std::uint32_t>(oversampling & 0x1) << 14) |
                                (static_cast<std::uint32_t>(numChannels & 0x1) << 15) |
                                (static_cast<std::uint32_t>(version & 0x7F) << 16);

    const std::uint16_t spliceInfo = static_cast<std::uint16_t>((request.spliceTrigger & 0x7FFF) |
                                                                 ((spliceVariable & 0x1) << 15));

    std::vector<std::uint8_t> payload;
    payload.reserve(11 + sliceCount * 9 + 6);

    appendLe32(payload, fsize);
    appendLe32(payload, flags);
    appendLe16(payload, spliceInfo);
    payload.push_back(static_cast<std::uint8_t>(sliceCount));

    for (double pos : request.sliceStartNorm)
    {
        std::int32_t s = static_cast<std::int32_t>(std::llround(pos * static_cast<double>(fsize)));
        s = (s / 4) * 4;
        if (s < 0)
            s = 0;
        if (s > static_cast<std::int32_t>(fsize))
            s = static_cast<std::int32_t>(fsize);
        appendLe32Signed(payload, s);
    }

    for (double pos : request.sliceStopNorm)
    {
        std::int32_t e = static_cast<std::int32_t>(std::llround(pos * static_cast<double>(fsize)));
        e = (e / 4) * 4;
        if (e < 0)
            e = 0;
        if (e > static_cast<std::int32_t>(fsize))
            e = static_cast<std::int32_t>(fsize);
        appendLe32Signed(payload, e);
    }

    for (std::size_t i = 0; i < sliceCount; ++i)
        payload.push_back(0); // sliceType

    appendLe16(payload, 0); // transient group 1 count
    appendLe16(payload, 0); // transient group 2 count
    appendLe16(payload, 0); // transient group 3 count

    return payload;
}

} // namespace

Result<ExportResult, ExportError> exportFromSourceAudio(const ExportRequest& request,
                                                        const AudioData& inputAudio)
{
    const auto valid = validateRequestCommon(request);
    if (valid.isErr())
        return Result<ExportResult, ExportError>::err(valid.error());

    const auto sourceValid = validateSourceAudio(inputAudio);
    if (sourceValid.isErr())
        return Result<ExportResult, ExportError>::err(sourceValid.error());

    std::error_code ec;
    fs::create_directories(request.outputBankFolder, ec);
    if (ec)
    {
        return Result<ExportResult, ExportError>::err(
            makeError(ExportErrorCode::FilesystemError, "Could not create output folder: " + request.outputBankFolder.string()));
    }

    AudioData source = inputAudio;
    const int targetChannels = request.preserveChannels ? source.channels : 1;
    source = convertChannels(source, targetChannels);

    const auto slotResult = findTargetSlot(request.outputBankFolder);
    if (slotResult.isErr())
        return Result<ExportResult, ExportError>::err(slotResult.error());

    const int slot = slotResult.value();

    const bool hasValidSourceBpm = request.sourceBpm > 0;
    const bool hasValidProjectBpm = request.projectBpm > 0;
    const int infoBpm = hasValidProjectBpm
        ? request.projectBpm
        : (hasValidSourceBpm ? request.sourceBpm : kDefaultBpm);

    AudioData convertedBase = source;
    if (hasValidSourceBpm && hasValidProjectBpm && request.sourceBpm != request.projectBpm)
    {
        const double tempoRatio =
            static_cast<double>(request.projectBpm) / static_cast<double>(request.sourceBpm);
        const auto convertedResult = request.preservePitch
            ? timeStretchRubberBand(source, tempoRatio)
            : varispeedConvert(source, tempoRatio);
        if (convertedResult.isErr())
            return Result<ExportResult, ExportError>::err(convertedResult.error());
        convertedBase = convertedResult.value();
    }

    const auto stretchedResult = timeStretchRubberBand(convertedBase, kTimestretchTempoFactor);
    if (stretchedResult.isErr())
        return Result<ExportResult, ExportError>::err(stretchedResult.error());

    const auto variant0Result = prepareProcessedVariant(convertedBase);
    if (variant0Result.isErr())
        return Result<ExportResult, ExportError>::err(variant0Result.error());

    const auto variant1Result = prepareProcessedVariant(stretchedResult.value());
    if (variant1Result.isErr())
        return Result<ExportResult, ExportError>::err(variant1Result.error());

    const AudioData& variant0 = variant0Result.value();
    const AudioData& variant1 = variant1Result.value();

    const fs::path wav0Path = request.outputBankFolder / (std::to_string(slot) + ".0.wav");
    const fs::path info0Path = request.outputBankFolder / (std::to_string(slot) + ".0.wav.info");
    const fs::path wav1Path = request.outputBankFolder / (std::to_string(slot) + ".1.wav");
    const fs::path info1Path = request.outputBankFolder / (std::to_string(slot) + ".1.wav.info");
    const fs::path sourcePath = request.outputBankFolder / (std::to_string(slot) + ".source.wav");

    const auto writeWav0 = writeWavAtomic(wav0Path, variant0);
    if (writeWav0.isErr())
        return Result<ExportResult, ExportError>::err(writeWav0.error());

    const auto writeWav1 = writeWavAtomic(wav1Path, variant1);
    if (writeWav1.isErr())
        return Result<ExportResult, ExportError>::err(writeWav1.error());

    const auto info0Payload = buildInfoPayload(request, variant0, computeInfoSizeBytes(variant0), infoBpm);
    const auto info1Payload = buildInfoPayload(request, variant1, computeInfoSizeBytes(variant1), infoBpm);

    const auto writeInfo0 = writeBinaryAtomic(info0Path, info0Payload);
    if (writeInfo0.isErr())
        return Result<ExportResult, ExportError>::err(writeInfo0.error());

    const auto writeInfo1 = writeBinaryAtomic(info1Path, info1Payload);
    if (writeInfo1.isErr())
        return Result<ExportResult, ExportError>::err(writeInfo1.error());

    if (request.writeSourceCopy)
    {
        const auto writeSource = writeWavAtomic(sourcePath, source);
        if (writeSource.isErr())
            return Result<ExportResult, ExportError>::err(writeSource.error());
    }

    ExportResult result;
    result.slotIndex = slot;
    result.wav0Path = fs::absolute(wav0Path);
    result.info0Path = fs::absolute(info0Path);
    result.wav1Path = fs::absolute(wav1Path);
    result.info1Path = fs::absolute(info1Path);
    if (request.writeSourceCopy)
        result.sourcePath = fs::absolute(sourcePath);

    return Result<ExportResult, ExportError>::ok(std::move(result));
}

Result<ExportResult, ExportError> exportToBankFolder(const ExportRequest& request)
{
    if (request.inputAudioPath.empty())
    {
        return Result<ExportResult, ExportError>::err(
            makeError(ExportErrorCode::InvalidRequest, "inputAudioPath is required"));
    }

    const auto readResult = readWavFile(request.inputAudioPath);
    if (readResult.isErr())
    {
        return Result<ExportResult, ExportError>::err(
            makeError(ExportErrorCode::InvalidWav, readResult.error().message));
    }

    return exportFromSourceAudio(request, readResult.value());
}

Result<ExportResult, ExportError> exportToBankFolderFromAudio(const ExportRequest& request,
                                                              const AudioData& inputAudio)
{
    return exportFromSourceAudio(request, inputAudio);
}

} // namespace amen::core
