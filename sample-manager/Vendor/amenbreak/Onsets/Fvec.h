#pragma once

#include <vector>
#include <cstddef>

namespace Onsets {

/**
 * Fvec represents a vector of real-valued floating-point data.
 * This is used throughout the onset detection system for audio samples
 * and intermediate calculations.
 */
class Fvec {
public:
    /**
     * Creates a new Fvec with the specified length.
     * All values are initialized to zero.
     */
    explicit Fvec(size_t length);

    /**
     * Gets the length of the vector.
     */
    size_t getLength() const { return data.size(); }

    /**
     * Sets all values in the vector to zero.
     */
    void zeros();

    /**
     * Sets a value at the specified position.
     * If position is out of bounds, this operation has no effect.
     */
    void set(size_t position, double value);

    /**
     * Gets the value at the specified position.
     * Returns 0 if position is out of bounds.
     */
    double get(size_t position) const;

    /**
     * Copies data from source to this vector.
     * Only copies up to the minimum of the two lengths.
     */
    void copy(const Fvec& source);

    /**
     * Calculates the mean of all values in the vector.
     */
    double mean() const;

    /**
     * Returns the maximum value in the vector.
     */
    double max() const;

    /**
     * Returns the minimum value in the vector.
     */
    double min() const;

    /**
     * Multiplies all elements by a scalar weight.
     */
    void weight(double w);

    /**
     * Copies data from source with a weight factor applied.
     */
    void weightedCopy(const Fvec& source, double w);

    /**
     * Calculates local energy in decibels.
     * Returns -90.0 dB if energy is zero or negative.
     */
    double localEnergyDB() const;

    /**
     * Direct access to the underlying data.
     */
    std::vector<double>& getData() { return data; }
    const std::vector<double>& getData() const { return data; }

private:
    std::vector<double> data;
};

} // namespace Onsets
