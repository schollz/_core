#pragma once
#include "CardFormat.h"
#include "Visualizer/Core.h"
#include "../../lib/screen_waveform_format.h"
namespace core::screenwaves {
inline String path(int bank, int slot) { return card::path(bank, slot) + ".wfm"; }
juce::MemoryBlock encode(const zv::Wave &, const juce::MemoryBlock &info,
                         const String &audioHash);
}
