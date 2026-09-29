#include "Cvec.h"
#include <cmath>
#include <algorithm>

namespace Onsets {

Cvec::Cvec(size_t fftLength)
{
    size_t size = fftLength / 2 + 1;
    norm.resize(size, 0.0);
    phas.resize(size, 0.0);
}

void Cvec::zeros()
{
    std::fill(norm.begin(), norm.end(), 0.0);
    std::fill(phas.begin(), phas.end(), 0.0);
}

void Cvec::setNorm(size_t position, double value)
{
    if (position < norm.size())
        norm[position] = value;
}

double Cvec::getNorm(size_t position) const
{
    if (position < norm.size())
        return norm[position];
    return 0.0;
}

void Cvec::setPhas(size_t position, double value)
{
    if (position < phas.size())
        phas[position] = value;
}

double Cvec::getPhas(size_t position) const
{
    if (position < phas.size())
        return phas[position];
    return 0.0;
}

void Cvec::copy(const Cvec& source)
{
    size_t length = std::min(norm.size(), source.norm.size());
    std::copy_n(source.norm.begin(), length, norm.begin());
    std::copy_n(source.phas.begin(), length, phas.begin());
}

void Cvec::logMag(double lambda)
{
    if (lambda > 0.0)
    {
        for (double& n : norm)
            n = std::log(1.0 + lambda * n);
    }
}

} // namespace Onsets
