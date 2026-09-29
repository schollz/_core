#pragma once

#include "Fvec.h"
#include <cstddef>
#include <cmath>

namespace Onsets {

/**
 * Utility functions for onset detection mathematics.
 */
namespace MathUtils {

/**
 * Checks if the input is below the silence threshold.
 * @param input The input vector to check
 * @param threshold The silence threshold in dB
 * @return true if the signal is below the threshold (silent)
 */
bool silenceDetection(const Fvec& input, double threshold);

/**
 * Pushes a new element to the end of the vector, shifting all elements left.
 * The first element is discarded.
 */
void fvecPush(Fvec& v, double newElem);

/**
 * Computes the median of a vector using quickselect algorithm.
 * Creates a copy internally, so the input is not modified.
 */
double fvecMedian(const Fvec& input);

/**
 * Computes the mean of a vector.
 */
double fvecMean(const Fvec& input);

/**
 * Checks if the position is a local peak in the onset function.
 * A peak must be greater than its neighbors and greater than zero.
 */
bool fvecPeakPick(const Fvec& onset, size_t pos);

/**
 * Finds the quadratic interpolated peak position.
 * Uses parabolic interpolation for sub-sample accuracy.
 */
double fvecQuadraticPeakPos(const Fvec& x, size_t pos);

/**
 * Simple median implementation using sort.
 */
double medianSimple(const std::vector<double>& data);

/**
 * Returns the maximum of two values.
 */
inline size_t max(size_t a, size_t b) { return (a > b) ? a : b; }

/**
 * Rounds a floating-point value to the nearest integer.
 */
inline int round(double x) { return static_cast<int>(std::floor(x + 0.5)); }

} // namespace MathUtils

} // namespace Onsets
