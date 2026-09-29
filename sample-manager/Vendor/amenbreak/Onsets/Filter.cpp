#include "Filter.h"

namespace Onsets {

Filter::Filter(size_t ord)
    : order(ord)
    , a(ord, 0.0)
    , b(ord, 0.0)
    , x(ord, 0.0)
    , y(ord, 0.0)
{
    // Set default to identity filter
    a[0] = 1.0;
    b[0] = 1.0;
}

Filter Filter::createBiquad(double b0, double b1, double b2, double a1, double a2)
{
    Filter f(3);
    f.b[0] = b0;
    f.b[1] = b1;
    f.b[2] = b2;
    f.a[0] = 1.0;
    f.a[1] = a1;
    f.a[2] = a2;
    return f;
}

void Filter::doFilter(Fvec& input)
{
    auto& data = input.getData();

    for (size_t j = 0; j < data.size(); ++j)
    {
        // New input
        x[0] = data[j];
        y[0] = b[0] * x[0];

        // Apply filter
        for (size_t l = 1; l < order; ++l)
        {
            y[0] += b[l] * x[l];
            y[0] -= a[l] * y[l];
        }

        // New output
        data[j] = y[0];

        // Store for next sample (shift delay lines)
        for (size_t l = order - 1; l > 0; --l)
        {
            x[l] = x[l - 1];
            y[l] = y[l - 1];
        }
    }
}

void Filter::doFiltFilt(Fvec& input, Fvec& tmp)
{
    size_t length = input.getLength();

    // Apply filtering forward
    doFilter(input);
    reset();

    // Mirror the signal
    auto& inputData = input.getData();
    auto& tmpData = tmp.getData();
    for (size_t j = 0; j < length; ++j)
        tmpData[length - j - 1] = inputData[j];

    // Apply filtering on mirrored signal
    doFilter(tmp);
    reset();

    // Invert back
    for (size_t j = 0; j < length; ++j)
        inputData[j] = tmpData[length - j - 1];
}

void Filter::reset()
{
    std::fill(x.begin(), x.end(), 0.0);
    std::fill(y.begin(), y.end(), 0.0);
}

} // namespace Onsets
