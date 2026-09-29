#pragma once
#include <juce_core/juce_core.h>
#include <cstdio>
#include <exception>
#include <mutex>
#include <sstream>
#include <thread>

namespace core::diagnostics {
inline bool enabled() {
  static const bool active =
      juce::SystemStats::getEnvironmentVariable("CORE_MANAGER_DEBUG", "0") == "1";
  return active;
}

// Call from control/worker threads only, never the audio or MIDI callbacks.
inline void log(const char *area, const juce::String &message) {
  if (!enabled())
    return;
  static std::mutex mutex;
  std::lock_guard<std::mutex> lock(mutex);
  std::ostringstream thread;
  thread << std::this_thread::get_id();
  auto line = juce::Time::getCurrentTime().toISO8601(true) + " [" +
              juce::String(thread.str()) + "] [" + area + "] " +
              message.replace("\r", "\\r").replace("\n", "\\n") + "\n";
  std::fputs(line.toRawUTF8(), stderr);
  std::fflush(stderr);
}

class Scope {
public:
  Scope(const char *category, juce::String description)
      : area(category), name(std::move(description)),
        started(juce::Time::getMillisecondCounterHiRes()),
        exceptions(std::uncaught_exceptions()) {
    log(area, "BEGIN " + name);
  }
  ~Scope() {
    log(area, (std::uncaught_exceptions() > exceptions ? "FAILED " : "END ") +
                  name + " elapsed_ms=" +
                  juce::String(juce::Time::getMillisecondCounterHiRes() - started, 1));
  }

private:
  const char *area;
  juce::String name;
  double started;
  int exceptions;
};

class JuceLogger final : public juce::Logger {
  void logMessage(const juce::String &message) override { log("JUCE", message); }
};
} // namespace core::diagnostics
