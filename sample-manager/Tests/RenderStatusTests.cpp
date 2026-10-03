#include "AppView.h"
#include <iostream>

namespace core {
namespace {
struct TempRenderProject {
  File root = File::getSpecialLocation(File::tempDirectory)
                  .getChildFile("core-render-status-" + uuid());
  TempRenderProject() { require(root.createDirectory().wasOk(), "Create render status fixture"); }
  ~TempRenderProject() { root.deleteRecursively(); }
};
ManagerState waitFor(Manager &manager, const std::function<bool(const ManagerState &)> &ready) {
  for (int n = 0; n < 6000; ++n) {
    auto state = manager.snapshot();
    if (ready(state))
      return state;
    juce::Thread::sleep(5);
  }
  throw std::runtime_error("Render status did not settle");
}
ManagerState settle(Manager &manager) {
  auto state = waitFor(manager, [](const ManagerState &s) { return !s.busy; });
  require(state.error.isEmpty(), "Render failed: " + state.error);
  return state;
}
void checkCompleted(const ManagerState &state, const String &id, double expected) {
  const auto status = state.renderStatus(id);
  const auto *sample = state.completedProject.find(id);
  require(sample && !status.pending && !status.failed && status.original == 8.,
          "Completed render is ready with its original duration");
  const auto header = card::inspect(child(state.root, sample->rendered));
  require(status.completed.frames == header.frames && status.completed.rate == header.rate &&
              std::abs(status.completed.duration() - expected) <= 1. / header.rate,
          "Rendered duration comes from the actual unpadded preview frames and rate");
  require(status.text() == "Original: 8.00 s" + String::fromUTF8(" · Rendered: ") +
                               String(expected, 2) + String::fromUTF8(" s · 44.1 kHz output"),
          "Both durations and the output rate are explicit, including equal durations");
  const auto padded = card::inspect(child(state.root, card::path(sample->bank, sample->slot)));
  require(padded.frames == header.frames + header.rate &&
              status.tooltip().contains("half-second") && status.tooltip().contains("each end"),
          "Hardware padding is excluded from the display and explained in its tooltip");
}
} // namespace

void renderStatusTests() {
  TempRenderProject fixture;
  juce::ScopedValueSetter<File> privateState(stateRootOverride, fixture.root.getChildFile("state"));
  const auto root = fixture.root.getChildFile("project");
  require(root.createDirectory().wasOk(), "Create project folder");
  const auto input = fixture.root.getChildFile("eight-seconds-bpm120.wav");
  {
    auto out = input.createOutputStream();
    require(out != nullptr, "Create eight-second audio");
    card::writeHeader(*out, 44100 * 8, 44100, 1);
    for (int n = 0; n < 44100 * 8; ++n)
      out->writeShort(short(std::sin(n * .031) * 12000));
  }
  String id, otherId;
  std::vector<std::pair<String, ManagerState>> screenshots;
  {
    Manager manager;
    manager.open(root);
    settle(manager);
    manager.import({input.getFullPathName(), input.getFullPathName()}, 0);
    auto ready = settle(manager);
    id = ready.project.samples[0].id;
    otherId = ready.project.samples[1].id;
    checkCompleted(ready, id, 8.);
    require(ready.status == "Ready" && !ready.renderStatus(id).pending &&
                !child(root, "bank1/0.1.wav").exists(),
            "Primary completion is final; no companion work is scheduled");
    for (bool preserve : {false, true}) {
      for (double bpm : {150., 60., 0.}) {
        const double expected = bpm == 150. ? 6.4 : bpm == 60. ? 16. : 8.;
        const auto lastDuration = ready.renderStatus(id).completed.duration();
        manager.edit(id, "Change render tempo", [=](Sample &s) {
          s.sourceBpm = 120;
          s.renderBpm = bpm;
          s.preservePitch = preserve;
        });
        auto pending = waitFor(manager, [&](const ManagerState &s) {
          return s.project.find(id)->renderBpm == bpm && s.renderStatus(id).pending;
        });
        auto status = pending.renderStatus(id);
        require(!status.failed && status.target == expected &&
                    status.completed.duration() == lastDuration &&
                    status.text() == "Original: 8.00 s" + String::fromUTF8(" · Target: ") +
                                         String(expected, 2) + String::fromUTF8(" s (rendering…)"),
                "Accepted BPM edit shows its target and keeps the last completed preview");
        require(status.previewTooltip().contains("last completed render") &&
                    status.previewTooltip().contains(String(lastDuration, 2)) &&
                    !pending.renderStatus(otherId).pending,
                "Preview tooltip explains the previous render; another sample stays ready");
        if (!preserve && bpm == 150.)
          screenshots.emplace_back("rendering", pending);
        ready = settle(manager);
        checkCompleted(ready, id, expected);
        if (!preserve && bpm == 150.)
          screenshots.emplace_back("completed", ready);
      }
    }
    screenshots.emplace_back("equal", ready);
    manager.undo();
    auto undoing = waitFor(manager, [&](const ManagerState &s) {
      return s.project.find(id)->renderBpm == 60. && s.renderStatus(id).pending;
    });
    require(undoing.renderStatus(id).completed.duration() == 8.,
            "Undo previews the last completed output while the restored tempo renders");
    checkCompleted(settle(manager), id, 16.);
    manager.redo();
    auto redoing = waitFor(manager, [&](const ManagerState &s) {
      return s.project.find(id)->renderBpm == 0. && s.renderStatus(id).pending;
    });
    require(redoing.renderStatus(id).completed.duration() == 16.,
            "Redo retains the actual previous completion rather than a history snapshot path");
    checkCompleted(settle(manager), id, 8.);

    manager.edit(id, "Start slow render", [](Sample &s) { s.renderBpm = 70.; });
    waitFor(manager, [&](const ManagerState &s) {
      return s.renderStatus(id).pending && s.status.startsWith("Processing audio");
    });
    for (double bpm : {150., 90., 0.})
      manager.edit(id, "Replace queued tempo", [=](Sample &s) { s.renderBpm = bpm; });
    ready = settle(manager);
    checkCompleted(ready, id, 8.);
    require(ready.project.find(id)->renderBpm == 0., "Quick edits finish at the newest tempo");

    // Break the immutable source, leaving the completed preview usable.
    const auto source = child(root, ready.project.find(id)->source);
    const auto parked = source.getSiblingFile("parked-source.wav");
    require(source.moveFileTo(parked), "Temporarily remove the render source");
    manager.edit(id, "Fail primary update", [](Sample &s) { s.renderBpm = 150.; });
    auto failed = waitFor(manager, [](const ManagerState &s) { return !s.busy; });
    auto status = failed.renderStatus(id);
    require(failed.error.isNotEmpty() && status.failed && status.pending &&
                status.completed.duration() == 8. && status.text().contains("Last rendered: 8.00 s") &&
                status.text().contains("Update failed") &&
                !failed.renderStatus(otherId).failed && !failed.renderStatus(otherId).pending,
            "Failed primary updates retain and label the previous duration for only that sample");
    require(status.previewTooltip().contains("failed") &&
                status.previewTooltip().contains("last completed render"),
            "Failure tooltip explains the available preview");
    screenshots.emplace_back("failed", failed);
    require(parked.moveFileTo(source), "Restore the render source");
    manager.retry();
    checkCompleted(settle(manager), id, 6.4);
    manager.edit(id, "Toggle pitch at the same tempo", [](Sample &s) { s.preservePitch = false; });
    auto pitching = waitFor(manager, [&](const ManagerState &s) { return s.renderStatus(id).pending; });
    require(pitching.renderStatus(id).target == 6.4 &&
                pitching.renderStatus(id).completed.duration() == 6.4,
            "Pitch processing changes show rendering even when the duration stays the same");
    checkCompleted(settle(manager), id, 6.4);
  }
  {
    Manager reopened;
    reopened.open(root);
    auto ready = settle(reopened);
    checkCompleted(ready, id, 6.4);
    // A different actual length must win over the mathematical target. UI reads
    // remain valid even after the project folder disappears from this snapshot.
    ready.completedAudio[id].frames += 4410;
    ready.root = fixture.root.getChildFile("unavailable");
    require(ready.renderStatus(id).text().contains("Rendered: 6.50 s"),
            "Cached actual frames, not the BPM estimate or UI file I/O, determine rendered duration");
    ready.completedAudio.erase(id);
    require(ready.renderStatus(id).text().contains("Rendered: unavailable"),
            "Missing cached metadata never substitutes an estimate for completed duration");
  }

  AppView app;
  app.selectedId = id;
  app.advancedButton.setToggleState(true, juce::dontSendNotification);
  app.advancedControls.setVisible(true);
  const auto output = juce::SystemStats::getEnvironmentVariable("CORE_RENDER_PREVIEW_DIR", "");
  for (int presentation = 0; presentation < 3; ++presentation) {
    app.presentation = presentation;
    app.presentationBox.setSelectedId(presentation + 1, juce::dontSendNotification);
    app.look.presentation(presentation);
    app.sendLookAndFeelChange();
    app.updatePresentation();
    for (bool docked : {false, true}) {
      if (bool(app.visualizer) != docked)
        app.toggleVisualizer();
      for (const auto &[name, state] : screenshots) {
        app.state = state;
        app.updateEditor();
        app.setSize(1120, 840);
        app.resized();
        if (docked) {
          // Use the actual divider path to reach the editor's 510-pixel minimum.
          app.layout.setItemPosition(1, 460);
          app.resized();
          require(app.editor.getWidth() == 510, "Exercise the minimum docked editor width");
        }
        require(app.sourceLabel.getText() == state.renderStatus(id).text() &&
                    app.play.getTooltip() == state.renderStatus(id).previewTooltip(),
                "All presentations use the selected sample's duration and preview status");
        require(app.sourceLabel.getBottom() < app.waveform.getY() &&
                    app.sourceLabel.getRight() <= app.editor.getWidth() &&
                    app.controlsViewport.getBottom() <= app.editor.getHeight(),
                "Duration wrapping leaves room for waveform and scrollable controls at minimum size");
        if (docked && name == "failed")
          require(app.sourceLabel.getHeight() > 22,
                  "Narrow visualizer dock wraps the duration rather than squeezing it");
        if (File::isAbsolutePath(output)) {
          File directory(output);
          require(directory.createDirectory().wasOk(), "Create layout preview directory");
          auto shot = app.createComponentSnapshot(app.getLocalBounds(), true, 1.f);
          auto out = directory.getChildFile(String(presentation) + (docked ? "-dock-" : "-wide-") +
                                           name + ".png").createOutputStream();
          require(out && out->setPosition(0) && out->truncate().wasOk(), "Write layout preview");
          require(juce::PNGImageFormat().writeImageToStream(shot, *out), "Render layout preview");
        }
      }
    }
  }
  std::cout << "PASS render durations: 120 -> 150/60/unset, preserve pitch off/on, undo/redo, "
               "rapid edits, failure/retry, reopen, per-sample status, and all three layouts\n";
}
} // namespace core
