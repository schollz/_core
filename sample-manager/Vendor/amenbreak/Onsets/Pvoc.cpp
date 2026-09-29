#include "Pvoc.h"
#include <cmath>

// Fallback definition for M_PI on platforms where it's not defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Onsets {

Pvoc::Pvoc(size_t winSizeIn, size_t hopSizeIn)
    : winSize(winSizeIn)
    , hopSize(hopSizeIn)
    , window(winSizeIn)
    , fftBuffer(winSizeIn * 2, 0.0f)  // Real + imaginary parts
{
    // Calculate FFT order (log2 of winSize)
    int order = 0;
    size_t size = winSize;
    while (size > 1)
    {
        size >>= 1;
        ++order;
    }

    // Create JUCE FFT object
    fft = std::make_unique<juce::dsp::FFT>(order);

    // Create Hann window
    auto& windowData = window.getData();
    for (size_t i = 0; i < winSize; ++i)
    {
        windowData[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * static_cast<double>(i) / static_cast<double>(winSize));
    }
}

void Pvoc::doAnalysis(const Fvec& input, Cvec& fftgrain)
{
    const auto& inputData = input.getData();
    auto& windowData = window.getData();

    // Copy input to FFT buffer with windowing
    for (size_t i = 0; i < winSize; ++i)
    {
        if (i < inputData.size())
            fftBuffer[i] = static_cast<float>(inputData[i] * windowData[i]);
        else
            fftBuffer[i] = 0.0f;
    }

    // Clear imaginary part
    for (size_t i = winSize; i < fftBuffer.size(); ++i)
        fftBuffer[i] = 0.0f;

    // Perform FFT using JUCE
    fft->performRealOnlyForwardTransform(fftBuffer.data());

    // Convert to polar form (magnitude and phase)
    // JUCE stores FFT output as: [real0, real1, ..., realN/2, imag1, ..., imagN/2-1]
    auto& norm = fftgrain.getNormData();
    auto& phas = fftgrain.getPhasData();

    size_t numBins = fftgrain.getLength();

    for (size_t i = 0; i < numBins; ++i)
    {
        float real, imag;

        if (i == 0)
        {
            // DC component (purely real)
            real = fftBuffer[0];
            imag = 0.0f;
        }
        else if (i == winSize / 2)
        {
            // Nyquist component (purely real)
            real = fftBuffer[winSize / 2];
            imag = 0.0f;
        }
        else
        {
            // Regular bins
            // Real part is in first half, imaginary in second half
            real = fftBuffer[i];
            imag = fftBuffer[winSize - i];  // Mirror layout
        }

        // Calculate magnitude and phase
        norm[i] = std::sqrt(real * real + imag * imag);
        phas[i] = std::atan2(imag, real);
    }
}

} // namespace Onsets
