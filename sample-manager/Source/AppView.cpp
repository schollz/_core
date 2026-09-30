#include "AppView.h"
#include <BinaryData.h>
namespace core {
namespace {
constexpr int railWidth = 72, railRowHeight = 39;
juce::Colour sampleRailColour(const Theme &theme) {
  return theme.bankRail.interpolatedWith(theme.background, .55f);
}
String railFilename(const Sample &sample) {
  // Old cards have no recorded filename. Keep their generated recovery label
  // in the editor, but don't present it as a filename in the compact rail.
  if (sample.origin == "recovered card audio" && sample.originalFilename.isEmpty())
    for (int slot = 1; slot <= 16; ++slot)
      if (sample.name == "Recovered card audio " + String(slot))
        return {};
  return sample.name.isNotEmpty() ? sample.name : sample.originalFilename;
}
struct AppFrame {
  explicit AppFrame(juce::Rectangle<int> bounds) {
    outer = bounds.reduced(12);
    header = outer.withHeight(65);
    footer = outer.withY(outer.getBottom() - 42).withHeight(42);
    auto body = outer.withTrimmedTop(65).withTrimmedBottom(42);
    bankRail = body.withWidth(railWidth);
    sampleRail = body.withTrimmedLeft(railWidth).withWidth(railWidth);
    workspace = body.withTrimmedLeft(railWidth * 2).withTrimmedRight(5);
    content = workspace.reduced(12);
  }
  juce::Rectangle<int> outer, header, footer, bankRail, sampleRail, workspace, content;
};
// Wrap whole control groups, keeping labels with their fields and related
// actions together. Each logical row starts at the same left edge.
struct ControlRow {
  ControlRow(int width, int top) : right(width - 18), y(top) {}
  juce::Rectangle<int> next(int width, int height) {
    width = std::min(width, right - 18);
    if (x > 18 && x + width > right) {
      x = 18;
      y += rowHeight + 8;
      rowHeight = 0;
    }
    juce::Rectangle<int> bounds(x, y, width, height);
    x += width + 8;
    rowHeight = std::max(rowHeight, height);
    return bounds;
  }
  int bottom() const { return y + rowHeight; }
  int right, y, x = 18, rowHeight = 0;
};
} // namespace
AppView::AdvancedButton::AdvancedButton(Look &l) : Button("Advanced"), look(l) {
  setClickingTogglesState(true);
  setTooltip("Show or hide tempo processing, slice timing, auto-slice tuning and online analysis. "
             "Settings are kept when hidden.");
}
void AppView::AdvancedButton::setAnalysisRunning(bool running) {
  const bool wasRunning = analysisRunning;
  analysisRunning = running;
  if (running != wasRunning)
    setTooltip(running ? "Online analysis is running. Use Cancel online analysis inside Advanced "
                         "to stop it. Collapsing these controls keeps analysis running."
                       : "Show or hide tempo processing, slice timing, auto-slice tuning and "
                         "online analysis. Settings are kept when hidden.");
  if (!getToggleState() && (running || wasRunning))
    repaint();
}
void AppView::AdvancedButton::paintButton(juce::Graphics &g, bool highlighted, bool down) {
  if (highlighted || down) {
    g.setColour(look.theme.sidebar.withAlpha(down ? .8f : .4f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);
  }
  if (hasKeyboardFocus(true)) {
    g.setColour(look.theme.accent);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f), 3.f, 1.f);
  }
  const auto ink = juce::Colour(0xff1a1a1a).withAlpha(isEnabled() ? 1.f : .5f);
  g.setColour(ink);
  const float cy = getHeight() * .5f;
  juce::Path chevron;
  if (getToggleState()) {
    chevron.startNewSubPath(5.f, cy - 2.f);
    chevron.lineTo(9.f, cy + 2.f);
    chevron.lineTo(13.f, cy - 2.f);
  } else {
    chevron.startNewSubPath(7.f, cy - 4.f);
    chevron.lineTo(11.f, cy);
    chevron.lineTo(7.f, cy + 4.f);
  }
  g.strokePath(chevron, juce::PathStrokeType(1.5f));
  g.setFont(look.font(13));
  g.drawText("Advanced", 21, 0, 76, getHeight(), juce::Justification::centredLeft);
  if (analysisRunning && !getToggleState())
    look.drawSpinningWaitAnimation(g, ink, 103, (getHeight() - 14) / 2, 14, 14);
}
AppView::AppView() {
  diagnostics::Scope trace("UI", "Create main view");
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
  diagnostics::log("UI", "Presentation=" + look.theme.name +
                             " preferences=" + preferencesFile().getFullPathName());
  if (auto *a = preferences["recents"].getArray())
    for (auto &r : *a)
      if (File::isAbsolutePath(r.toString()))
        recents.add(r.toString());
  ectoLogo =
      juce::ImageCache::getFromMemory(BinaryData::ectocore_png, BinaryData::ectocore_pngSize);
  for (size_t i = 0; i < headerRunes.size(); ++i) {
    int size = 0;
    const auto resource = "rune" + String(int(i) + 1) + "_svg";
    const auto *data = BinaryData::getNamedResource(resource.toRawUTF8(), size);
    headerRunes[i] = juce::Drawable::createFromImageData(data, size);
  }
  for (auto *c : std::initializer_list<juce::Component *>{
           &open,         &recent,          &reveal,        &duplicate,
           &importButton, &settingsButton,  &deviceButton,  &visualizerButton,
           &more,         &presentationBox, &folderLabel,   &statusLabel,
           &activity,     &banks,           &samples,       &divider,
           &editor,       &empty,           &createProject, &undoButton,
           &redoButton})
    addAndMakeVisible(c);
  activity.setPercentageDisplay(false);
  activity.setStyle(juce::ProgressBar::Style::linear);
  activity.setVisible(false);
  for (auto *c :
       std::initializer_list<juce::Component *>{&waveform, &name, &sourceLabel, &controlsViewport})
    editor.addAndMakeVisible(c);
  controlsViewport.setViewedComponent(&controls, false);
  controlsViewport.setScrollBarsShown(true, false);
  controlsViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
  for (auto *c : std::initializer_list<juce::Component *>{
           &hint,         &play,       &markerMode,     &sliceCount,
           &evenButton,   &autoButton, &sourceBpmLabel, &sourceBpm,
           &channelLabel, &channel,    &playModeLabel,  &playMode,
           &tempo,        &oneShot,    &removeButton,   &mergeButton,
           &up,           &down,       &moveBank,       &advancedButton})
    controls.addAndMakeVisible(c);
  controls.addChildComponent(advancedControls);
  for (auto *c : std::initializer_list<juce::Component *>{
           &tempoProcessingLabel, &renderBpmLabel, &renderBpm, &preserve, &sliceTimingLabel,
           &variable, &autoSliceLabel, &detectorLabel, &detector, &spacingLabel, &spacing,
           &onlineAnalysisLabel, &onlineButton})
    advancedControls.addAndMakeVisible(c);
  advancedButton.onClick = [this] {
    // This is session-only view state. Text fields retain their normal
    // focus-loss commit; toggling the disclosure itself never edits a sample.
    advancedControls.setVisible(advancedButton.getToggleState());
    resized();
    if (!advancedButton.getToggleState())
      controlsViewport.setViewPosition(0, 0);
  };
  presentationBox.addItemList({"Ezeptocore", "Zeptocore", "Ectocore"}, 1);
  presentationBox.setSelectedId(presentation + 1, juce::dontSendNotification);
  presentationBox.onChange = [this] {
    presentation = presentationBox.getSelectedId() - 1;
    look.presentation(presentation);
    diagnostics::log("UI", "Presentation changed to " + look.theme.name);
    settingsWindow.reset();
    savePreferences();
    sendLookAndFeelChange();
    if (deviceWindow) {
      deviceWindow->sendLookAndFeelChange();
      deviceWindow->repaint();
    }
    updatePresentation();
    resized();
    repaint();
  };
  deviceButton.onClick = [this] {
    deviceWindow = std::make_unique<AuxiliaryWindow>(
        "Local device tools", new DeviceView(device, look));
  };
  visualizerButton.onClick = [this] { toggleVisualizer(); };
  open.onClick = [this] { chooseFolder(); };
  empty.onClick = open.onClick;
  createProject.onClick = [this] { chooseNewProject(); };
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
    manager.detect(selectedId, sliceCount.getText().getIntValue(), detector.getText(),
                   spacing.getText().getDoubleValue());
  };
  sliceCount.setInputRestrictions(4, "0123456789");
  sliceCount.setTooltip("Number of slices for Even slices and target for Auto slice.");
  autoButton.setTooltip(
      "Target this many slices using the strongest detected attacks. "
      "Fewer may be found with sparse audio or larger minimum spacing. "
      "Enables variable splice timing.");
  evenButton.setTooltip("Create evenly spaced slices, calculate their timing from Source BPM "
                        "and sample length, and disable variable splice timing.");
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
  tempoProcessingLabel.setText("Tempo processing", juce::dontSendNotification);
  sliceTimingLabel.setText("Slice timing", juce::dontSendNotification);
  autoSliceLabel.setText("Auto-slice tuning", juce::dontSendNotification);
  detectorLabel.setText("Detector method", juce::dontSendNotification);
  spacingLabel.setText("Minimum spacing (ms)", juce::dontSendNotification);
  onlineAnalysisLabel.setText("Online analysis", juce::dontSendNotification);
  hint.setText(
      juce::String::fromUTF8("Click: audition • Right-click: add • Double-click: remove\n"
                             "Drag: move marker • Wheel: zoom • Shift-wheel / middle-drag: pan"),
      juce::dontSendNotification);
  hint.setFont(look.font(11));
  hint.setBorderSize({0, 0, 0, 0});
  hint.setJustificationType(juce::Justification::topLeft);
  hint.setMinimumHorizontalScale(1.f);
  channel.addItemList({"Mono", "Stereo"}, 1);
  playMode.addItemList(
      {"Slice stop", "Slice loop", "Sample stop", "Sample loop", "Granular"},
      1);
  updatePresentation();
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
  for (auto *edit : {&sourceBpm, &renderBpm}) {
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
  for (auto *button : {&preserve, &tempo, &oneShot})
    button->onClick = [this] { editControls(); };
  variable.onClick = [this] {
    if (updating)
      return;
    if (const auto *sample = selected())
      manager.edit(
          sample->id, "Variable splice timing",
          [enabled = variable.getToggleState()](Sample &s) { s.spliceVariable = enabled; });
  };
  waveform.onEdit = [this](const Sample &updated) {
    manager.editMarkers(updated.id, updated.slices, updated.transients);
  };
  waveform.onAudition = [this](double a, double b) { audition(a, b); };
  bankRows.count = [] { return 16; };
  bankRows.description = [this](int row) {
    int count = 0;
    for (const auto &s : state.project.samples)
      if (s.bank == row)
        ++count;
    return "Bank " + String(row + 1) + ": " + String(count) + " / 16 samples";
  };
  bankRows.tooltip = [this](int row) {
    return bankRows.description(row) + "\nSelect this bank to edit its samples. "
                                       "The last selected sample is remembered for this session.";
  };
  bankRows.paint = [this](int row, juce::Graphics &g, int w, int h, bool selected) {
    g.fillAll(selected ? sampleRailColour(look.theme) : look.theme.bankRail);
    std::array<bool, 16> occupied{};
    for (const auto &s : state.project.samples)
      if (s.bank == row && s.slot >= 0 && s.slot < 16)
        occupied[s.slot] = true;

    constexpr int cellSize = 6, gap = 2, padding = 3;
    constexpr int tileSize = cellSize * 4 + gap * 3 + padding * 2;
    const juce::Rectangle<int> tile((w - tileSize) / 2, (h - tileSize) / 2, tileSize, tileSize);
    for (int slot = 0; slot < 16; ++slot) {
      g.setColour(occupied[slot] ? juce::Colour(0xff404044) : look.theme.background);
      g.fillRect(tile.getX() + padding + (slot % 4) * (cellSize + gap),
                 tile.getY() + padding + (slot / 4) * (cellSize + gap), cellSize, cellSize);
    }
  };
  bankRows.select = [this](int row) { selectBank(row); };
  bankRows.click = [this](int row) {
    // The initially active bank may not have a sample selected yet. Clicking
    // an already-selected bank does not emit selectedRowsChanged.
    if (row == bank && selectedId.isEmpty())
      selectBank(row);
  };
  sampleRows.count = [] { return 16; };
  sampleRows.description = [this](int row) {
    auto title = String(row + 1).paddedLeft('0', 2);
    for (const auto &sample : state.project.samples)
      if (sample.bank == bank && sample.slot == row) {
        auto filename = railFilename(sample);
        if (filename.isNotEmpty())
          title += " " + filename;
        break;
      }
    return title;
  };
  sampleRows.tooltip = [this](int row) {
    for (const auto &sample : state.project.samples)
      if (sample.bank == bank && sample.slot == row) {
        auto tip = sampleRows.description(row);
        if (sample.protectedEntry)
          return tip + "\n" + sample.problem;
        return tip + "\nSelect to edit. Shift-click selects a range; "
                     "Ctrl/Cmd-click adds or removes a sample from the selection.";
      }
    return "Slot " + String(row + 1) +
           " is empty. Import or drop audio into this bank to add samples.";
  };
  sampleRows.paint = [this](int row, juce::Graphics &g, int w, int h, bool selected) {
    const Sample *sample = nullptr;
    for (const auto &s : state.project.samples)
      if (s.bank == bank && s.slot == row)
        sample = &s;
    const auto filename = sample ? railFilename(*sample) : String();
    g.fillAll(selected ? look.theme.background : sampleRailColour(look.theme));
    g.setColour(sample ? juce::Colour(0xff1a1a1a) : juce::Colour(0xff737780));
    g.setFont(look.font(12));
    g.drawText(String(row + 1).paddedLeft('0', 2), 6, 0, w - 12, filename.isEmpty() ? h : 21,
               juce::Justification::centred);
    if (filename.isNotEmpty()) {
      g.setFont(look.font(9));
      g.drawText(filename, 6, 20, w - 12, h - 23, juce::Justification::centred);
    }
  };
  sampleRows.select = [this](int) { selection(); };
  banks.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
  samples.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
  banks.setRowHeight(railRowHeight);
  samples.setRowHeight(railRowHeight);
  samples.setMultipleSelectionEnabled(true);
  banks.selectRow(0);
  layout.setItemLayout(0, 340, 460, 340);
  layout.setItemLayout(1, 6, 6, 6);
  layout.setItemLayout(2, 510, -1, -1);
  state = manager.snapshot();
  updateEditor();
  open.setTooltip("Open an existing project or Core card folder. Ctrl/Cmd+O.");
  empty.setTooltip(open.getTooltip());
  createProject.setTooltip("Choose a name and location for a new project folder. "
                           "Existing folders are never overwritten.");
  recent.setTooltip("Reopen one of your recently used project folders.");
  reveal.setTooltip("Show the current project folder in your system's file manager.");
  duplicate.setTooltip("Copy the whole project, including original audio and settings, "
                       "to an empty folder. Wait for saving to finish first.");
  importButton.setTooltip("Add audio files or XRNI instruments to the selected bank. "
                          "Each bank holds up to 16 samples. Ctrl/Cmd+I.");
  settingsButton.setTooltip("Edit the project's hardware settings and effect banks. "
                            "Changes save to this project's folder.");
  deviceButton.setTooltip(
      "Choose MIDI ports, view device messages, or install a local firmware file.");
  visualizerButton.setTooltip(
      "Show or hide the visualizer dock. Choose Device or Preview inside it.");
  more.setTooltip("Retry saving, reconcile external card changes, clear this bank, "
                  "or clean unused originals and recovery history.");
  presentationBox.setTooltip("Choose Ezeptocore, Zeptocore or Ectocore appearance and controls. "
                             "Switching keeps saved sample settings and the MIDI connection.");
  undoButton.setTooltip("Undo the last project edit. Ctrl/Cmd+Z.");
  redoButton.setTooltip("Redo an undone project edit. Ctrl/Cmd+Shift+Z.");
  play.setTooltip(
      "Play the selected sample's last completed rendering, or stop the current preview. Space.");
  name.setTooltip("Rename this sample. Press Enter or click elsewhere to save the name.");
  sourceBpm.setTooltip(
      "The tempo of the original audio. Check the estimate before changing Render BPM "
      "or using tempo matching on the instrument.");
  renderBpm.setTooltip(
      "Set a new tempo for the rendered audio, or leave empty to keep the source tempo. "
      "Preserve pitch controls whether the pitch changes too.");
  channel.setTooltip("Render this sample as mono or stereo from the original audio.");
  playMode.setTooltip("Choose how the instrument plays this sample: stop or loop at a slice or "
                      "sample boundary, or use granular playback.");
  preserve.setTooltip("Keep the original pitch when Render BPM changes the speed. "
                      "When off, speed and pitch change together.");
  tempo.setTooltip("Let the instrument change this sample's playback speed to follow its tempo. "
                   "Source BPM tells it the sample's original tempo.");
  oneShot.setTooltip("Enable the instrument's one-shot behavior for this sample. "
                     "Playback mode controls what happens at slice and sample boundaries.");
  variable.setTooltip("Use the actual spacing of slice markers for timing. Manual slice edits and "
                      "Auto slice turn this on; Even slices turns it off.");
  detector.setTooltip("Choose the local onset detector used by Auto slice. HFC is the default "
                      "for finding sharp attacks. This runs offline.");
  spacing.setTooltip("Minimum time between detected slice boundaries, in milliseconds. "
                     "Larger values can produce fewer slices than the requested count.");
  removeButton.setTooltip(
      "Remove the selected samples from this project. Undo restores them. Delete.");
  mergeButton.setTooltip("Create one sample from the selected samples in slot order, using their "
                         "current audio settings. The original entries are kept.");
  up.setTooltip("Move the selected samples one slot earlier in this bank.");
  down.setTooltip("Move the selected samples one slot later in this bank.");
  moveBank.setTooltip(
      "Move the selected samples to another bank. The destination must have enough free slots.");
  divider.setTooltip("Drag left or right to resize the visualizer dock and sample editor.");
  sourceBpmLabel.setTooltip(sourceBpm.getTooltip());
  renderBpmLabel.setTooltip(renderBpm.getTooltip());
  channelLabel.setTooltip(channel.getTooltip());
  playModeLabel.setTooltip(playMode.getTooltip());
  detectorLabel.setTooltip(detector.getTooltip());
  spacingLabel.setTooltip(spacing.getTooltip());
  tempoProcessingLabel.setTooltip(renderBpm.getTooltip());
  sliceTimingLabel.setTooltip(variable.getTooltip());
  autoSliceLabel.setTooltip("Tune the detector and spacing used by the Auto slice button above.");
  onlineAnalysisLabel.setTooltip(onlineButton.getTooltip());
  hint.setTooltip("Choose Slices or a transient lane in the marker selector. "
                  "Hover over the waveform for editing instructions for that selection.");
  for (auto *button : {&open,         &recent,
                       &reveal,       &duplicate,
                       &importButton, &settingsButton,
                       &deviceButton, &visualizerButton,
                       &more,         &undoButton,
                       &redoButton,   &play,
                       &evenButton,   &autoButton,
                       &onlineButton, &removeButton,
                       &mergeButton,  &up,
                       &down,         &empty,
                       &createProject}) {
    auto action = button->onClick;
    button->onClick = [button, action] {
      diagnostics::log("UI", "Click " + button->getButtonText());
      if (action)
        action();
    };
  }
  startTimerHz(30);
  auto last = preferences["lastProject"].toString();
  if (File::isAbsolutePath(last) && File(last).isDirectory())
    manager.open(File(last));
}
AppView::~AppView() {
  diagnostics::Scope trace("UI", "Destroy main view");
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
    diagnostics::log("UI", "Error: " + localError);
  }
  repaint();
}
const Sample *AppView::selected() const {
  auto *sample = state.project.find(selectedId);
  return sample && sample->bank == bank ? sample : nullptr;
}
std::vector<String> AppView::selectedIds() const {
  std::vector<String> ids;
  auto rows = samples.getSelectedRows();
  for (const auto &s : state.project.samples)
    if (s.bank == bank && rows.contains(s.slot))
      ids.push_back(s.id);
  return ids;
}
void AppView::selectBank(int row) {
  if (row < 0 || row >= 16)
    return;
  // Commit text belonging to the outgoing sample before restoring the editor.
  editControls();
  diagnostics::log("UI", "Select bank=" + String(row + 1));
  bank = row;
  samples.setSelectedRows({}, juce::dontSendNotification);
  selectedId.clear();
  visualSession.selectedId.clear();
  samples.updateContent();
  const auto *last = state.project.find(lastSelectedInBank[size_t(bank)]);
  if (!last || last->bank != bank) {
    last = nullptr;
    // A bank without selection memory starts at its first occupied slot.
    for (const auto &sample : state.project.samples)
      if (sample.bank == bank && (!last || sample.slot < last->slot))
        last = &sample;
  }
  if (last) {
    samples.selectRow(last->slot);
    samples.scrollToEnsureRowIsOnscreen(last->slot);
  } else
    updateEditor();
  // All banks have 16 rows; changing model data also needs a repaint.
  samples.repaint();
  repaint();
}
void AppView::selection() {
  selectedId.clear();
  int row = samples.getLastRowSelected();
  for (const auto &s : state.project.samples)
    if (s.bank == bank && s.slot == row)
      selectedId = s.id;
  if (selectedId.isNotEmpty())
    lastSelectedInBank[size_t(bank)] = selectedId;
  visualSession.selectedId = selectedId;
  diagnostics::log("UI", "Select sample=" + selectedId + " bank=" + String(bank + 1) +
                             " slot=" + String(row + 1));
  updateEditor();
}
void AppView::changeListenerCallback(juce::ChangeBroadcaster *) {
  auto before = state.root;
  auto previousProject = state.project.id;
  auto previousImport = state.importGeneration;
  auto next = manager.snapshot();
  juce::SparseSet<int> movedRows;
  int focusedRow = -1;
  bool selectionMoved = false;
  if (next.root == before && next.project.id == previousProject) {
    // Row numbers change on reorder and undo; keep the selected sample IDs.
    for (const auto &id : selectedIds()) {
      const auto *previous = state.project.find(id);
      const auto *current = next.project.find(id);
      if (current && current->bank == bank) {
        movedRows.addRange({current->slot, current->slot + 1});
        selectionMoved |= current->slot != previous->slot;
        if (id == selectedId)
          focusedRow = current->slot;
      } else
        selectionMoved = true;
    }
  }
  state = std::move(next);
  if (state.root != before || state.project.id != previousProject) {
    lastSelectedInBank.fill({});
    samples.setSelectedRows({}, juce::dontSendNotification);
    selectedId.clear();
    controlsId.clear();
    visualSession.selectedId.clear();
  }
  diagnostics::log("UI", "Manager update generation=" + String(juce::int64(state.generation)) +
                             " busy=" + String(state.busy ? 1 : 0) + " available=" +
                             String(state.available ? 1 : 0) + " status=" + state.status);
  if (state.root != File() && state.root != before) {
    recents.removeString(state.root.getFullPathName());
    recents.insert(0, state.root.getFullPathName());
    while (recents.size() > 12)
      recents.remove(recents.size() - 1);
    savePreferences();
  }
  banks.updateContent();
  samples.updateContent();
  if (selectionMoved) {
    if (focusedRow < 0 && !movedRows.isEmpty())
      focusedRow = movedRows[0];
    // Restore the keyboard anchor first, then the rest of a multi-selection.
    juce::SparseSet<int> focus;
    if (focusedRow >= 0)
      focus.addRange({focusedRow, focusedRow + 1});
    samples.setSelectedRows(focus, juce::dontSendNotification);
    samples.setSelectedRows(movedRows, juce::dontSendNotification);
    selection();
    if (focusedRow >= 0)
      samples.scrollToEnsureRowIsOnscreen(focusedRow);
  }
  if (state.importGeneration != previousImport)
    if (const auto *imported = state.project.find(state.importedSampleId)) {
      banks.selectRow(imported->bank);
      // Also notify selection when the import filled an already-selected empty slot.
      samples.setSelectedRows({}, juce::dontSendNotification);
      samples.selectRow(imported->slot);
      samples.scrollToEnsureRowIsOnscreen(imported->slot);
    }
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
  createProject.setVisible(state.root == File());
  createProject.setEnabled(!state.busy);
  importButton.setEnabled(state.available);
  duplicate.setEnabled(state.available && !state.busy);
  settingsButton.setEnabled(state.available);
  activity.setVisible(state.busy && state.error.isEmpty());
  if (s) {
    bool editingText = name.hasKeyboardFocus(true) || sourceBpm.hasKeyboardFocus(true) ||
                       renderBpm.hasKeyboardFocus(true);
    bool refresh = controlsId != s->id || !editingText;
    controlsId = s->id;
    if (refresh) {
      name.setText(s->name, false);
      sourceBpm.setText(String(s->sourceBpm, 2), false);
      renderBpm.setText(s->renderBpm > 0 ? String(s->renderBpm, 2) : String(), false);
      channel.setSelectedId(s->channels, juce::dontSendNotification);
      playMode.setSelectedId(s->playMode + 1, juce::dontSendNotification);
      preserve.setToggleState(s->preservePitch, juce::dontSendNotification);
      tempo.setToggleState(s->tempoMatch, juce::dontSendNotification);
      oneShot.setToggleState(s->oneShot, juce::dontSendNotification);
    }
    variable.setToggleState(s->spliceVariable, juce::dontSendNotification);
    sourceLabel.setText(s->protectedEntry
                            ? s->problem
                            : String(s->sourceDuration, 2) + juce::String::fromUTF8(" s · ") +
                                  String(s->rate) + " Hz output",
                        juce::dontSendNotification);
    sourceLabel.setTooltip(s->protectedEntry
                               ? s->problem
                               : "Original duration and hardware output sample rate.\n" +
                                     sourceLabel.getText());
    waveform.set(s, state.editorWaveform(s->id));
    editor.setEnabled(!s->protectedEntry && state.available);
  } else {
    controlsId.clear();
    waveform.set(nullptr, {});
  }
  folderLabel.setText(
      state.root == File()
          ? "A portable folder for your samples and hardware files"
          : state.root.getFullPathName(),
      juce::dontSendNotification);
  folderLabel.setTooltip(
      state.root == File()
          ? "Create a project or open a folder to keep your samples and hardware files together."
          : state.root.getFullPathName() +
                "\nUse Reveal to open this folder in your file manager.");
  updating = false;
}
void AppView::updatePresentation() {
  const bool showTransients = presentation != 1;
  oneShot.setVisible(presentation != 1);
  waveform.setTransientLanesVisible(showTransients);
  const int marker = std::max(1, markerMode.getSelectedId());
  markerMode.clear(juce::dontSendNotification);
  markerMode.addItem("Slices", 1);
  if (showTransients)
    markerMode.addItemList({"Kick transients", "Snare transients", "Other transients"}, 2);
  markerMode.setSelectedId(showTransients ? marker : 1, juce::dontSendNotification);
  waveform.lane = markerMode.getSelectedId() - 2;
  markerMode.setTooltip(
      showTransients
          ? "Choose Slices, Kick, Snare or Other to edit that lane. For transient markers, "
            "click to add, drag to move, and double-click to remove."
          : "Edit slice boundaries: right-click to add, drag to move, and double-click to remove.");
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
  next.channels = channel.getSelectedId();
  next.playMode = playMode.getSelectedId() - 1;
  next.preservePitch = preserve.getToggleState();
  next.tempoMatch = tempo.getToggleState();
  const bool editOneShot = oneShot.isVisible();
  if (editOneShot)
    next.oneShot = oneShot.getToggleState();
  if (juce::JSON::toString(next.json()) ==
      juce::JSON::toString(current->json()))
    return;
  manager.edit(current->id, "Sample settings", [next, editOneShot](Sample &s) {
    s.name = next.name;
    s.sourceBpm = next.sourceBpm;
    s.renderBpm = next.renderBpm;
    s.channels = next.channels;
    s.playMode = next.playMode;
    s.preservePitch = next.preservePitch;
    s.tempoMatch = next.tempoMatch;
    if (editOneShot)
      s.oneShot = next.oneShot;
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
  const bool analysing = online.busy();
  const String onlineText = analysing ? "Cancel online analysis" : "Analyze drums online";
  if (onlineButton.getButtonText() != onlineText) {
    onlineButton.setButtonText(onlineText);
    onlineButton.setIcon(analysing ? "x" : "cloud-upload");
    onlineButton.setTooltip(
        analysing ? "Cancel the running online drum analysis. Existing markers are kept."
                  : "Upload this sample as mono 44.1 kHz audio to tool.getectocore.com for "
                    "kick/snare/other markers. Click again to cancel.");
    resized();
  }
  advancedButton.setAnalysisRunning(analysing);
  auto *s = selected();
  waveform.setPosition(s && preview.playing() && preview.sampleId() == s->id &&
                               preview.duration() > 0
                           ? preview.position() / preview.duration()
                           : -1);
  String message = localError.isNotEmpty()    ? localError
                   : state.error.isNotEmpty() ? state.error
                                              : state.status;
  if (state.error.isEmpty() && !state.project.warnings.isEmpty())
    message += juce::String::fromUTF8(" · ") +
               state.project.warnings.joinIntoString(juce::String::fromUTF8(" · "));
  statusLabel.setText(message, juce::dontSendNotification);
  statusLabel.setTooltip(message);
  const auto activityInk = presentation == 1 ? juce::Colour(0xff211d2d) : juce::Colours::white;
  activity.setColour(juce::ProgressBar::backgroundColourId, activityInk.withAlpha(.15f));
  activity.setColour(juce::ProgressBar::foregroundColourId, activityInk.withAlpha(.85f));
  if (message != lastDiagnosticStatus) {
    lastDiagnosticStatus = message;
    diagnostics::log("UI", "Status displayed: " + message);
  }
  statusLabel.setColour(juce::Label::textColourId,
                        state.error.isNotEmpty() || localError.isNotEmpty()
                            ? (presentation == 0 ? juce::Colour(0xffffb4a8)
                                                 : juce::Colour(0xff400c12))
                            : (presentation == 1 ? juce::Colour(0xff211d2d)
                                                 : juce::Colours::white));
}
void AppView::chooseNewProject() {
  auto parent = state.root == File() ? File::getSpecialLocation(File::userDocumentsDirectory)
                                     : state.root.getParentDirectory();
  chooser = std::make_unique<juce::FileChooser>("Create project folder",
                                                parent.getChildFile("New Core Project"));
  auto safe = juce::Component::SafePointer<AppView>(this);
  // Save mode collects a new name and location; the chooser writes no file.
  chooser->launchAsync(
      juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
      [safe](const juce::FileChooser &f) {
        const auto destination = f.getResult();
        if (!safe || destination == File())
          return;
        safe->safely([&] {
          require(!destination.exists() && !destination.isSymbolicLink(),
                  "That destination already exists. Choose a new project folder name.");
          // A single, exclusive mkdir also rejects a destination created after
          // the existence check. Never adopt or overwrite it as a new project.
          std::error_code error;
          const bool created = std::filesystem::create_directory(
              std::filesystem::u8path(destination.getFullPathName().toStdString()), error);
          require(created && !error,
                  "Cannot create project folder: " +
                      (error ? String(error.message()) : String("destination already exists")));
          safe->manager.open(destination);
        });
      });
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
  menu.setLookAndFeel(&look);
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
  menu.setLookAndFeel(&look);
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
  const AppFrame frame(getLocalBounds());
  g.fillAll(look.theme.background);
  // A single rounded silhouette keeps the header, footer and right edge joined.
  g.setColour(look.theme.header);
  g.fillRoundedRectangle(frame.outer.toFloat(), 12.f);
  g.setColour(look.theme.bankRail);
  g.fillRect(frame.bankRail);
  g.setColour(sampleRailColour(look.theme));
  g.fillRect(frame.sampleRail);
  g.setColour(look.theme.background);
  g.fillRect(frame.workspace);
  const auto headerInk = presentation == 1 ? juce::Colour(0xff211d2d) : juce::Colours::white;
  g.setColour(headerInk);
  g.setFont(look.font(21));
  const int x = frame.header.getX() + 66;
  const int headerY = frame.header.getY();
  const juce::Rectangle<int> iconBounds(frame.header.getX() + 20, headerY + 14, 36, 36);
  if (presentation == 2) {
    g.drawImageWithin(ectoLogo, iconBounds.getX(), iconBounds.getY(), 36, 36,
                      juce::RectanglePlacement::centred, true);
  } else {
    if (headerRuneInk != headerInk) {
      for (auto &rune : headerRunes)
        if (rune)
          rune->replaceColour(headerRuneInk, headerInk);
      headerRuneInk = headerInk;
    }
    // Like the website's URL-based choice, keep the rune stable per project.
    const auto index = size_t(uint32_t(state.project.id.hashCode())) % headerRunes.size();
    if (auto *rune = headerRunes[index].get())
      rune->drawWithin(g, iconBounds.toFloat(), juce::RectanglePlacement::centred, 1.f);
  }
  g.setColour(headerInk);
  g.drawText(look.theme.name, x, headerY + 13, 245, 28, juce::Justification::left);
  g.setFont(look.font(10));
  g.drawText("CORE SAMPLE MANAGER", x, headerY + 39, 245, 17, juce::Justification::left);
  if (!selected() && state.root != File()) {
    g.setColour(juce::Colours::darkgrey);
    g.setFont(look.font(17));
    g.drawFittedText("Drop audio files here\nor choose Import",
                     editor.getBounds().reduced(18),
                     juce::Justification::centred, 3);
  }
}
void AppView::toggleVisualizer() {
  diagnostics::log("UI", "Toggle visualizer");
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
  diagnostics::log("UI", "Detach visualizer");
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
int AppView::layoutControls(int width) {
  // Measure the tips with the same font the label draws, so narrow docks wrap
  // without shrinking the text or clipping the last line.
  const auto tipFont = look.getLabelFont(hint);
  juce::AttributedString tips;
  tips.append(hint.getText(), tipFont);
  juce::TextLayout tipLayout;
  tipLayout.createLayout(tips, float(std::max(1, width - 36)));
  const int tipHeight = int(std::ceil(std::max(tipLayout.getHeight(),
                                               tipLayout.getNumLines() * tipFont.getHeight()))) +
                        4;
  hint.setBounds(18, 0, width - 36, tipHeight);

  ControlRow tools(width, tipHeight + 6);
  const int playWidth = play.preferredWidth(29);
  auto transport = tools.next(playWidth + 8 + 166, 29);
  play.setBounds(transport.removeFromLeft(playWidth));
  transport.removeFromLeft(8);
  markerMode.setBounds(transport);
  const int evenWidth = evenButton.preferredWidth(29);
  auto slicing = tools.next(40 + 8 + evenWidth + 8 + autoButton.preferredWidth(29), 29);
  sliceCount.setBounds(slicing.removeFromLeft(40));
  slicing.removeFromLeft(8);
  evenButton.setBounds(slicing.removeFromLeft(evenWidth));
  slicing.removeFromLeft(8);
  autoButton.setBounds(slicing);

  auto field = [](juce::Rectangle<int> bounds, juce::Label &label, juce::Component &control) {
    label.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(2);
    control.setBounds(bounds);
  };
  ControlRow settings(width, tools.bottom() + 10);
  field(settings.next(120, 47), sourceBpmLabel, sourceBpm);
  field(settings.next(120, 47), channelLabel, channel);
  field(settings.next(182, 47), playModeLabel, playMode);

  ControlRow playback(width, settings.bottom() + 8);
  tempo.setBounds(playback.next(180, 27));
  if (oneShot.isVisible())
    oneShot.setBounds(playback.next(110, 27));

  ControlRow actions(width, playback.bottom() + 8);
  const int removeWidth = removeButton.preferredWidth(27);
  auto sampleActions = actions.next(removeWidth + 8 + mergeButton.preferredWidth(27), 27);
  removeButton.setBounds(sampleActions.removeFromLeft(removeWidth));
  sampleActions.removeFromLeft(8);
  mergeButton.setBounds(sampleActions);
  const int upWidth = up.preferredWidth(27);
  auto reorder = actions.next(upWidth + 8 + down.preferredWidth(27), 27);
  up.setBounds(reorder.removeFromLeft(upWidth));
  reorder.removeFromLeft(8);
  down.setBounds(reorder);
  moveBank.setBounds(actions.next(180, 27));

  advancedButton.setBounds(18, actions.bottom() + 8, 122, 27);
  const int advancedTop = advancedButton.getBottom() + 6;
  if (!advancedButton.getToggleState())
    return advancedTop;

  int y = 0;
  auto heading = [&](juce::Label &label) {
    label.setBounds(18, y, width - 36, 18);
    y += 22;
  };
  heading(tempoProcessingLabel);
  ControlRow processing(width, y);
  field(processing.next(184, 47), renderBpmLabel, renderBpm);
  preserve.setBounds(processing.next(170, 47).withTrimmedTop(20));
  y = processing.bottom() + 12;

  heading(sliceTimingLabel);
  variable.setBounds(18, y, width - 36, 27);
  y += 39;

  heading(autoSliceLabel);
  ControlRow tuning(width, y);
  field(tuning.next(160, 47), detectorLabel, detector);
  field(tuning.next(178, 47), spacingLabel, spacing);
  y = tuning.bottom() + 12;

  heading(onlineAnalysisLabel);
  const int onlineWidth = onlineButton.preferredWidth(27);
  onlineButton.setBounds(18, y, std::min(width - 36, onlineWidth), 27);
  y += 31;
  advancedControls.setBounds(0, advancedTop, width, y);
  return advancedTop + y;
}
void AppView::resized() {
  const AppFrame frame(getLocalBounds());
  presentationBox.setBounds(frame.header.getRight() - 210,
                             frame.header.getY() + 20, 186, 28);
  int x = frame.content.getX();
  int toolbarY = frame.content.getY() + 3;
  for (auto *button :
       {&open, &recent, &reveal, &duplicate, &importButton, &settingsButton,
        &deviceButton, &visualizerButton, &more}) {
    const int w = button->preferredWidth(30);
    if (x > frame.content.getX() && x + w > frame.content.getRight()) {
      x = frame.content.getX();
      toolbarY += 38;
    }
    button->setBounds(x, toolbarY, w, 30);
    x += w + 7;
  }
  const int metadataY = toolbarY + 39;
  const int undoWidth = undoButton.preferredWidth(25), redoWidth = redoButton.preferredWidth(25);
  redoButton.setBounds(frame.content.getRight() - redoWidth, metadataY, redoWidth, 25);
  undoButton.setBounds(redoButton.getX() - 8 - undoWidth, metadataY, undoWidth, 25);
  folderLabel.setBounds(frame.content.getX(), metadataY - 2,
                        undoButton.getX() - frame.content.getX() - 18, 27);
  auto statusBounds = frame.footer.reduced(16, 4);
  if (activity.isVisible()) {
    auto activityBounds = statusBounds.removeFromRight(100);
    activity.setBounds(activityBounds.withSizeKeepingCentre(100, 8));
    statusBounds.removeFromRight(12);
  }
  statusLabel.setBounds(statusBounds);
  banks.setBounds(frame.bankRail.reduced(0, 12));
  samples.setBounds(frame.sampleRail.reduced(0, 12));
  auto columnsArea = frame.content.withTrimmedTop(metadataY - frame.content.getY() + 39);
  divider.setVisible(visualizer != nullptr);
  if (visualizer) {
    juce::Component *columns[]{visualizer.get(), &divider, &editor};
    layout.layOutComponents(columns, 3, columnsArea.getX(), columnsArea.getY(),
                            columnsArea.getWidth(), columnsArea.getHeight(), false, true);
  } else
    editor.setBounds(columnsArea);
  createProject.setBounds(frame.content.getCentreX() - 145, frame.content.getCentreY() - 50, 290,
                          44);
  empty.setBounds(frame.content.getCentreX() - 145, createProject.getBottom() + 10, 290, 44);
  auto w = editor.getWidth();
  name.setBounds(18, 0, w - 36, 31);
  sourceLabel.setBounds(18, 34, w - 36, 22);
  constexpr int waveTop = 65, minWaveHeight = 145, controlGap = 8;
  const int maxControlsHeight =
      std::max(0, editor.getHeight() - waveTop - minWaveHeight - controlGap);
  int controlsWidth = w;
  int controlsHeight = layoutControls(controlsWidth);
  if (controlsHeight > maxControlsHeight) {
    controlsWidth -= controlsViewport.getScrollBarThickness();
    controlsHeight = layoutControls(controlsWidth);
  }
  const int viewportHeight = std::min(controlsHeight, maxControlsHeight);
  waveform.setBounds(
      6, waveTop, w - 12,
      std::max(minWaveHeight, editor.getHeight() - waveTop - controlGap - viewportHeight));
  controlsViewport.setBounds(0, waveform.getBottom() + controlGap, w, viewportHeight);
  controls.setSize(controlsWidth, controlsHeight);
  if (diagnostics::enabled()) {
    auto geometry = "window=" + getLocalBounds().toString() + " frame=" + frame.outer.toString() +
                    " banks=" + banks.getBounds().toString() +
                    " samples=" + samples.getBounds().toString() +
                    " editor=" + editor.getBounds().toString() +
                    " waveform=" + waveform.getBounds().toString() +
                    " controls=" + controlsViewport.getBounds().toString() +
                    " controls_height=" + String(controlsHeight) +
                    " advanced=" + String(advancedButton.getToggleState() ? 1 : 0) +
                    " auto_slice=" + autoButton.getBounds().toString() +
                    " docked=" + String(visualizer ? 1 : 0);
    if (geometry != lastDiagnosticLayout) {
      lastDiagnosticLayout = geometry;
      diagnostics::log("LAYOUT", geometry);
    }
  }
}
} // namespace core
