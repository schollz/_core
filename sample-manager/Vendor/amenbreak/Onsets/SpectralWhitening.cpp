#include "SpectralWhitening.h"
#include <cmath>
#include <algorithm>

namespace Onsets {

namespace {
    constexpr double DEFAULT_RELAX_TIME = 250.0;  // seconds
    constexpr double DEFAULT_DECAY = 0.001;       // -60dB attenuation
    constexpr double DEFAULT_FLOOR = 1.0e-4;
}

SpectralWhitening::SpectralWhitening(size_t bufSizeIn, size_t hopSizeIn, size_t samplerateIn)
    : hopSize(hopSizeIn)
    , samplerate(samplerateIn)
    , relaxTime(DEFAULT_RELAX_TIME)
    , rDecay(0.0)
    , floor(DEFAULT_FLOOR)
    , peakValues(bufSizeIn / 2 + 1)
{
    updateDecayCoefficient();
    reset();
}

void SpectralWhitening::doWhitening(Cvec& fftgrain)
{
    auto& norm = fftgrain.getNormData();
    auto& peakData = peakValues.getData();

    size_t length = std::min(norm.size(), peakData.size());

    for (size_t i = 0; i < length; ++i)
    {
        double tmp = std::max(rDecay * peakData[i], floor);
        peakData[i] = std::max(norm[i], tmp);

        if (peakData[i] > 0.0)
            norm[i] /= peakData[i];
    }
}

void SpectralWhitening::setRelaxTime(double newRelaxTime)
{
    relaxTime = newRelaxTime;
    updateDecayCoefficient();
}

void SpectralWhitening::setFloor(double newFloor)
{
    floor = newFloor;
}

void SpectralWhitening::reset()
{
    auto& peakData = peakValues.getData();
    std::fill(peakData.begin(), peakData.end(), floor);
}

void SpectralWhitening::updateDecayCoefficient()
{
    // Calculate decay coefficient based on relax time
    // rDecay = DECAY^((hopSize/samplerate) / relaxTime)
    rDecay = std::pow(DEFAULT_DECAY,
                      (static_cast<double>(hopSize) / static_cast<double>(samplerate)) / relaxTime);
}

} // namespace Onsets
