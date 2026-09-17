#include "Midi.h"
namespace zv
{
std::shared_ptr<MidiLink> MidiLink::acquire(const juce::String &in, const juce::String &out)
{
    // Only accessed on the message thread. Expired keys are removed on acquisition.
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    static std::map<std::pair<juce::String, juce::String>, std::weak_ptr<MidiLink>> links;
    for (auto it = links.begin(); it != links.end();)
        if (it->second.expired())
            it = links.erase(it);
        else
            ++it;
    const auto key = std::make_pair(in, out);
    if (auto link = links[key].lock())
        return link;
    auto link = std::shared_ptr<MidiLink>(new MidiLink(in, out));
    links[key] = link;
    return link;
}
MidiLink::MidiLink(juce::String in, juce::String out) : inputId(std::move(in)), outputId(std::move(out))
{
    refresh();
    startTimerHz(120);
}
MidiLink::~MidiLink()
{
    stopTimer();
    close();
}
void MidiLink::close()
{
    if (input)
        input->stop();
    input.reset();
    output.reset();
    const juce::ScopedLock lock(queueMutex);
    head = tail = 0;
    overflow = false;
    state = {};
}
void MidiLink::refresh()
{
    const auto inputs = juce::MidiInput::getAvailableDevices(),
               outputs = juce::MidiOutput::getAvailableDevices();
    auto has = [](const auto &ports, const juce::String &id)
    {
        for (auto &p : ports)
            if (p.identifier == id)
                return true;
        return false;
    };
    if (!has(inputs, inputId) || !has(outputs, outputId))
    {
        if (connected())
            close();
        error = "Zeptocore disconnected";
        return;
    }
    if (connected())
        return;
    close();
    output = juce::MidiOutput::openDevice(outputId);
    input = juce::MidiInput::openDevice(inputId, this);
    if (!input || !output)
    {
        close();
        error = "Cannot open MIDI ports. Check the device and port selection.";
        return;
    }
    error.clear();
    input->start();
    polled = -1e9;
}
void MidiLink::handleIncomingMidiMessage(juce::MidiInput *, const juce::MidiMessage &message)
{
    const auto n = message.getRawDataSize();
    if (n < 3 || n > 128)
        return;
    const juce::ScopedTryLock lock(queueMutex);
    if (!lock.isLocked())
    {
        overflow = true;
        return;
    }
    const auto next = (head + 1) % queue.size();
    if (next == tail)
    {
        overflow = true;
        return;
    }
    auto &p = queue[head];
    p.size = (size_t)n;
    p.at = nowMs();
    std::memcpy(p.data.data(), message.getRawData(), p.size);
    head = next;
}
void MidiLink::timerCallback()
{
    const double now = nowMs();
    if (now - checked >= 2000)
    {
        refresh();
        checked = now;
    }
    if (overflow.exchange(false))
    {
        const juce::ScopedLock lock(queueMutex);
        tail = head;
        state = {};
        polled = -1e9;
    }
    for (;;)
    {
        Packet packet;
        {
            const juce::ScopedLock lock(queueMutex);
            if (tail == head)
                break;
            packet = queue[tail];
            tail = (tail + 1) % queue.size();
        }
        if (auto m = decode(packet.data.data(), packet.size))
            state.receive(*m, packet.at);
    }
    if (connected() && now - polled >= 500)
    {
        const uint8_t lease[]{0x89, 5, 0}, legacy[]{0x89, 4, 0};
        output->sendMessageNow(juce::MidiMessage(lease, 3));
        if (!fresh(state.receivedAt, now))
            output->sendMessageNow(juce::MidiMessage(legacy, 3));
        polled = now;
    }
}
} // namespace zv
