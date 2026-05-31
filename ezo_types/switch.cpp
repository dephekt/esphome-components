#include "switch.h"
#include "esphome/core/log.h"
#include "ezo_types.h"

namespace esphome {
namespace ezo_types {

static const char *const TAG = "ezo_types.switch";

void DataloggerSwitch::dump_config() {
  LOG_SWITCH("", "Datalogger", this);
  ESP_LOGCONFIG(TAG, "  Interval: %d s", this->interval_);
}

void DataloggerSwitch::write_state(bool state) {
  if (this->rtd_sensor_) {
    this->rtd_sensor_->set_datalogger(state, this->interval_);
    this->publish_state(state);
    ESP_LOGI(TAG, "Datalogger %s (interval %d s)", state ? "ENABLED" : "DISABLED", this->interval_);
  }
}

void ExtendedScaleSwitch::dump_config() { LOG_SWITCH("", "Extended Scale", this); }

void ExtendedScaleSwitch::write_state(bool state) {
  if (this->orp_sensor_) {
    this->orp_sensor_->set_extended_scale(state);
    this->publish_state(state);
    ESP_LOGI(TAG, "Extended scale %s", state ? "ENABLED" : "DISABLED");
  }
}

}  // namespace ezo_types
}  // namespace esphome
