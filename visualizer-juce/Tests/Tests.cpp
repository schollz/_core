#include "Core.h"
#include "Library.h"
#include "PluginProcessor.h"
#include "VisualizerView.h"
#include <juce_graphics/juce_graphics.h>
#include <juce_cryptography/juce_cryptography.h>
#include <FixtureData.h>
#include <iostream>
#include <cmath>
#include <stdexcept>

namespace
{
void expect(bool condition, const char *label)
{
    if (!condition)
        throw std::runtime_error(label);
}
void near(double a, double b, const char *label)
{
    expect(std::abs(a - b) < 1e-8, label);
}
void put16(uint8_t *p, uint16_t n)
{
    p[0] = (uint8_t)n;
    p[1] = (uint8_t)(n >> 8);
}
void put32(uint8_t *p, uint32_t n)
{
    for (int i = 0; i < 4; ++i)
        p[i] = (uint8_t)(n >> (i * 8));
}
struct Files
{
    juce::MemoryBlock wav, info;
};
Files fixture(int channels = 2, int rate = 44100, bool tone = false)
{
    const int frames = rate * 2, stride = channels * 2;
    Files f;
    f.wav.setSize((size_t)(44 + frames * stride), true);
    auto *w = (uint8_t *)f.wav.getData();
    std::memcpy(w, "RIFF", 4);
    put32(w + 4, (uint32_t)f.wav.getSize() - 8);
    std::memcpy(w + 8, "WAVEfmt ", 8);
    put32(w + 16, 16);
    put16(w + 20, 1);
    put16(w + 22, (uint16_t)channels);
    put32(w + 24, (uint32_t)rate);
    put32(w + 28, (uint32_t)(rate * stride));
    put16(w + 32, (uint16_t)stride);
    put16(w + 34, 16);
    std::memcpy(w + 36, "data", 4);
    put32(w + 40, (uint32_t)(frames * stride));
    for (int i = 0; i < frames; ++i)
        for (int ch = 0; ch < channels; ++ch)
        {
            int value = i < rate / 2 || i >= rate * 3 / 2 ? 30000 : ch ? -1000 : 2000;
            if (tone)
            {
                value = (int)std::floor(
                    16000 * std::sin(2 * juce::MathConstants<double>::pi * 1000 * (i - rate / 2) / rate) +
                    0.5);
                if (ch)
                    value = -value;
            }
            put16(w + 44 + i * stride + ch * 2, (uint16_t)value);
        }
    f.info.setSize(29, true);
    auto *info = (uint8_t *)f.info.getData();
    uint32_t size = (uint32_t)(rate * stride);
    put32(info, size);
    put32(info + 4, 120 | (1 << 13) | ((uint32_t)(rate / 44100 - 1) << 14) |
                        ((uint32_t)(channels - 1) << 15) | (1 << 16));
    info[10] = 2;
    put32(info + 11, 0);
    put32(info + 15, size / 4);
    put32(info + 19, size / 4);
    put32(info + 23, size);
    return f;
}
std::optional<zv::Message> sysex(const juce::String &text)
{
    std::vector<uint8_t> data{0xf0};
    for (auto c : text.toStdString())
        data.push_back((uint8_t)c);
    data.push_back(0xf7);
    return zv::decode(data.data(), data.size());
}
zv::Playback state()
{
    zv::Playback p;
    p.bpm = 120;
    p.valid = true;
    p.trigger = 1;
    return p;
}
std::shared_ptr<zv::Wave> wave()
{
    auto w = std::make_shared<zv::Wave>();
    w->bpm = 120;
    w->tempoMatch = true;
    w->playMode = 1;
    w->duration = 2;
    w->slices = {{0, 1}, {1, 2}};
    return w;
}
void protocolTests()
{
    expect(sysex("view=1,0,0,0,1,120,1,0,0,1").has_value(), "v1 decode");
    auto v2 = sysex("view=2,15,15,254,65535,511,0,1,1,1,65535");
    expect(v2 && v2->playback.effects == 65535, "v2 effects");
    for (auto text : {"view=2,0,0,0,1,120,1,0,0,1", "view=1,16,0,0,1,120,1,0,0,1",
                      "view=1,0,0,-1,1,120,1,0,0,1", "view=1,0,0,0,4294967296,120,1,0,0,1",
                      "view=1,0,0,0,1.5,120,1,0,0,1", "view=2,0,0,0,1,120,1,0,0,1,65536"})
        expect(!sysex(text), "strict decode");
    expect(sysex("info=1,2,170,180,0,1,0")->legacy.stopped, "legacy");
    const uint8_t partial[]{0xf0, 'v', 'i', 'e', 'w'};
    expect(!zv::decode(partial, sizeof partial), "partial sysex ignored");
    for (int channel = 0; channel < 16; ++channel)
    {
        const uint8_t note[]{(uint8_t)(0x90 + channel), 5, 100};
        expect(zv::decode(note, 3).has_value() == (channel < 3), "physical pads only");
    }
    const uint8_t off[]{0x90, 5, 0};
    expect(!zv::decode(off, 3), "zero velocity ignored");
    expect(zv::fresh(0, 1499) && !zv::fresh(0, 1500) && !zv::fresh({}, 0), "stale deadline");
    zv::DeviceState d;
    auto m = *sysex("view=2,0,0,0,1,120,1,0,0,1,1");
    d.receive(m, 100);
    d.receive(zv::Message{zv::Message::button, {}, {}, 6}, 110);
    m.playback.slice = 1;
    d.receive(m, 115);
    expect(d.press && d.press->button == 6 && d.press->slice == 1, "first snapshot associates pad");
    m.playback.slice = 0;
    d.receive(m, 120);
    expect(d.press->slice == 1, "automatic step does not move pad association");
}
void playbackTests()
{
    zv::Playhead clock;
    auto p = state();
    auto w = wave();
    clock.update(p, w, 1000);
    near(*clock.value(1400), .4, "advance");
    clock.update(p, w, 1500);
    near(*clock.value(1700), .7, "heartbeat");
    p.trigger = 2;
    clock.update(p, w, 1800);
    near(*clock.value(1900), .1, "retrigger");
    near(*clock.value(5000), 1, "slice clamp");
    p = state();
    clock.reset();
    clock.update(p, w, 0);
    p.bpm = 240;
    clock.update(p, w, 200);
    near(*clock.value(400), .6, "tempo continuity");
    p.bpm = 120;
    p.forward = false;
    clock.update(p, w, 500);
    near(*clock.value(700), .8, "reverse");
    p.stopped = true;
    clock.update(p, w, 800);
    near(*clock.value(2000), .7, "stop");
    p.valid = false;
    clock.update(p, w, 2100);
    expect(!clock.value(2200), "invalid state");
    for (int mode = 0; mode < 5; ++mode)
        for (bool forward : {false, true})
        {
            w = wave();
            w->duration = 4;
            w->slices = {{1, 3}};
            w->playMode = mode;
            p = state();
            p.forward = forward;
            clock.reset();
            clock.update(p, w, 0);
            const double expected = mode == 1   ? (forward ? 3 : 1)
                                    : mode == 2 ? (forward ? 1.5 : 2.5)
                                                : (forward ? 3.5 : .5);
            near(*clock.value(2500), expected, "play modes");
        }
    w = wave();
    w->playMode = 0;
    w->duration = 30;
    w->slices = {{0, 10.27}, {10.27, 30}};
    p = state();
    clock.reset();
    clock.update(p, w, 0);
    near(*clock.value(11000), 11, "normal crosses slices");
    p.trigger = 65535;
    clock.update(p, w, 12000);
    p.trigger = 0;
    clock.update(p, w, 13000);
    near(*clock.value(14000), 1, "serial wraps");
    zv::Transition transition;
    p = state();
    transition.update(p, 0);
    p.sample = 1;
    p.valid = false;
    auto first = transition.update(p, 100);
    expect(first.state.estimated && first.state.valid, "provisional sample");
    expect(transition.update(p, 500).at == 100, "fixed estimate anchor");
    expect(!transition.update(p, 1600).state.valid, "estimate expires");
    p.valid = true;
    auto valid = transition.update(p, 1700);
    expect(!valid.state.estimated && valid.at == 1700, "valid replaces estimate");
    zv::Transition initial;
    p.valid = false;
    expect(!initial.update(p, 0).state.valid, "no initial speculation");
}
void analysisTests()
{
    auto golden =
        juce::JSON::parse(juce::String::fromUTF8(FixtureData::parity_json, FixtureData::parity_jsonSize));
    for (auto &item : *golden["cases"].getArray())
    {
        auto f = fixture((int)item["channels"], (int)item["rate"], true);
        auto w = zv::analyse(f.wav, f.info, 0, 0);
        juce::MemoryOutputStream peaks;
        for (auto &channel : w.peaks)
            for (auto value : channel)
                peaks.writeShort(value);
        expect(juce::SHA256(peaks.getData(), peaks.getDataSize()).toHexString() ==
                   item["peaksSHA256"].toString(),
               "browser peak parity");
        near(w.duration, (double)item["duration"], "browser duration parity");
        auto *slices = item["slices"].getArray();
        expect((int)w.slices.size() == slices->size(), "browser slice count");
        for (size_t i = 0; i < w.slices.size(); ++i)
        {
            near(w.slices[i].start, (double)(*slices)[(int)i]["start"], "browser slice start");
            near(w.slices[i].stop, (double)(*slices)[(int)i]["stop"], "browser slice stop");
        }
        juce::MemoryOutputStream expected;
        expect(juce::Base64::convertFromBase64(expected, item["spectrum"]["levels"].toString()),
               "decode oracle");
        expect(expected.getDataSize() == w.spectrum.size(), "browser spectrum frames");
        const auto *data = (const uint8_t *)expected.getData();
        for (size_t i = 0; i < w.spectrum.size(); ++i)
            expect(std::abs((int)data[i] - (int)w.spectrum[i]) <= 1, "browser spectrum parity");
    }
    for (int rate : {44100, 88200})
        for (int channels : {1, 2})
        {
            auto f = fixture(channels, rate);
            auto w = zv::analyse(f.wav, f.info, 0, 0);
            near(w.duration, 1, "remove padding");
            near(w.slices[0].stop, .25, "slice offsets");
            expect(w.peaks[0].size() == 8192, "peak resolution");
            for (auto x : w.peaks[0])
                expect(x == 2000, "mono peaks");
            if (channels == 2)
                for (auto x : w.peaks[1])
                    expect(x == -1000, "stereo polarity");
            auto round = zv::waveFromJson(zv::waveToJson(w));
            expect(round->spectrum == w.spectrum && round->peaks == w.peaks, "cache roundtrip");
            auto t = fixture(channels, rate, true);
            auto tone = zv::analyse(t.wav, t.info, 0, 0);
            auto bars = zv::spectrumAt(&tone, .5);
            size_t peak = (size_t)(std::max_element(bars.begin(), bars.end()) - bars.begin());
            expect(50 * std::pow(320.0, peak / 32.0) < 1000 &&
                       50 * std::pow(320.0, (peak + 1) / 32.0) > 1000 && bars[peak] > .8f,
                   "tone band");
        }
    auto mono = fixture(1, 44100, true), stereo = fixture(2, 44100, true);
    expect(zv::analyse(mono.wav, mono.info, 0, 0).spectrum ==
               zv::analyse(stereo.wav, stereo.info, 0, 0).spectrum,
           "stereo powers do not cancel");
    auto invalid = fixture();
    invalid.wav.setSize(invalid.wav.getSize() - 1);
    bool rejected = false;
    try
    {
        zv::analyse(invalid.wav, invalid.info, 0, 0);
    }
    catch (...)
    {
        rejected = true;
    }
    expect(rejected, "truncated WAV rejected");
    invalid = fixture();
    put32((uint8_t *)invalid.info.getData() + 19, 9999999);
    rejected = false;
    try
    {
        zv::analyse(invalid.wav, invalid.info, 0, 0);
    }
    catch (...)
    {
        rejected = true;
    }
    expect(rejected, "invalid slice rejected");
    auto w = wave();
    w->spectrum.resize(64);
    std::fill(w->spectrum.begin() + 32, w->spectrum.end(), 255);
    near(zv::spectrumAt(w.get(), .025)[0], .5, "spectrum interpolation");
    expect(zv::spectrumAt(w.get(), {})[0] == 0, "inactive spectrum");
}
void libraryTests()
{
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getNonexistentChildFile("zeptocore-test", {}, false);
    root.createDirectory();
    struct Cleanup
    {
        juce::File f;
        ~Cleanup()
        {
            f.deleteRecursively();
        }
    } cleanup{root};
    auto ref = root.getChildFile("reference"), cache = root.getChildFile("cache");
    ref.getChildFile("bank2").createDirectory();
    auto files = fixture();
    auto audio = ref.getChildFile("bank2/3.0.wav"), info = ref.getChildFile("bank2/3.0.wav.info");
    audio.replaceWithData(files.wav.getData(), files.wav.getSize());
    auto bad = zv::Library::prepare(ref, cache);
    expect(bad.samples.size() == 1 && !bad.samples[0].error.isEmpty(), "bad sample isolated");
    info.replaceWithData(files.info.getData(), files.info.getSize());
    ref.getChildFile("bank2/3.1.wav").replaceWithData(files.wav.getData(), files.wav.getSize());
    auto first = zv::Library::prepare(ref, cache);
    expect(first.prepared == 1 && first.total == 1 && first.samples[0].wave, "library discovery");
    auto second = zv::Library::prepare(ref, cache);
    expect(second.reused == 1 && second.prepared == 0, "cache reused");
    auto entries = cache.findChildFiles(juce::File::findFiles, true, "*.json");
    expect(entries.size() == 1, "cache stored");
    entries[0].replaceWithText("broken");
    expect(zv::Library::prepare(ref, cache).prepared == 1, "corrupt cache rebuilt");
    put32((uint8_t *)files.info.getData() + 4, 121 | (1 << 13) | (1 << 15) | (1 << 16));
    info.replaceWithData(files.info.getData(), files.info.getSize());
    auto changed = zv::Library::prepare(ref, cache);
    expect(changed.prepared == 1 && changed.samples[0].wave->bpm == 121, "changed metadata rebuilt");
    juce::MemoryBlock after;
    audio.loadFileAsData(after);
    expect(after == files.wav, "reference unchanged");
    audio.deleteFile();
    expect(zv::Library::prepare(ref, cache).samples.empty(), "deleted sample removed");
    expect(!zv::Library::prepare(root.getChildFile("missing"), cache).error.isEmpty(), "missing folder");
    zv::Library async;
    async.load(ref);
    async.load(root.getChildFile("missing")); // destruction must cancel/join safely
}
template <typename T> void audioTest(zv::Processor &p)
{
    for (int channels : {1, 2})
        for (int size : {1, 64, 257, 1024})
        {
            juce::AudioProcessor::BusesLayout layout;
            layout.inputBuses.add(channels == 1 ? juce::AudioChannelSet::mono()
                                                : juce::AudioChannelSet::stereo());
            layout.outputBuses = layout.inputBuses;
            expect(p.setBusesLayout(layout), "audio layout");
            p.prepareToPlay(48000, size);
            juce::AudioBuffer<T> b(channels, size), original(channels, size);
            juce::MidiBuffer midi;
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < size; ++i)
                    b.setSample(c, i, (T)std::sin(i * .01 + c));
            original.makeCopyOf(b);
            p.processBlock(b, midi);
            p.processBlockBypassed(b, midi);
            for (int c = 0; c < channels; ++c)
                expect(std::memcmp(b.getReadPointer(c), original.getReadPointer(c),
                                   (size_t)size * sizeof(T)) == 0,
                       "bit exact passthrough");
        }
}
void processorTests()
{
    zv::Processor p;
    audioTest<float>(p);
    audioTest<double>(p);
    expect(p.getLatencySamples() == 0 && p.getTailLengthSeconds() == 0, "zero latency and tail");
    auto s = p.session.settings();
    s.root = "/tmp/reference with spaces";
    s.input = "test-in";
    s.output = "test-out";
    s.reduceMotion = true;
    s.width = 812;
    p.session.setSettings(s);
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    zv::Processor restored;
    restored.setStateInformation(saved.getData(), (int)saved.getSize());
    auto r = restored.session.settings();
    expect(r.root == s.root && r.input == s.input && r.output == s.output && r.width == 812 && r.reduceMotion,
           "host state restore");
    const char bad[] = "not json";
    restored.setStateInformation(bad, sizeof bad);
    expect(restored.session.settings().root == s.root, "bad state ignored");
    for (int i = 0; i < 3; ++i)
    {
        const int expectedWidth = p.session.settings().width;
        const int expectedHeight = p.session.settings().height;
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        expect(editor->getWidth() == expectedWidth && editor->getHeight() == expectedHeight,
               "editor restores saved size");
        editor->setSize(640, 400);
    }
}

} // namespace
int runTests()
{
    int failures = 0;
    for (auto test : {std::make_pair("protocol", protocolTests), std::make_pair("playback", playbackTests),
                      std::make_pair("analysis", analysisTests), std::make_pair("library", libraryTests),
                      std::make_pair("processor", processorTests)})
    {
        try
        {
            test.second();
            std::cout << "PASS " << test.first << std::endl;
        }
        catch (const std::exception &e)
        {
            ++failures;
            std::cerr << "FAIL " << test.first << ": " << e.what() << std::endl;
        }
    }
    return failures ? 1 : 0;
}
int analyseFile(const juce::StringArray &args)
{
    try
    {
        juce::MemoryBlock wav, info;
        if (!juce::File(args[1]).loadFileAsData(wav) || !juce::File(args[2]).loadFileAsData(info))
            throw std::runtime_error("Cannot read input files");
        auto w = zv::analyse(wav, info, 0, 0);
        return juce::File(args[3]).replaceWithText(juce::JSON::toString(zv::waveToJson(w))) ? 0 : 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
int renderFixtures(const juce::File &directory)
{
    directory.createDirectory();
    auto f = fixture(2, 44100, true);
    auto w = std::make_shared<zv::Wave>(zv::analyse(f.wav, f.info, 0, 0));
    // A varied display envelope makes slice clipping and low-amplitude geometry visible.
    for (size_t i = 0; i < w->peaks[0].size() / 2; ++i)
    {
        int16_t peak = (int16_t)(28000 * (.08 + .92 * std::pow(std::abs(std::sin(i * .011)), 3)));
        for (auto &ch : w->peaks)
        {
            ch[i * 2] = (int16_t)-peak;
            ch[i * 2 + 1] = peak;
        }
    }
    const double now = zv::nowMs();
    for (const auto name : {"playing", "reverse", "stopped", "muted", "stale", "effects", "callout",
                            "compact", "setup", "missing"})
    {
        zv::Processor p;
        auto s = state();
        s.effects = 0xa551;
        s.slice = 1;
        s.forward = juce::String(name) != "reverse";
        s.stopped = juce::String(name) == "stopped";
        s.muted = juce::String(name) == "muted";
        zv::DeviceState device;
        device.receive({zv::Message::view, s, {}, 0}, now - (juce::String(name) == "stale" ? 2000 : 180));
        if (juce::String(name) == "callout")
            device.press = zv::Press{7, 0, 0, 1, now - 200};
        p.session.useFixture(w, device, now);
        zv::Editor editor(p);
        editor.setSize(juce::String(name) == "compact" ? 480 : 1000,
                       juce::String(name) == "compact" ? 320 : 650);
        if (juce::String(name) == "missing")
        {
            p.session.wave.reset();
            auto settings = p.session.settings();
            settings.root.clear();
            p.session.setSettings(settings);
        }
        if (juce::String(name) == "setup")
        {
            p.session.choosePorts = true;
            p.session.inputs.add({"zeptocore A", "a"});
            p.session.outputs.add({"zeptocore B", "b"});
        }
        editor.renderAt(now);
        auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0);
        juce::FileOutputStream output(directory.getChildFile(juce::String(name) + ".png"));
        if (!output.openedOk() || !output.setPosition(0) || output.truncate().failed() ||
            !juce::PNGImageFormat().writeImageToStream(image, output))
            return 1;
    }
    std::cout << "Rendered fixtures to " << directory.getFullPathName() << std::endl;
    return 0;
}
