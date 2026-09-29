#include "Specdesc.h"
#include <algorithm>
#include <cmath>

namespace Onsets {

Specdesc::Specdesc(const std::string& onsetMode, size_t size)
    : onsetType(SpecdescType::HFC)
    , threshold(0.1)
    , oldMag(size / 2 + 1)
    , dev1(size / 2 + 1)
    , theta1(size / 2 + 1)
    , theta2(size / 2 + 1)
{
    // Parse onset mode string
    std::string mode = onsetMode;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

    if (mode == "energy")
        onsetType = SpecdescType::Energy;
    else if (mode == "specdiff")
        onsetType = SpecdescType::Specdiff;
    else if (mode == "hfc" || mode == "default")
        onsetType = SpecdescType::HFC;
    else if (mode == "complexdomain" || mode == "complex")
        onsetType = SpecdescType::Complex;
    else if (mode == "phase")
        onsetType = SpecdescType::Phase;
    else if (mode == "wphase")
        onsetType = SpecdescType::WPhase;
    else if (mode == "kl")
        onsetType = SpecdescType::KL;
    else if (mode == "mkl")
        onsetType = SpecdescType::MKL;
    else if (mode == "specflux")
        onsetType = SpecdescType::Specflux;
    else
        onsetType = SpecdescType::HFC;  // Default
}

void Specdesc::computeOnset(const Cvec& fftgrain, Fvec& onset)
{
    switch (onsetType)
    {
        case SpecdescType::Energy:
            energy(fftgrain, onset);
            break;
        case SpecdescType::HFC:
            hfc(fftgrain, onset);
            break;
        case SpecdescType::Complex:
            complexDomain(fftgrain, onset);
            break;
        case SpecdescType::Phase:
            phase(fftgrain, onset);
            break;
        case SpecdescType::WPhase:
            wphase(fftgrain, onset);
            break;
        case SpecdescType::Specdiff:
            specdiff(fftgrain, onset);
            break;
        case SpecdescType::KL:
            kl(fftgrain, onset);
            break;
        case SpecdescType::MKL:
            mkl(fftgrain, onset);
            break;
        case SpecdescType::Specflux:
            specflux(fftgrain, onset);
            break;
        default:
            hfc(fftgrain, onset);
            break;
    }
}

void Specdesc::energy(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    auto& onsetData = onset.getData();

    onsetData[0] = 0.0;
    for (size_t j = 0; j < norm.size(); ++j)
        onsetData[0] += norm[j] * norm[j];
}

void Specdesc::hfc(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    auto& onsetData = onset.getData();

    onsetData[0] = 0.0;
    for (size_t j = 0; j < norm.size(); ++j)
        onsetData[0] += static_cast<double>(j + 1) * norm[j];
}

void Specdesc::complexDomain(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    const auto& phas = fftgrain.getPhasData();
    auto& onsetData = onset.getData();
    auto& dev1Data = dev1.getData();
    auto& theta1Data = theta1.getData();
    auto& theta2Data = theta2.getData();
    auto& oldMagData = oldMag.getData();

    onsetData[0] = 0.0;

    for (size_t j = 0; j < norm.size(); ++j)
    {
        // Predict phase
        dev1Data[j] = 2.0 * theta1Data[j] - theta2Data[j];

        // Euclidean distance in complex domain
        double dev = dev1Data[j] - phas[j];
        double val = oldMagData[j] * oldMagData[j] +
                    norm[j] * norm[j] -
                    2.0 * oldMagData[j] * norm[j] * std::cos(dev);

        if (val > 0.0)
            onsetData[0] += std::sqrt(val);

        // Store old phase data
        theta2Data[j] = theta1Data[j];
        theta1Data[j] = phas[j];
        oldMagData[j] = norm[j];
    }
}

void Specdesc::phase(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    const auto& phas = fftgrain.getPhasData();
    auto& onsetData = onset.getData();
    auto& theta1Data = theta1.getData();

    onsetData[0] = 0.0;

    for (size_t j = 0; j < norm.size(); ++j)
    {
        double dev = std::abs(phas[j] - theta1Data[j]);
        if (threshold < norm[j])
            onsetData[0] += dev;

        theta1Data[j] = phas[j];
    }
}

void Specdesc::wphase(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    const auto& phas = fftgrain.getPhasData();
    auto& onsetData = onset.getData();
    auto& theta1Data = theta1.getData();

    onsetData[0] = 0.0;

    for (size_t j = 0; j < norm.size(); ++j)
    {
        double dev = std::abs(phas[j] - theta1Data[j]);
        if (threshold < norm[j])
            onsetData[0] += norm[j] * dev;

        theta1Data[j] = phas[j];
    }
}

void Specdesc::specdiff(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    auto& onsetData = onset.getData();
    auto& dev1Data = dev1.getData();
    auto& oldMagData = oldMag.getData();

    onsetData[0] = 0.0;

    for (size_t j = 0; j < norm.size(); ++j)
    {
        double val = norm[j] * norm[j] - oldMagData[j] * oldMagData[j];
        if (val > 0.0)
            dev1Data[j] = std::sqrt(val);
        else
            dev1Data[j] = 0.0;

        if (threshold < norm[j])
            onsetData[0] += std::abs(dev1Data[j]);

        oldMagData[j] = norm[j];
    }
}

void Specdesc::kl(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    auto& onsetData = onset.getData();
    auto& oldMagData = oldMag.getData();

    onsetData[0] = 0.0;

    for (size_t j = 0; j < norm.size(); ++j)
    {
        onsetData[0] += norm[j] * std::log(1.0 + norm[j] / (oldMagData[j] + 1e-1));
        oldMagData[j] = norm[j];
    }

    if (std::isnan(onsetData[0]))
        onsetData[0] = 0.0;
}

void Specdesc::mkl(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    auto& onsetData = onset.getData();
    auto& oldMagData = oldMag.getData();

    onsetData[0] = 0.0;

    for (size_t j = 0; j < norm.size(); ++j)
    {
        onsetData[0] += std::log(1.0 + norm[j] / (oldMagData[j] + 1e-1));
        oldMagData[j] = norm[j];
    }

    if (std::isnan(onsetData[0]))
        onsetData[0] = 0.0;
}

void Specdesc::specflux(const Cvec& fftgrain, Fvec& onset)
{
    const auto& norm = fftgrain.getNormData();
    auto& onsetData = onset.getData();
    auto& oldMagData = oldMag.getData();

    onsetData[0] = 0.0;

    for (size_t j = 0; j < norm.size(); ++j)
    {
        if (norm[j] > oldMagData[j])
            onsetData[0] += norm[j] - oldMagData[j];

        oldMagData[j] = norm[j];
    }
}

} // namespace Onsets
