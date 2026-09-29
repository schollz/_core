#pragma once
#include "Project.h"
#include "Theme.h"
#include "Visualizer/Core.h"
namespace core {
// Adapted from amenbreak WaveformDisplay: normalized marker hit testing, wheel
// zoom, middle-button panning and drag gestures. Peaks share the completed
// project waveform/spectrum cache with the integrated visualizer.
class WaveformEditor final : public juce::Component {
public:
  explicit WaveformEditor(Look &);
  void set(const Sample *, std::shared_ptr<const zv::Wave>);
  void setPosition(double normalized);
  void paint(juce::Graphics &) override;
  void mouseDown(const juce::MouseEvent &) override;
  void mouseDrag(const juce::MouseEvent &) override;
  void mouseUp(const juce::MouseEvent &) override;
  void mouseDoubleClick(const juce::MouseEvent &) override;
  void mouseWheelMove(const juce::MouseEvent &,
                      const juce::MouseWheelDetails &) override;
  std::function<void(const Sample &)> onEdit;
  std::function<void(double, double)> onAudition;
  int lane = -1; // -1 slice, 0 kick, 1 snare, 2 other
private:
  double at(float x) const;
  float x(double normalized) const;
  int nearest(float x) const;
  void commit();
  Look &look;
  std::shared_ptr<const zv::Wave> peaks;
  Sample sample;
  bool hasSample = false, dragging = false, changed = false;
  int marker = -1;
  double zoom = 1, pan = 0, playhead = -1, dragPan = 0;
  float dragX = 0;
};
} // namespace core
