#pragma once

#include <vector>
#include <string>

namespace Onsets {

/**
 * Configuration options for slice analysis.
 */
struct SliceAnalyzerOptions {
    /**
     * Number of slices to find.
     * If 0 (default), all onsets are detected.
     * If > 0, the best N onsets based on energy are selected.
     */
    int numSlices = 0;

    /**
     * Enable optimization of onset positions using variance analysis.
     * Default is true.
     */
    bool optimize = true;

    /**
     * Window size in milliseconds for onset optimization.
     * Default is 100.0 ms.
     */
    double optimizeWindowMs = 100.0;

    /**
     * Onset detection method to use.
     * Supported: "hfc", "energy", "complex", "phase", "wphase",
     *            "specdiff", "kl", "mkl", "specflux", "consensus"
     * Default is "hfc".
     * The special "consensus" method uses all methods and generates consensus markers.
     */
    std::string method = "hfc";

    /**
     * Minimum number of onset markers required for a cluster to be considered
     * valid when using the "consensus" method.
     * Default is 3. Only applies when method is "consensus".
     */
    int minConsensusClusterSize = 3;

    /**
     * Enable minimum spacing filter between slices.
     * When true, if multiple slices fall within minimumSpacing window,
     * only the first is kept.
     * Default is true.
     */
    bool useMinimumSpacing = true;

    /**
     * Minimum spacing in milliseconds between slices.
     * If multiple slices fall within this window, only the first is kept.
     * Default is 80.0 ms. Only applies when useMinimumSpacing is true.
     */
    double minimumSpacing = 80.0;
};

/**
 * Result of slice analysis.
 */
struct SliceAnalyzerResult {
    /**
     * Detected onset times in seconds.
     */
    std::vector<double> onsets;
};

/**
 * High-level API for audio slice analysis and onset detection.
 * This class provides a complete solution for detecting transients in audio,
 * with support for multiple detection methods, consensus analysis, and
 * optimization features.
 */
class SliceAnalyzer {
public:
    /**
     * Analyzes audio samples and detects slice points.
     * @param samples Audio samples (mono)
     * @param sampleRate Sample rate in Hz
     * @param options Analysis options
     * @return Result containing detected onset times
     */
    static SliceAnalyzerResult analyzeSlices(
        const std::vector<double>& samples,
        double sampleRate,
        const SliceAnalyzerOptions& options = SliceAnalyzerOptions());

private:
    // Internal helper functions

    /**
     * Finds all onsets using a single detection method.
     */
    static std::vector<double> findAllOnsets(
        const std::vector<double>& samples,
        double sampleRate,
        const std::string& method);

    /**
     * Finds the best N onsets based on energy.
     */
    static std::vector<double> findBestOnsets(
        const std::vector<double>& samples,
        double sampleRate,
        int targetSlices,
        const std::string& method);

    /**
     * Uses consensus method: runs all methods and clusters results.
     */
    static std::vector<double> findConsensusOnsets(
        const std::vector<double>& samples,
        double sampleRate,
        const SliceAnalyzerOptions& options);

    /**
     * Detects all onsets with given parameters.
     */
    static std::vector<double> detectAllOnsets(
        const std::vector<double>& samples,
        double sampleRate,
        const std::string& method,
        size_t bufSize,
        size_t hopSize);

    /**
     * Detects onsets with custom threshold and minioi.
     */
    static std::vector<double> detectOnsetsInternal(
        const std::vector<double>& samples,
        double sampleRate,
        const std::string& method,
        size_t bufSize,
        size_t hopSize,
        double threshold,
        double minioi);

    /**
     * Calculates RMS energy around an onset.
     */
    static double calculateOnsetEnergy(
        const std::vector<double>& samples,
        double sampleRate,
        double onsetTime);

    /**
     * Optimizes onset positions using variance analysis.
     */
    static std::vector<double> optimizeOnsetPositions(
        const std::vector<double>& samples,
        double sampleRate,
        const std::vector<double>& onsets,
        double windowMs);

    /**
     * Finds optimal onset position by variance difference.
     */
    static double findOptimalOnsetPosition(
        const std::vector<double>& samples,
        double sampleRate,
        double onsetTime,
        double windowMs);

    /**
     * Calculates variance of a sample range.
     */
    static double calculateVariance(
        const std::vector<double>& samples,
        size_t start,
        size_t end);

    /**
     * Applies minimum spacing filter.
     */
    static std::vector<double> applyMinimumSpacing(
        const std::vector<double>& onsets,
        double minimumSpacingMs);

    /**
     * Calculates cluster midpoint with outlier removal.
     */
    static double calculateClusterMidpoint(const std::vector<double>& cluster);

    /**
     * Removes outliers using IQR method.
     */
    static std::vector<double> removeOutliers(const std::vector<double>& data);

    /**
     * Calculates percentile of sorted data.
     */
    static double calculatePercentile(const std::vector<double>& sorted, double percentile);

    // Helper struct for energy-based sorting
    struct OnsetWithEnergy {
        double time;
        double energy;
    };
};

} // namespace Onsets
