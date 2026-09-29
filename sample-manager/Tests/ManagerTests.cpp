#include "Manager.h"
#include "Visualizer/Session.h"
#include <iostream>
namespace core {
namespace {
ManagerState settle(Manager &m) {
  for (int n = 0; n < 2000; ++n) {
    auto s = m.snapshot();
    if (!s.busy)
      return s;
    juce::Thread::sleep(10);
  }
  throw std::runtime_error("Manager job did not settle");
}
void successful(const ManagerState &s) {
  require(s.error.isEmpty(), "Manager: " + s.error);
  require(!s.busy, "Manager still busy");
}
} // namespace
void managerTests() {
  auto root = File::getSpecialLocation(File::tempDirectory)
                  .getChildFile("core-manager-job-" + uuid());
  require(root.createDirectory().wasOk(), "Manager test folder");
  struct Clean {
    File root;
    ~Clean() { root.deleteRecursively(); }
  } clean{root};
  auto audioFile = root.getChildFile("input.wav");
  {
    auto out = audioFile.createOutputStream();
    card::writeHeader(*out, 4410, 44100, 1);
    for (int n = 0; n < 4410; ++n)
      out->writeShort(short(std::sin(n * .1) * 10000));
  }
  {
    Manager m;
    m.open(root);
    successful(settle(m));
    m.import({audioFile.getFullPathName()}, 0);
    auto state = settle(m);
    successful(state);
    require(state.project.samples.size() == 1, "Background import");
    auto id = state.project.samples.front().id;
    Device device;
    Preview preview;
    zv::Session visual(m, device, preview);
    visual.selectedId = id;
    visual.previewSource = true;
    visual.enable(true);
    visual.tick(1000);
    require(visual.wave && visual.device.display &&
                visual.device.display->state.stopped &&
                !visual.device.display->state.effects && !visual.device.press &&
                !visual.position(1000),
            "Preview source uses committed sample without fabricated hardware "
            "activity");
    visual.enable(false);
    visual.tick(1100);
    require(!visual.wave && !visual.device.display,
            "Disabled visualizer does no session work");
    visual.enable(true);
    visual.tick(1200);
    require(visual.wave != nullptr,
            "Visualizer re-enable restores completed data");
    visual.previewSource = false;
    visual.enable(true);
    visual.tick(1300);
    require(visual.wave && !visual.device.display,
            "Device source clears synthetic preview state");
    visual.enable(false);

    require(root.getChildFile("bank1/0.0.wav").existsAsFile() &&
                root.getChildFile("bank1/0.1.wav").existsAsFile(),
            "Continuous primary and companion output");
    auto before = hashFile(root.getChildFile("bank1/0.0.wav"));
    auto modified =
        root.getChildFile("bank1/0.0.wav").getLastModificationTime();
    m.edit(id, "Test metadata", [](Sample &s) {
      s.sourceBpm = 172;
      s.slices = {{0, .5, 0}, {.5, 1, 0}};
      s.playMode = 2;
      s.spliceTrigger = 48;
    });
    state = settle(m);
    successful(state);
    require(hashFile(root.getChildFile("bank1/0.0.wav")) == before &&
                root.getChildFile("bank1/0.0.wav").getLastModificationTime() ==
                    modified,
            "Metadata edit does not rewrite audio");
    visual.previewSource = true;
    visual.enable(true);
    visual.tick(1400);
    require(
        visual.wave && visual.wave->bpm == 172 &&
            visual.wave->slices.size() == 2,
        "Completed metadata revision refreshes the shared visualizer waveform");
    visual.enable(false);
    auto unchangedRevision = state.project.revision;
    m.edit(id, "Unchanged metadata", [](Sample &s) { s.sourceBpm = 172; });
    state = settle(m);
    successful(state);
    require(state.project.revision == unchangedRevision,
            "Unchanged field submission creates no revision or undo step");
    m.edit(id, "One shot exception", [](Sample &s) {
      s.oneShot = true;
      s.tempoMatch = false;
    });
    successful(settle(m));
    require(!root.getChildFile("bank1/0.1.wav").exists() &&
                root.getChildFile("bank1/0.0.wav").getLastModificationTime() ==
                    modified,
            "Companion exception preserves primary WAV");
    m.undo();
    successful(settle(m));
    require(root.getChildFile("bank1/0.1.wav").existsAsFile(),
            "Restore companion from cache");
    m.move({id}, 1);
    state = settle(m);
    successful(state);
    require(!root.getChildFile("bank1/0.0.wav").existsAsFile() &&
                hashFile(root.getChildFile("bank2/0.0.wav")) == before,
            "Slot move preserves rendered bytes");
    m.undo();
    state = settle(m);
    successful(state);
    require(root.getChildFile("bank1/0.0.wav").existsAsFile() &&
                !root.getChildFile("bank2/0.0.wav").existsAsFile(),
            "Undo restores slot assignment");
    m.remove({id});
    successful(settle(m));
    require(!root.getChildFile("bank1/0.0.wav").existsAsFile(),
            "Remove hardware output");
    m.undo();
    state = settle(m);
    successful(state);
    require(hashFile(root.getChildFile("bank1/0.0.wav")) == before,
            "Undo removal from immutable source");
    m.settings({{"clock_stop_sync", "on"}});
    successful(settle(m));
    m.settings({{"clock_stop_sync", "off"}});
    successful(settle(m));
    require(
        !root.getChildFile("settings/clock_stop_sync-on").existsAsFile() &&
            root.getChildFile("settings/clock_stop_sync-off").existsAsFile(),
        "Exclusive settings markers committed together");
    m.edit(id, "Superseded render", [](Sample &s) { s.renderBpm = 60; });
    m.edit(id, "Final render", [](Sample &s) { s.renderBpm = 150; });
    state = settle(m);
    successful(state);
    require(state.project.find(id)->renderBpm == 150 &&
                state.project.revision == state.project.completedRevision,
            "Latest revision reaches Ready");
    m.import({audioFile.getFullPathName()}, 0);
    auto withTwo = settle(m);
    successful(withTwo);
    auto secondId = withTwo.project.samples.back().id;
    auto beforeEdgeMove = withTwo.project.revision;
    m.move({id, secondId}, 0, -1);
    auto edge = settle(m);
    successful(edge);
    require(
        edge.project.find(id)->slot == 0 &&
            edge.project.find(secondId)->slot == 1 &&
            edge.project.revision == beforeEdgeMove,
        "Moving a selected group past bank start preserves order and revision");
    m.remove({secondId});
    successful(settle(m));
    auto external = root.getChildFile("bank1/0.0.wav");
    external.appendText("external change");
    auto altered = hashFile(external);
    m.edit(id, "External conflict", [](Sample &s) { s.spliceTrigger = 72; });
    state = settle(m);
    require(state.error.contains("External change") &&
                hashFile(external) == altered,
            "Do not overwrite externally modified audio");
    m.reconcile();
    state = settle(m);
    successful(state);
    require(hashFile(external) == altered && state.project.find(id),
            "Reconcile preserves external bytes and stable sample ID");
    m.undo();
    state = settle(m);
    successful(state);
    require(state.project.find(id)->spliceTrigger == 72,
            "Undo reload retains pending edits");
    auto revision = state.project.find(id)->revision;
    m.edit(id, "Invalidate late analysis",
           [](Sample &s) { s.spliceTrigger = 24; });
    m.applyAnalysis(state.project.id, id, revision, {{{.01}, {.02}, {.03}}});
    state = settle(m);
    require(state.error.contains("after the sample changed") &&
                state.project.find(id)->transients[0].empty(),
            "Late online response cannot replace markers");
    m.retry();
    successful(settle(m));
    // Destructor drains queued edits into the portable pending manifest.
    m.edit(id, "Close with pending edit", [](Sample &s) { s.sourceBpm = 166; });
  }
  {
    Manager reopened;
    reopened.open(root);
    auto state = settle(reopened);
    successful(state);
    require(state.project.samples.front().sourceBpm == 166 &&
                state.project.revision == state.project.completedRevision,
            "Reopen resumes a pending shutdown edit");
    auto id = state.project.samples.front().id;
    auto completed = state.project.completedRevision;
    reopened.edit(id, "Disconnect during debounce",
                  [](Sample &s) { s.spliceTrigger = 96; });
    for (int n = 0;
         n < 1000 && reopened.snapshot().project.revision == completed; ++n)
      juce::Thread::sleep(1);
    auto parked = root.getSiblingFile(root.getFileName() + "-unplugged");
    require(root.moveFileTo(parked), "Simulated volume disconnect");
    auto disconnected = settle(reopened);
    require(disconnected.error.isNotEmpty() &&
                !disconnected.library.samples.empty(),
            "Missing folder retains completed visualization");
    require(!root.exists(), "Disconnected paths must not be recreated");
    require(parked.moveFileTo(root), "Simulated reconnect");
    reopened.retry();
    successful(settle(reopened));
  }
  std::cout << "PASS background import, metadata-only save, move, undo, "
               "remove, settings, stale jobs, conflicts\n";
}
} // namespace core
