#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/text/text.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/time/posix_tz.h"

namespace esphome {
namespace timezone {

// POSIX TZ strings with explicit rules ("CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00")
// top out well under this; anything longer is rejected, not truncated.
static const size_t TZ_BUFFER_SIZE = 64;

struct TzBuffer {
  char value[TZ_BUFFER_SIZE];
};

/// A text entity holding a runtime POSIX-TZ override for a time component.
///
/// Empty state means no override: the timezone baked into the firmware at
/// build time stays in effect. A non-empty value is validated with the
/// platform's POSIX TZ parser, applied to the time component, and persisted
/// so it survives reboots. Invalid input is rejected and the previous state
/// re-published, so an observer (MQTT) can see the write did not take.
class TimezoneText : public text::Text, public Component {
 public:
  void setup() override;
  void dump_config() override;
  // The build-time timezone is applied by statements in generated main before
  // any component's setup() runs, so any priority sees it; LATE is only a
  // defensive choice in case upstream ever moves that application into a time
  // platform's own setup().
  float get_setup_priority() const override { return setup_priority::LATE; }
  void set_time(time::RealTimeClock *rtc) { this->rtc_ = rtc; }

 protected:
  void control(const std::string &value) override;
  // Snapshot the build-time timezone once, before any override is applied,
  // so clearing the override can restore it immediately instead of waiting
  // for a reboot.
  void capture_build_default_();
  // control() can run before setup() (e.g. text.set in an on_boot
  // automation); create the preference lazily so such a write persists.
  void ensure_pref_();
  // Queue the save and flush it to storage now — save() alone only stages the
  // write in RAM until the periodic preferences sync, so an acknowledged
  // write could be lost to a power cut.
  void persist_(const TzBuffer &buf);

  time::RealTimeClock *rtc_{nullptr};
  ESPPreferenceObject pref_{};
  time::ParsedTimezone build_default_tz_{};
  bool default_captured_{false};
  bool pref_ready_{false};
  bool setup_done_{false};
  // Set when a control() write lands before setup(); setup() must not
  // clobber that write with stale flash contents.
  bool wrote_before_setup_{false};
};

}  // namespace timezone
}  // namespace esphome
