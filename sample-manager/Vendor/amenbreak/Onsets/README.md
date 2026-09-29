# Onset Detection Library

A complete C++ implementation of onset detection algorithms, ported from the Go implementation and using JUCE's FFT.

## Features

- **9 Onset Detection Methods:**
  - Energy
  - HFC (High Frequency Content)
  - Complex Domain
  - Phase
  - Weighted Phase (WPhase)
  - Spectral Difference (Specdiff)
  - Kullback-Liebler (KL)
  - Modified Kullback-Liebler (MKL)
  - Spectral Flux (Specflux)

- **Consensus Mode:** Runs all methods and clusters the results
- **Optimization:** Variance-based onset position refinement
- **Minimum Spacing Filter:** Prevents too-close onset detection
- **Energy-based Selection:** Find the N strongest onsets

## Quick Start

### Basic Usage

```cpp
#include "Onsets/SliceAnalyzer.h"

// Your audio samples (mono)
std::vector<double> audioSamples = { /* ... */ };
double sampleRate = 44100.0;

// Simple onset detection with default options
Onsets::SliceAnalyzerOptions options;
options.method = "hfc";  // High Frequency Content (default)

auto result = Onsets::SliceAnalyzer::analyzeSlices(audioSamples, sampleRate, options);

// result.onsets contains onset times in seconds
for (double onsetTime : result.onsets) {
    std::cout << "Onset at " << onsetTime << " seconds\n";
}
```

### Find Best N Slices

```cpp
Onsets::SliceAnalyzerOptions options;
options.numSlices = 8;  // Find 8 best slices
options.method = "hfc";

auto result = Onsets::SliceAnalyzer::analyzeSlices(audioSamples, sampleRate, options);
```

### Consensus Mode

```cpp
Onsets::SliceAnalyzerOptions options;
options.method = "consensus";  // Use all methods and cluster results
options.minConsensusClusterSize = 3;  // Require at least 3 methods to agree

auto result = Onsets::SliceAnalyzer::analyzeSlices(audioSamples, sampleRate, options);
```

### Advanced Options

```cpp
Onsets::SliceAnalyzerOptions options;
options.method = "hfc";
options.optimize = true;  // Enable variance-based optimization
options.optimizeWindowMs = 100.0;  // 100ms optimization window
options.useMinimumSpacing = true;  // Filter close onsets
options.minimumSpacing = 80.0;  // 80ms minimum spacing

auto result = Onsets::SliceAnalyzer::analyzeSlices(audioSamples, sampleRate, options);
```

### Low-Level API

For more control, use the OnsetDetector directly:

```cpp
#include "Onsets/OnsetDetector.h"
#include "Onsets/Fvec.h"

size_t bufSize = 512;
size_t hopSize = 256;
size_t sampleRate = 44100;

Onsets::OnsetDetector detector("hfc", bufSize, hopSize, sampleRate);
detector.setThreshold(0.3);
detector.setMinioiMs(50.0);  // 50ms minimum between onsets

Onsets::Fvec input(hopSize);
Onsets::Fvec output(1);

// Process audio in chunks
for (size_t pos = 0; pos + hopSize < samples.size(); pos += hopSize) {
    // Fill input buffer
    for (size_t i = 0; i < hopSize; ++i)
        input.getData()[i] = samples[pos + i];

    // Process
    detector.processFrame(input, output);

    // Check for onset
    if (output.getData()[0] > 0.0) {
        double onsetTime = detector.getLastOnsetS();
        std::cout << "Onset at " << onsetTime << " seconds\n";
    }
}
```

## Architecture

### Class Hierarchy

1. **Fvec** - Floating-point vector container
2. **Cvec** - Complex vector in polar form (magnitude + phase)
3. **MathUtils** - Utility functions for signal processing
4. **Filter** - Biquad IIR filter
5. **Pvoc** - Phase vocoder (FFT analysis using JUCE)
6. **SpectralWhitening** - Adaptive spectral whitening
7. **Specdesc** - Spectral descriptors (9 methods)
8. **PeakPicker** - Adaptive peak picking
9. **OnsetDetector** - Main onset detector (combines all components)
10. **SliceAnalyzer** - High-level API with advanced features

### Detection Methods

- **energy**: Simple energy-based detection
- **hfc**: High Frequency Content (recommended for general use)
- **complex**: Complex domain detection (good for harmonic content)
- **phase**: Phase-based detection
- **wphase**: Weighted phase deviation
- **specdiff**: Spectral difference
- **kl**: Kullback-Liebler divergence
- **mkl**: Modified Kullback-Liebler
- **specflux**: Spectral flux
- **consensus**: Combines all methods via clustering

## Method Selection Guide

- **General percussion/drums:** `hfc` or `complex`
- **Harmonic instruments:** `complex` or `mkl`
- **Noisy/complex material:** `specflux` or `consensus`
- **Fast/lightweight:** `energy` or `hfc`
- **Maximum accuracy:** `consensus` (slower, uses all methods)

## Performance

- FFT size: 512 samples
- Hop size: 256 samples
- Typical latency: ~12ms at 44.1kHz
- Real-time capable for single-method detection
- Consensus mode is ~9x slower (runs all methods)

## Thread Safety

The library is not thread-safe by default. Each thread should use its own detector instance.

## Dependencies

- JUCE (for juce::dsp::FFT)
- C++14 or later
- Standard library (vector, algorithm, cmath)
