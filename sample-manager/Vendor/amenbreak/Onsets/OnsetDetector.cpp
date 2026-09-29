#include "OnsetDetector.h"
#include "MathUtils.h"
#include <algorithm>

namespace Onsets {

OnsetDetector::OnsetDetector(const std::string& onsetMode, size_t bufSizeIn, size_t hopSizeIn, size_t samplerateIn)
    : pvoc(bufSizeIn, hopSizeIn)
    , onsetDesc(onsetMode, bufSizeIn)
    , peakPicker()
    , fftgrain(bufSizeIn)
    , desc(1)
    , silence(-70.0)
    , minioi(0)
    , delay(0)
    , samplerate(samplerateIn)
    , hopSize(hopSizeIn)
    , totalFrames(0)
    , lastOnset(0)
    , applyCompression(false)
    , lambdaCompression(0.0)
    , applyAWhitening(false)
    , spectralWhitening(bufSizeIn, hopSizeIn, samplerateIn)
{
    setDefaultParameters(onsetMode);
    reset();
}

void OnsetDetector::processFrame(const Fvec& input, Fvec& onset)
{
    auto& onsetData = onset.getData();
    double isonset = 0.0;

    // Phase vocoder
    pvoc.doAnalysis(input, fftgrain);

    // Apply adaptive whitening if enabled
    if (applyAWhitening)
        spectralWhitening.doWhitening(fftgrain);

    // Apply compression if enabled
    if (applyCompression)
        fftgrain.logMag(lambdaCompression);

    // Compute spectral descriptor
    onsetDesc.computeOnset(fftgrain, desc);

    // Peak picking
    peakPicker.doPeakPicking(desc, onset);
    isonset = onsetData[0];

    if (isonset > 0.0)
    {
        if (MathUtils::silenceDetection(input, silence))
        {
            // Silent onset, not marking
            isonset = 0.0;
        }
        else
        {
            // We have an onset
            size_t newOnset = totalFrames + static_cast<size_t>(MathUtils::round(isonset * static_cast<double>(hopSize)));

            // Check if last onset time was more than minioi ago
            if (lastOnset + minioi < newOnset)
            {
                // Start of file: make sure (new_onset - delay) >= 0
                if (lastOnset > 0 && delay > newOnset)
                    isonset = 0.0;
                else
                    lastOnset = MathUtils::max(delay, newOnset);
            }
            else
            {
                // Doubled onset, not marking
                isonset = 0.0;
            }
        }
    }
    else
    {
        // We are at the beginning of the file
        if (totalFrames <= delay)
        {
            // And we don't find silence
            if (!MathUtils::silenceDetection(input, silence))
            {
                size_t newOnset = totalFrames;
                if (totalFrames == 0 || lastOnset + minioi < newOnset)
                {
                    isonset = static_cast<double>(delay) / static_cast<double>(hopSize);
                    lastOnset = totalFrames + delay;
                }
            }
        }
    }

    onsetData[0] = isonset;
    totalFrames += hopSize;
}

size_t OnsetDetector::getLastOnset() const
{
    if (delay > lastOnset)
        return 0;
    return lastOnset - delay;
}

double OnsetDetector::getLastOnsetS() const
{
    return static_cast<double>(getLastOnset()) / static_cast<double>(samplerate);
}

double OnsetDetector::getLastOnsetMs() const
{
    return getLastOnsetS() * 1000.0;
}

void OnsetDetector::setCompression(double lambda)
{
    if (lambda < 0.0)
        return;

    lambdaCompression = lambda;
    applyCompression = (lambda > 0.0);
}

double OnsetDetector::getCompression() const
{
    if (applyCompression)
        return lambdaCompression;
    return 0.0;
}

void OnsetDetector::setMinioiS(double minioiSec)
{
    setMinioi(static_cast<size_t>(MathUtils::round(minioiSec * static_cast<double>(samplerate))));
}

double OnsetDetector::getMinioiS() const
{
    return static_cast<double>(minioi) / static_cast<double>(samplerate);
}

void OnsetDetector::setMinioiMs(double minioiMs)
{
    setMinioiS(minioiMs / 1000.0);
}

double OnsetDetector::getMinioiMs() const
{
    return getMinioiS() * 1000.0;
}

void OnsetDetector::setDelayS(double delaySec)
{
    setDelay(static_cast<size_t>(delaySec * static_cast<double>(samplerate)));
}

double OnsetDetector::getDelayS() const
{
    return static_cast<double>(delay) / static_cast<double>(samplerate);
}

void OnsetDetector::setDelayMs(double delayMs)
{
    setDelayS(delayMs / 1000.0);
}

double OnsetDetector::getDelayMs() const
{
    return getDelayS() * 1000.0;
}

double OnsetDetector::getThresholdedDescriptor() const
{
    return peakPicker.getThresholdedInput().getData()[0];
}

void OnsetDetector::reset()
{
    lastOnset = 0;
    totalFrames = 0;
}

void OnsetDetector::setDefaultParameters(const std::string& onsetMode)
{
    // Set default parameters
    setThreshold(0.3);
    setDelay(static_cast<size_t>(4.3 * static_cast<double>(hopSize)));
    setMinioiMs(50.0);
    setSilence(-70.0);
    setAWhitening(false);
    setCompression(0.0);

    // Method-specific optimizations
    std::string mode = onsetMode;
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

    if (mode == "energy")
    {
        // Use defaults
    }
    else if (mode == "hfc" || mode == "default")
    {
        setThreshold(0.058);
        setCompression(1.0);
    }
    else if (mode == "complexdomain" || mode == "complex")
    {
        setDelay(static_cast<size_t>(4.6 * static_cast<double>(hopSize)));
        setThreshold(0.15);
        setAWhitening(true);
        setCompression(1.0);
    }
    else if (mode == "phase")
    {
        setAWhitening(false);
        setCompression(0.0);
    }
    else if (mode == "wphase")
    {
        // Use defaults
    }
    else if (mode == "mkl")
    {
        setThreshold(0.05);
        setAWhitening(true);
        setCompression(0.02);
    }
    else if (mode == "kl")
    {
        setThreshold(0.35);
        setAWhitening(true);
        setCompression(0.02);
    }
    else if (mode == "specflux")
    {
        setThreshold(0.18);
        setAWhitening(true);
        spectralWhitening.setRelaxTime(100.0);
        spectralWhitening.setFloor(1.0);
        setCompression(10.0);
    }
    else if (mode == "specdiff")
    {
        // Use defaults
    }
    else if (mode == "old_default")
    {
        setThreshold(0.3);
        setMinioiMs(20.0);
        setCompression(0.0);
    }
}

} // namespace Onsets
