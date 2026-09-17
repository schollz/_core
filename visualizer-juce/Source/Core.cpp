#include "Core.h"
#include <cmath>
#include <stdexcept>
#include <limits>

namespace zv
{
std::optional<Message> decode(const uint8_t *bytes, size_t length)
{
    if (length == 3 && bytes[0] >= 0x90 && bytes[0] <= 0x92 && bytes[1] < 16 && bytes[2] > 0 &&
        bytes[2] <= 127)
        return Message{Message::button, {}, {}, bytes[1] + 1};
    if (length < 3 || length > 128 || bytes[0] != 0xf0 || bytes[length - 1] != 0xf7)
        return {};
    std::string text;
    for (size_t i = 1; i + 1 < length; ++i)
    {
        if (bytes[i] < 32 || bytes[i] > 126)
            return {};
        text += static_cast<char>(bytes[i]);
    }
    auto equal = text.find('=');
    if (equal == std::string::npos)
        return {};
    const auto name = text.substr(0, equal);
    std::vector<uint32_t> v;
    uint64_t n = 0;
    bool digit = false;
    for (size_t i = equal + 1; i <= text.size(); ++i)
    {
        const char c = i == text.size() ? ',' : text[i];
        if (c == ',')
        {
            if (!digit)
                return {};
            v.push_back(static_cast<uint32_t>(n));
            n = 0;
            digit = false;
        }
        else if (c >= '0' && c <= '9')
        {
            n = n * 10 + static_cast<unsigned>(c - '0');
            digit = true;
            if (n > 0xffffffffu)
                return {};
        }
        else
            return {};
    }
    if (name == "view" && ((v.size() == 10 && v[0] == 1) || (v.size() == 11 && v[0] == 2)))
    {
        if (v[1] > 15 || v[2] > 15 || v[3] > 254 || v[5] > 511 || v[6] > 1 || v[7] > 1 || v[8] > 1 ||
            v[9] > 1 || (v.size() == 11 && v[10] > 65535))
            return {};
        Playback p;
        p.bank = (int)v[1];
        p.sample = (int)v[2];
        p.slice = (int)v[3];
        p.trigger = v[4];
        p.bpm = (int)v[5];
        p.forward = v[6] != 0;
        p.stopped = v[7] != 0;
        p.muted = v[8] != 0;
        p.valid = v[9] != 0;
        if (v.size() == 11)
            p.effects = static_cast<uint16_t>(v[10]);
        return Message{Message::view, p, {}, 0};
    }
    if (name == "info" && v.size() == 7 && v[0] < 16 && v[1] < 16 && v[2] <= 511 && v[5] <= 1 && v[6] <= 1)
        return Message{Message::info, {}, {(int)v[0], (int)v[1], (int)v[2], v[5] != 0, v[6] != 0}, 0};
    return {};
}
void DeviceState::receive(const Message &m, double at)
{
    if (m.kind == Message::button)
    {
        if (playback && playback->valid && fresh(receivedAt, at))
            press = Press{m.pad, playback->bank, playback->sample, playback->slice, at};
    }
    else if (m.kind == Message::view)
    {
        if (press && press->at >= receivedAt.value_or(0))
        {
            if (m.playback.valid && m.playback.bank == press->bank && m.playback.sample == press->sample)
                press->slice = m.playback.slice;
            else
                press.reset();
        }
        playback = m.playback;
        receivedAt = at;
        display = transition.update(m.playback, at);
    }
    else
    {
        legacy = m.legacy;
        legacyAt = at;
    }
}
Display Transition::update(const Playback &s, double at)
{
    bool changed = previous && (previous->bank != s.bank || previous->sample != s.sample);
    previous = s;
    if (s.valid)
    {
        pending.reset();
        return {s, at};
    }
    if (changed)
    {
        auto p = s;
        p.slice = 0;
        p.valid = true;
        p.estimated = true;
        p.effects.reset();
        pending = Display{p, at};
    }
    if (pending && fresh(pending->at, at))
    {
        pending->state.bpm = s.bpm;
        pending->state.stopped = s.stopped;
        pending->state.muted = s.muted;
        return *pending;
    }
    return {s, at};
}
void Playhead::update(const Playback &s, std::shared_ptr<const Wave> w, double time)
{
    if (!w || s.slice < 0 || s.slice >= (int)w->slices.size() || w->bank != s.bank || w->sample != s.sample)
        return;
    const auto region = w->slices[(size_t)s.slice];
    bool reset = !state || state->estimated != s.estimated || state->trigger != s.trigger ||
                 state->bank != s.bank || state->sample != s.sample || state->slice != s.slice ||
                 state->forward != s.forward || wave != w;
    position = reset ? (s.forward ? region.start : region.stop) : value(time).value_or(region.start);
    state = s;
    wave = std::move(w);
    at = time;
}
std::optional<double> Playhead::value(double now) const
{
    if (!state || !wave || !state->valid)
        return {};
    const auto &s = *state;
    const auto &w = *wave;
    const auto region = w.slices[(size_t)s.slice];
    const double speed = w.tempoMatch && w.bpm > 0 ? (double)s.bpm / w.bpm : 1;
    const double p =
        position + (s.stopped ? 0 : std::max(0.0, now - at) / 1000) * speed * (s.forward ? 1 : -1);
    auto clamp = [&](double a, double b) { return juce::jlimit(a, b, p); };
    auto wrap = [&](double a, double b)
    {
        if (p >= a && p <= b)
            return p;
        const double len = b - a;
        return len > 0 ? a + std::fmod(std::fmod(p - a, len) + len, len) : a;
    };
    switch (w.playMode)
    {
    case 1:
        return clamp(region.start, region.stop);
    case 2:
        return wrap(region.start, region.stop);
    case 3:
        return clamp(0, w.duration);
    case 4:
        return s.forward ? wrap(region.start, w.duration) : wrap(0, region.stop);
    default:
        return wrap(0, w.duration);
    }
}
namespace
{
uint16_t u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}
uint32_t u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void checkCancel(const std::function<bool()> &c)
{
    if (c && c())
        throw std::runtime_error("Cancelled");
}
std::vector<uint8_t> spectrum(const uint8_t *data, size_t frames, int rate, int channels,
                              const std::function<bool()> &cancelled)
{
    constexpr int size = 4096, bands = 32;
    const size_t count = std::max((size_t)1, (size_t)std::ceil((double)frames / rate * 20));
    std::vector<uint8_t> levels(count * bands);
    std::array<double, size> real{}, imag{}, window{};
    std::array<double, size / 2 + 1> power{};
    std::array<double, size / 2> cosine{}, sine{};
    std::array<int, size> reverse{};
    std::array<std::pair<int, int>, bands> ranges{};
    double sum = 0;
    for (int i = 0; i < size; ++i)
    {
        window[(size_t)i] = 0.5 - 0.5 * std::cos(2 * juce::MathConstants<double>::pi * i / (size - 1));
        sum += window[(size_t)i];
        int v = i, r = 0;
        for (int bit = 1; bit < size; bit *= 2)
        {
            r = r * 2 + (v & 1);
            v >>= 1;
        }
        reverse[(size_t)i] = r;
        if (i < size / 2)
        {
            cosine[(size_t)i] = std::cos(-2 * juce::MathConstants<double>::pi * i / size);
            sine[(size_t)i] = std::sin(-2 * juce::MathConstants<double>::pi * i / size);
        }
    }
    for (int b = 0; b < bands; ++b)
    {
        double lo = 50 * std::pow(320.0, (double)b / bands),
               hi = 50 * std::pow(320.0, (double)(b + 1) / bands);
        int first = std::max(1, (int)std::ceil(lo * size / rate));
        ranges[(size_t)b] = {std::min(size / 2, first),
                             std::min(size / 2, std::max(first, (int)std::ceil(hi * size / rate) - 1))};
    }
    double scale = std::pow(2 / sum, 2) / channels;
    for (size_t f = 0; f < count; ++f)
    {
        checkCancel(cancelled);
        power.fill(0);
        int64_t start = (int64_t)std::floor((double)f * rate / 20 + 0.5) - size / 2;
        for (int ch = 0; ch < channels; ++ch)
        {
            imag.fill(0);
            for (int i = 0; i < size; ++i)
            {
                auto sample = start + i;
                real[(size_t)reverse[(size_t)i]] =
                    sample >= 0 && sample < (int64_t)frames
                        ? (int16_t)u16(data + ((size_t)sample * (size_t)channels + (size_t)ch) * 2) /
                              32768.0 * window[(size_t)i]
                        : 0;
            }
            for (int length = 2; length <= size; length *= 2)
            {
                int half = length / 2, step = size / length;
                for (int offset = 0; offset < size; offset += length)
                    for (int j = 0; j < half; ++j)
                    {
                        auto a = (size_t)(offset + j), b = a + (size_t)half, k = (size_t)(j * step);
                        double r = real[b] * cosine[k] - imag[b] * sine[k],
                               im = real[b] * sine[k] + imag[b] * cosine[k];
                        real[b] = real[a] - r;
                        imag[b] = imag[a] - im;
                        real[a] += r;
                        imag[a] += im;
                    }
            }
            for (size_t i = 1; i < power.size(); ++i)
                power[i] += (real[i] * real[i] + imag[i] * imag[i]) * scale;
        }
        for (int b = 0; b < bands; ++b)
        {
            double peak = 0;
            for (int i = ranges[(size_t)b].first; i <= ranges[(size_t)b].second; ++i)
                peak = std::max(peak, power[(size_t)i]);
            double db = 10 * std::log10(std::max(1e-12, peak));
            levels[f * bands + (size_t)b] =
                (uint8_t)std::floor(255 * juce::jlimit(0.0, 1.0, (db + 72) / 72) + 0.5);
        }
    }
    return levels;
}
} // namespace
Wave analyse(const juce::MemoryBlock &wav, const juce::MemoryBlock &info, int bank, int sample,
             std::function<bool()> cancelled)
{
    const auto *meta = (const uint8_t *)info.getData();
    const auto *bytes = (const uint8_t *)wav.getData();
    require(info.getSize() >= 11, "Truncated sample metadata");
    uint32_t size = u32(meta), flags = u32(meta + 4);
    int count = meta[10];
    require(size > 0 && count > 0 && info.getSize() >= 11 + (size_t)count * 9, "Invalid slice metadata");
    require(((flags >> 16) & 127) <= 1, "Unsupported sample metadata version");
    Wave w;
    w.bank = bank;
    w.sample = sample;
    w.playMode = (int)((flags >> 9) & 7);
    w.bpm = (int)(flags & 511);
    w.tempoMatch = (flags & (1 << 13)) != 0;
    w.sampleRate = 44100 * (int)(((flags >> 14) & 1) + 1);
    w.channels = (int)(((flags >> 15) & 1) + 1);
    require(wav.getSize() >= 12 && std::memcmp(bytes, "RIFF", 4) == 0 &&
                std::memcmp(bytes + 8, "WAVE", 4) == 0,
            "Expected a RIFF WAV file");
    size_t end = (size_t)u32(bytes + 4) + 8;
    require(end <= wav.getSize(), "Truncated WAV file");
    const uint8_t *data = nullptr;
    size_t length = 0;
    int rate = 0, channels = 0, align = 0;
    for (size_t p = 12; p + 8 <= end;)
    {
        size_t n = u32(bytes + p + 4), a = p + 8;
        require(n <= end - a, "Truncated WAV chunk");
        if (std::memcmp(bytes + p, "fmt ", 4) == 0)
        {
            require(n >= 16 && u16(bytes + a) == 1 && u16(bytes + a + 14) == 16, "Expected 16-bit PCM audio");
            channels = u16(bytes + a + 2);
            rate = (int)u32(bytes + a + 4);
            align = u16(bytes + a + 12);
            require((channels == 1 || channels == 2) && (rate == 44100 || rate == 88200) &&
                        align == channels * 2,
                    "Unsupported WAV format");
        }
        else if (std::memcmp(bytes + p, "data", 4) == 0)
        {
            data = bytes + a;
            length = n;
        }
        p = a + n + (n & 1);
    }
    require(data && align > 0 && length % (size_t)align == 0, "Missing or invalid WAV data");
    const size_t padding = (size_t)rate * (size_t)align / 2;
    require(rate == w.sampleRate && channels == w.channels && length >= padding * 2 &&
                size == length - padding * 2,
            "WAV and metadata disagree about format or padding");
    require(size % (size_t)align == 0, "Unaligned sample boundaries");
    const double bytesPerSecond = (double)rate * align;
    w.duration = size / bytesPerSecond;
    for (int i = 0; i < count; ++i)
    {
        int32_t a = (int32_t)u32(meta + 11 + i * 4), b = (int32_t)u32(meta + 11 + count * 4 + i * 4);
        require(a >= 0 && b > a && (uint32_t)b <= size, "Slice outside sample bounds");
        require(a % align == 0 && b % align == 0, "Unaligned sample boundaries");
        w.slices.push_back({a / bytesPerSecond, b / bytesPerSecond});
    }
    size_t frames = size / (size_t)align, bins = std::min((size_t)4096, frames);
    w.peaks.resize((size_t)channels);
    for (auto &p : w.peaks)
        p.reserve(bins * 2);
    for (size_t i = 0; i < bins; ++i)
    {
        checkCancel(cancelled);
        size_t first = i * frames / bins, last = (i + 1) * frames / bins;
        for (int ch = 0; ch < channels; ++ch)
        {
            int16_t lo = 32767, hi = -32768;
            for (size_t f = first; f < last; ++f)
            {
                auto v = (int16_t)u16(data + padding + f * (size_t)align + (size_t)ch * 2);
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
            w.peaks[(size_t)ch].push_back(lo);
            w.peaks[(size_t)ch].push_back(hi);
        }
    }
    w.spectrum = spectrum(data + padding, frames, rate, channels, cancelled);
    return w;
}
juce::var waveToJson(const Wave &w)
{
    auto *o = new juce::DynamicObject;
    o->setProperty("bank", w.bank);
    o->setProperty("sample", w.sample);
    o->setProperty("sampleRate", w.sampleRate);
    o->setProperty("channels", w.channels);
    o->setProperty("duration", w.duration);
    o->setProperty("bpm", w.bpm);
    o->setProperty("tempoMatch", w.tempoMatch);
    o->setProperty("playMode", w.playMode);
    juce::Array<juce::var> slices, peaks;
    for (auto s : w.slices)
    {
        auto *v = new juce::DynamicObject;
        v->setProperty("start", s.start);
        v->setProperty("stop", s.stop);
        slices.add(v);
    }
    for (auto &channel : w.peaks)
    {
        juce::Array<juce::var> p;
        for (auto x : channel)
            p.add((int)x);
        peaks.add(p);
    }
    o->setProperty("slices", slices);
    o->setProperty("peaks", peaks);
    auto *spec = new juce::DynamicObject;
    spec->setProperty("bands", 32);
    spec->setProperty("frameRate", 20);
    spec->setProperty("minHz", 50);
    spec->setProperty("maxHz", 16000);
    spec->setProperty("frames", (int)(w.spectrum.size() / 32));
    spec->setProperty("levels", juce::Base64::toBase64(w.spectrum.data(), w.spectrum.size()));
    o->setProperty("spectrum", spec);
    return o;
}
std::shared_ptr<const Wave> waveFromJson(const juce::var &v)
{
    require(v.isObject(), "Invalid cache object");
    auto w = std::make_shared<Wave>();
    w->bank = (int)v["bank"];
    w->sample = (int)v["sample"];
    w->sampleRate = (int)v["sampleRate"];
    w->channels = (int)v["channels"];
    w->duration = (double)v["duration"];
    w->bpm = (int)v["bpm"];
    w->tempoMatch = (bool)v["tempoMatch"];
    w->playMode = (int)v["playMode"];
    require(w->bank >= 0 && w->bank < 16 && w->sample >= 0 && w->sample < 16 && w->duration > 0 &&
                std::isfinite(w->duration) && (w->channels == 1 || w->channels == 2) &&
                (w->sampleRate == 44100 || w->sampleRate == 88200),
            "Invalid cache metadata");
    auto *slices = v["slices"].getArray();
    auto *peaks = v["peaks"].getArray();
    require(slices && slices->size() > 0 && slices->size() <= 255 && peaks && peaks->size() == w->channels,
            "Invalid cache arrays");
    for (auto &s : *slices)
    {
        double a = (double)s["start"], b = (double)s["stop"];
        require(std::isfinite(a) && std::isfinite(b) && a >= 0 && b > a && b <= w->duration,
                "Invalid cache slices");
        w->slices.push_back({a, b});
    }
    for (auto &p : *peaks)
    {
        auto *a = p.getArray();
        require(a && a->size() > 0 && a->size() <= 8192 && a->size() % 2 == 0, "Invalid cache peaks");
        std::vector<int16_t> channel;
        for (auto &x : *a)
        {
            int n = (int)x;
            require(n >= -32768 && n <= 32767, "Invalid peak value");
            channel.push_back((int16_t)n);
        }
        w->peaks.push_back(std::move(channel));
    }
    require(w->channels == 1 || w->peaks[0].size() == w->peaks[1].size(), "Invalid channel lengths");
    auto s = v["spectrum"];
    juce::MemoryOutputStream bytes;
    require((int)s["bands"] == 32 && (int)s["frameRate"] == 20 && (int)s["minHz"] == 50 &&
                (int)s["maxHz"] == 16000 && juce::Base64::convertFromBase64(bytes, s["levels"].toString()),
            "Invalid cache spectrum");
    require(bytes.getDataSize() == 32 * (size_t)std::max(1, (int)std::ceil(w->duration * 20)) &&
                (int)s["frames"] == (int)(bytes.getDataSize() / 32),
            "Invalid spectrum length");
    const auto *data = (const uint8_t *)bytes.getData();
    w->spectrum.assign(data, data + bytes.getDataSize());
    return w;
}
std::array<float, 32> spectrumAt(const Wave *w, std::optional<double> position)
{
    std::array<float, 32> result{};
    if (!w || !position || !std::isfinite(*position) || w->spectrum.empty())
        return result;
    size_t frames = w->spectrum.size() / 32;
    double index = juce::jlimit(0.0, (double)frames - 1, *position * 20);
    size_t first = (size_t)index, next = std::min(first + 1, frames - 1);
    double mix = index - first;
    for (size_t b = 0; b < 32; ++b)
        result[b] =
            (float)((w->spectrum[first * 32 + b] * (1 - mix) + w->spectrum[next * 32 + b] * mix) / 255);
    return result;
}
} // namespace zv
