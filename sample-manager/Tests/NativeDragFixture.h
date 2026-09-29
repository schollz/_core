#pragma once
#include "Storage.h"
#include <ImportData.h>

// Optional native acceptance aid: the OS drags a real synthetic file URL back
// through JUCE's platform drop handler. No filesDropped() call is simulated.
class NativeDragFixture final : public juce::Component {
public:
  NativeDragFixture() {
    folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                 .getChildFile("core-native-drag-" + core::uuid());
    core::ensureDirectory(folder);
    file = folder.getChildFile("native-drop.ogg");
    core::durableWrite(file, ImportData::import_ogg,
                       size_t(ImportData::import_oggSize));
    setName("Drag synthetic Ogg fixture into the manager");
  }
  ~NativeDragFixture() override { folder.deleteRecursively(); }
  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xfff2d66d));
    g.setColour(juce::Colours::black);
    g.drawFittedText("DRAG SYNTHETIC OGG INTO THE MANAGER",
                     getLocalBounds().reduced(6), juce::Justification::centred,
                     2);
  }
  void mouseDown(const juce::MouseEvent &) override { started = false; }
  void mouseDrag(const juce::MouseEvent &event) override {
    if (started || event.getDistanceFromDragStart() < 4)
      return;
    started = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        {file.getFullPathName()}, false, this);
  }

private:
  juce::File folder, file;
  bool started = false;
};
