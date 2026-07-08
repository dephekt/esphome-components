#include "timezone_text.h"

#include <cstring>

#include "esphome/core/log.h"
// NOTE: parse_posix_tz is upstream bridge code scheduled for removal before
// ESPHome 2026.9.0 (esphome/backlog#91). Validation here must migrate to
// whatever replaces it before tracking core past that release.
#include "esphome/components/time/posix_tz.h"

namespace esphome {
namespace timezone {

static const char *const TAG = "timezone.text";

void TimezoneText::capture_build_default_() {
  if (this->default_captured_)
    return;
  this->build_default_tz_ = time::get_global_tz();
  this->default_captured_ = true;
}

void TimezoneText::ensure_pref_() {
  if (this->pref_ready_)
    return;
  this->pref_ = this->make_entity_preference<TzBuffer>();
  this->pref_ready_ = true;
}

void TimezoneText::persist_(const TzBuffer &buf) {
  if (!this->pref_.save(&buf)) {
    ESP_LOGW(TAG, "Failed to persist timezone override; it will not survive a reboot");
    return;
  }
  global_preferences->sync();
}

void TimezoneText::setup() {
  this->capture_build_default_();
  this->ensure_pref_();
  // A time platform may re-assert its own timezone when it syncs — the
  // homeassistant platform without an explicit timezone: does so on every
  // GetTimeResponse, right after this callback fires. Re-apply the override
  // on the next loop so it wins.
  this->rtc_->add_on_time_sync_callback([this]() {
    this->defer("tz_reapply", [this]() {
      if (!this->state.empty())
        this->rtc_->set_timezone(this->state);
    });
  });
  if (this->wrote_before_setup_) {
    // A pre-setup control() write already applied and persisted a value;
    // don't clobber it with stale flash contents.
    this->setup_done_ = true;
    return;
  }
  TzBuffer stored{};
  if (this->pref_.load(&stored) && stored.value[0] != '\0') {
    stored.value[TZ_BUFFER_SIZE - 1] = '\0';
    time::ParsedTimezone parsed{};
    if (time::parse_posix_tz(stored.value, parsed)) {
      this->rtc_->set_timezone(stored.value);
      this->publish_state(stored.value);
      ESP_LOGI(TAG, "Restored timezone override '%s'", stored.value);
      this->setup_done_ = true;
      return;
    }
    ESP_LOGW(TAG, "Clearing invalid persisted timezone '%s'", stored.value);
    TzBuffer cleared{};
    this->persist_(cleared);
  }
  // No (valid) override stored — the build-time default stays in effect.
  this->publish_state("");
  this->setup_done_ = true;
}

void TimezoneText::control(const std::string &value) {
  this->capture_build_default_();
  this->ensure_pref_();
  if (this->has_state() && value == this->state) {
    // No-op write from a reconciling controller: acknowledge without
    // re-parsing, re-applying, or touching flash.
    this->publish_state(value);
    return;
  }
  if (value.empty()) {
    TzBuffer cleared{};
    this->persist_(cleared);
    // Restore the snapshot taken before any override was applied, so the
    // build-time default takes effect now rather than at the next reboot.
    time::set_global_tz(this->build_default_tz_);
    this->publish_state("");
    ESP_LOGI(TAG, "Cleared timezone override; build-time default restored");
    if (!this->setup_done_)
      this->wrote_before_setup_ = true;
    return;
  }
  // Everything downstream (parser, set_timezone, persistence) is C-string
  // based and stops at the first NUL, but publish_state() would carry the
  // full byte string — reject embedded NULs so published state can never
  // contain bytes that were not validated and applied.
  if (value.size() >= TZ_BUFFER_SIZE || value.find('\0') != std::string::npos) {
    ESP_LOGW(TAG, "Rejecting timezone: must be at most %u chars with no embedded NUL",
             (unsigned) (TZ_BUFFER_SIZE - 1));
    this->publish_state(this->state);
    return;
  }
  time::ParsedTimezone parsed{};
  if (!time::parse_posix_tz(value.c_str(), parsed)) {
    ESP_LOGW(TAG, "Rejecting invalid POSIX TZ string '%s'", value.c_str());
    this->publish_state(this->state);
    return;
  }
  // The parser is lenient about trailing garbage: a typo like
  // "CST6CDT M3.2.0,M11.1.0/2" (space instead of comma) parses as fixed CST6
  // with the DST rules silently dropped. A comma only ever introduces DST
  // rules, so comma-present-but-no-DST means part of the value was ignored.
  if (!parsed.has_dst() && value.find(',') != std::string::npos) {
    ESP_LOGW(TAG, "Rejecting POSIX TZ string '%s': DST rules were not understood", value.c_str());
    this->publish_state(this->state);
    return;
  }
  this->rtc_->set_timezone(value);
  TzBuffer stored{};
  std::memcpy(stored.value, value.c_str(), value.size());
  this->persist_(stored);
  this->publish_state(value);
  ESP_LOGI(TAG, "Applied timezone override '%s'", value.c_str());
  if (!this->setup_done_)
    this->wrote_before_setup_ = true;
}

void TimezoneText::dump_config() {
  ESP_LOGCONFIG(TAG, "Timezone Text '%s'", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Override: %s",
                this->state.empty() ? "(none - build default active)" : this->state.c_str());
}

}  // namespace timezone
}  // namespace esphome
