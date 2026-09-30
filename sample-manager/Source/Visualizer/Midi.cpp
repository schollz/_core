#include "Midi.h"
namespace zv {
std::shared_ptr<MidiLink> MidiLink::acquire(const juce::String &in,
                                            const juce::String &out) {
  // Only accessed on the message thread. Expired keys are removed on
  // acquisition.
  jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
  static std::map<std::pair<juce::String, juce::String>,
                  std::weak_ptr<MidiLink>>
      links;
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
MidiLink::MidiLink(juce::String in, juce::String out)
    : inputId(std::move(in)), outputId(std::move(out)) {
  refresh();
  startTimerHz(120);
}
MidiLink::~MidiLink() {
  stopTimer();
  close();
}
void MidiLink::close() {
  if (input)
    input->stop();
  input.reset();
  output.reset();
  const juce::ScopedLock lock(queueMutex);
  head = tail = 0;
  overflow = false;
  state = {};
  protocol = Protocol::unknown;
  probeStage = 0;
}
void MidiLink::refresh() {
  const auto inputs = juce::MidiInput::getAvailableDevices(),
             outputs = juce::MidiOutput::getAvailableDevices();
  auto has = [](const auto &ports, const juce::String &id) {
    for (auto &p : ports)
      if (p.identifier == id)
        return true;
    return false;
  };
  if (!has(inputs, inputId) || !has(outputs, outputId)) {
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
  if (!input || !output) {
    close();
    error = "Cannot open MIDI ports. Check the device and port selection.";
    return;
  }
  error.clear();
  input->start();
  polled = -1e9;
  probingAt = nowMs();
  sendHello();
}
void MidiLink::handleIncomingMidiMessage(juce::MidiInput *,
                                         const juce::MidiMessage &message) {
  const auto n = message.getRawDataSize();
  if (n < 3 || n > 128)
    return;
  const juce::ScopedTryLock lock(queueMutex);
  if (!lock.isLocked()) {
    overflow = true;
    return;
  }
  const auto next = (head + 1) % queue.size();
  if (next == tail) {
    overflow = true;
    return;
  }
  auto &p = queue[head];
  p.size = (size_t)n;
  p.at = nowMs();
  std::memcpy(p.data.data(), message.getRawData(), p.size);
  head = next;
}
void MidiLink::timerCallback() {
  const double now = nowMs();
  if (now - checked >= 2000) {
    refresh();
    checked = now;
  }
  if (overflow.exchange(false)) {
    const juce::ScopedLock lock(queueMutex);
    tail = head;
    state = {};
    polled = -1e9;
  }
  for (;;) {
    Packet packet;
    {
      const juce::ScopedLock lock(queueMutex);
      if (tail == head)
        break;
      packet = queue[tail];
      tail = (tail + 1) % queue.size();
    }
    if (packet.data[0] == 0xf0 && packet.size > 2 && packet.data[packet.size - 1] == 0xf7) {
      auto text = juce::String::fromUTF8(
          reinterpret_cast<const char *>(packet.data.data() + 1), int(packet.size - 2));
      log.add(text);
      if (text == "core_caps=1") { protocol = Protocol::sysex; error.clear(); }
      else if (probeStage >= 2 && text.startsWith("version=") && text.length() > 8 &&
               protocol != Protocol::sysex) { protocol = Protocol::legacy; error.clear(); }
    } else
      log.add(juce::String::toHexString(packet.data.data(), int(packet.size)));
    while (log.size() > 500)
      log.remove(0);
    if (auto m = decode(packet.data.data(), packet.size))
      state.receive(*m, packet.at);
  }
  if (connected() && protocol == Protocol::unknown) {
    const auto elapsed = now - probingAt;
    if (probeStage == 0 && elapsed >= 500) { probeStage = 1; sendHello(); }
    if (probeStage == 1 && elapsed >= 1000) {
      probeStage = 2;
      output->sendMessageNow(juce::MidiMessage::controllerEvent(1, 1, 0));
    }
    if (probeStage == 2 && elapsed >= 2000) {
      probeStage = 3;
      error = "Device management is unavailable: no protocol response. Reconnect to retry.";
    }
  }
  if (telemetry && connected() && protocol != Protocol::unknown && now - polled >= 500) {
    const uint8_t lease[]{0x89, 5, 0}, legacy[]{0x89, 4, 0};
    sendManagement("view", lease);
    if (!fresh(state.receivedAt, now)) sendManagement("info", legacy);
    polled = now;
  }
}
void MidiLink::sendHello() {
  const juce::String text = "core_cmd=1,hello";
  output->sendMessageNow(juce::MidiMessage::createSysExMessage(text.toRawUTF8(), text.getNumBytesAsUTF8()));
}
void MidiLink::sendManagement(const juce::String &operation, const uint8_t *legacy) {
  if (protocol == Protocol::unknown)
    throw std::runtime_error("Device management is not ready. Wait for protocol detection or reconnect.");
  if (protocol == Protocol::sysex) {
    const auto text = "core_cmd=1," + operation;
    output->sendMessageNow(juce::MidiMessage::createSysExMessage(text.toRawUTF8(), text.getNumBytesAsUTF8()));
  } else output->sendMessageNow(juce::MidiMessage(legacy, 3));
}

void MidiLink::command(int command) {
  if (!connected())
    throw std::runtime_error("Select a connected device first");
  if (command < 0 || command > 1)
    throw std::runtime_error("Unsupported device command");
  const uint8_t legacy[]{0xb0, uint8_t(command), 0};
  sendManagement(command ? "version" : "bootloader", legacy);
}
} // namespace zv
