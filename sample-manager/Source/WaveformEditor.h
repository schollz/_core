#pragma once
#include "Project.h"
#include "Theme.h"
#include "Visualizer/Core.h"
namespace core {
// Adapted from amenbreak WaveformDisplay: normalized marker hit testing, wheel
// zoom, middle-button panning and drag gestures. New imports show source peaks
// immediately; completed peaks share the integrated visualizer's cache.
class WaveformEditor final : public juce::Component, public juce::TooltipClient {
public:
  explicit WaveformEditor(Look &);
  void set(const Sample *, std::shared_ptr<const zv::Wave>, uint64_t acknowledged = UINT64_MAX);
  void setPosition(double normalized);
  void setTransientLanesVisible(bool);
  void setSnapToSlices(bool enabled) {
    snapToSlices = enabled;
    snapTarget = -1;
    repaint();
  }
  String getTooltip() override;
  void paint(juce::Graphics &) override;
  void mouseDown(const juce::MouseEvent &) override;
  void mouseDrag(const juce::MouseEvent &) override;
  void mouseUp(const juce::MouseEvent &) override;
  void mouseDoubleClick(const juce::MouseEvent &) override;
  void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;
  std::function<void(const Sample &, int, uint64_t)> onEdit;
  std::function<void(double, double)> onAudition;
  int lane = -1; // -1 slice, 0 kick, 1 snare, 2 other
private:
  friend void waveformEditorTests();
  double at(float x) const;
  double snapped(float x, bool bypass);
  float x(double normalized) const;
  int nearest(float x) const;
  void commit();
  Look &look;
  std::shared_ptr<const zv::Wave> peaks;
  Sample sample;
  bool hasSample = false, dragging = false, changed = false;
  bool showTransientLanes = true;
  bool snapToSlices = true;
  int draftLanes = 0;
  uint64_t editSequence = 0, pendingSequence = 0;
  double snapTarget = -1;
  int marker = -1;
  double zoom = 1, pan = 0, playhead = -1, dragPan = 0;
  float dragX = 0;
};
} // namespace core
