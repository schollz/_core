#include "Session.h"
namespace zv
{
namespace
{
juce::File preferences()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("com.infinitedigits.zeptocorevisualizer/settings.json");
}
} // namespace
juce::var Settings::toJson() const
{
    auto *o = new juce::DynamicObject;
    o->setProperty("version", 1);
    o->setProperty("referenceRoot", root);
    o->setProperty("input", input);
    o->setProperty("output", output);
    o->setProperty("width", width);
    o->setProperty("height", height);
    o->setProperty("reduceMotion", reduceMotion);
    return o;
}
Settings Settings::fromJson(const juce::var &v)
{
    Settings s;
    if (!v.isObject() || (int)v["version"] != 1)
        return s;
    s.root = v["referenceRoot"].toString();
    if (s.root.isNotEmpty() && !juce::File::isAbsolutePath(s.root))
        s.root.clear();
    s.input = v["input"].toString();
    s.output = v["output"].toString();
    s.width = juce::jlimit(480, 4000, (int)v.getProperty("width", 1000));
    s.height = juce::jlimit(320, 2400, (int)v.getProperty("height", 650));
    s.reduceMotion = (bool)v["reduceMotion"];
    return s;
}
Settings Settings::defaults()
{
    return fromJson(juce::JSON::parse(preferences()));
}
void Settings::saveDefaults() const
{
    const auto file = preferences();
    file.getParentDirectory().createDirectory();
    juce::TemporaryFile tmp(file);
    if (tmp.getFile().replaceWithText(juce::JSON::toString(toJson())))
        tmp.overwriteTargetFileWithTemporary();
}
Session::Session() : config(Settings::defaults())
{
    startTimerHz(60);
}
Session::~Session()
{
    // Hosts may delete processors from a non-UI thread. Finish any current tick
    // before releasing MIDI and library state; no lock enters audio processing.
    const juce::MessageManagerLock lock;
    stopTimer();
    midi.reset();
}
Settings Session::settings() const
{
    const juce::ScopedLock lock(settingsMutex);
    return config;
}
void Session::setSettings(Settings s, bool persist)
{
    const juce::ScopedLock lock(settingsMutex);
    config = std::move(s);
    dirty = true;
    persistRequested |= persist;
}
void Session::rescan()
{
    const auto s = settings();
    if (s.root.isNotEmpty())
        library.load(juce::File(s.root));
}
void Session::refreshPorts()
{
    inputs = juce::MidiInput::getAvailableDevices();
    outputs = juce::MidiOutput::getAvailableDevices();
    const auto s = settings();
    auto select = [](const auto &ports, const juce::String &requested)
    {
        for (auto &p : ports)
            if (p.identifier == requested)
                return p.identifier;
        juce::String found;
        int count = 0;
        for (auto &p : ports)
            if (p.name.containsIgnoreCase("zeptocore"))
            {
                found = p.identifier;
                ++count;
            }
        return count == 1 ? found : juce::String();
    };
    auto in = select(inputs, s.input), out = select(outputs, s.output);
    choosePorts = in.isEmpty() || out.isEmpty();
    if (!choosePorts)
    {
        if (!midi || midi->inputId != in || midi->outputId != out)
        {
            midi = MidiLink::acquire(in, out);
            device = {};
            lastDisplay.reset();
            playhead.reset();
        }
    }
    else
    {
        midi.reset();
        connected = false;
    }
}
bool Session::displayFresh(double now) const
{
    return connected && device.display && fresh(device.display->at, now) && device.display->state.valid &&
           wave && device.display->state.bank == wave->bank && device.display->state.sample == wave->sample &&
           device.display->state.slice < (int)wave->slices.size();
}
void Session::timerCallback()
{
    tick(nowMs());
}
void Session::tick(double now)
{
    if (fixture)
        return;
    bool changed, save;
    Settings s;
    {
        const juce::ScopedLock lock(settingsMutex);
        changed = dirty;
        save = persistRequested;
        dirty = persistRequested = false;
        s = config;
    }
    if (changed)
    {
        if (!applied || applied->root != s.root)
        {
            wave.reset();
            playhead.reset();
            lastDisplay.reset();
            libraryState = {};
            library.load(s.root.isEmpty() ? juce::File() : juce::File(s.root));
        }
        if (!applied || applied->input != s.input || applied->output != s.output)
            lastPorts = -1e9;
        applied = s;
        if (save)
            s.saveDefaults();
    }
    if (now - lastPorts >= 2000)
    {
        refreshPorts();
        lastPorts = now;
    }
    connected = midi && midi->connected();
    connectionError = midi ? midi->error : juce::String();
    connection = connected                                                ? "Connected"
                 : choosePorts && inputs.size() > 0 && outputs.size() > 0 ? "Select MIDI input and output"
                                                                          : "Waiting for zeptocore";
    if (midi)
        device = midi->state;
    libraryState = library.snapshot();
    bank = sample = -1;
    if (connected && fresh(device.receivedAt, now) && device.playback)
    {
        bank = device.playback->bank;
        sample = device.playback->sample;
    }
    else if (device.legacy &&
             (!device.playback || device.legacyAt.value_or(0) > device.receivedAt.value_or(0)))
    {
        bank = device.legacy->bank;
        sample = device.legacy->sample;
    }
    else if (device.playback)
    {
        bank = device.playback->bank;
        sample = device.playback->sample;
    }
    else if (!libraryState.samples.empty())
    {
        bank = libraryState.samples.front().bank;
        sample = libraryState.samples.front().sample;
    }
    library.prioritise(bank, sample);
    std::shared_ptr<const Wave> next;
    for (auto &item : libraryState.samples)
        if (item.bank == bank && item.sample == sample)
        {
            next = item.wave;
            break;
        }
    const bool waveChanged = next != wave;
    wave = next;
    if (waveChanged)
    {
        playhead.reset();
        lastDisplay.reset();
    }
    if (device.display && wave && (lastDisplay != device.receivedAt || waveChanged))
    {
        playhead.update(device.display->state, wave, device.display->at);
        lastDisplay = device.receivedAt;
    }
}
void Session::useFixture(std::shared_ptr<const Wave> w, DeviceState d, double now)
{
    fixture = true;
    stopTimer();
    midi.reset();
    device = std::move(d);
    wave = std::move(w);
    connected = true;
    choosePorts = false;
    bank = wave->bank;
    sample = wave->sample;
    libraryState = {};
    libraryState.samples.push_back({bank, sample, "fixture", {}, wave});
    if (device.display)
        playhead.update(device.display->state, wave, device.display->at);
    connection = "Connected";
    juce::ignoreUnused(now);
}
} // namespace zv
