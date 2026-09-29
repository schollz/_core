#pragma once

#include <vector>
#include <cstddef>

namespace Onsets {

/**
 * Cvec represents a vector of complex-valued data stored in polar coordinates.
 * The data is stored as magnitude (norm) and phase arrays.
 * This format is efficient for spectral processing in onset detection.
 */
class Cvec {
public:
    /**
     * Creates a new Cvec from an FFT of the given length.
     * The actual size will be (length/2 + 1) for the positive frequencies.
     */
    explicit Cvec(size_t fftLength);

    /**
     * Gets the length of the complex vector (number of frequency bins).
     */
    size_t getLength() const { return norm.size(); }

    /**
     * Sets all norm and phase values to zero.
     */
    void zeros();

    /**
     * Sets the norm (magnitude) at a given position.
     */
    void setNorm(size_t position, double value);

    /**
     * Gets the norm (magnitude) at a given position.
     */
    double getNorm(size_t position) const;

    /**
     * Sets the phase at a given position.
     */
    void setPhas(size_t position, double value);

    /**
     * Gets the phase at a given position.
     */
    double getPhas(size_t position) const;

    /**
     * Copies data from source to this cvec.
     */
    void copy(const Cvec& source);

    /**
     * Applies logarithmic compression to magnitudes.
     * norm[i] = log(1 + lambda * norm[i])
     */
    void logMag(double lambda);

    /**
     * Direct access to norm array.
     */
    std::vector<double>& getNormData() { return norm; }
    const std::vector<double>& getNormData() const { return norm; }

    /**
     * Direct access to phase array.
     */
    std::vector<double>& getPhasData() { return phas; }
    const std::vector<double>& getPhasData() const { return phas; }

private:
    std::vector<double> norm;  // Magnitude array
    std::vector<double> phas;  // Phase array
};

} // namespace Onsets
