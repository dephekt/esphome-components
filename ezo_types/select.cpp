#include "select.h"
#include "esphome/core/log.h"
#include "ezo_types.h"

namespace esphome {
namespace ezo_types {

static const char *const TAG = "ezo_types.select";

void CellConstantSelect::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Cell Constant Select...");
  // Request initial cell constant value from EC sensor
  if (this->ec_sensor_) {
    this->ec_sensor_->request_cell_constant_query();
  }
}

void CellConstantSelect::dump_config() {
  LOG_SELECT("", "Cell Constant", this);
  ESP_LOGCONFIG(TAG, "  Options: 0.1, 1.0, 10.0");
}

void CellConstantSelect::set_ec_sensor(ECSensor *ec_sensor) { this->ec_sensor_ = ec_sensor; }

void CellConstantSelect::control(const std::string &value) {
  if (this->ec_sensor_) {
    this->ec_sensor_->set_cell_constant(value);
    this->publish_state(value);
    ESP_LOGI(TAG, "Setting cell constant to %s", value.c_str());
  }
}

}  // namespace ezo_types
}  // namespace esphome
