/**
 * Example usage of the Onset Detection Library
 *
 * This file demonstrates how to use the onset detection library
 * in a JUCE plugin context. It is not compiled - just for reference.
 */

#include "SliceAnalyzer.h"
#include <juce_audio_basics/juce_audio_basics.h>

// Example 1: Analyze a JUCE AudioBuffer
void analyzeJuceBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // Convert JUCE buffer to std::vector<double> (left channel only)
    std::vector<double> samples;
    samples.reserve(buffer.getNumSamples());

    const float* channelData = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        samples.push_back(static_cast<double>(channelData[i]));

    // Analyze with default options
    Onsets::SliceAnalyzerOptions options;
    options.method = "hfc";

    auto result = Onsets::SliceAnalyzer::analyzeSlices(samples, sampleRate, options);

    // Print results
    DBG("Found " + juce::String(result.onsets.size()) + " onsets:");
    for (double onsetTime : result.onsets)
        DBG("  " + juce::String(onsetTime, 3) + " seconds");
}

// Example 2: Find 8 best slices for a drum loop sampler
std::vector<double> findDrumSlices(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // Convert to double vector
    std::vector<double> samples;
    samples.reserve(buffer.getNumSamples());

    const float* channelData = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        samples.push_back(static_cast<double>(channelData[i]));

    // Configure for 8-slice drum sampler
    Onsets::SliceAnalyzerOptions options;
    options.numSlices = 8;  // Find exactly 8 slices
    options.method = "hfc";  // Good for drums
    options.optimize = true;  // Refine positions
    options.optimizeWindowMs = 100.0;
    options.useMinimumSpacing = true;  // Prevent too-close slices
    options.minimumSpacing = 80.0;  // 80ms minimum spacing

    auto result = Onsets::SliceAnalyzer::analyzeSlices(samples, sampleRate, options);
    return result.onsets;
}

// Example 3: Real-time onset detection in audio callback
class RealtimeOnsetDetector
{
public:
    RealtimeOnsetDetector(double sampleRate)
        : detector("hfc", 512, 256, static_cast<size_t>(sampleRate))
        , input(256)
        , output(1)
        , buffer(256)
    {
        detector.setThreshold(0.3);
        detector.setMinioiMs(50.0);
    }

    void processBlock(const juce::AudioBuffer<float>& audioBuffer)
    {
        const float* channelData = audioBuffer.getReadPointer(0);
        int numSamples = audioBuffer.getNumSamples();

        // Accumulate samples
        for (int i = 0; i < numSamples; ++i)
        {
            buffer.push_back(static_cast<double>(channelData[i]));

            // Process when we have enough samples
            if (buffer.size() >= 256)
            {
                // Copy to input buffer
                auto& inputData = input.getData();
                for (size_t j = 0; j < 256; ++j)
                    inputData[j] = buffer[j];

                // Remove processed samples
                buffer.erase(buffer.begin(), buffer.begin() + 256);

                // Detect onset
                detector.processFrame(input, output);

                if (output.getData()[0] > 0.0)
                {
                    double onsetTime = detector.getLastOnsetS();
                    onOnsetDetected(onsetTime);
                }
            }
        }
    }

    void onOnsetDetected(double time)
    {
        DBG("Real-time onset detected at " + juce::String(time, 3) + " seconds");
        // Trigger something, send MIDI, etc.
    }

private:
    Onsets::OnsetDetector detector;
    Onsets::Fvec input;
    Onsets::Fvec output;
    std::vector<double> buffer;
};

// Example 4: Compare different detection methods
void compareDetectionMethods(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    std::vector<double> samples;
    samples.reserve(buffer.getNumSamples());

    const float* channelData = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        samples.push_back(static_cast<double>(channelData[i]));

    // Test each method
    std::vector<std::string> methods = {
        "energy", "hfc", "complex", "phase", "wphase",
        "specdiff", "kl", "mkl", "specflux"
    };

    for (const auto& method : methods)
    {
        Onsets::SliceAnalyzerOptions options;
        options.method = method;

        auto result = Onsets::SliceAnalyzer::analyzeSlices(samples, sampleRate, options);

        DBG(juce::String(method) + ": " +
            juce::String(result.onsets.size()) + " onsets detected");
    }

    // Try consensus mode
    Onsets::SliceAnalyzerOptions consensusOptions;
    consensusOptions.method = "consensus";
    consensusOptions.minConsensusClusterSize = 3;

    auto consensusResult = Onsets::SliceAnalyzer::analyzeSlices(
        samples, sampleRate, consensusOptions);

    DBG("Consensus: " + juce::String(consensusResult.onsets.size()) + " onsets detected");
}

// Example 5: Convert onset times to sample positions
std::vector<int> onsetTimesToSamplePositions(
    const std::vector<double>& onsetTimes,
    double sampleRate)
{
    std::vector<int> samplePositions;
    samplePositions.reserve(onsetTimes.size());

    for (double time : onsetTimes)
    {
        int samplePos = static_cast<int>(time * sampleRate);
        samplePositions.push_back(samplePos);
    }

    return samplePositions;
}

// Example 6: Advanced configuration for specific use case
std::vector<double> detectBassKickOnsets(
    const juce::AudioBuffer<float>& buffer,
    double sampleRate)
{
    std::vector<double> samples;
    samples.reserve(buffer.getNumSamples());

    const float* channelData = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        samples.push_back(static_cast<double>(channelData[i]));

    // Configuration optimized for detecting bass/kick drum hits
    Onsets::SliceAnalyzerOptions options;
    options.method = "specflux";  // Good for low-frequency transients
    options.optimize = true;
    options.optimizeWindowMs = 50.0;  // Smaller window for tight kicks
    options.useMinimumSpacing = true;
    options.minimumSpacing = 100.0;  // Kicks usually don't occur < 100ms apart

    // Could also use low-level API for more control:
    // Create detector with custom settings
    Onsets::OnsetDetector detector("specflux", 512, 256, static_cast<size_t>(sampleRate));
    detector.setThreshold(0.18);
    detector.setAWhitening(true);
    detector.getSpectralWhitening().setRelaxTime(100.0);
    detector.getSpectralWhitening().setFloor(1.0);
    detector.setCompression(10.0);
    detector.setMinioiMs(100.0);

    auto result = Onsets::SliceAnalyzer::analyzeSlices(samples, sampleRate, options);
    return result.onsets;
}
