#include "AppView.h"
#include <BinaryData.h>
namespace core {
AppView::AppView() {
  setLookAndFeel(&look);
  setWantsKeyboardFocus(true);
  setSize(1320, 840);
  manager.addChangeListener(this);
  if (!preferencesFile().existsAsFile()) {
    auto old = File::getSpecialLocation(File::userApplicationDataDirectory)
                   .getChildFile(
                       "com.infinitedigits.zeptocorevisualizer/settings.json");
    try {
      if (migrateVisualizerPreferences(old, preferencesFile())) {
        auto migrated = parseJson(preferencesFile());
        device.inputId = migrated["input"].toString();
        device.outputId = migrated["output"].toString();
        device.reduceMotion = bool(migrated["reduceMotion"]);
        device.refresh();
      }
    } catch (...) {
    }
  }
  if (preferencesFile().existsAsFile()) {
    try {
      preferences = parseJson(preferencesFile());
    } catch (...) {
    }
  }
  presentation = juce::jlimit(0, 2, int(preferences["presentation"]));
  look.presentation(presentation);
  if (auto *a = preferences["recents"].getArray())
    for (auto &r : *a)
      if (File::isAbsolutePath(r.toString()))
        recents.add(r.toString());
  ectoLogo = juce::ImageCache::getFromMemory(BinaryData::ectocore_png,
                                             BinaryData::ectocore_pngSize);
  for (auto *c : std::initializer_list<juce::Component *>{
           &open, &recent, &reveal, &duplicate, &importButton, &settingsButton,
           &deviceButton, &visualizerButton, &more, &presentationBox,
           &folderLabel, &statusLabel, &banks, &samples, &divider, &editor,
           &empty, &undoButton, &redoButton})
    addAndMakeVisible(c);
  for (auto *c : std::initializer_list<juce::Component *>{
           &waveform,       &play,           &evenButton,    &autoButton,
           &onlineButton,   &removeButton,   &mergeButton,   &up,
           &down,           &sourceBpm,      &renderBpm,     &sliceCount,
           &spacing,        &spliceTrigger,  &name,          &preserve,
           &tempo,          &oneShot,        &variable,      &sourceLabel,
           &sourceBpmLabel, &renderBpmLabel, &channelLabel,  &playModeLabel,
           &triggerLabel,   &hint,           &advancedLabel, &channel,
           &playMode,       &markerMode,     &detector,      &moveBank})
    editor.addAndMakeVisible(c);
  presentationBox.addItemList({"Ezeptocore", "Zeptocore", "Ectocore"}, 1);
  presentationBox.setSelectedId(presentation + 1, juce::dontSendNotification);
  presentationBox.onChange = [this] {
    presentation = presentationBox.getSelectedId() - 1;
    look.presentation(presentation);
    settingsWindow.reset();
    savePreferences();
    sendLookAndFeelChange();
    repaint();
  };
  deviceButton.onClick = [this] {
    deviceWindow = std::make_unique<AuxiliaryWindow>(
        "Local device tools", new DeviceView(device, look));
  };
  visualizerButton.onClick = [this] { toggleVisualizer(); };
  open.onClick = [this] { chooseFolder(); };
  empty.onClick = open.onClick;
  recent.onClick = [this] { recentMenu(); };
  importButton.onClick = [this] { chooseImport(); };
  reveal.onClick = [this] {
    if (state.root.isDirectory())
      state.root.revealToUser();
  };
  duplicate.onClick = [this] { chooseDuplicate(); };
  settingsButton.onClick = [this] { showSettings(); };
  more.onClick = [this] { moreMenu(); };
  undoButton.onClick = [this] { manager.undo(); };
  redoButton.onClick = [this] { manager.redo(); };
  removeButton.onClick = [this] { manager.remove(selectedIds()); };
  mergeButton.onClick = [this] { manager.merge(selectedIds(), bank); };
  up.onClick = [this] { manager.move(selectedIds(), bank, -1); };
  down.onClick = [this] { manager.move(selectedIds(), bank, 1); };
  play.onClick = [this] {
    if (preview.playing())
      preview.stop();
    else
      audition();
  };
  evenButton.onClick = [this] {
    manager.even(selectedId, sliceCount.getText().getIntValue());
  };
  autoButton.onClick = [this] {
    manager.detect(selectedId, detector.getText(),
                   spacing.getText().getDoubleValue());
  };
  onlineButton.onClick = [this] {
    if (online.busy()) {
      online.cancel();
      return;
    }
    auto *s = selected();
    if (!s)
      return;
    auto safe = juce::Component::SafePointer<AppView>(this);
    auto projectId = state.project.id;
    auto id = s->id;
    auto revision = s->revision;
    safely([&] {
      online.start(child(state.root, s->source), s->sourceDuration,
                   [safe, projectId, id, revision](OnlineAnalysis::Lanes lanes,
                                                   String error) {
                     if (!safe)
                       return;
                     if (error.isNotEmpty())
                       safe->localError = error;
                     else
                       safe->manager.applyAnalysis(projectId, id, revision,
                                                   std::move(lanes));
                   });
    });
  };
  onlineButton.setTooltip(
      "Upload this sample as mono 44.1 kHz audio to tool.getectocore.com for "
      "kick/snare/other markers. Click again to cancel.");
  sourceBpmLabel.setText("Source BPM", juce::dontSendNotification);
  renderBpmLabel.setText("Render BPM (optional)", juce::dontSendNotification);
  channelLabel.setText("Channels", juce::dontSendNotification);
  playModeLabel.setText("Playback mode", juce::dontSendNotification);
  triggerLabel.setText("Splice ticks", juce::dontSendNotification);
  advancedLabel.setText("Advanced onset: method / minimum spacing (ms)",
                        juce::dontSendNotification);
  hint.setText(juce::String::fromUTF8(
                   "Click: audition • Right-click: add slice • Drag: move • "
                   "Double-click: remove\nWheel: zoom • Shift-wheel / "
                   "middle-drag: pan • Select a transient lane to edit it"),
               juce::dontSendNotification);
  hint.setFont(look.font(11));
  channel.addItemList({"Mono", "Stereo"}, 1);
  playMode.addItemList(
      {"Slice stop", "Slice loop", "Sample stop", "Sample loop", "Granular"},
      1);
  markerMode.addItemList(
      {"Slices", "Kick transients", "Snare transients", "Other transients"}, 1);
  markerMode.setSelectedId(1, juce::dontSendNotification);
  markerMode.onChange = [this] {
    waveform.lane = markerMode.getSelectedId() - 2;
    waveform.repaint();
  };
  detector.addItemList({"hfc", "energy", "complex", "phase", "wphase",
                        "specdiff", "kl", "mkl", "specflux"},
                       1);
  detector.setSelectedId(1, juce::dontSendNotification);
  sliceCount.setText("16");
  spacing.setText("80");
  preserve.setToggleState(true, juce::dontSendNotification);
  for (int b = 0; b < 16; ++b)
    moveBank.addItem("Move to bank " + String(b + 1), b + 1);
  moveBank.setTextWhenNothingSelected(juce::String::fromUTF8("Move to bank…"));
  moveBank.onChange = [this] {
    if (!updating && moveBank.getSelectedId() > 0) {
      manager.move(selectedIds(), moveBank.getSelectedId() - 1);
      moveBank.setSelectedId(0, juce::dontSendNotification);
    }
  };
  for (auto *edit : {&sourceBpm, &renderBpm, &spliceTrigger}) {
    edit->setInputRestrictions(8, "0123456789.");
    edit->onReturnKey = [this] {
      editControls();
      grabKeyboardFocus();
    };
    edit->onFocusLost = [this] { editControls(); };
  }
  name.onReturnKey = [this] {
    editControls();
    grabKeyboardFocus();
  };
  name.onFocusLost = [this] { editControls(); };
  channel.onChange = [this] { editControls(); };
  playMode.onChange = [this] { editControls(); };
  for (auto *button : {&preserve, &tempo, &oneShot, &variable})
    button->onClick = [this] { editControls(); };
  waveform.onEdit = [this](const Sample &updated) {
    manager.edit(
        updated.id, "Edit markers",
        [slices = updated.slices, lanes = updated.transients](Sample &s) {
          s.slices = slices;
          s.transients = lanes;
        });
  };
  waveform.onAudition = [this](double a, double b) { audition(a, b); };
  bankRows.count = [] { return 16; };
  bankRows.paint = [this](int row, juce::Graphics &g, int w, int h,
                          bool selected) {
    g.fillAll(selected ? look.theme.accent : look.theme.sidebar);
    g.setColour(selected ? juce::Colours::white : juce::Colour(0xff1a1a1a));
    g.setFont(look.font(13));
    int count = 0;
    for (const auto &s : state.project.samples)
      if (s.bank == row)
        ++count;
    g.drawText("Bank " + String(row + 1), 10, 0, w - 20, h - 12,
               juce::Justification::centredLeft);
    g.setFont(look.font(10));
    g.drawText(String(count) + " / 16", 10, h - 17, w - 20, 16,
               juce::Justification::left);
  };
  bankRows.select = [this](int row) {
    if (row >= 0) {
      bank = row;
      samples.deselectAllRows();
      selectedId.clear();
      samples.updateContent();
      updateEditor();
    }
  };
  sampleRows.count = [] { return 16; };
  sampleRows.paint = [this](int row, juce::Graphics &g, int w, int h,
                            bool selected) {
    const Sample *sample = nullptr;
    for (const auto &s : state.project.samples)
      if (s.bank == bank && s.slot == row)
        sample = &s;
    g.fillAll(selected ? look.theme.sidebar : look.theme.background);
    g.setColour(juce::Colour(0xff1a1a1a));
    g.setFont(look.font(12));
    g.drawText(String(row + 1).paddedLeft('0', 2), 10, 0, 25, h,
               juce::Justification::left);
    g.drawText(sample ? sample->name : "Empty slot", 42, 0, w - 50, h - 13,
               juce::Justification::centredLeft);
    g.setColour(sample && sample->protectedEntry ? juce::Colours::darkred
                                                 : juce::Colours::grey);
    g.setFont(look.font(9));
    if (sample)
      g.drawText(sample->protectedEntry
                     ? "Protected: " + sample->problem
                     : String(sample->sourceDuration, 2) +
                           juce::String::fromUTF8(" s  ·  ") +
                           String(sample->slices.size()) + " slices",
                 42, h - 20, w - 50, 18, juce::Justification::left);
  };
  sampleRows.select = [this](int) { selection(); };
  banks.setRowHeight(39);
  samples.setRowHeight(43);
  samples.setMultipleSelectionEnabled(true);
  banks.selectRow(0);
  layout.setItemLayout(0, 190, 460, 270);
  layout.setItemLayout(1, 6, 6, 6);
  layout.setItemLayout(2, 510, -1, -1);
  state = manager.snapshot();
  updateEditor();
  startTimerHz(30);
  auto last = preferences["lastProject"].toString();
  if (File::isAbsolutePath(last) && File(last).isDirectory())
    manager.open(File(last));
}
AppView::~AppView() {
  stopTimer();
  manager.removeChangeListener(this);
  visualizerWindow.reset();
  visualizer.reset();
  deviceWindow.reset();
  settingsWindow.reset();
  savePreferences();
  setLookAndFeel(nullptr);
}
void AppView::safely(const std::function<void()> &fn) {
  try {
    fn();
    localError.clear();
  } catch (const std::exception &e) {
    localError = e.what();
  }
  repaint();
}
const Sample *AppView::selected() const {
  return state.project.find(selectedId);
}
std::vector<String> AppView::selectedIds() const {
  std::vector<String> ids;
  auto rows = samples.getSelectedRows();
  for (const auto &s : state.project.samples)
    if (s.bank == bank && rows.contains(s.slot))
      ids.push_back(s.id);
  return ids;
}
void AppView::selection() {
  selectedId.clear();
  int row = samples.getLastRowSelected();
  for (const auto &s : state.project.samples)
    if (s.bank == bank && s.slot == row)
      selectedId = s.id;
  visualSession.selectedId = selectedId;
  updateEditor();
}
void AppView::changeListenerCallback(juce::ChangeBroadcaster *) {
  auto before = state.root;
  state = manager.snapshot();
  if (state.root != File() && state.root != before) {
    recents.removeString(state.root.getFullPathName());
    recents.insert(0, state.root.getFullPathName());
    while (recents.size() > 12)
      recents.remove(recents.size() - 1);
    savePreferences();
  }
  banks.updateContent();
  samples.updateContent();
  banks.repaint();
  samples.repaint();
  updateEditor();
  resized();
  repaint();
}
void AppView::updateEditor() {
  updating = true;
  const auto *s = selected();
  editor.setVisible(s != nullptr);
  empty.setVisible(state.root == File());
  importButton.setEnabled(state.available);
  duplicate.setEnabled(state.available && !state.busy);
  settingsButton.setEnabled(state.available);
  if (s) {
    bool editingText = name.hasKeyboardFocus(true) ||
                       sourceBpm.hasKeyboardFocus(true) ||
                       renderBpm.hasKeyboardFocus(true) ||
                       spliceTrigger.hasKeyboardFocus(true);
    bool refresh = controlsId != s->id || !editingText;
    controlsId = s->id;
    if (refresh) {
      name.setText(s->name, false);
      sourceBpm.setText(String(s->sourceBpm, 2), false);
      renderBpm.setText(s->renderBpm > 0 ? String(s->renderBpm, 2) : String(),
                        false);
      spliceTrigger.setText(String(s->spliceTrigger), false);
      channel.setSelectedId(s->channels, juce::dontSendNotification);
      playMode.setSelectedId(s->playMode + 1, juce::dontSendNotification);
      preserve.setToggleState(s->preservePitch, juce::dontSendNotification);
      tempo.setToggleState(s->tempoMatch, juce::dontSendNotification);
      oneShot.setToggleState(s->oneShot, juce::dontSendNotification);
      variable.setToggleState(s->spliceVariable, juce::dontSendNotification);
    }
    sourceLabel.setText(s->protectedEntry
                            ? s->problem
                            : s->origin + juce::String::fromUTF8(" · ") +
                                  String(s->sourceDuration, 2) +
                                  juce::String::fromUTF8(" s · ") +
                                  String(s->rate) + " Hz output",
                        juce::dontSendNotification);
    File audio;
    if (s->rendered.isNotEmpty() && state.root != File())
      safely([&] { audio = child(state.root, s->rendered); });
    std::shared_ptr<const zv::Wave> cached;
    for (const auto &entry : state.library.samples)
      if (auto *completed = state.completedProject.find(s->id);
          completed && entry.bank == completed->bank &&
          entry.sample == completed->slot)
        cached = entry.wave;
    waveform.set(s, cached);
    editor.setEnabled(!s->protectedEntry && state.available);
  } else
    waveform.set(nullptr, {});
  folderLabel.setText(
      state.root == File()
          ? "A portable folder for your samples and hardware files"
          : state.root.getFullPathName(),
      juce::dontSendNotification);
  updating = false;
}
void AppView::editControls() {
  if (updating)
    return;
  const auto *current = state.project.find(controlsId);
  if (!current || current->protectedEntry)
    return;
  Sample next = *current;
  next.name = name.getText();
  next.sourceBpm = sourceBpm.getText().getDoubleValue();
  next.renderBpm =
      renderBpm.getText().isEmpty() ? 0 : renderBpm.getText().getDoubleValue();
  next.spliceTrigger = spliceTrigger.getText().getIntValue();
  next.channels = channel.getSelectedId();
  next.playMode = playMode.getSelectedId() - 1;
  next.preservePitch = preserve.getToggleState();
  next.tempoMatch = tempo.getToggleState();
  next.oneShot = oneShot.getToggleState();
  next.spliceVariable = variable.getToggleState();
  if (juce::JSON::toString(next.json()) ==
      juce::JSON::toString(current->json()))
    return;
  manager.edit(current->id, "Sample settings", [next](Sample &s) {
    s.name = next.name;
    s.sourceBpm = next.sourceBpm;
    s.renderBpm = next.renderBpm;
    s.spliceTrigger = next.spliceTrigger;
    s.channels = next.channels;
    s.playMode = next.playMode;
    s.preservePitch = next.preservePitch;
    s.tempoMatch = next.tempoMatch;
    s.oneShot = next.oneShot;
    s.spliceVariable = next.spliceVariable;
  });
}
void AppView::audition(double a, double b) {
  const auto *s = selected();
  if (!s)
    return;
  safely([&] {
    require(s->completedRevision > 0 && s->rendered.isNotEmpty(),
            "Finish rendering before preview");
    preview.play(child(state.root, s->rendered), s->id, a, b);
  });
}
void AppView::timerCallback() {
  onlineButton.setButtonText(online.busy() ? "Cancel online analysis"
                                           : "Analyze drums online");
  auto *s = selected();
  waveform.setPosition(s && preview.playing() && preview.sampleId() == s->id &&
                               preview.duration() > 0
                           ? preview.position() / preview.duration()
                           : -1);
  String message = localError.isNotEmpty()    ? localError
                   : state.error.isNotEmpty() ? state.error
                                              : state.status;
  if (state.error.isEmpty() && !state.project.warnings.isEmpty())
    message +=
        juce::String::fromUTF8(" · ") +
        state.project.warnings.joinIntoString(juce::String::fromUTF8(" · "));
  statusLabel.setText(message, juce::dontSendNotification);
  statusLabel.setColour(juce::Label::textColourId,
                        state.error.isNotEmpty() || localError.isNotEmpty()
                            ? juce::Colours::darkred
                            : juce::Colour(0xff333333));
}
void AppView::chooseFolder() {
  chooser = std::make_unique<juce::FileChooser>("Open a portable sample folder",
                                                state.root);
  auto safe = juce::Component::SafePointer<AppView>(this);
  chooser->launchAsync(juce::FileBrowserComponent::openMode |
                           juce::FileBrowserComponent::canSelectDirectories,
                       [safe](const juce::FileChooser &f) {
                         if (safe && f.getResult().isDirectory())
                           safe->manager.open(f.getResult());
                       });
}
void AppView::chooseImport() {
  chooser = std::make_unique<juce::FileChooser>(
      "Import samples", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.xrni");
  auto safe = juce::Component::SafePointer<AppView>(this);
  chooser->launchAsync(juce::FileBrowserComponent::openMode |
                           juce::FileBrowserComponent::canSelectFiles |
                           juce::FileBrowserComponent::canSelectMultipleItems,
                       [safe](const juce::FileChooser &f) {
                         if (safe) {
                           juce::StringArray paths;
                           for (const auto &path : f.getResults())
                             paths.add(path.getFullPathName());
                           if (!paths.isEmpty())
                             safe->manager.import(paths, safe->bank);
                         }
                       });
}
void AppView::chooseDuplicate() {
  chooser = std::make_unique<juce::FileChooser>(
      "Choose an empty destination folder", state.root.getParentDirectory());
  auto safe = juce::Component::SafePointer<AppView>(this);
  chooser->launchAsync(juce::FileBrowserComponent::openMode |
                           juce::FileBrowserComponent::canSelectDirectories,
                       [safe](const juce::FileChooser &f) {
                         if (safe && f.getResult().isDirectory())
                           safe->manager.duplicate(f.getResult());
                       });
}
void AppView::filesDropped(const juce::StringArray &files, int, int) {
  if (files.size() == 1 && File(files[0]).isDirectory())
    manager.open(File(files[0]));
  else
    manager.import(files, bank);
}
void AppView::recentMenu() {
  juce::PopupMenu menu;
  for (int n = 0; n < recents.size(); ++n)
    menu.addItem(n + 1, recents[n], File(recents[n]).isDirectory());
  if (recents.isEmpty())
    menu.addItem(1, "No recent folders", false);
  auto safe = juce::Component::SafePointer<AppView>(this);
  menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&recent),
                     [safe](int n) {
                       if (safe && n > 0 && n <= safe->recents.size())
                         safe->manager.open(File(safe->recents[n - 1]));
                     });
}
void AppView::moreMenu() {
  juce::PopupMenu menu;
  menu.addItem(1, "Retry pending save");
  menu.addItem(2, "Reload / reconcile completed card");
  menu.addItem(3, "Clear this bank");
  menu.addSeparator();
  menu.addItem(4, juce::String::fromUTF8(
                      "Clean unused originals and recovery history…"));
  auto safe = juce::Component::SafePointer<AppView>(this);
  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetComponent(&more), [safe](int n) {
        if (!safe)
          return;
        if (n == 1)
          safe->manager.retry();
        if (n == 2)
          juce::AlertWindow::showAsync(
              juce::MessageBoxOptions()
                  .withTitle("Reload external card changes")
                  .withMessage(
                      "Use the files currently in the project folder? Current "
                      "edits and originals will be retained in recovery "
                      "history, and this reload can be undone.")
                  .withButton("Reload")
                  .withButton("Cancel"),
              [safe](int result) {
                if (safe && result == 1)
                  safe->manager.reconcile();
              });
        if (n == 3)
          safe->manager.clearBank(safe->bank);
        if (n == 4)
          juce::AlertWindow::showAsync(
              juce::MessageBoxOptions()
                  .withTitle("Clean recovery history")
                  .withMessage("Remove unused originals and saved recovery "
                               "history? This also clears undo and redo.")
                  .withButton("Clean")
                  .withButton("Cancel"),
              [safe](int result) {
                if (safe && result == 1)
                  safe->manager.cleanup();
              });
      });
}
void AppView::showSettings() {
  settingsWindow = std::make_unique<AuxiliaryWindow>(
      "Device settings", new SettingsView(manager, look, presentation));
}
void AppView::savePreferences() {
  try {
    preferences = parseJson(preferencesFile());
  } catch (...) {
  }
  put(preferences, "presentation", presentation);
  if (state.root != File())
    put(preferences, "lastProject", state.root.getFullPathName());
  juce::Array<var> folders;
  for (const auto &r : recents)
    folders.add(r);
  put(preferences, "recents", folders);
  try {
    durableJson(preferencesFile(), preferences);
  } catch (...) {
  }
}
bool AppView::keyPressed(const juce::KeyPress &key) {
  if (key.getModifiers().isCommandDown()) {
    if (key.getKeyCode() == 'Z') {
      if (key.getModifiers().isShiftDown())
        manager.redo();
      else
        manager.undo();
      return true;
    }
    if (key.getKeyCode() == 'O') {
      chooseFolder();
      return true;
    }
    if (key.getKeyCode() == 'I') {
      chooseImport();
      return true;
    }
  }
  if (key == juce::KeyPress::spaceKey) {
    play.triggerClick();
    return true;
  }
  if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
    manager.remove(selectedIds());
    return true;
  }
  return false;
}
void AppView::paint(juce::Graphics &g) {
  g.fillAll(look.theme.background);
  g.setColour(look.theme.header);
  g.fillRect(0, 0, getWidth(), 65);
  g.setColour(presentation == 1 ? juce::Colour(0xff211d2d)
                                : juce::Colours::white);
  g.setFont(look.font(21));
  int x = 24;
  if (presentation == 2) {
    g.drawImageWithin(ectoLogo, 20, 14, 36, 36,
                      juce::RectanglePlacement::centred, true);
    x = 66;
  }
  g.drawText(look.theme.name, x, 13, 245, 28, juce::Justification::left);
  g.setFont(look.font(10));
  g.drawText("CORE SAMPLE MANAGER", x, 39, 245, 17, juce::Justification::left);
  g.setColour(look.theme.sidebar);
  g.fillRect(0, getHeight() - 42, getWidth(), 42);
  if (!selected() && state.root != File()) {
    g.setColour(juce::Colours::darkgrey);
    g.setFont(look.font(17));
    g.drawFittedText("Drop audio files here\nor choose Import",
                     getLocalBounds()
                         .withTrimmedLeft(420)
                         .withTrimmedTop(180)
                         .withTrimmedBottom(80),
                     juce::Justification::centred, 3);
  }
}
void AppView::toggleVisualizer() {
  if (visualizerWindow && !visualizerWindow->isVisible())
    visualizerWindow.reset();
  if (visualizer || visualizerWindow) {
    visualizerWindow.reset();
    visualizer.reset();
    visualSession.enable(false);
    visualizerButton.setToggleState(false, juce::dontSendNotification);
  } else {
    visualizer = std::make_unique<zv::Editor>(visualSession);
    visualizer->onDetach = [this] { detachVisualizer(); };
    addAndMakeVisible(*visualizer);
    visualizerButton.setToggleState(true, juce::dontSendNotification);
  }
  resized();
}
void AppView::detachVisualizer() {
  if (!visualizer)
    return;
  auto *view = visualizer.release();
  view->setDetached();
  view->onDetach = [this] {
    if (visualizerWindow)
      visualizerWindow->setFullScreen(!visualizerWindow->isFullScreen());
  };
  visualizerWindow = std::make_unique<AuxiliaryWindow>("Core Visualizer", view);
  visualizerWindow->setResizeLimits(480, 320, 4000, 2400);
  visualizerWindow->onClose = [this, view] {
    view->setActive(false);
    visualizerButton.setToggleState(false, juce::dontSendNotification);
  };
  resized();
}
void AppView::resized() {
  auto header = getLocalBounds().removeFromTop(65);
  presentationBox.setBounds(getWidth() - 210, 20, 186, 28);
  int x = 18;
  for (auto *button :
       {&open, &recent, &reveal, &duplicate, &importButton, &settingsButton,
        &deviceButton, &visualizerButton, &more}) {
    int w = button == &open               ? 128
            : button == &visualizerButton ? 114
            : button == &settingsButton   ? 100
            : button == &more             ? 48
                                          : 88;
    button->setBounds(x, 80, w, 30);
    x += w + 7;
  }
  undoButton.setBounds(getWidth() - 168, 119, 70, 25);
  redoButton.setBounds(getWidth() - 90, 119, 70, 25);
  folderLabel.setBounds(18, 117, getWidth() - 205, 27);
  statusLabel.setBounds(18, getHeight() - 38, getWidth() - 36, 33);
  banks.setBounds(16, 158, 117, getHeight() - 215);
  juce::Component *columns[]{&samples, &divider, &editor};
  layout.layOutComponents(columns, 3, 145, 158, getWidth() - 161,
                          getHeight() - 215, false, true);
  if (visualizer) {
    int height = std::min(290, (getHeight() - 215) / 2);
    banks.setBounds(banks.getBounds().withTrimmedBottom(height + 8));
    samples.setBounds(samples.getBounds().withTrimmedBottom(height + 8));
    visualizer->setBounds(16, getHeight() - 57 - height, editor.getX() - 28,
                          height);
  }
  empty.setBounds(getWidth() / 2 - 145, getHeight() / 2 - 22, 290, 44);
  auto w = editor.getWidth();
  name.setBounds(18, 0, w - 36, 31);
  sourceLabel.setBounds(18, 34, w - 36, 22);
  waveform.setBounds(6, 65, w - 12, std::max(145, editor.getHeight() - 490));
  int y = waveform.getBottom() + 8;
  play.setBounds(18, y, 122, 29);
  markerMode.setBounds(148, y, 166, 29);
  sliceCount.setBounds(322, y, 40, 29);
  evenButton.setBounds(370, y, 117, 29);
  y += 38;
  hint.setBounds(18, y, w - 36, 40);
  y += 47;
  int unit = (w - 48) / 3;
  sourceBpmLabel.setBounds(18, y, unit, 18);
  renderBpmLabel.setBounds(30 + unit, y, unit, 18);
  channelLabel.setBounds(42 + unit * 2, y, unit, 18);
  y += 20;
  sourceBpm.setBounds(18, y, unit, 27);
  renderBpm.setBounds(30 + unit, y, unit, 27);
  channel.setBounds(42 + unit * 2, y, unit, 27);
  y += 33;
  preserve.setBounds(16, y, 170, 25);
  tempo.setBounds(190, y, 180, 25);
  oneShot.setBounds(372, y, 110, 25);
  y += 33;
  playModeLabel.setBounds(18, y, 145, 18);
  triggerLabel.setBounds(214, y, 100, 18);
  y += 20;
  playMode.setBounds(18, y, 182, 27);
  spliceTrigger.setBounds(214, y, 90, 27);
  variable.setBounds(314, y, w - 325, 27);
  y += 36;
  advancedLabel.setBounds(18, y, w - 36, 18);
  y += 20;
  detector.setBounds(18, y, 130, 27);
  spacing.setBounds(156, y, 70, 27);
  autoButton.setBounds(234, y, 114, 27);
  onlineButton.setBounds(356, y, w - 374, 27);
  y += 37;
  removeButton.setBounds(18, y, 88, 27);
  mergeButton.setBounds(114, y, 80, 27);
  up.setBounds(202, y, 36, 27);
  down.setBounds(246, y, 36, 27);
  moveBank.setBounds(290, y, std::max(150, w - 308), 27);
}
} // namespace core
