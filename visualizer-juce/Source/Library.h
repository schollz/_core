#pragma once
#include "Core.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace zv
{
struct Sample
{
    int bank = 0, sample = 0;
    juce::String path, error;
    std::shared_ptr<const Wave> wave;
};
struct LibraryState
{
    std::vector<Sample> samples;
    juce::String root, current, error, warning;
    int completed = 0, total = 0, reused = 0, prepared = 0;
    bool loading = false;
};
class Library
{
  public:
    Library();
    ~Library();
    void load(const juce::File &root);
    void prioritise(int bank, int sample)
    {
        priority.store(bank * 16 + sample);
    }
    LibraryState snapshot() const;
    static juce::File defaultCache();
    static LibraryState prepare(const juce::File &root, const juce::File &cache,
                                std::function<bool()> cancelled = {},
                                std::function<void(const LibraryState &)> progress = {},
                                const std::atomic<int> *priority = nullptr);

  private:
    void run();
    mutable std::mutex mutex;
    std::condition_variable wake;
    LibraryState state;
    juce::File requested;
    std::atomic<uint64_t> generation{0};
    std::atomic<bool> stopping{false};
    std::atomic<int> priority{-1};
    std::thread worker;
};
} // namespace zv
