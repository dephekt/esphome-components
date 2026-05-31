#include "switch.h"
#include "esphome/core/log.h"
#include "ezo_types.h"

namespace esphome {
namespace ezo_types {

static const char *const TAG = "ezo_types.switch";

void ExtendedScaleSwitch::setup() {
  // Sync the displayed state from the circuit on startup (read-only, no write-back).
  if (this->orp_sensor_ != nullptr) {
    this->orp_sensor_->request_extended_scale_query();
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
