#include "VisualizerView.h"
#include "Platform.h"
#include <BinaryData.h>
#include <cmath>
namespace zv
{
namespace
{
const juce::Colour grey(0xffc4c4c4), quiet(0xff777777);
juce::String number(int n)
{
    return n < 0 ? juce::String(juce::CharPointer_UTF8("—")) : juce::String(n + 1).paddedLeft('0', 2);
}
struct Effect
{
    const char *name;
    juce::juce_wchar glyph;
    int bit;
};
constexpr Effect effects[]{{"Saturate", 0xf185, 0},   {"Shaper", 0xf8d7, 1},       {"Fuzz", 0xf6be, 2},
                           {"Bitcrush", 0xf83e, 3},   {"Time stretch", 0xf252, 4}, {"Delay", 0xf1da, 5},
                           {"Comb", 0xf55d, 6},       {"Beat repeat", 0xf2f9, 7},  {"Tighten", 0xf5cb, 8},
                           {"Expand", 0xf773, 9},     {"Pan", 0xf025, 10},         {"Scratch", 0xf51f, 11},
                           {"Filter", 0xf0b0, 12},    {"Repitch", 0xf001, 13},     {"Reverse", 0xf2ea, 14},
                           {"Tape stop", 0xf4db, 15}, {"Slow down", 0xf554, -1},   {"Speed up", 0xf70c, -1},
                           {"Retrigger", 0xf2a1, -1}};
void trace(juce::Graphics &g, const std::vector<juce::Point<float>> &points, float progress, float width)
{
    float total = 0;
    for (size_t i = 1; i < points.size(); ++i)
        total += points[i].getDistanceFrom(points[i - 1]);
    float remaining = total * juce::jlimit(0.f, 1.f, progress);
    juce::Path p;
    p.startNewSubPath(points.front());
    for (size_t i = 1; i < points.size() && remaining > 0; ++i)
    {
        const float len = points[i].getDistanceFrom(points[i - 1]);
        if (len <= 0)
            continue;
        p.lineTo(points[i - 1] + (points[i] - points[i - 1]) * std::min(1.f, remaining / len));
        remaining -= len;
    }
    g.strokePath(p, juce::PathStrokeType(width));
}
} // namespace
MonoLook::MonoLook()
{
    regular = juce::Typeface::createSystemTypefaceFor(BinaryData::IBMPlexMonoRegular_ttf,
                                                      BinaryData::IBMPlexMonoRegular_ttfSize);
    medium = juce::Typeface::createSystemTypefaceFor(BinaryData::IBMPlexMonoMedium_ttf,
                                                     BinaryData::IBMPlexMonoMedium_ttfSize);
    icons =
        juce::Typeface::createSystemTypefaceFor(BinaryData::fasolid900_ttf, BinaryData::fasolid900_ttfSize);
    setColour(juce::TextButton::textColourOffId, grey);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, juce::Colours::black);
    setColour(juce::ComboBox::textColourId, grey);
    setColour(juce::ComboBox::outlineColourId, quiet);
    setColour(juce::PopupMenu::backgroundColourId, juce::Colours::black);
    setColour(juce::PopupMenu::textColourId, grey);
    setColour(juce::ToggleButton::textColourId, grey);
    setColour(juce::ToggleButton::tickColourId, juce::Colours::white);
}
juce::Font MonoLook::font(float size, bool bold) const
{
    return juce::Font(juce::FontOptions(bold ? medium : regular).withHeight(size));
}
juce::Font MonoLook::icon(float size) const
{
    return juce::Font(juce::FontOptions(icons).withHeight(size));
}
juce::Font MonoLook::getTextButtonFont(juce::TextButton &, int height)
{
    return font(juce::jlimit(11.f, 17.f, height * 0.53f));
}
juce::Font MonoLook::getComboBoxFont(juce::ComboBox &)
{
    return font(13);
}
void MonoLook::drawButtonBackground(juce::Graphics &g, juce::Button &b, const juce::Colour &, bool over,
                                    bool down)
{
    g.fillAll(down ? juce::Colour(0xff222222) : juce::Colours::black);
    g.setColour(over ? juce::Colours::white : quiet);
    g.drawRect(b.getLocalBounds(), 1);
}
Editor::Editor(Processor &p) : AudioProcessorEditor(p), processor(p), session(p.session)
{
    // Installing the resize constrainer can invoke resized() at its minimum.
    // Capture the persisted dimensions before that callback updates the model.
    const auto initialSettings = session.settings();
    setLookAndFeel(&look);
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(480, 320, 4000, 2400);
    for (auto *b : {&folder, &midiButton, &rescan})
        addAndMakeVisible(b);
    for (auto *c : {&input, &output})
    {
        addChildComponent(c);
        c->setTextWhenNothingSelected("Select port");
    }
    inputLabel.setText("MIDI INPUT", juce::dontSendNotification);
    outputLabel.setText("MIDI OUTPUT", juce::dontSendNotification);
    for (auto *l : {&inputLabel, &outputLabel})
    {
        addChildComponent(l);
        l->setFont(look.font(11));
        l->setColour(juce::Label::textColourId, quiet);
    }
    addChildComponent(motion);
    motion.setTooltip("Disable effect traces and spectrum smoothing");
    folder.setTooltip("Choose the SD-card reference folder containing bank1-bank16");
    rescan.setTooltip("Reload changed reference samples");
    folder.onClick = [this] { chooseFolder(); };
    rescan.onClick = [this] { session.rescan(); };
    midiButton.onClick = [this]
    {
        setup = !setup;
        session.refreshPorts();
        updateControls();
        resized();
    };
    auto selected = [this]
    {
        if (updating)
            return;
        auto s = session.settings();
        s.input =
            input.getSelectedId() > 0 ? session.inputs[input.getSelectedId() - 1].identifier : juce::String();
        s.output = output.getSelectedId() > 0 ? session.outputs[output.getSelectedId() - 1].identifier
                                              : juce::String();
        session.setSettings(s, true);
    };
    input.onChange = selected;
    output.onChange = selected;
    motion.onClick = [this]
    {
        auto s = session.settings();
        s.reduceMotion = motion.getToggleState();
        session.setSettings(s, true);
    };
    setSize(initialSettings.width, initialSettings.height);
    updateControls();
    startTimerHz(60);
}
Editor::~Editor()
{
    stopTimer();
    chooser.reset();
    auto s = session.settings();
    s.width = getWidth();
    s.height = getHeight();
    session.setSettings(s, true);
    setLookAndFeel(nullptr);
}
void Editor::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser>(
        "Choose the SD-card reference folder",
        session.settings().root.isEmpty() ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                          : juce::File(session.settings().root));
    juce::Component::SafePointer<Editor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories,
                         [safe](const juce::FileChooser &fc)
                         {
                             if (!safe)
                                 return;
                             const auto result = fc.getResult();
                             if (result.isDirectory())
                             {
                                 auto s = safe->session.settings();
                                 s.root = result.getFullPathName();
                                 safe->session.setSettings(s, true);
                             }
                         });
}
void Editor::updateControls()
{
    updating = true;
    const auto s = session.settings();
    reduceMotion = s.reduceMotion || systemReducedMotion();
    juce::String key;
    for (auto &p : session.inputs)
        key += p.identifier + p.name;
    key += "/";
    for (auto &p : session.outputs)
        key += p.identifier + p.name;
    if (key != portKey)
    {
        portKey = key;
        input.clear(juce::dontSendNotification);
        output.clear(juce::dontSendNotification);
        int i = 1;
        for (auto &p : session.inputs)
            input.addItem(p.name, i++);
        i = 1;
        for (auto &p : session.outputs)
            output.addItem(p.name, i++);
    }
    auto select = [](juce::ComboBox &c, const auto &ports, const juce::String &id)
    {
        int selected = 0;
        for (int i = 0; i < ports.size(); ++i)
            if (ports[i].identifier == id)
                selected = i + 1;
        if (selected == 0)
        {
            int count = 0;
            for (int i = 0; i < ports.size(); ++i)
                if (ports[i].name.containsIgnoreCase("zeptocore"))
                {
                    selected = i + 1;
                    ++count;
                }
            if (count != 1)
                selected = 0;
        }
        c.setSelectedId(selected, juce::dontSendNotification);
    };
    select(input, session.inputs, s.input);
    select(output, session.outputs, s.output);
    const bool show =
        setup || (session.choosePorts && session.inputs.size() > 0 && session.outputs.size() > 0);
    bool relayout = input.isVisible() != show;
    input.setVisible(show);
    output.setVisible(show);
    inputLabel.setVisible(show);
    outputLabel.setVisible(show);
    motion.setVisible(show);
    motion.setToggleState(s.reduceMotion, juce::dontSendNotification);
    rescan.setEnabled(s.root.isNotEmpty() && !session.libraryState.loading);
    folder.setButtonText(s.root.isEmpty() ? "CHOOSE FOLDER" : "FOLDER");
    midiButton.setButtonText(session.connected ? "MIDI" : "CONNECT");
    folder.setTooltip(s.root.isEmpty() ? "Choose the SD-card reference folder containing bank1-bank16"
                                       : s.root);
    updating = false;
    if (relayout)
        resized();
}
void Editor::resized()
{
    const int margin = std::max(18, (int)(getWidth() * 0.04f));
    const int top = std::max(14, (int)(getHeight() * 0.04f));
    int x = getWidth() - margin;
    const int h = 28;
    midiButton.setBounds(x - 84, top, 84, h);
    x -= 94;
    rescan.setBounds(x - 76, top, 76, h);
    x -= 86;
    const int fw = session.settings().root.isEmpty() ? 124 : 76;
    folder.setBounds(x - fw, top, fw, h);
    int scopeTop = top + 48;
    if (getWidth() < 720)
    {
        folder.setTopLeftPosition(margin, top + 37);
        rescan.setTopLeftPosition(margin + fw + 10, top + 37);
        scopeTop += 36;
    }
    if (input.isVisible())
    {
        const int width = (getWidth() - 2 * margin - 16) / 2;
        inputLabel.setBounds(margin, scopeTop, width, 16);
        outputLabel.setBounds(margin + width + 16, scopeTop, width, 16);
        input.setBounds(margin, scopeTop + 18, width, 26);
        output.setBounds(margin + width + 16, scopeTop + 18, width, 26);
        motion.setBounds(margin, scopeTop + 49, 210, 22);
        scopeTop += 80;
    }
    scope = {(float)margin, (float)scopeTop, (float)(getWidth() - 2 * margin),
             (float)std::max(45, getHeight() - scopeTop - top - 66)};
    if (getWidth() >= 480 && getHeight() >= 320)
    {
        auto s = session.settings();
        if (s.width != getWidth() || s.height != getHeight())
        {
            s.width = getWidth();
            s.height = getHeight();
            session.setSettings(s);
        }
    }
}
void Editor::timerCallback()
{
    updateControls();
    animate(fixedTime.value_or(nowMs()));
    if (isShowing())
        repaint();
}
void Editor::animate(double now)
{
    auto position = session.displayFresh(now) && !session.device.display->state.stopped &&
                            !session.device.display->state.muted
                        ? session.playhead.value(now)
                        : std::nullopt;
    auto target = spectrumAt(session.wave.get(), position);
    juce::String trigger;
    if (session.device.display)
    {
        const auto &s = session.device.display->state;
        trigger = juce::String(s.bank) + ":" + juce::String(s.sample) + ":" +
                  juce::String((juce::int64)s.trigger) + ":" + juce::String((int)s.estimated);
    }
    const bool reset = trigger != lastTrigger || !position || reduceMotion;
    const double dt = juce::jlimit(0.0, 100.0, now - lastFrame);
    for (size_t i = 0; i < 32; ++i)
        displayed[i] =
            reset ? target[i]
                  : displayed[i] + (target[i] - displayed[i]) *
                                       (float)(1 - std::exp(-dt / (target[i] > displayed[i] ? 35 : 140)));
    lastFrame = now;
    lastTrigger = trigger;
    uint16_t mask = 0;
    if (session.connected && fresh(session.device.receivedAt, now) && session.device.playback &&
        session.device.playback->valid && session.wave &&
        session.device.playback->bank == session.wave->bank &&
        session.device.playback->sample == session.wave->sample)
        mask = session.device.playback->effects.value_or(0);
    for (int i = 0; i < 16; ++i)
        if ((mask & (1 << i)) && !(previousEffects & (1 << i)))
            effectStarted[(size_t)i] = now;
    previousEffects = mask;
}
void Editor::text(juce::Graphics &g, const juce::String &s, juce::Rectangle<float> r, float size,
                  juce::Colour colour, juce::Justification align, bool bold)
{
    g.setColour(colour);
    g.setFont(look.font(size, bold));
    g.drawText(s, r, align, false);
}
void Editor::paint(juce::Graphics &g)
{
    g.fillAll(juce::Colours::black);
    const double now = fixedTime.value_or(nowMs());
    const float fs = juce::jlimit(14.f, 24.f, getWidth() * 0.025f);
    const float left = scope.getX(), top = (float)std::max(14, (int)(getHeight() * 0.04f));
    text(g, "BANK " + number(session.bank) + "   SAMPLE " + number(session.sample),
         {left, top, (float)(getWidth() < 720 ? getWidth() - 140 : getWidth() - 360), 28}, fs * 0.85f, grey,
         juce::Justification::centredLeft, true);
    g.setColour(quiet);
    for (auto p : {scope.getTopLeft(), scope.getTopRight(), scope.getBottomLeft(), scope.getBottomRight()})
    {
        float dx = p.x == scope.getX() ? 12.f : -12.f, dy = p.y == scope.getY() ? 12.f : -12.f;
        g.drawLine(p.x, p.y, p.x + dx, p.y);
        g.drawLine(p.x, p.y, p.x, p.y + dy);
    }
    auto inner = scope.reduced(8);
    auto upper = inner.withHeight(scope.getHeight() * 0.7f - 16);
    drawEffects(g, upper.reduced(4), now);
    if (session.wave)
    {
        drawWave(g, upper, now);
        auto lower = inner.withTop(scope.getY() + scope.getHeight() * 0.72f);
        drawSpectrum(g, lower);
    }
    else
    {
        juce::String title = "Choose the SD-card folder",
                     message =
                         "Select the folder containing bank1, bank2, and the original WAV + .info files.";
        if (session.settings().root.isNotEmpty())
        {
            title = session.libraryState.loading ? "Preparing reference" : "No matching sample";
            message = session.libraryState.loading ? session.libraryState.current
                                                   : "Add matching WAV and .info files, then rescan.";
            if (session.libraryState.error.isNotEmpty())
            {
                title = "Reference unavailable";
                message = session.libraryState.error;
            }
            for (auto &s : session.libraryState.samples)
                if (s.bank == session.bank && s.sample == session.sample && !s.error.isEmpty())
                {
                    title = "Waveform unavailable";
                    message = s.error;
                }
            if (session.libraryState.samples.empty() && !session.libraryState.loading &&
                session.libraryState.error.isEmpty())
                message = "No bank1-bank16 original WAV files were found in this folder.";
        }
        g.setColour(juce::Colours::black.withAlpha(0.85f));
        g.fillRect(inner.withSizeKeepingCentre(inner.getWidth(), 100));
        text(g, title, inner.withSizeKeepingCentre(inner.getWidth(), 30).translated(0, -20), fs, grey,
             juce::Justification::centred, true);
        g.setFont(look.font(fs * 0.65f));
        g.setColour(quiet);
        g.drawFittedText(
            message, inner.withSizeKeepingCentre(inner.getWidth() - 20, 60).translated(0, 25).toNearestInt(),
            juce::Justification::centred, 3);
    }
    const bool active = session.displayFresh(now);
    int slice = active ? session.device.display->state.slice : -1;
    int bpm = session.wave ? session.wave->bpm : 0;
    if (session.device.playback)
        bpm = session.device.playback->bpm;
    if (session.device.legacy && (!session.device.playback || session.device.legacyAt.value_or(0) >
                                                                  session.device.receivedAt.value_or(0)))
        bpm = session.device.legacy->bpm;
    const float y = scope.getBottom() + 10;
    text(g,
         "SLICE " + number(slice) + " / " +
             (session.wave ? juce::String((int)session.wave->slices.size()) : number(-1)),
         {left, y, scope.getWidth() / 2, 26}, fs * 0.85f, grey);
    text(g, (bpm > 0 ? juce::String(bpm) : number(-1)) + " BPM",
         {left + scope.getWidth() / 2, y, scope.getWidth() / 2, 26}, fs * 0.85f, grey,
         juce::Justification::centredRight);
    juce::String status = session.connection;
    if (session.connected)
    {
        if (!fresh(session.device.receivedAt, now))
            status = session.device.legacy && fresh(session.device.legacyAt, now)
                         ? "SLICE TRACKING UNAVAILABLE / enable visualizer firmware"
                         : "Waiting for telemetry / enable visualizer firmware";
        else if (!active)
            status = "Slice tracking unavailable";
        else if (session.device.display->state.stopped)
            status = "STOPPED";
        else if (session.device.display->state.muted)
            status = "MUTED";
        else
            status = session.device.display->state.forward ? "FORWARD" : "REVERSE";
    }
    if (session.libraryState.loading)
        status = "Preparing " + juce::String(session.libraryState.completed) + " / " +
                 juce::String(session.libraryState.total) + " / " + session.libraryState.current;
    else if (session.connectionError.isNotEmpty())
        status = session.connectionError;
    else if (session.libraryState.warning.isNotEmpty())
        status = session.libraryState.warning;
    text(g, status, {left, y + 29, scope.getWidth(), 20}, juce::jlimit(10.f, 14.f, fs * 0.6f), quiet);
}
void Editor::drawEffects(juce::Graphics &g, juce::Rectangle<float> bounds, double now)
{
    const float cw = bounds.getWidth() / 4, ch = bounds.getHeight() / 5;
    const float size =
        juce::jlimit(14.f, 60.f, std::min(bounds.getWidth() * 0.09f, bounds.getHeight() * 0.12f));
    for (int i = 0; i < 19; ++i)
    {
        const auto &effect = effects[i];
        auto cell =
            juce::Rectangle<float>(bounds.getX() + (i % 4) * cw, bounds.getY() + (i / 4) * ch, cw, ch);
        const bool active = effect.bit >= 0 && (previousEffects & (1 << effect.bit));
        g.setColour(juce::Colours::white.withAlpha(active ? 1.f : 0.09f));
        g.setFont(look.icon(size));
        g.drawText(juce::String::charToString(effect.glyph), cell, juce::Justification::centred);
        if (active && !reduceMotion)
        {
            double age = now - effectStarted[(size_t)effect.bit];
            if (age >= 0 && age < 650)
            {
                const float progress = (float)(age < 182     ? age / 182
                                               : age < 357.5 ? 1
                                                             : 1 - (age - 357.5) / 292.5);
                auto r = cell.withSizeKeepingCentre(size + 10, size + 10);
                g.setColour(juce::Colours::white);
                trace(
                    g,
                    {r.getTopLeft(), r.getTopRight(), r.getBottomRight(), r.getBottomLeft(), r.getTopLeft()},
                    progress, 1);
            }
        }
    }
}
void Editor::drawWave(juce::Graphics &g, juce::Rectangle<float> bounds, double now)
{
    if (bounds.getHeight() <= 0)
        return;
    const auto &wave = *session.wave;
    if (cachedWave != session.wave || cachedBounds != bounds)
    {
        cachedWave = session.wave;
        cachedBounds = bounds;
        envelope.clear();
        int columns = juce::jlimit(1, 640, (int)bounds.getWidth());
        size_t bins = wave.peaks.front().size() / 2;
        float amplitude = std::max(1.f, bounds.getHeight() - 24) / 2, middle = bounds.getY() + 12 + amplitude;
        for (int c = 0; c < columns; ++c)
        {
            size_t first = (size_t)std::floor((double)c / columns * bins),
                   last = std::min(bins,
                                   std::max(first + 1, (size_t)std::ceil((double)(c + 1) / columns * bins)));
            int peak = 0;
            for (auto &channel : wave.peaks)
                for (size_t b = first; b < last; ++b)
                    peak = std::max({peak, std::abs((int)channel[b * 2]), std::abs((int)channel[b * 2 + 1])});
            float h = peak / 32768.f * amplitude, x = std::floor((float)c / columns * bounds.getWidth()),
                  end = std::floor((float)(c + 1) / columns * bounds.getWidth());
            envelope.addRectangle(bounds.getX() + x, middle - h, end - x, h * 2);
        }
    }
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.fillPath(envelope);
    g.setColour(juce::Colour(0xff404040));
    for (int tick = 0; tick <= 8; ++tick)
    {
        float x = bounds.getX() +
                  std::min(bounds.getWidth() - 1, std::floor(tick / 8.f * bounds.getWidth())) + 0.5f;
        g.drawLine(x, bounds.getBottom() - 4, x, bounds.getBottom() - (tick % 2 ? 7 : 10));
    }
    if (!session.displayFresh(now))
        return;
    const auto &state = session.device.display->state;
    const auto region = wave.slices[(size_t)state.slice];
    if (state.stopped || state.muted)
        return;
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(
            juce::Rectangle<float>(
                bounds.getX() + (float)(region.start / wave.duration) * bounds.getWidth(), bounds.getY(),
                (float)((region.stop - region.start) / wave.duration) * bounds.getWidth(), bounds.getHeight())
                .toNearestInt());
        g.setColour(juce::Colours::white);
        g.fillPath(envelope);
    }
    auto position = session.playhead.value(now);
    if (!position)
        return;
    const float px = bounds.getX() + (float)(*position / wave.duration) * bounds.getWidth();
    g.setColour(juce::Colours::white);
    g.drawLine(std::floor(px) + 0.5f, bounds.getY(), std::floor(px) + 0.5f, bounds.getBottom(), 3);
    juce::Path diamond;
    float dy = bounds.getBottom() - 7;
    diamond.startNewSubPath(px, dy - 4);
    diamond.lineTo(px + 3, dy);
    diamond.lineTo(px, dy + 4);
    diamond.lineTo(px - 3, dy);
    diamond.closeSubPath();
    g.fillPath(diamond);
    const auto press = session.device.press;
    if (!press || press->bank != wave.bank || press->sample != wave.sample ||
        press->slice >= (int)wave.slices.size())
        return;
    const double age = now - press->at;
    if (age < 0 || age >= 480)
        return;
    if (lastPress != press->at)
    {
        lastPress = press->at;
        juce::Random random((juce::int64)(press->at * 1000) + press->button);
        pressSide = random.nextBool() ? 1.f : -1.f;
        pressHorizontal = random.nextFloat();
        pressVertical = random.nextFloat();
    }
    const float progress = (float)(age < 140 ? age / 140 : age < 260 ? 1 : 1 - (age - 260) / 220);
    const float scale = juce::jlimit(14.f, 24.f, getWidth() * 0.025f) / 16;
    const auto slice = wave.slices[(size_t)press->slice];
    const float sliceWidth = (float)((slice.stop - slice.start) / wave.duration) * bounds.getWidth();
    const float inset = 3 * scale,
                bw = std::min(bounds.getWidth() - inset * 2,
                              std::max(40 * scale, std::min(60 * scale, std::round(sliceWidth * 1.8f)))),
                bh = std::min(bounds.getHeight() - inset * 2, 28 * scale);
    const float gap = 10 * scale + pressHorizontal * std::min(60 * scale, bounds.getWidth() * 0.15f);
    float side = pressSide;
    if (side < 0 && px - bounds.getX() < bw + gap + inset)
        side = 1;
    else if (side > 0 && px + bw + gap > bounds.getRight() - inset)
        side = -1;
    const float left = juce::jlimit(bounds.getX() + inset, bounds.getRight() - bw - inset,
                                    px + (side > 0 ? gap : -bw - gap));
    const float y = bounds.getY() + inset +
                    pressVertical * std::max(0.f, bounds.getHeight() - bh - inset * 2),
                join = side > 0 ? left : left + bw, far = side > 0 ? left + bw : left;
    trace(g,
          {{std::floor(px) + 0.5f, bounds.getCentreY()},
           {join, y + bh / 2},
           {join, y},
           {far, y},
           {far, y + bh},
           {join, y + bh},
           {join, y + bh / 2}},
          progress, std::max(1.f, scale));
    if (progress >= 1)
        text(g, juce::String(press->button).paddedLeft('0', 2), {left, y, bw, bh}, 14 * scale,
             juce::Colours::white, juce::Justification::centred);
}
void Editor::drawSpectrum(juce::Graphics &g, juce::Rectangle<float> r)
{
    if (r.getHeight() < 8)
        return;
    const float font = juce::jlimit(8.f, 12.f, getWidth() * 0.0105f);
    auto axis = r.removeFromBottom(14);
    text(g, "50 Hz", axis, font, quiet);
    text(g, "1 kHz", axis, font, quiet, juce::Justification::centred);
    text(g, "16 kHz", axis, font, quiet, juce::Justification::centredRight);
    r.removeFromBottom(4);
    float step = r.getWidth() / 32, gap = juce::jlimit(2.f, 5.f, r.getWidth() / 160);
    for (size_t b = 0; b < 32; ++b)
    {
        float h = std::max(1.f, displayed[b] * std::max(0.f, r.getHeight() - 2));
        g.setColour(juce::Colours::white.withAlpha(0.55f * juce::jlimit(0.f, 1.f, displayed[b])));
        g.fillRect(r.getX() + std::floor((float)b * step), r.getBottom() - h,
                   std::max(1.f, std::floor(step - gap)), h);
    }
}
} // namespace zv
