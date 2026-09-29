#include "SliceAnalyzer.h"
#include "OnsetDetector.h"
#include "Fvec.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

namespace Onsets {

SliceAnalyzerResult SliceAnalyzer::analyzeSlices(
    const std::vector<double>& samples,
    double sampleRate,
    const SliceAnalyzerOptions& options)
{
    SliceAnalyzerResult result;

    // Default to "hfc" if method is not specified
    std::string method = options.method;
    if (method.empty())
        method = "hfc";

    std::vector<double> onsets;

    if (method == "consensus")
    {
        // Use consensus method: run all methods and generate consensus
        onsets = findConsensusOnsets(samples, sampleRate, options);
    }
    else if (options.numSlices > 0)
    {
        // Find the best N onsets based on energy
        onsets = findBestOnsets(samples, sampleRate, options.numSlices, method);
    }
    else
    {
        // Find all onsets
        onsets = findAllOnsets(samples, sampleRate, method);
    }

    // Optimize onset positions if requested
    if (options.optimize && !onsets.empty())
        onsets = optimizeOnsetPositions(samples, sampleRate, onsets, options.optimizeWindowMs);

    // Apply minimum spacing filter if requested
    if (options.useMinimumSpacing && !onsets.empty())
        onsets = applyMinimumSpacing(onsets, options.minimumSpacing);

    result.onsets = onsets;
    return result;
}

std::vector<double> SliceAnalyzer::findAllOnsets(
    const std::vector<double>& samples,
    double sampleRate,
    const std::string& method)
{
    size_t bufSize = 512;
    size_t hopSize = 256;
    return detectAllOnsets(samples, sampleRate, method, bufSize, hopSize);
}

std::vector<double> SliceAnalyzer::findBestOnsets(
    const std::vector<double>& samples,
    double sampleRate,
    int targetSlices,
    const std::string& method)
{
    size_t bufSize = 512;
    size_t hopSize = 256;

    // Detect all onsets with relaxed parameters
    std::vector<double> allOnsets = detectAllOnsets(samples, sampleRate, method, bufSize, hopSize);

    if (allOnsets.empty())
        return {};

    // Calculate energy at each onset
    std::vector<OnsetWithEnergy> onsetsWithEnergy;
    onsetsWithEnergy.reserve(allOnsets.size());

    for (double onsetTime : allOnsets)
    {
        double energy = calculateOnsetEnergy(samples, sampleRate, onsetTime);
        onsetsWithEnergy.push_back({onsetTime, energy});
    }

    // Sort by energy (descending)
    std::sort(onsetsWithEnergy.begin(), onsetsWithEnergy.end(),
        [](const OnsetWithEnergy& a, const OnsetWithEnergy& b) {
            return a.energy > b.energy;
        });

    // Take top N onsets
    size_t numToSelect = std::min(static_cast<size_t>(targetSlices), onsetsWithEnergy.size());
    auto selectionEnd = std::next(onsetsWithEnergy.begin(), static_cast<std::ptrdiff_t>(numToSelect));
    std::vector<OnsetWithEnergy> bestOnsets(onsetsWithEnergy.begin(), selectionEnd);

    // Sort back by time for output
    std::sort(bestOnsets.begin(), bestOnsets.end(),
        [](const OnsetWithEnergy& a, const OnsetWithEnergy& b) {
            return a.time < b.time;
        });

    // Extract just the times
    std::vector<double> result;
    result.reserve(bestOnsets.size());
    for (const auto& onset : bestOnsets)
        result.push_back(onset.time);

    return result;
}

std::vector<double> SliceAnalyzer::findConsensusOnsets(
    const std::vector<double>& samples,
    double sampleRate,
    const SliceAnalyzerOptions& options)
{
    size_t bufSize = 512;
    size_t hopSize = 256;

    // All available methods
    std::vector<std::string> methods = {
        "energy", "hfc", "complex", "phase", "wphase",
        "specdiff", "kl", "mkl", "specflux"
    };

    // Collect all onsets from all methods
    std::vector<double> allOnsets;
    for (const auto& method : methods)
    {
        std::vector<double> methodOnsets = detectAllOnsets(samples, sampleRate, method, bufSize, hopSize);
        allOnsets.insert(allOnsets.end(), methodOnsets.begin(), methodOnsets.end());
    }

    if (allOnsets.empty())
        return {};

    // Sort all onsets by time
    std::sort(allOnsets.begin(), allOnsets.end());

    // Cluster nearby onsets together
    double clusterThreshold = 0.05;  // 50ms threshold

    int minClusterSize = options.minConsensusClusterSize;
    if (minClusterSize <= 0)
        minClusterSize = 3;

    std::vector<double> consensusOnsets;
    std::vector<double> currentCluster = {allOnsets[0]};

    for (size_t i = 1; i < allOnsets.size(); ++i)
    {
        if (allOnsets[i] - currentCluster.back() <= clusterThreshold)
        {
            // Add to current cluster
            currentCluster.push_back(allOnsets[i]);
        }
        else
        {
            // Finalize current cluster if it meets minimum size
            if (static_cast<int>(currentCluster.size()) >= minClusterSize)
                consensusOnsets.push_back(calculateClusterMidpoint(currentCluster));

            currentCluster = {allOnsets[i]};
        }
    }

    // Don't forget the last cluster
    if (static_cast<int>(currentCluster.size()) >= minClusterSize)
        consensusOnsets.push_back(calculateClusterMidpoint(currentCluster));

    // If numSlices is specified, select the best N based on energy
    if (options.numSlices > 0 && static_cast<int>(consensusOnsets.size()) > options.numSlices)
    {
        std::vector<OnsetWithEnergy> onsetsWithEnergy;
        onsetsWithEnergy.reserve(consensusOnsets.size());

        for (double onsetTime : consensusOnsets)
        {
            double energy = calculateOnsetEnergy(samples, sampleRate, onsetTime);
            onsetsWithEnergy.push_back({onsetTime, energy});
        }

        // Sort by energy (descending)
        std::sort(onsetsWithEnergy.begin(), onsetsWithEnergy.end(),
            [](const OnsetWithEnergy& a, const OnsetWithEnergy& b) {
                return a.energy > b.energy;
            });

        // Take top N
        auto selectionEnd = std::next(
            onsetsWithEnergy.begin(),
            static_cast<std::ptrdiff_t>(options.numSlices));
        std::vector<OnsetWithEnergy> bestOnsets(onsetsWithEnergy.begin(), selectionEnd);

        // Sort back by time
        std::sort(bestOnsets.begin(), bestOnsets.end(),
            [](const OnsetWithEnergy& a, const OnsetWithEnergy& b) {
                return a.time < b.time;
            });

        // Extract times
        std::vector<double> result;
        result.reserve(bestOnsets.size());
        for (const auto& onset : bestOnsets)
            result.push_back(onset.time);

        return result;
    }

    return consensusOnsets;
}

std::vector<double> SliceAnalyzer::detectAllOnsets(
    const std::vector<double>& samples,
    double sampleRate,
    const std::string& method,
    size_t bufSize,
    size_t hopSize)
{
    // Use low threshold and short minioi to detect all possible onsets
    double threshold = 0.02;
    double minioi = 10.0;  // milliseconds

    return detectOnsetsInternal(samples, sampleRate, method, bufSize, hopSize, threshold, minioi);
}

std::vector<double> SliceAnalyzer::detectOnsetsInternal(
    const std::vector<double>& samples,
    double sampleRate,
    const std::string& method,
    size_t bufSize,
    size_t hopSize,
    double threshold,
    double minioi)
{
    OnsetDetector detector(method, bufSize, hopSize, static_cast<size_t>(sampleRate));
    detector.setThreshold(threshold);
    detector.setMinioiMs(minioi);

    Fvec input(hopSize);
    Fvec output(1);

    std::vector<double> onsets;

    // Process audio in chunks
    for (size_t pos = 0; pos + hopSize < samples.size(); pos += hopSize)
    {
        // Fill input buffer
        auto& inputData = input.getData();
        for (size_t i = 0; i < hopSize; ++i)
        {
            if (pos + i < samples.size())
                inputData[i] = samples[pos + i];
            else
                inputData[i] = 0.0;
        }

        // Process
        detector.processFrame(input, output);

        // Check for onset
        if (output.getData()[0] > 0.0)
        {
            double onsetTime = detector.getLastOnsetS();
            onsets.push_back(onsetTime);
        }
    }

    return onsets;
}

double SliceAnalyzer::calculateOnsetEnergy(
    const std::vector<double>& samples,
    double sampleRate,
    double onsetTime)
{
    // Calculate energy in a window around the onset
    double windowMs = 50.0;  // 50ms window
    size_t windowSamples = static_cast<size_t>(windowMs * sampleRate / 1000.0);

    size_t onsetSample = static_cast<size_t>(onsetTime * sampleRate);

    // Window starts at onset and extends forward
    size_t startSample = onsetSample;
    size_t endSample = onsetSample + windowSamples;

    // Clamp to valid range
    if (endSample > samples.size())
        endSample = samples.size();

    // Calculate RMS energy
    double sumSquares = 0.0;
    size_t count = 0;
    for (size_t i = startSample; i < endSample; ++i)
    {
        sumSquares += samples[i] * samples[i];
        ++count;
    }

    if (count == 0)
        return 0.0;

    return std::sqrt(sumSquares / static_cast<double>(count));
}

std::vector<double> SliceAnalyzer::optimizeOnsetPositions(
    const std::vector<double>& samples,
    double sampleRate,
    const std::vector<double>& onsets,
    double windowMs)
{
    std::vector<double> optimized;
    optimized.reserve(onsets.size());

    for (double onsetTime : onsets)
        optimized.push_back(findOptimalOnsetPosition(samples, sampleRate, onsetTime, windowMs));

    return optimized;
}

double SliceAnalyzer::findOptimalOnsetPosition(
    const std::vector<double>& samples,
    double sampleRate,
    double onsetTime,
    double windowMs)
{
    // Convert onset time to sample index
    size_t onsetSample = static_cast<size_t>(onsetTime * sampleRate);

    // Calculate window size in samples (centered around onset)
    size_t windowSamples = static_cast<size_t>(windowMs * sampleRate / 1000.0);
    size_t halfWindow = windowSamples / 2;

    // Define search window boundaries
    size_t windowStart = (onsetSample > halfWindow) ? onsetSample - halfWindow : 0;
    size_t windowEnd = std::min(onsetSample + halfWindow, samples.size());

    // If window is too small, return original onset
    if (windowEnd - windowStart < 10)
        return onsetTime;

    // Search for the midpoint with maximum variance difference
    double maxDiff = -std::numeric_limits<double>::max();
    size_t bestPosition = onsetSample;

    // Try each position in the window as a potential midpoint
    size_t minMargin = 5;  // minimum samples on each side
    for (size_t midpoint = windowStart + minMargin; midpoint < windowEnd - minMargin; ++midpoint)
    {
        // Calculate variance of left side
        double leftVariance = calculateVariance(samples, windowStart, midpoint);

        // Calculate variance of right side
        double rightVariance = calculateVariance(samples, midpoint, windowEnd);

        // Calculate difference (right - left)
        double diff = rightVariance - leftVariance;

        // Track maximum difference
        if (diff > maxDiff)
        {
            maxDiff = diff;
            bestPosition = midpoint;
        }
    }

    // Convert best position back to time
    return static_cast<double>(bestPosition) / sampleRate;
}

double SliceAnalyzer::calculateVariance(
    const std::vector<double>& samples,
    size_t start,
    size_t end)
{
    if (start >= end || end > samples.size())
        return 0.0;

    size_t count = end - start;
    if (count == 0)
        return 0.0;

    // Calculate mean
    double sum = 0.0;
    for (size_t i = start; i < end; ++i)
        sum += samples[i];
    double mean = sum / static_cast<double>(count);

    // Calculate variance
    double sumSquaredDiff = 0.0;
    for (size_t i = start; i < end; ++i)
    {
        double diff = samples[i] - mean;
        sumSquaredDiff += diff * diff;
    }

    return sumSquaredDiff / static_cast<double>(count);
}

std::vector<double> SliceAnalyzer::applyMinimumSpacing(
    const std::vector<double>& onsets,
    double minimumSpacingMs)
{
    if (onsets.empty())
        return onsets;

    double minimumSpacingSec = minimumSpacingMs / 1000.0;

    std::vector<double> filtered;
    filtered.push_back(onsets[0]);

    for (size_t i = 1; i < onsets.size(); ++i)
    {
        double timeDiff = onsets[i] - filtered.back();
        if (timeDiff >= minimumSpacingSec)
            filtered.push_back(onsets[i]);
    }

    return filtered;
}

double SliceAnalyzer::calculateClusterMidpoint(const std::vector<double>& cluster)
{
    if (cluster.empty())
        return 0.0;

    // For small clusters, don't remove outliers
    if (cluster.size() < 4)
    {
        double sum = 0.0;
        for (double time : cluster)
            sum += time;
        return sum / static_cast<double>(cluster.size());
    }

    // Remove outliers using IQR method
    std::vector<double> cleanedCluster = removeOutliers(cluster);

    // If all values were outliers, use original cluster
    if (cleanedCluster.empty())
        cleanedCluster = cluster;

    double sum = 0.0;
    for (double time : cleanedCluster)
        sum += time;

    return sum / static_cast<double>(cleanedCluster.size());
}

std::vector<double> SliceAnalyzer::removeOutliers(const std::vector<double>& data)
{
    if (data.size() < 4)
        return data;

    // Create a sorted copy
    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    // Calculate Q1 and Q3
    double q1 = calculatePercentile(sorted, 25.0);
    double q3 = calculatePercentile(sorted, 75.0);

    // Calculate IQR
    double iqr = q3 - q1;

    // Define outlier bounds
    double lowerBound = q1 - 1.5 * iqr;
    double upperBound = q3 + 1.5 * iqr;

    // Filter out outliers
    std::vector<double> result;
    for (double value : data)
    {
        if (value >= lowerBound && value <= upperBound)
            result.push_back(value);
    }

    return result;
}

double SliceAnalyzer::calculatePercentile(const std::vector<double>& sorted, double percentile)
{
    if (sorted.empty())
        return 0.0;

    if (sorted.size() == 1)
        return sorted[0];

    // Calculate the rank
    double rank = (percentile / 100.0) * static_cast<double>(sorted.size() - 1);
    size_t lowerIndex = static_cast<size_t>(std::floor(rank));
    size_t upperIndex = static_cast<size_t>(std::ceil(rank));

    // Handle edge cases
    if (upperIndex >= sorted.size())
        upperIndex = sorted.size() - 1;

    // Linear interpolation
    if (lowerIndex == upperIndex)
        return sorted[lowerIndex];

    double weight = rank - static_cast<double>(lowerIndex);
    return sorted[lowerIndex] * (1.0 - weight) + sorted[upperIndex] * weight;
}

} // namespace Onsets
