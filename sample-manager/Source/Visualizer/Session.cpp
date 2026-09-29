#include "Session.h"
namespace zv {
void Session::enable(bool enabled) {
  active = enabled;
  sharedDevice.telemetry(enabled && !previewSource);
  if (!enabled) {
    wave.reset();
    playhead.reset();
    device = {};
  }
}
bool Session::displayFresh(double now) const {
  return wave && device.display && device.display->state.valid &&
         (previewSource || (connected && fresh(device.display->at, now))) &&
         device.display->state.bank == wave->bank &&
         device.display->state.sample == wave->sample &&
         device.display->state.slice < int(wave->slices.size());
}
std::optional<double> Session::position(double now) const {
  if (previewSource)
    return preview.playing() ? std::optional<double>(preview.position())
                             : std::nullopt;
  return playhead.value(now);
}
void Session::tick(double now) {
  if (!active)
    return;
  auto state = manager.snapshot();
  libraryState = state.library;
  wave.reset();
  connectionError.clear();
  bank = sample = -1;
  if (previewSource) {
    device = {};
    auto id = preview.playing() ? preview.sampleId() : selectedId;
    const auto *s = state.completedProject.find(id);
    if (s) {
      bank = s->bank;
      sample = s->slot;
    }
    connected = false;
    connection =
        preview.playing() ? "PREVIEW / actual transport" : "PREVIEW / stopped";
  } else {
    connected = sharedDevice.connected();
    device = sharedDevice.state();
    connection = connected ? "DEVICE" : "Waiting for Core hardware";
    connectionError = sharedDevice.error;
    if (device.playback) {
      bank = device.playback->bank;
      sample = device.playback->sample;
    } else if (device.legacy) {
      bank = device.legacy->bank;
      sample = device.legacy->sample;
    } else if (!libraryState.samples.empty()) {
      bank = libraryState.samples.front().bank;
      sample = libraryState.samples.front().sample;
    }
  }
  for (const auto &s : libraryState.samples)
    if (s.bank == bank && s.sample == sample)
      wave = s.wave;
  if (previewSource && wave) {
    Playback p;
    p.bank = bank;
    p.sample = sample;
    p.bpm = wave->bpm;
    p.valid = true;
    p.stopped = !preview.playing();
    p.effects.reset();
    double position = preview.position();
    for (size_t n = 0; n < wave->slices.size(); ++n)
      if (position >= wave->slices[n].start && position < wave->slices[n].stop)
        p.slice = int(n);
    device.display = Display{p, now};
  } else if (device.display && wave)
    playhead.update(device.display->state, wave, device.display->at);
}
} // namespace zv
