#include "Library.h"
#include <juce_cryptography/juce_cryptography.h>
#include <filesystem>
#include <sys/stat.h>

namespace zv
{
namespace
{
juce::String hash(const juce::String &s)
{
    return juce::SHA256(s.toRawUTF8(), s.getNumBytesAsUTF8()).toHexString();
}
juce::String fingerprint(const juce::File &f)
{
    // Include ctime as well as mtime: copying a file over an old one may retain mtime.
    struct stat s{};
    if (::stat(f.getFullPathName().toRawUTF8(), &s) != 0)
        throw std::runtime_error("Missing WAV or .info file");
    juce::String result((juce::int64)s.st_size);
#if JUCE_MAC
    return result + ":" + juce::String((juce::int64)s.st_mtimespec.tv_sec) + ":" +
           juce::String((juce::int64)s.st_mtimespec.tv_nsec) + ":" +
           juce::String((juce::int64)s.st_ctimespec.tv_sec) + ":" +
           juce::String((juce::int64)s.st_ctimespec.tv_nsec);
#else
    return result + ":" + juce::String((juce::int64)s.st_mtime) + ":" + juce::String((juce::int64)s.st_ctime);
#endif
}
} // namespace
juce::File Library::defaultCache()
{
#if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Caches/com.infinitedigits.zeptocorevisualizer");
#else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("zeptocore-visualizer/cache");
#endif
}
Library::Library() : worker([this] { run(); }) {}
Library::~Library()
{
    stopping = true;
    ++generation;
    wake.notify_all();
    if (worker.joinable())
        worker.join();
}
void Library::load(const juce::File &root)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        requested = root;
        ++generation;
        state = {};
        state.root = root.getFullPathName();
        state.loading = true;
    }
    wake.notify_one();
}
LibraryState Library::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return state;
}
void Library::run()
{
    uint64_t handled = 0;
    while (!stopping)
    {
        juce::File root;
        uint64_t token;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [&] { return stopping || generation != handled; });
            if (stopping)
                return;
            token = generation;
            root = requested;
        }
        auto cancelled = [&, token] { return stopping || generation != token; };
        auto publish = [&, token](const LibraryState &value)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (generation == token && !stopping)
                state = value;
        };
        publish(prepare(root, defaultCache(), cancelled, publish, &priority));
        handled = token;
    }
}
LibraryState Library::prepare(const juce::File &folder, const juce::File &cache,
                              std::function<bool()> cancelled,
                              std::function<void(const LibraryState &)> progress,
                              const std::atomic<int> *priority)
{
    LibraryState state;
    state.root = folder.getFullPathName();
    state.loading = true;
    auto report = [&]
    {
        if (progress)
            progress(state);
    };
    auto stop = [&] { return cancelled && cancelled(); };
    try
    {
        if (!folder.isDirectory())
            throw std::runtime_error(
                "Reference folder is missing. Choose the folder containing bank1-bank16.");
        std::error_code ec;
        auto canonical = std::filesystem::canonical(folder.getFullPathName().toStdString(), ec);
        if (ec)
            throw std::runtime_error("Cannot read the reference folder");
        const juce::File root(juce::String(canonical.string()));
        state.root = root.getFullPathName();
        for (int b = 0; b < 16; ++b)
        {
            const auto dir = root.getChildFile("bank" + juce::String(b + 1));
            if (!dir.isDirectory())
                continue;
            for (const auto &file : dir.findChildFiles(juce::File::findFiles, false, "*.0.wav"))
            {
                auto name = file.getFileName().upToFirstOccurrenceOf(".", false, false);
                if (name.isEmpty() || !name.containsOnly("0123456789") || name.length() > 9)
                    continue;
                int sample = name.getIntValue();
                if (sample >= 0 && sample < 16)
                    state.samples.push_back({b, sample, file.getRelativePathFrom(root), {}, nullptr});
            }
        }
        std::sort(state.samples.begin(), state.samples.end(), [](const auto &a, const auto &b)
                  { return std::tie(a.bank, a.sample, a.path) < std::tie(b.bank, b.sample, b.path); });
        state.total = (int)state.samples.size();
        report();
        std::vector<size_t> pending;
        for (size_t i = 0; i < state.samples.size(); ++i)
            pending.push_back(i);
        const auto cacheDir = cache.getChildFile(hash(state.root));
        const bool writable = cacheDir.createDirectory().wasOk();
        if (!writable)
            state.warning = "Cache unavailable; analysis will be repeated next launch.";
        while (!pending.empty() && !stop())
        {
            size_t q = 0;
            if (priority)
            {
                const int key = priority->load();
                for (size_t j = 0; j < pending.size(); ++j)
                {
                    auto &s = state.samples[pending[j]];
                    if (s.bank * 16 + s.sample == key)
                    {
                        q = j;
                        break;
                    }
                }
            }
            const size_t index = pending[q];
            pending.erase(pending.begin() + (ptrdiff_t)q);
            auto &s = state.samples[index];
            state.current = s.path;
            report();
            try
            {
                const auto audio = root.getChildFile(s.path),
                           info = audio.getSiblingFile(audio.getFileName() + ".info");
                const auto stamp = fingerprint(audio) + ":" + fingerprint(info);
                const auto entry = cacheDir.getChildFile(hash(s.path) + ".json");
                auto saved = juce::JSON::parse(entry);
                if ((int)saved["version"] == 1 && saved["stamp"].toString() == stamp &&
                    saved["path"].toString() == s.path)
                {
                    const auto text = saved["data"].toString();
                    if (saved["hash"].toString() == hash(text))
                        try
                        {
                            s.wave = waveFromJson(juce::JSON::parse(text));
                        }
                        catch (...)
                        {
                        }
                    if (s.wave && (s.wave->bank != s.bank || s.wave->sample != s.sample))
                        s.wave.reset();
                }
                if (s.wave)
                {
                    ++state.reused;
                }
                else
                {
                    juce::MemoryBlock wav, metadata;
                    if (!audio.loadFileAsData(wav) || !info.loadFileAsData(metadata))
                        throw std::runtime_error("Cannot read WAV or .info file");
                    s.wave = std::make_shared<Wave>(analyse(wav, metadata, s.bank, s.sample, cancelled));
                    if (stamp != fingerprint(audio) + ":" + fingerprint(info))
                        throw std::runtime_error("Sample changed during analysis; rescan the folder");
                    ++state.prepared;
                    if (writable && !stop())
                    {
                        const auto text = juce::JSON::toString(waveToJson(*s.wave), true);
                        auto *o = new juce::DynamicObject;
                        o->setProperty("version", 1);
                        o->setProperty("path", s.path);
                        o->setProperty("stamp", stamp);
                        o->setProperty("data", text);
                        o->setProperty("hash", hash(text));
                        juce::TemporaryFile temporary(entry);
                        if (!temporary.getFile().replaceWithText(juce::JSON::toString(juce::var(o), true)) ||
                            !temporary.overwriteTargetFileWithTemporary())
                            state.warning = "Could not save analysis cache.";
                    }
                }
            }
            catch (const std::exception &e)
            {
                s.wave.reset();
                s.error = e.what();
            }
            if (stop())
                break;
            ++state.completed;
            report();
        }
    }
    catch (const std::exception &e)
    {
        state.error = e.what();
    }
    state.loading = false;
    state.current.clear();
    return state;
}
} // namespace zv
