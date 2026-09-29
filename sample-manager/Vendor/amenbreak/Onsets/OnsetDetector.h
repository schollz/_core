#pragma once

#include "Fvec.h"
#include "Cvec.h"
#include "Pvoc.h"
#include "Specdesc.h"
#include "PeakPicker.h"
#include "SpectralWhitening.h"
#include <memory>
#include <string>
#include <cstddef>

namespace Onsets {

/**
 * Main onset detector class.
 * Combines phase vocoder, spectral descriptor, and peak picker
 * to detect note onsets in audio.
 */
class OnsetDetector {
public:
    /**
     * Creates a new onset detector.
     * @param onsetMode Detection method ("hfc", "energy", "complex", etc.)
     * @param bufSize FFT buffer size (must be power of 2)
     * @param hopSize Hop size in samples
     * @param samplerate Sample rate in Hz
     */
    OnsetDetector(const std::string& onsetMode, size_t bufSizeIn, size_t hopSizeIn, size_t samplerateIn);

    /**
     * Processes an audio frame and detects onsets.
     * @param input Input audio samples (length = hopSize)
     * @param onset Output: positive value if onset detected, 0 otherwise
     */
    void processFrame(const Fvec& input, Fvec& onset);

    /**
     * Gets the time of the latest onset in samples.
     */
    size_t getLastOnset() const;

    /**
     * Gets the time of the latest onset in seconds.
     */
    double getLastOnsetS() const;

    /**
     * Gets the time of the latest onset in milliseconds.
     */
    double getLastOnsetMs() const;

    /**
     * Enables or disables adaptive whitening.
     */
    void setAWhitening(bool enable) { applyAWhitening = enable; }

    /**
     * Gets whether adaptive whitening is enabled.
     */
    bool getAWhitening() const { return applyAWhitening; }

    /**
     * Sets the compression lambda value.
     * Lambda = 0 disables compression.
     */
    void setCompression(double lambda);

    /**
     * Gets the compression lambda value (0 if disabled).
     */
    double getCompression() const;

    /**
     * Sets the silence threshold in dB.
     */
    void setSilence(double silenceThresh) { silence = silenceThresh; }

    /**
     * Gets the silence threshold.
     */
    double getSilence() const { return silence; }

    /**
     * Sets the peak picking threshold.
     */
    void setThreshold(double thresh) { peakPicker.setThreshold(thresh); }

    /**
     * Gets the peak picking threshold.
     */
    double getThreshold() const { return peakPicker.getThreshold(); }

    /**
     * Sets the minimum inter-onset interval in samples.
     */
    void setMinioi(size_t newMinioi) { minioi = newMinioi; }

    /**
     * Gets the minimum inter-onset interval in samples.
     */
    size_t getMinioi() const { return minioi; }

    /**
     * Sets the minimum inter-onset interval in seconds.
     */
    void setMinioiS(double minioiSec);

    /**
     * Gets the minimum inter-onset interval in seconds.
     */
    double getMinioiS() const;

    /**
     * Sets the minimum inter-onset interval in milliseconds.
     */
    void setMinioiMs(double minioiMs);

    /**
     * Gets the minimum inter-onset interval in milliseconds.
     */
    double getMinioiMs() const;

    /**
     * Sets the delay in samples.
     */
    void setDelay(size_t delayValue) { delay = delayValue; }

    /**
     * Gets the delay in samples.
     */
    size_t getDelay() const { return delay; }

    /**
     * Sets the delay in seconds.
     */
    void setDelayS(double delaySec);

    /**
     * Gets the delay in seconds.
     */
    double getDelayS() const;

    /**
     * Sets the delay in milliseconds.
     */
    void setDelayMs(double delayMs);

    /**
     * Gets the delay in milliseconds.
     */
    double getDelayMs() const;

    /**
     * Gets the current onset detection function value.
     */
    double getDescriptor() const { return desc.getData()[0]; }

    /**
     * Gets the thresholded onset detection function value.
     */
    double getThresholdedDescriptor() const;

    /**
     * Resets the detector state.
     */
    void reset();

    /**
     * Gets access to spectral whitening for advanced configuration.
     */
    SpectralWhitening& getSpectralWhitening() { return spectralWhitening; }

private:
    Pvoc pvoc;
    Specdesc onsetDesc;
    PeakPicker peakPicker;
    Cvec fftgrain;
    Fvec desc;

    double silence;
    size_t minioi;
    size_t delay;
    size_t samplerate;
    size_t hopSize;
    size_t totalFrames;
    size_t lastOnset;

    bool applyCompression;
    double lambdaCompression;
    bool applyAWhitening;
    SpectralWhitening spectralWhitening;

    void setDefaultParameters(const std::string& onsetMode);
};

} // namespace Onsets
