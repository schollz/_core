#include "WaveformEditor.h"
namespace core {
WaveformEditor::WaveformEditor(Look &l) : look(l) {
  setMouseCursor(juce::MouseCursor::CrosshairCursor);
  setWantsKeyboardFocus(true);
}
void WaveformEditor::set(const Sample *s,
                         std::shared_ptr<const zv::Wave> cached) {
  if (dragging)
    return;
  bool identity = !s || !hasSample || sample.id != s->id;
  hasSample = s != nullptr;
  if (s)
    sample = *s;
  if (identity) {
    zoom = 1;
    pan = 0;
  }
  peaks = std::move(cached);
  repaint();
}
void WaveformEditor::setPosition(double p) {
  if (std::abs(playhead - p) > .0001) {
    playhead = p;
    repaint();
  }
}
double WaveformEditor::at(float px) const {
  return juce::jlimit(
      0., 1., pan + double(px - 12) / std::max(1, getWidth() - 24) / zoom);
}
float WaveformEditor::x(double n) const {
  return 12 + float((n - pan) * zoom * (getWidth() - 24));
}
int WaveformEditor::nearest(float px) const {
  if (!hasSample)
    return -1;
  int best = -1;
  float distance = 9;
  if (lane < 0) {
    for (size_t n = 0; n < sample.slices.size(); ++n)
      if (std::abs(x(sample.slices[n].start) - px) < distance) {
        distance = std::abs(x(sample.slices[n].start) - px);
        best = int(n);
      }
  } else
    for (size_t n = 0; n < sample.transients[size_t(lane)].size(); ++n)
      if (std::abs(
              x(sample.transients[size_t(lane)][n] / sample.sourceDuration) -
              px) < distance) {
        distance = std::abs(
            x(sample.transients[size_t(lane)][n] / sample.sourceDuration) - px);
        best = int(n);
      }
  return best;
}
void WaveformEditor::paint(juce::Graphics &g) {
  g.fillAll(look.theme.background.brighter(.12f));
  auto area = getLocalBounds().reduced(12);
  g.setColour(look.theme.sidebar);
  g.drawRect(area);
  if (!hasSample) {
    g.setFont(look.font(15));
    g.setColour(juce::Colours::darkgrey);
    g.drawFittedText("Choose a sample to edit its waveform", area,
                     juce::Justification::centred, 2);
    return;
  }
  auto wave = area;
  wave.removeFromBottom(56);
  g.setColour(look.theme.wave);
  if (peaks && !peaks->peaks.empty()) {
    float channelHeight = float(wave.getHeight()) / float(peaks->peaks.size());
    for (size_t c = 0; c < peaks->peaks.size(); ++c) {
      const auto &data = peaks->peaks[c];
      size_t bins = data.size() / 2;
      float mid = float(wave.getY()) + (float(c) + .5f) * channelHeight;
      for (int px = wave.getX(); px < wave.getRight() && bins > 0; ++px) {
        size_t first = std::min(bins - 1, size_t(at(float(px)) * double(bins)));
        size_t last =
            std::min(bins - 1, size_t(at(float(px + 1)) * double(bins)));
        int16_t lo = 32767, hi = -32768;
        for (size_t n = first; n <= last; ++n) {
          lo = std::min(lo, data[n * 2]);
          hi = std::max(hi, data[n * 2 + 1]);
        }
        float scale = channelHeight * .45f / 32768.f;
        g.drawVerticalLine(px, mid - float(hi) * scale,
                           mid - float(lo) * scale + .5f);
      }
    }
  }
  g.setFont(look.font(11));
  for (size_t n = 0; n < sample.slices.size(); ++n) {
    auto px = x(sample.slices[n].start);
    if (px < area.getX() || px > area.getRight())
      continue;
    g.setColour(look.theme.accent);
    g.drawVerticalLine(int(px), float(wave.getY()), float(wave.getBottom()));
    g.drawText(String(int(n + 1)), int(px) + 4, wave.getY() + 3, 30, 16,
               juce::Justification::left);
  }
  const juce::Colour colours[]{juce::Colour(0xffb13f48),
                               juce::Colour(0xff386bc4),
                               juce::Colour(0xff568243)};
  const char *names[]{"KICK", "SNARE", "OTHER"};
  for (int l = 0; l < 3; ++l) {
    float y = float(wave.getBottom() + 10 + l * 17);
    g.setColour(colours[l].withAlpha(l == lane ? 1.f : .6f));
    g.drawText(names[l], area.getX() + 4, int(y) - 7, 60, 15,
               juce::Justification::left);
    for (auto t : sample.transients[size_t(l)]) {
      auto px = x(t / sample.sourceDuration);
      if (px >= area.getX() && px < area.getRight())
        g.fillEllipse(px - 3, y - 3, 6, 6);
    }
  }
  if (playhead >= 0) {
    g.setColour(juce::Colours::black);
    g.drawVerticalLine(int(x(playhead)), float(area.getY()),
                       float(area.getBottom()));
  }
}
void WaveformEditor::mouseDown(const juce::MouseEvent &e) {
  if (!hasSample || sample.protectedEntry)
    return;
  dragX = e.position.x;
  dragPan = pan;
  changed = false;
  marker = nearest(e.position.x);
  dragging = true;
  if (e.mods.isMiddleButtonDown())
    return;
  if (lane >= 0 && marker < 0) {
    sample.transients[size_t(lane)].push_back(at(e.position.x) *
                                              sample.sourceDuration);
    marker = int(sample.transients[size_t(lane)].size() - 1);
    changed = true;
  } else if (lane < 0 && e.mods.isPopupMenu() && marker < 0) {
    auto point = at(e.position.x);
    for (size_t n = 0; n < sample.slices.size(); ++n)
      if (point > sample.slices[n].start && point < sample.slices[n].stop) {
        auto old = sample.slices[n];
        sample.slices[n].stop = point;
        sample.slices.insert(sample.slices.begin() + std::ptrdiff_t(n + 1),
                             {point, old.stop, old.type});
        marker = int(n + 1);
        changed = true;
        break;
      }
  } else if (lane < 0 && marker < 0 && onAudition) {
    auto p = at(e.position.x);
    for (auto m : sample.slices)
      if (p >= m.start && p < m.stop) {
        onAudition(m.start, m.stop);
        break;
      }
  }
  repaint();
}
void WaveformEditor::mouseDrag(const juce::MouseEvent &e) {
  if (!dragging)
    return;
  if (e.mods.isMiddleButtonDown()) {
    pan = juce::jlimit(0., 1. - 1. / zoom,
                       dragPan - double(e.position.x - dragX) /
                                     std::max(1, getWidth() - 24) / zoom);
    repaint();
    return;
  }
  if (marker < 0)
    return;
  double p = at(e.position.x);
  if (lane >= 0)
    sample.transients[size_t(lane)][size_t(marker)] = p * sample.sourceDuration;
  else {
    auto &m = sample.slices[size_t(marker)];
    double lower =
        marker > 0 ? sample.slices[size_t(marker - 1)].start + 1e-5 : 0.;
    p = juce::jlimit(lower, m.stop - 1e-5, p);
    m.start = p;
    if (marker > 0)
      sample.slices[size_t(marker - 1)].stop = p;
  }
  changed = true;
  repaint();
}
void WaveformEditor::commit() {
  if (changed && onEdit)
    onEdit(sample);
  changed = false;
}
void WaveformEditor::mouseUp(const juce::MouseEvent &) {
  dragging = false;
  commit();
}
void WaveformEditor::mouseDoubleClick(const juce::MouseEvent &e) {
  if (!hasSample)
    return;
  int n = nearest(e.position.x);
  if (n < 0)
    return;
  if (lane >= 0)
    sample.transients[size_t(lane)].erase(
        sample.transients[size_t(lane)].begin() + n);
  else if (n > 0) {
    sample.slices[size_t(n - 1)].stop = sample.slices[size_t(n)].stop;
    sample.slices.erase(sample.slices.begin() + n);
  } else
    return;
  changed = true;
  dragging = false;
  commit();
  repaint();
}
void WaveformEditor::mouseWheelMove(const juce::MouseEvent &e,
                                    const juce::MouseWheelDetails &wheel) {
  double anchor = at(e.position.x);
  if (e.mods.isShiftDown())
    pan = juce::jlimit(0., 1 - 1 / zoom, pan - wheel.deltaY * .3 / zoom);
  else {
    zoom = juce::jlimit(1., 128., zoom * std::exp(wheel.deltaY * 2));
    pan = juce::jlimit(0., 1 - 1 / zoom,
                       anchor - double(e.position.x - 12) /
                                    std::max(1, getWidth() - 24) / zoom);
  }
  repaint();
}
} // namespace core
