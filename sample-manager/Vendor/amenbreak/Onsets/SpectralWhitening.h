#pragma once

#include "Fvec.h"
#include "Cvec.h"
#include <cstddef>

namespace Onsets {

/**
 * Adaptive spectral whitening for onset detection.
 * Normalizes the spectrum by tracking peak values over time,
 * which helps emphasize transients and suppress steady-state components.
 */
class SpectralWhitening {
public:
    /**
     * Creates a new spectral whitening object.
     * @param bufSize FFT buffer size
     * @param hopSize Hop size in samples
     * @param samplerate Sample rate in Hz
     */
    SpectralWhitening(size_t bufSizeIn, size_t hopSizeIn, size_t samplerateIn);

    /**
     * Applies spectral whitening to the FFT grain.
     * Modifies the magnitude values in-place.
     */
    void doWhitening(Cvec& fftgrain);

    /**
     * Sets the relax time (decay time constant) in seconds.
     * Controls how quickly the peak tracking decays.
     */
    void setRelaxTime(double relaxTime);

    /**
     * Gets the current relax time in seconds.
     */
    double getRelaxTime() const { return relaxTime; }

    /**
     * Sets the floor value (minimum magnitude).
     */
    void setFloor(double floor);

    /**
     * Gets the floor value.
     */
    double getFloor() const { return floor; }

    /**
     * Resets the whitening state.
     */
    void reset();

private:
    size_t hopSize;
    size_t samplerate;
    double relaxTime;
    double rDecay;  // Decay coefficient
    double floor;

    Fvec peakValues;  // Track peak magnitudes per bin

    void updateDecayCoefficient();
};

} // namespace Onsets
