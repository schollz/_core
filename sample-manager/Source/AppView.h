#pragma once
#include "DeviceView.h"
#include "OnlineAnalysis.h"
#include "Preview.h"
#include "SettingsView.h"
#include "Visualizer/VisualizerView.h"
#include "WaveformEditor.h"
namespace core {
class AppView final : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::ChangeListener,
                      private juce::Timer {
public:
  AppView();
  ~AppView() override;
  void paint(juce::Graphics &) override;
  void resized() override;
  bool keyPressed(const juce::KeyPress &) override;
  bool isInterestedInFileDrag(const juce::StringArray &) override {
    return true;
  }
  void filesDropped(const juce::StringArray &, int, int) override;
  bool pending() const { return manager.snapshot().busy; }

private:
  class Divider final : public juce::StretchableLayoutResizerBar,
                        public juce::SettableTooltipClient {
  public:
    using StretchableLayoutResizerBar::StretchableLayoutResizerBar;
  };
  class AdvancedButton final : public juce::Button {
  public:
    explicit AdvancedButton(Look &);
    void setAnalysisRunning(bool);

  private:
    void paintButton(juce::Graphics &, bool highlighted, bool down) override;
    Look &look;
    bool analysisRunning = false;
  };
  struct Rows : juce::ListBoxModel {
    std::function<int()> count;
    std::function<void(int, juce::Graphics &, int, int, bool)> paint;
    std::function<void(int)> select;
    std::function<void(int)> click;
    std::function<String(int)> description;
    std::function<String(int)> tooltip;
    int getNumRows() override { return count ? count() : 0; }
    String getNameForRow(int i) override {
      return description ? description(i) : juce::ListBoxModel::getNameForRow(i);
    }
    String getTooltipForRow(int i) override { return tooltip ? tooltip(i) : getNameForRow(i); }
    void paintListBoxItem(int i, juce::Graphics &g, int w, int h,
                          bool s) override {
      if (paint)
        paint(i, g, w, h, s);
    }
    void selectedRowsChanged(int i) override {
      if (select)
        select(i);
    }
    void listBoxItemClicked(int i, const juce::MouseEvent &) override {
      if (click)
        click(i);
    }
  };
  class AuxiliaryWindow final : public juce::DocumentWindow {
  public:
    AuxiliaryWindow(String title, juce::Component *content)
        : DocumentWindow(title, juce::Colour(0xffeeeeee), closeButton) {
      setUsingNativeTitleBar(true);
      setContentOwned(content, true);
      setResizable(true, false);
      centreWithSize(content->getWidth(), content->getHeight());
      setVisible(true);
    }
    std::function<void()> onClose;
    void closeButtonPressed() override {
      setVisible(false);
      if (onClose)
        onClose();
    }
  };
  void changeListenerCallback(juce::ChangeBroadcaster *) override;
  void timerCallback() override;
  void chooseNewProject();
  void chooseFolder();
  void chooseImport();
  void chooseDuplicate();
  void recentMenu();
  void moreMenu();
  void toggleVisualizer();
  void detachVisualizer();
  void selection();
  void selectBank(int);
  void updateEditor();
  void updatePresentation();
  int layoutControls(int width);
  void editControls();
  void audition(double start = 0, double stop = 1);
  void showSettings();
  void savePreferences();
  void safely(const std::function<void()> &);
  std::vector<String> selectedIds() const;
  const Sample *selected() const;
  Manager manager;
  OnlineAnalysis online;
  Preview preview;
  Look look;
  Device device;
  zv::Session visualSession{manager, device, preview};
  std::unique_ptr<zv::Editor> visualizer;
  std::unique_ptr<AuxiliaryWindow> visualizerWindow, deviceWindow;
  ManagerState state;
  int bank = 0, presentation = 0;
  String selectedId, controlsId, localError;
  std::array<String, 16> lastSelectedInBank;
  String lastDiagnosticStatus, lastDiagnosticLayout;
  bool updating = false;
  Rows bankRows, sampleRows;
  juce::ListBox banks{"Banks", &bankRows}, samples{"Samples", &sampleRows};
  juce::Component editor, controls, advancedControls;
  juce::Viewport controlsViewport;
  AdvancedButton advancedButton{look};
  WaveformEditor waveform{look};
  juce::StretchableLayoutManager layout;
  Divider divider{&layout, 1, true};
  juce::TextButton open{"Open Folder"}, recent{"Recent"}, reveal{"Reveal"},
      duplicate{"Duplicate"}, importButton{"Import"},
      settingsButton{"Settings"}, deviceButton{"Device"},
      visualizerButton{"Visualizer"}, more{"More"};
  juce::ComboBox presentationBox, channel, playMode, markerMode, detector,
      moveBank;
  juce::TextButton play{"Play / Stop"}, evenButton{"Even slices"}, autoButton{"Auto slice"},
      onlineButton{"Analyze drums online"}, removeButton{"Remove"}, mergeButton{"Merge"},
      up{juce::String::fromUTF8("↑")}, down{juce::String::fromUTF8("↓")}, undoButton{"Undo"},
      redoButton{"Redo"}, createProject{"Create project"}, empty{"Open a project folder"};
  juce::TextEditor sourceBpm, renderBpm, sliceCount, spacing, name;
  juce::ToggleButton preserve{"Preserve pitch"}, tempo{"Tempo matching"},
      oneShot{"One-shot"}, variable{"Variable splice timing"};
  juce::Label folderLabel, statusLabel, sourceLabel, sourceBpmLabel, renderBpmLabel, channelLabel,
      playModeLabel, hint, tempoProcessingLabel, sliceTimingLabel, autoSliceLabel, detectorLabel,
      spacingLabel, onlineAnalysisLabel;
  double activityProgress = -1.;
  juce::ProgressBar activity{activityProgress};
  std::unique_ptr<juce::FileChooser> chooser;
  std::unique_ptr<AuxiliaryWindow> settingsWindow;
  juce::StringArray recents;
  var preferences = object();
  juce::Image ectoLogo;
  std::array<std::unique_ptr<juce::Drawable>, 7> headerRunes;
  juce::Colour headerRuneInk{0xff1a1a1a};
  Tooltips tooltips{*this};
};
} // namespace core
