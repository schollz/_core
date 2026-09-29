#pragma once
#include "../Device.h"
#include "../Manager.h"
#include "../Preview.h"
namespace zv {
struct Settings {
  juce::String root;
  bool reduceMotion = false;
};
class Session {
public:
  Session(core::Manager &m, core::Device &d, core::Preview &p)
      : manager(m), sharedDevice(d), preview(p) {}
  core::Manager &manager;
  core::Device &sharedDevice;
  core::Preview &preview;
  bool previewSource = false, connected = false, active = false;
  int bank = -1, sample = -1;
  juce::String selectedId, connection, connectionError;
  LibraryState libraryState;
  DeviceState device;
  std::shared_ptr<const Wave> wave;
  Playhead playhead;
  void tick(double now);
  void enable(bool);
  Settings settings() const {
    return {libraryState.root, sharedDevice.reduceMotion};
  }
  bool displayFresh(double now) const;
  std::optional<double> position(double now) const;
};
} // namespace zv
