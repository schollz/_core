#include "Midi.h"
#include <iostream>
namespace
{
class MidiTest final : private juce::MidiInputCallback, private juce::Timer
{
  public:
    explicit MidiTest(std::function<void(int)> completion)
        : finished(std::move(completion)), name("visualizer-test-" + juce::Uuid().toString())
    {
        notifications = juce::MidiDeviceListConnection::make([] {});
        tx = juce::MidiOutput::createNewDevice(name + "-source");
        rx = juce::MidiInput::createNewDevice(name + "-destination", this);
        if (rx)
            rx->start();
        startTimer(60);
    }
    ~MidiTest() override
    {
        stopTimer();
        shared.reset();
        link.reset();
        if (rx)
            rx->stop();
        rx.reset();
        tx.reset();
    }

  private:
    void check(bool ok, const char *message)
    {
        if (!ok)
            throw std::runtime_error(message);
    }
    bool open()
    {
        juce::String in, out;
        for (auto &p : juce::MidiInput::getAvailableDevices())
        {
            std::cout << "MIDI input: " << p.name << std::endl;
            if (p.name.contains(name + "-source"))
                in = p.identifier;
        }
        for (auto &p : juce::MidiOutput::getAvailableDevices())
        {
            std::cout << "MIDI output: " << p.name << std::endl;
            if (p.name.contains(name + "-destination"))
                out = p.identifier;
        }
        if (in.isEmpty() || out.isEmpty())
            return false;
        link = zv::MidiLink::acquire(in, out);
        shared = zv::MidiLink::acquire(in, out);
        check(link == shared && link->connected(), "share native MIDI connection");
        return true;
    }
    void handleIncomingMidiMessage(juce::MidiInput *, const juce::MidiMessage &m) override
    {
        const auto *b = m.getRawData();
        if (m.getRawDataSize() == 3 && b[0] == 0x89 && b[2] == 0)
        {
            if (b[1] == 5)
                ++leases;
            if (b[1] == 4)
                ++legacy;
        }
    }
    void timerCallback() override
    {
        try
        {
            switch (stage++)
            {
            case 0:
                check(tx && rx, "create native virtual MIDI endpoints");
                if (!open())
                {
                    check(++attempts < 30, "enumerate virtual endpoints");
                    --stage;
                }
                startTimer(100);
                break;
            case 1:
            {
                check(leases >= 1 && legacy >= 1, "subscription and legacy fallback");
                const juce::String body = "view=2,0,0,1,65535,120,1,0,0,1,17";
                tx->sendMessageNow(
                    juce::MidiMessage::createSysExMessage(body.toRawUTF8(), (int)body.getNumBytesAsUTF8()));
                break;
            }
            case 2:
            {
                check(link->state.playback && link->state.playback->trigger == 65535 &&
                          link->state.playback->effects == 17,
                      "complete native SysEx reception");
                const uint8_t note[]{0x90, 6, 100};
                tx->sendMessageNow(juce::MidiMessage(note, 3));
                break;
            }
            case 3:
                check(link->state.press && link->state.press->button == 7, "native pad reception");
                before = leases;
                fallback = legacy;
                startTimer(600);
                break;
            case 4:
                check(leases - before >= 1 && leases - before <= 2 && legacy == fallback,
                      "single renewal stream while fresh");
                tx.reset();
                startTimer(100);
                break;
            case 5:
                link->refresh();
                if (link->connected())
                {
                    check(++attempts < 30, "unplug removes endpoint");
                    --stage;
                    break;
                }
                check(!link->state.playback, "unplug clears stale device state");
                shared.reset();
                link.reset();
                before = leases;
                startTimer(600);
                break;
            case 6:
                check(leases == before, "last subscriber closes polling");
                tx = juce::MidiOutput::createNewDevice(name + "-source");
                check(tx != nullptr, "reconnect virtual device");
                startTimer(100);
                break;
            case 7:
                if (!open())
                {
                    check(++attempts < 60, "enumerate reconnected endpoints");
                    --stage;
                }
                break;
            case 8:
                check(leases > before, "reconnect resumes polling");
                stopTimer();
                std::cout << "PASS native MIDI / SysEx, pads, subscriptions, shared instances, "
                             "unplug/reconnect, teardown"
                          << std::endl;
                finished(0);
                break;
            }
        }
        catch (const std::exception &e)
        {
            stopTimer();
            std::cerr << "FAIL native MIDI: " << e.what() << std::endl;
            finished(1);
        }
    }
    std::function<void(int)> finished;
    juce::String name;
    std::atomic<int> leases{0}, legacy{0};
    int stage = 0, before = 0, fallback = 0, attempts = 0;
    juce::MidiDeviceListConnection notifications;
    std::unique_ptr<juce::MidiOutput> tx;
    std::unique_ptr<juce::MidiInput> rx;
    std::shared_ptr<zv::MidiLink> link, shared;
};
} // namespace
std::shared_ptr<void> startMidiTests(std::function<void(int)> completed)
{
    return std::make_shared<MidiTest>(std::move(completed));
}
