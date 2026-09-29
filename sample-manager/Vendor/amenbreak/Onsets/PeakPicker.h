#pragma once

#include "Fvec.h"
#include "Filter.h"
#include <memory>
#include <cstddef>

namespace Onsets {

/**
 * Peak picker for onset detection.
 * Identifies peaks in the onset detection function using adaptive thresholding
 * and temporal filtering.
 */
class PeakPicker {
public:
    /**
     * Creates a new peak picker with default parameters.
     */
    PeakPicker();

    /**
     * Performs peak picking on the onset detection function.
     * @param onset Input onset detection function value
     * @param out Output: positive value if peak detected, 0 otherwise
     */
    void doPeakPicking(const Fvec& onset, Fvec& out);

    /**
     * Sets the peak picking threshold multiplier.
     * Higher values make detection more conservative.
     */
    void setThreshold(double thresh) { threshold = thresh; }

    /**
     * Gets the threshold.
     */
    double getThreshold() const { return threshold; }

    /**
     * Gets the thresholded input (for debugging/visualization).
     */
    const Fvec& getThresholdedInput() const { return thresholded; }

private:
    double threshold;
    size_t winPost;  // Look-ahead window size
    size_t winPre;   // Look-back window size

    Filter biquad;      // Lowpass filter for smoothing
    Fvec onsetKeep;     // Raw onset history
    Fvec onsetProc;     // Processed onset history
    Fvec onsetPeek;     // 3-point window for peak detection
    Fvec thresholded;   // Thresholded value
    Fvec scratch;       // Scratch buffer for filtering
};

} // namespace Onsets
