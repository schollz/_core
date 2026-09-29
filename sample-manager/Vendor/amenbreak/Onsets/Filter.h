#pragma once

#include "Fvec.h"
#include <vector>
#include <cstddef>

namespace Onsets {

/**
 * Digital filter implementation (biquad and higher order).
 * Implements an IIR filter with feedforward (B) and feedback (A) coefficients.
 */
class Filter {
public:
    /**
     * Creates a new filter with the given order.
     * Default coefficients create an identity filter.
     */
    explicit Filter(size_t order);

    /**
     * Creates a biquad filter (2nd order) with the given coefficients.
     * Transfer function: H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
     */
    static Filter createBiquad(double b0, double b1, double b2, double a1, double a2);

    /**
     * Applies the filter to the input vector in-place.
     */
    void doFilter(Fvec& input);

    /**
     * Applies the filter forward and backward to avoid phase distortion.
     * This is similar to MATLAB's filtfilt function.
     */
    void doFiltFilt(Fvec& input, Fvec& tmp);

    /**
     * Resets the filter history (clears all delay lines).
     */
    void reset();

    /**
     * Gets the filter order.
     */
    size_t getOrder() const { return order; }

private:
    size_t order;
    std::vector<double> a;  // Feedback coefficients
    std::vector<double> b;  // Feedforward coefficients
    std::vector<double> x;  // Input history
    std::vector<double> y;  // Output history
};

} // namespace Onsets
