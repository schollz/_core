#pragma once

#include "Fvec.h"
#include "Cvec.h"
#include <string>
#include <cstddef>

namespace Onsets {

/**
 * Spectral descriptor types for onset detection.
 */
enum class SpecdescType {
    Energy,      // Energy-based detection
    Specdiff,    // Spectral difference
    HFC,         // High Frequency Content
    Complex,     // Complex domain
    Phase,       // Phase-based
    WPhase,      // Weighted phase deviation
    KL,          // Kullback-Liebler divergence
    MKL,         // Modified Kullback-Liebler
    Specflux     // Spectral flux
};

/**
 * Spectral descriptor for onset detection.
 * Implements 9 different onset detection methods that analyze
 * the spectral content to identify transients.
 */
class Specdesc {
public:
    /**
     * Creates a new spectral descriptor.
     * @param onsetMode String identifier for the onset detection method
     *                  ("energy", "hfc", "complex", "phase", "wphase",
     *                   "specdiff", "kl", "mkl", "specflux")
     * @param size FFT buffer size
     */
    Specdesc(const std::string& onsetMode, size_t size);

    /**
     * Computes the onset detection function value for the given FFT frame.
     * @param fftgrain Input spectrum (magnitude and phase)
     * @param onset Output onset detection function value (single value in onset.data[0])
     */
    void computeOnset(const Cvec& fftgrain, Fvec& onset);

    /**
     * Gets the current onset detection type.
     */
    SpecdescType getType() const { return onsetType; }

    /**
     * Sets the threshold for phase-based methods.
     */
    void setThreshold(double thresh) { threshold = thresh; }

    /**
     * Gets the threshold.
     */
    double getThreshold() const { return threshold; }

private:
    SpecdescType onsetType;
    double threshold;

    // State variables for different methods
    Fvec oldMag;    // Previous magnitude values
    Fvec dev1;      // Development/difference buffer
    Fvec theta1;    // Previous phase values
    Fvec theta2;    // Phase values from 2 frames ago

    // Individual onset detection methods
    void energy(const Cvec& fftgrain, Fvec& onset);
    void hfc(const Cvec& fftgrain, Fvec& onset);
    void complexDomain(const Cvec& fftgrain, Fvec& onset);
    void phase(const Cvec& fftgrain, Fvec& onset);
    void wphase(const Cvec& fftgrain, Fvec& onset);
    void specdiff(const Cvec& fftgrain, Fvec& onset);
    void kl(const Cvec& fftgrain, Fvec& onset);
    void mkl(const Cvec& fftgrain, Fvec& onset);
    void specflux(const Cvec& fftgrain, Fvec& onset);
};

} // namespace Onsets
