#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <vector>
#include "Theme.h"

/**
 * WaveformDisplay - Displays audio waveform with draggable slice markers
 */
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // Load waveform data
    void setBuffer(const juce::AudioBuffer<float>* buffer);

    // Set slice positions (normalized 0-1)
    void setSlicePositions(const std::vector<float>& positions);

    // Get slice positions
    const std::vector<float>& getSlicePositions() const { return slicePositions; }

    // Set current playback positions (normalized 0-1)
    void setPlaybackPosition(float position); // primary (active) for compatibility
    void setPlayheadPositions(float activePosition, bool hasActive,
                              float secondaryPosition, bool hasSecondary,
                              float activeAlpha = 0.85f, float secondaryAlpha = 0.4f);
    void setVirtualPlayheadPositions(float activePosition, bool hasActive,
                                     float secondaryPosition, bool hasSecondary,
                                     float activeAlpha = 0.7f, float secondaryAlpha = 0.35f);
    void setPlaybackState(bool isPlaying);

    // Theme support
    void setTheme(const ThemePalette& theme);

    // Callback when slice marker is dragged
    std::function<void(int sliceIndex, float newPosition)> onSliceMarkerMoved;

    // Callback when a slice is clicked for preview
    std::function<void(int sliceIndex)> onSliceClicked;
    // Callback when a slice marker is double-clicked
    std::function<void(int markerIndex)> onSliceMarkerDoubleClicked;
    // Callback when a slice marker is added
    std::function<void(float position)> onSliceMarkerAdded;

private:
    const juce::AudioBuffer<float>* audioBuffer = nullptr;
    std::vector<float> slicePositions;
    float playbackPosition = 0.0f; // active head
    float secondaryPlaybackPosition = 0.0f;
    bool showPrimary = false;
    bool showSecondary = false;
    float primaryAlpha = 0.85f;
    float secondaryAlpha = 0.4f;
    float virtualPlaybackPosition = 0.0f;
    float virtualSecondaryPlaybackPosition = 0.0f;
    bool showVirtualPrimary = false;
    bool showVirtualSecondary = false;
    float virtualPrimaryAlpha = 0.7f;
    float virtualSecondaryAlpha = 0.35f;
    bool isPlaying = false;

    int draggedMarkerIndex = -1;
    int hoveredMarkerIndex = -1;

    // Middle-button panning state
    bool isMiddleButtonDragging = false;
    float middleDragStartX = 0.0f;
    float middleDragStartOffset = 0.0f;

    // Zoom and panning state
    float zoomFactor = 1.0f;
    float viewOffsetX = 0.0f;  // Normalized offset (0-1)

    // Waveform thumbnail cache
    std::vector<float> waveformCache;
    int cachedWidth = 0;

    void updateWaveformCache();
    int getMarkerAtPosition(int x) const;
    int getSliceAtPosition(int x) const;

    // Coordinate conversion helpers
    float screenXToNormalized(float screenX) const;
    float normalizedToScreenX(float normalized) const;

    static constexpr int markerHitWidth = 8;
    static constexpr float minimumMarkerSpacing = 0.0f;

    // Theme colors (default to dark mode)
    ThemePalette currentTheme = ThemePalette::darkMode();
};
