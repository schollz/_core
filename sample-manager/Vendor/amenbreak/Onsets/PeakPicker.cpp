#include "PeakPicker.h"
#include "MathUtils.h"

namespace Onsets {

PeakPicker::PeakPicker()
    : threshold(0.1)
    , winPost(5)
    , winPre(1)
    , biquad(Filter::createBiquad(0.15998789, 0.31997577, 0.15998789, 0.23484048, 0.0))
    , onsetKeep(winPost + winPre + 1)
    , onsetProc(winPost + winPre + 1)
    , onsetPeek(3)
    , thresholded(1)
    , scratch(winPost + winPre + 1)
{
}

void PeakPicker::doPeakPicking(const Fvec& onset, Fvec& out)
{
    auto& outData = out.getData();

    // Push new novelty to the end
    MathUtils::fvecPush(onsetKeep, onset.getData()[0]);

    // Store a copy
    onsetProc.copy(onsetKeep);

    // Filter this copy
    biquad.doFiltFilt(onsetProc, scratch);

    // Calculate mean
    double mean = MathUtils::fvecMean(onsetProc);

    // Calculate median
    scratch.copy(onsetProc);
    double median = MathUtils::fvecMedian(scratch);

    // Shift peek array
    auto& peekData = onsetPeek.getData();
    for (size_t j = 0; j < 2; ++j)
        peekData[j] = peekData[j + 1];

    // Calculate new thresholded value
    auto& threshData = thresholded.getData();
    auto& procData = onsetProc.getData();
    threshData[0] = procData[winPost] - median - mean * threshold;
    peekData[2] = threshData[0];

    // Check for peak
    if (MathUtils::fvecPeakPick(onsetPeek, 1))
        outData[0] = MathUtils::fvecQuadraticPeakPos(onsetPeek, 1);
    else
        outData[0] = 0.0;
}

} // namespace Onsets
