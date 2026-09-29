#include "Fvec.h"
#include <cmath>
#include <algorithm>

namespace Onsets {

Fvec::Fvec(size_t length)
    : data(length, 0.0)
{
}

void Fvec::zeros()
{
    std::fill(data.begin(), data.end(), 0.0);
}

void Fvec::set(size_t position, double value)
{
    if (position < data.size())
        data[position] = value;
}

double Fvec::get(size_t position) const
{
    if (position < data.size())
        return data[position];
    return 0.0;
}

void Fvec::copy(const Fvec& source)
{
    size_t length = std::min(data.size(), source.data.size());
    std::copy_n(source.data.begin(), length, data.begin());
}

double Fvec::mean() const
{
    if (data.empty())
        return 0.0;

    double sum = 0.0;
    for (double v : data)
        sum += v;

    return sum / static_cast<double>(data.size());
}

double Fvec::max() const
{
    if (data.empty())
        return 0.0;

    return *std::max_element(data.begin(), data.end());
}

double Fvec::min() const
{
    if (data.empty())
        return 0.0;

    return *std::min_element(data.begin(), data.end());
}

void Fvec::weight(double w)
{
    for (double& v : data)
        v *= w;
}

void Fvec::weightedCopy(const Fvec& source, double w)
{
    size_t length = std::min(data.size(), source.data.size());
    for (size_t i = 0; i < length; ++i)
        data[i] = source.data[i] * w;
}

double Fvec::localEnergyDB() const
{
    double energy = 0.0;
    for (double v : data)
        energy += v * v;

    if (energy > 0.0)
        return 10.0 * std::log10(energy / static_cast<double>(data.size()));

    return -90.0;
}

} // namespace Onsets
