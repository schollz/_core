#include "MathUtils.h"
#include <algorithm>
#include <cmath>

namespace Onsets {
namespace MathUtils {

bool silenceDetection(const Fvec& input, double threshold)
{
    double db = input.localEnergyDB();
    return db < threshold;
}

void fvecPush(Fvec& v, double newElem)
{
    auto& data = v.getData();
    for (size_t i = 0; i < data.size() - 1; ++i)
        data[i] = data[i + 1];
    data[data.size() - 1] = newElem;
}

double fvecMedian(const Fvec& input)
{
    const auto& inputData = input.getData();
    if (inputData.empty())
        return 0.0;

    // Create a copy to avoid modifying original
    std::vector<double> arr = inputData;
    size_t n = arr.size();
    size_t low = 0;
    size_t high = n - 1;
    size_t median = (low + high) / 2;

    while (true)
    {
        if (high <= low)
            return arr[median];

        if (high == low + 1)
        {
            if (arr[low] > arr[high])
                std::swap(arr[low], arr[high]);
            return arr[median];
        }

        // Find median of low, middle and high items
        size_t middle = (low + high) / 2;
        if (arr[middle] > arr[high])
            std::swap(arr[middle], arr[high]);
        if (arr[low] > arr[high])
            std::swap(arr[low], arr[high]);
        if (arr[middle] > arr[low])
            std::swap(arr[middle], arr[low]);

        // Swap low item into position (low+1)
        std::swap(arr[middle], arr[low + 1]);

        // Partition
        size_t ll = low + 1;
        size_t hh = high;
        while (true)
        {
            do { ++ll; } while (arr[low] > arr[ll]);
            do { --hh; } while (arr[hh] > arr[low]);

            if (hh < ll)
                break;

            std::swap(arr[ll], arr[hh]);
        }

        // Swap middle item back
        std::swap(arr[low], arr[hh]);

        // Re-set active partition
        if (hh <= median)
            low = ll;
        if (hh >= median)
            high = hh - 1;
    }
}

double fvecMean(const Fvec& input)
{
    return input.mean();
}

bool fvecPeakPick(const Fvec& onset, size_t pos)
{
    const auto& data = onset.getData();
    if (pos == 0 || pos >= data.size() - 1)
        return false;

    return data[pos] > data[pos - 1] &&
           data[pos] > data[pos + 1] &&
           data[pos] > 0.0;
}

double fvecQuadraticPeakPos(const Fvec& x, size_t pos)
{
    const auto& data = x.getData();
    if (pos == 0 || pos >= data.size() - 1)
        return static_cast<double>(pos);

    size_t x0 = (pos >= 1) ? pos - 1 : pos;
    size_t x2 = (pos + 1 < data.size()) ? pos + 1 : pos;

    if (x0 == pos)
    {
        if (data[pos] <= data[x2])
            return static_cast<double>(pos);
        return static_cast<double>(x2);
    }

    if (x2 == pos)
    {
        if (data[pos] <= data[x0])
            return static_cast<double>(pos);
        return static_cast<double>(x0);
    }

    double s0 = data[x0];
    double s1 = data[pos];
    double s2 = data[x2];

    return static_cast<double>(pos) + 0.5 * (s0 - s2) / (s0 - 2.0 * s1 + s2);
}

double medianSimple(const std::vector<double>& data)
{
    if (data.empty())
        return 0.0;

    std::vector<double> sorted = data;
    std::sort(sorted.begin(), sorted.end());

    size_t n = sorted.size();
    if (n % 2 == 0)
        return (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;

    return sorted[n / 2];
}

} // namespace MathUtils
} // namespace Onsets
