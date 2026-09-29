#pragma once

#include "Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace amen::core
{

struct AudioData;

enum class ExportErrorCode
{
    InvalidRequest,
    InvalidWav,
    SliceMismatch,
    FilesystemError,
    RubberBandError,
    WavIoError
};

struct ExportError
{
    ExportErrorCode code = ExportErrorCode::InvalidRequest;
    std::string message;
};

struct ExportRequest
{
    std::filesystem::path inputAudioPath;
    std::filesystem::path outputBankFolder;
    int sourceBpm = 170;
    int projectBpm = 170;
    std::vector<double> sliceStartNorm;
    std::vector<double> sliceStopNorm;
    bool tempoMatch = true;
    bool oneShot = false;
    bool preservePitch = false;
    bool preserveChannels = true;
    bool writeSourceCopy = false;
    int spliceTrigger = 96;
};

struct ExportResult
{
    int slotIndex = 0;
    std::filesystem::path wav0Path;
    std::filesystem::path info0Path;
    std::filesystem::path wav1Path;
    std::filesystem::path info1Path;
    std::filesystem::path sourcePath;
};

Result<ExportResult, ExportError> exportToBankFolder(const ExportRequest& request);
Result<ExportResult, ExportError> exportToBankFolderFromAudio(const ExportRequest& request,
                                                              const AudioData& inputAudio);

} // namespace amen::core
