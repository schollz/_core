#pragma once

#include "Fvec.h"
#include "Cvec.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <cstddef>

namespace Onsets {

/**
 * Phase Vocoder - performs FFT analysis for onset detection.
 * Uses JUCE's FFT implementation for efficient spectral analysis.
 */
class Pvoc {
public:
    /**
     * Creates a new phase vocoder.
     * @param winSize Window size for FFT (must be a power of 2)
     * @param hopSize Hop size (number of samples between successive FFTs)
     */
    Pvoc(size_t winSizeIn, size_t hopSizeIn);

    /**
     * Processes input through the phase vocoder.
     * Applies windowing, performs FFT, and converts to polar form.
     *
     * @param input Input audio samples
     * @param fftgrain Output complex spectrum in polar form (magnitude and phase)
     */
    void doAnalysis(const Fvec& input, Cvec& fftgrain);

    /**
     * Gets the window size.
     */
    size_t getWinSize() const { return winSize; }

    /**
     * Gets the hop size.
     */
    size_t getHopSize() const { return hopSize; }

private:
    size_t winSize;
    size_t hopSize;

    std::unique_ptr<juce::dsp::FFT> fft;
    Fvec window;
    std::vector<float> fftBuffer;  // JUCE FFT works with floats
};

} // namespace Onsets
