#include "ezo_types.h"
#include "select.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ezo_types {

static const char *const TAG = "ezo_types";

static const char *const STATUS_PREFIX = "?STATUS,";
static const char *const CAL_PREFIX = "?CAL,";

void EZOSensor::setup() {
  ezo::EZOSensor::setup();
  this->add_custom_callback([this](std::string response) { this->handle_custom_response_(response); });
  this->add_device_infomation_callback(
      [this](std::string response) { this->parse_device_information_response_(response); });

  // Only send initial commands if circuit is powered
  if (this->is_circuit_powered_()) {
    this->send_custom("STATUS");
    this->get_device_information();
    this->request_calibration_status();
  }
}

void EZOSensor::update() {
  // Only send commands if circuit is powered
  if (!this->is_circuit_powered_()) {
    return;
  }

  // Implement the parent's read command logic with power control
  // This replaces ezo::EZOSensor::update() to prevent power control bypass

  // Check if a read is in there already and if not insert one in the second position
  if (!this->commands_.empty() && this->commands_.front()->command_type != ezo::EzoCommandType::EZO_READ &&
      this->commands_.size() > 1) {
    bool found = false;

    for (auto &i : this->commands_) {
      if (i->command_type == ezo::EzoCommandType::EZO_READ) {
        found = true;
        break;
      }
    }

    if (!found) {
      std::unique_ptr<ezo::EzoCommand> ezo_command(new ezo::EzoCommand);
      ezo_command->command = "R";
      ezo_command->command_type = ezo::EzoCommandType::EZO_READ;
      ezo_command->delay_ms = 900;

      auto it = this->commands_.begin();
      ++it;
      this->commands_.insert(it, std::move(ezo_command));
    }

    return;
  }

  this->send_compensated_read_();

  // Send STATUS command periodically to update voltage and reset reason (only if sensors configured)
  if (this->voltage_sensor_ || this->reset_reason_sensor_) {
    this->send_custom("STATUS");
  }

  // Send device info query occasionally
  static uint8_t device_info_counter = 0;
  if (++device_info_counter >= 10) {
    device_info_counter = 0;
    this->get_device_information();
  }

  // Update diagnostic sensors
  this->update_diagnostic_sensors_();
}

void EZOSensor::loop() {
  // Check for calibration mode changes
  if (this->calibration_mode_switch_ != nullptr) {
    bool current_cal_mode_state = this->calibration_mode_switch_->state;
    if (current_cal_mode_state && !this->last_calibration_mode_state_) {
      // Calibration mode was just turned ON
      ESP_LOGI(TAG, "Calibration mode ON. Clearing queue and setting temperature compensation to 25.0°C");
      this->commands_.clear();
      this->add_command_("T,25.0", ezo::EzoCommandType::EZO_T, 300);
    }
    this->last_calibration_mode_state_ = current_cal_mode_state;
  }

  // Check for power state changes and clear queue if powered off
  bool current_power_state = this->is_circuit_powered_();
  if (current_power_state != this->last_power_state_) {
    if (!current_power_state) {
      // Power turned OFF - clear the command queue
      ESP_LOGI(TAG, "Circuit powered off, clearing %zu commands from queue", this->commands_.size());
      this->commands_.clear();
      this->initialization_pending_ = false;
    } else {
      // Power turned ON - start delay timer
      ESP_LOGI(TAG, "Circuit powered on, starting 2-second warm-up delay");
      this->power_on_time_ = millis();
      this->initialization_pending_ = true;
    }
    this->last_power_state_ = current_power_state;
  }

  // Only process commands if circuit is powered
  if (!current_power_state) {
    this->update_diagnostic_sensors_();
    return;
  }

  // Check if we're in the power-on delay period
  if (this->initialization_pending_) {
    uint32_t time_since_power_on = millis() - this->power_on_time_;
    if (time_since_power_on < POWER_ON_DELAY_MS) {
      // Still in delay period - don't process commands
      this->update_diagnostic_sensors_();
      return;
    } else {
      // Delay period finished - initialize and resume normal operation
      ESP_LOGI(TAG, "Warm-up complete, initializing circuit");
      this->initialization_pending_ = false;
      this->reinitialize();
    }
  }

  // Call parent loop to process commands and track completions
  size_t commands_before = this->commands_.size();
  std::string current_command_before = "";
  if (!this->commands_.empty()) {
    current_command_before = this->commands_.front()->command;
  }

  ezo::EZOSensor::loop();

  // Check if a command was completed (queue size decreased and it was the same command)
  if (commands_before > 0 && this->commands_.size() < commands_before && !current_command_before.empty()) {
    this->last_completed_command_ = current_command_before;
  }

  // Update diagnostic sensors after processing
  this->update_diagnostic_sensors_();
}

void EZOSensor::dump_config_base_(const char *sensor_type) {
  ESP_LOGCONFIG(TAG, "%s EZO Sensor:", sensor_type);
  LOG_SENSOR("", sensor_type, this);
  if (this->voltage_sensor_) {
    LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  }
  if (this->reset_reason_sensor_) {
    LOG_TEXT_SENSOR("", "Reset Reason", this->reset_reason_sensor_);
  }
  if (this->firmware_version_sensor_) {
    LOG_TEXT_SENSOR("", "Firmware Version", this->firmware_version_sensor_);
  }
  if (this->calibration_status_sensor_) {
    LOG_TEXT_SENSOR("", "Calibration Status", this->calibration_status_sensor_);
  }
}

void EZOSensor::handle_common_responses_(const std::string &response) {
  if (response.rfind(STATUS_PREFIX, 0) == 0) {
    this->parse_common_status_response_(response);
    return;
  }

  if (response.rfind(CAL_PREFIX, 0) == 0) {
    this->parse_common_calibration_response_(response);
    return;
  }
}

void EZOSensor::parse_common_status_response_(const std::string &response) {
  std::string payload = response.substr(strlen(STATUS_PREFIX));
  size_t delim = payload.find(',');

  if (delim != std::string::npos) {
    std::string code = payload.substr(0, delim);
    std::string voltage_str = payload.substr(delim + 1);
    float voltage = parse_number<float>(voltage_str).value_or(0.0f);

    std::string reason;
    if (code == "P")
      reason = "Powered off";
    else if (code == "S")
      reason = "Software reset";
    else if (code == "B")
      reason = "Brown out";
    else if (code == "W")
      reason = "Watchdog";
    else if (code == "U")
      reason = "Unknown";
    else
      reason = "Unrecognized";

    if (this->reset_reason_sensor_) {
      this->reset_reason_sensor_->publish_state(reason);
    }
    if (this->voltage_sensor_) {
      this->voltage_sensor_->publish_state(voltage);
    }

    ESP_LOGI(TAG, "Last Reset: %s | Vcc: %.3f V", reason.c_str(), voltage);
  } else {
    ESP_LOGW(TAG, "Malformed status response: %s", response.c_str());
  }
}

void EZOSensor::parse_common_calibration_response_(const std::string &response) {
  std::string cal_state_str = response.substr(strlen(CAL_PREFIX));
  int cal_state = parse_number<int>(cal_state_str).value_or(0);
  ESP_LOGI(TAG, "Calibration state: %d", cal_state);
}

void EZOSensor::parse_device_information_response_(const std::string &response) {
  ESP_LOGI(TAG, "Raw device info: %s", response.c_str());

  if (this->firmware_version_sensor_) {
    // Parse device info format: "DEVICE,VERSION"
    size_t delim = response.find(',');
    if (delim != std::string::npos) {
      std::string device_type = response.substr(0, delim);
      std::string firmware_version = response.substr(delim + 1);

      this->firmware_version_sensor_->publish_state(firmware_version);
      ESP_LOGI(TAG, "Device: %s, Firmware: %s", device_type.c_str(), firmware_version.c_str());
    } else {
      // Fallback: if no comma found, publish the whole response
      this->firmware_version_sensor_->publish_state(response);
      ESP_LOGW(TAG, "Unexpected device info format: %s", response.c_str());
    }
  }
}

void EZOSensor::factory_reset() { this->send_custom("Factory"); }

void PHSensor::setup() {
  EZOSensor::setup();

  // Add slope callback to parse and update individual sensors
  this->add_slope_callback([this](std::string slope_str) { this->parse_slope_response_(slope_str); });

  // Also query slope on setup to populate sensors faster
  if (this->is_circuit_powered_() &&
      (this->acid_slope_quality_sensor_ || this->alkaline_slope_quality_sensor_ || this->asymmetry_potential_sensor_)) {
    this->get_slope();
  }
}

void PHSensor::update() {
  // Abort if circuit is not powered
  if (!this->is_circuit_powered_()) {
    return;
  }

  // Call parent update first
  EZOSensor::update();

  // Send temperature compensation query if sensor is configured
  if (this->temperature_compensation_sensor_) {
    this->send_custom("T,?");
  }

  // Query slope periodically (every 10th update) if slope sensors are configured
  static uint8_t slope_counter = 0;
  if ((this->acid_slope_quality_sensor_ || this->alkaline_slope_quality_sensor_ || this->asymmetry_potential_sensor_) &&
      ++slope_counter >= 10) {
    slope_counter = 0;
    this->get_slope();
  }

  // Query calibration status periodically (every 15th update) if sensor is configured
  static uint8_t cal_status_counter = 0;
  if (++cal_status_counter >= 15) {
    cal_status_counter = 0;
    this->request_calibration_status();
  }
}

void PHSensor::dump_config() {
  this->dump_config_base_("pH");
  if (this->acid_slope_quality_sensor_) {
    LOG_SENSOR("", "Acid Slope Quality", this->acid_slope_quality_sensor_);
  }
  if (this->alkaline_slope_quality_sensor_) {
    LOG_SENSOR("", "Alkaline Slope Quality", this->alkaline_slope_quality_sensor_);
  }
  if (this->asymmetry_potential_sensor_) {
    LOG_SENSOR("", "Asymmetry Potential", this->asymmetry_potential_sensor_);
  }
}

void PHSensor::reinitialize() {
  // Call parent reinitialize first
  EZOSensor::reinitialize();

  // Also query slope immediately after power-on if slope sensors are configured
  if (this->is_circuit_powered_() &&
      (this->acid_slope_quality_sensor_ || this->alkaline_slope_quality_sensor_ || this->asymmetry_potential_sensor_)) {
    this->get_slope();
    ESP_LOGI(TAG, "[PH] Requesting slope data after power-on");
  }
}

void PHSensor::parse_common_calibration_response_(const std::string &response) {
  std::string cal_state_str = response.substr(strlen("?CAL,"));
  int cal_state = parse_number<int>(cal_state_str).value_or(0);

  std::string status_text;
  switch (cal_state) {
    case 0:
      status_text = "Factory";
      break;
    case 1:
      status_text = "One Point";
      break;
    case 2:
      status_text = "Two Point";
      break;
    case 3:
      status_text = "Three Point";
      break;
    default:
      status_text = "Unknown";
      break;
  }

  if (this->calibration_status_sensor_) {
    this->calibration_status_sensor_->publish_state(status_text);
  }
  ESP_LOGI(TAG, "[PH] Calibration Status: %s (%d)", status_text.c_str(), cal_state);
}

void PHSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[PH] Custom response: '%s'", response.c_str());

  // Handle temperature compensation response "?T,19.5"
  const std::string temp_prefix = "?T,";
  if (response.rfind(temp_prefix, 0) == 0) {
    this->parse_temperature_compensation_response_(response);
    return;
  }

  // Try common responses first
  this->handle_common_responses_(response);

  // Let the base EZO class handle other responses (like slope, calibration, etc.)
  // This ensures we don't block the built-in EZO functionality
}

void PHSensor::parse_temperature_compensation_response_(const std::string &response) {
  const std::string temp_prefix = "?T,";
  std::string temp_str = response.substr(temp_prefix.size());
  float temperature = parse_number<float>(temp_str).value_or(0.0f);

  if (this->temperature_compensation_sensor_) {
    this->temperature_compensation_sensor_->publish_state(temperature);
  }
  ESP_LOGI(TAG, "[PH] Temperature Compensation: %.2f°C", temperature);
}

void PHSensor::parse_slope_response_(const std::string &response) {
  // Parse slope response format: "acid_slope,alkaline_slope,asymmetry_potential"
  // Example: "99.7,100.3,-0.89"

  std::vector<std::string> values;
  std::string current = response;
  size_t pos = 0;

  // Split by comma
  while ((pos = current.find(',')) != std::string::npos) {
    values.push_back(current.substr(0, pos));
    current.erase(0, pos + 1);
  }
  values.push_back(current);  // Last value

  if (values.size() >= 3) {
    float acid_slope = parse_number<float>(values[0]).value_or(0.0f);
    float alkaline_slope = parse_number<float>(values[1]).value_or(0.0f);
    float asymmetry_potential = parse_number<float>(values[2]).value_or(0.0f);

    if (this->acid_slope_quality_sensor_) {
      this->acid_slope_quality_sensor_->publish_state(acid_slope);
    }
    if (this->alkaline_slope_quality_sensor_) {
      this->alkaline_slope_quality_sensor_->publish_state(alkaline_slope);
    }
    if (this->asymmetry_potential_sensor_) {
      this->asymmetry_potential_sensor_->publish_state(asymmetry_potential);
    }

    ESP_LOGI(TAG, "[PH] Slope Quality - Acid: %.1f%%, Alkaline: %.1f%%, Asymmetry: %.2f mV", acid_slope, alkaline_slope,
             asymmetry_potential);
  } else {
    ESP_LOGW(TAG, "[PH] Invalid slope response format: %s", response.c_str());
  }
}

void ECSensor::setup() {
  EZOSensor::setup();  // Call parent setup - handles all standard EZO functionality

  // Enable/disable outputs to match configured sub-sensors so the R CSV order is deterministic.
  // EC is always enabled; TDS/SAL/SG mirror whether the sub-sensor is wired.
  if (this->is_circuit_powered_()) {
    this->add_command_("O,EC,1", ezo::EzoCommandType::EZO_CUSTOM, 300);
    this->add_command_(this->tds_sensor_ ? "O,TDS,1" : "O,TDS,0", ezo::EzoCommandType::EZO_CUSTOM, 300);
    this->add_command_(this->salinity_sensor_ ? "O,S,1" : "O,S,0", ezo::EzoCommandType::EZO_CUSTOM, 300);
    this->add_command_(this->relative_density_sensor_ ? "O,SG,1" : "O,SG,0", ezo::EzoCommandType::EZO_CUSTOM, 300);
  }
  // Cell constant query will be requested by the select component's setup() method
}

void ECSensor::update() {
  // Call parent update first
  EZOSensor::update();

  // Also query the internal temperature compensation value if a sensor is configured
  if (this->temperature_compensation_sensor_) {
    this->send_custom("T,?");
  }

  // Query cell constant periodically (every 10th update) if select is configured
  static uint8_t cell_constant_counter = 0;
  if (this->cell_constant_select_ && ++cell_constant_counter >= 10) {
    cell_constant_counter = 0;
    this->request_cell_constant_query();
  }

  // Query calibration status periodically (every 15th update) if sensor is configured
  static uint8_t cal_status_counter = 0;
  if (++cal_status_counter >= 15) {
    cal_status_counter = 0;
    this->request_calibration_status();
  }
}

void ECSensor::dump_config() {
  this->dump_config_base_("EC");
  if (this->temperature_compensation_sensor_) {
    LOG_SENSOR("", "Temperature Compensation", this->temperature_compensation_sensor_);
  }
  if (this->cell_constant_select_) {
    LOG_SELECT("", "Cell Constant", this->cell_constant_select_);
  }
}

void ECSensor::set_calibration_point_dry() { this->add_command_("Cal,dry", ezo::EzoCommandType::EZO_CUSTOM, 600); }

void ECSensor::set_cell_constant(const std::string &value) {
  char command[16];
  snprintf(command, sizeof(command), "K,%s", value.c_str());
  this->add_command_(command, ezo::EzoCommandType::EZO_CUSTOM, 300);
}

void ECSensor::request_cell_constant_query() {
  if (this->is_circuit_powered_()) {
    this->send_custom("K,?");
    ESP_LOGD(TAG, "[EC] Requesting cell constant");
  }
}

void ECSensor::request_tds_query() {
  if (this->is_circuit_powered_()) {
    this->send_custom("TDS,?");
    ESP_LOGD(TAG, "[EC] Requesting TDS conversion factor");
  }
}

void ECSensor::set_tds_conversion_factor(float factor) {
  std::string cmd = "TDS," + to_string(factor);
  this->add_command_(cmd.c_str(), ezo::EzoCommandType::EZO_CUSTOM, 300);
}

void ECSensor::loop() {
  // When sub-sensors are configured we need the full CSV from the R response before
  // ezo::EZOSensor::loop() truncates it at the first comma.  Intercept EZO_READ commands
  // that are ready to be read and parse the raw I2C bytes ourselves.
  if ((this->tds_sensor_ || this->salinity_sensor_ || this->relative_density_sensor_) &&
      !this->commands_.empty()) {
    auto *front = this->commands_.front().get();
    if (front->command_type == ezo::EzoCommandType::EZO_READ && front->command_sent &&
        (millis() - this->start_time_ >= front->delay_ms)) {
      uint8_t buf[64];
      buf[0] = 0;
      if (!this->read_bytes_raw(buf, sizeof(buf))) {
        ESP_LOGE(TAG, "[EC] read error");
        this->commands_.pop_front();
        return;
      }
      switch (buf[0]) {
        case 1:
          break;
        case 2:
          ESP_LOGE(TAG, "[EC] device returned a syntax error");
          this->commands_.pop_front();
          return;
        case 254:
          return;  // keep waiting
        case 255:
          ESP_LOGE(TAG, "[EC] device returned no data");
          this->commands_.pop_front();
          return;
        default:
          ESP_LOGE(TAG, "[EC] device returned unknown response: %d", buf[0]);
          this->commands_.pop_front();
          return;
      }
      std::string payload = reinterpret_cast<char *>(&buf[1]);
      this->commands_.pop_front();
      // Log completion so the diagnostic tracking in the parent loop() picks it up
      this->last_completed_command_ = "R";
      this->parse_reading_csv_(payload);
      // Continue with the rest of the queue via the parent (not calling ezo:: directly again)
      ezo_types::EZOSensor::loop();
      return;
    }
  }
  // Default path: delegate to the ezo_types base loop (power/warm-up logic + ezo base loop)
  ezo_types::EZOSensor::loop();
}

void ECSensor::parse_reading_csv_(const std::string &response) {
  // The R response CSV has the enabled outputs in FIXED order: EC, TDS, SAL, SG.
  // setup() enabled exactly the outputs that have a configured sub-sensor, so the
  // fields present here map 1:1 to: EC (always), then TDS if tds_sensor_, etc.
  std::vector<std::string> fields;
  std::string remainder = response;
  size_t pos;
  while ((pos = remainder.find(',')) != std::string::npos) {
    fields.push_back(remainder.substr(0, pos));
    remainder.erase(0, pos + 1);
  }
  fields.push_back(remainder);

  // Field 0 is always EC
  if (!fields.empty()) {
    float ec_val = parse_number<float>(fields[0]).value_or(NAN);
    this->publish_state(ec_val);
    ESP_LOGI(TAG, "[EC] EC: %.2f µS/cm", ec_val);
  }

  // Remaining fields fill TDS → SAL → SG in order, only for enabled sub-sensors
  size_t field_idx = 1;
  if (this->tds_sensor_ && field_idx < fields.size()) {
    float tds_val = parse_number<float>(fields[field_idx++]).value_or(NAN);
    this->tds_sensor_->publish_state(tds_val);
    ESP_LOGI(TAG, "[EC] TDS: %.0f ppm", tds_val);
  }
  if (this->salinity_sensor_ && field_idx < fields.size()) {
    float sal_val = parse_number<float>(fields[field_idx++]).value_or(NAN);
    this->salinity_sensor_->publish_state(sal_val);
    ESP_LOGI(TAG, "[EC] Salinity: %.2f PSU", sal_val);
  }
  if (this->relative_density_sensor_ && field_idx < fields.size()) {
    float sg_val = parse_number<float>(fields[field_idx++]).value_or(NAN);
    this->relative_density_sensor_->publish_state(sg_val);
    ESP_LOGI(TAG, "[EC] Specific Gravity: %.3f", sg_val);
  }
}

void ECSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[EC] Custom response: '%s'", response.c_str());

  // Handle temperature compensation response "?T,19.5"
  const std::string temp_prefix = "?T,";
  if (response.rfind(temp_prefix, 0) == 0) {
    this->parse_temperature_compensation_response_(response);
    return;
  }

  // Handle TDS conversion factor query response "?TDS,0.54"
  const std::string tds_prefix = "?TDS,";
  if (response.rfind(tds_prefix, 0) == 0) {
    this->parse_tds_response_(response);
    return;
  }

  // Check for STATUS response first - don't process further if it's a STATUS
  if (response.rfind("?STATUS,", 0) == 0) {
    this->handle_common_responses_(response);
    return;
  }

  // Check for CAL response - don't process further if it's a CAL
  if (response.rfind("?CAL,", 0) == 0) {
    this->handle_common_responses_(response);
    return;
  }

  // Handle Cell Constant query response "?K,n.n"
  const std::string k_prefix = "?K,";
  if (response.rfind(k_prefix, 0) == 0) {
    this->parse_cell_constant_response_(response);
    return;
  }

  // Try common responses for any other standard EZO responses
  this->handle_common_responses_(response);
}

void ECSensor::parse_temperature_compensation_response_(const std::string &response) {
  const std::string temp_prefix = "?T,";
  std::string temp_str = response.substr(temp_prefix.size());
  float temperature = parse_number<float>(temp_str).value_or(0.0f);

  if (this->temperature_compensation_sensor_) {
    this->temperature_compensation_sensor_->publish_state(temperature);
  }
  ESP_LOGI(TAG, "[EC] Temperature Compensation: %.2f°C", temperature);
}

void ECSensor::parse_common_calibration_response_(const std::string &response) {
  std::string cal_state_str = response.substr(strlen("?CAL,"));
  int cal_state = parse_number<int>(cal_state_str).value_or(0);

  std::string status_text;
  switch (cal_state) {
    case 0:
      status_text = "Factory";
      break;
    case 1:
      status_text = "Two Point";
      break;
    case 2:
      status_text = "Three Point";
      break;
    default:
      status_text = "Unknown";
      break;
  }

  if (this->calibration_status_sensor_) {
    this->calibration_status_sensor_->publish_state(status_text);
  }
  ESP_LOGI(TAG, "[EC] Calibration Status: %s (%d)", status_text.c_str(), cal_state);
}

void ECSensor::parse_cell_constant_response_(const std::string &response) {
  const std::string k_prefix = "?K,";
  std::string cell_constant_str = response.substr(k_prefix.size());

  // Normalize the cell constant to match select options format
  float cell_constant = parse_number<float>(cell_constant_str).value_or(0.0f);
  std::string normalized_value;
  if (std::abs(cell_constant - 0.1f) < 0.01f) {
    normalized_value = "0.1";
  } else if (std::abs(cell_constant - 1.0f) < 0.01f) {
    normalized_value = "1.0";
  } else if (std::abs(cell_constant - 10.0f) < 0.01f) {
    normalized_value = "10.0";
  } else {
    ESP_LOGW(TAG, "[EC] Unknown cell constant value: %s", cell_constant_str.c_str());
    return;
  }

  if (this->cell_constant_select_) {
    this->cell_constant_select_->publish_state(normalized_value);
  }
  ESP_LOGI(TAG, "[EC] Cell Constant: %s (normalized to %s)", cell_constant_str.c_str(), normalized_value.c_str());
}

void ECSensor::parse_tds_response_(const std::string &response) {
  const std::string tds_prefix = "?TDS,";
  std::string factor_str = response.substr(tds_prefix.size());
  float factor = parse_number<float>(factor_str).value_or(0.0f);
  // Display-only sync: reflect the circuit's stored factor without re-sending it.
  if (this->tds_conversion_factor_number_) {
    this->tds_conversion_factor_number_->publish_state(factor);
  }
  ESP_LOGI(TAG, "[EC] TDS Conversion Factor: %.2f", factor);
}

void RTDSensor::setup() {
  // Call parent setup first
  EZOSensor::setup();

  // Register callback to intercept temperature readings and check for changes
  this->add_on_state_callback([this](float state) { this->check_temperature_change_(state); });
}

void RTDSensor::update() {
  // Call parent update first
  EZOSensor::update();

  // Query calibration status periodically (every 15th update) if sensor is configured
  static uint8_t cal_status_counter = 0;
  if (++cal_status_counter >= 15) {
    cal_status_counter = 0;
    this->request_calibration_status();
  }
}

void RTDSensor::dump_config() { this->dump_config_base_("RTD"); }

void RTDSensor::check_temperature_change_(float new_temp) {
  // Skip if new temperature is invalid
  if (std::isnan(new_temp)) {
    return;
  }

  // If this is the first reading, store it and trigger callback
  if (std::isnan(this->last_known_temperature_)) {
    this->last_known_temperature_ = new_temp;
    this->temperature_change_callback_.call(new_temp);
    ESP_LOGI(TAG, "[RTD] Initial temperature compensation: %.2f°C", new_temp);
    return;
  }

  // Check if temperature changed by at least the threshold
  float temp_diff = std::abs(new_temp - this->last_known_temperature_);
  if (temp_diff >= TEMP_CHANGE_THRESHOLD) {
    this->last_known_temperature_ = new_temp;
    this->temperature_change_callback_.call(new_temp);
    ESP_LOGI(TAG, "[RTD] Temperature compensation update: %.2f°C (change: %.3f°C)", new_temp, temp_diff);
  }
}

void RTDSensor::parse_common_calibration_response_(const std::string &response) {
  std::string cal_state_str = response.substr(strlen("?CAL,"));
  int cal_state = parse_number<int>(cal_state_str).value_or(0);

  std::string status_text;
  switch (cal_state) {
    case 0:
      status_text = "Factory";
      break;
    case 1:
      status_text = "User Calibrated";
      break;
    default:
      status_text = "Unknown";
      break;
  }

  if (this->calibration_status_sensor_) {
    this->calibration_status_sensor_->publish_state(status_text);
  }
  ESP_LOGI(TAG, "[RTD] Calibration Status: %s (%d)", status_text.c_str(), cal_state);
}

void RTDSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[RTD] Custom response: '%s'", response.c_str());

  // Check if it's a temperature scale response
  std::string upper = response;
  std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
  if (upper == "C" || upper == "F" || upper == "K") {
    this->parse_temp_scale_response_(upper);
    return;
  }

  // Handle datalogger interval response "?D,n"
  const std::string dl_prefix = "?D,";
  if (response.rfind(dl_prefix, 0) == 0) {
    this->parse_datalogger_response_(response);
    return;
  }

  // Try common class responses if nothing else returned
  this->handle_common_responses_(response);
}

void RTDSensor::parse_temp_scale_response_(const std::string &response) {
  this->temp_scale_callback_.call(response);
  ESP_LOGI(TAG, "[RTD] Temperature scale: %s", response.c_str());
}

void RTDSensor::parse_datalogger_response_(const std::string &response) {
  const std::string dl_prefix = "?D,";
  std::string interval_str = response.substr(dl_prefix.size());
  int interval = parse_number<int>(interval_str).value_or(0);

  bool enabled = (interval != 0);
  this->datalogger_callback_.call(enabled, interval);

  ESP_LOGI(TAG, "[RTD] Datalogger %s (interval %d s)", enabled ? "ENABLED" : "DISABLED", interval);
}

void RTDSensor::set_datalogger(bool enabled, int interval) {
  std::string cmd = enabled ? "D," + to_string(interval) : std::string("D,0");
  this->add_command_(cmd.c_str(), ezo::EzoCommandType::EZO_CUSTOM, 300);
}

void ORPSensor::update() {
  // Call parent update first
  EZOSensor::update();

  // Query calibration status periodically (every 15th update) if sensor is configured
  static uint8_t cal_status_counter = 0;
  if (++cal_status_counter >= 15) {
    cal_status_counter = 0;
    this->request_calibration_status();
  }
}

void ORPSensor::dump_config() { this->dump_config_base_("ORP"); }

void ORPSensor::parse_common_calibration_response_(const std::string &response) {
  std::string cal_state_str = response.substr(strlen("?CAL,"));
  int cal_state = parse_number<int>(cal_state_str).value_or(0);

  std::string status_text;
  switch (cal_state) {
    case 0:
      status_text = "Factory";
      break;
    case 1:
      status_text = "User Calibrated";
      break;
    default:
      status_text = "Unknown";
      break;
  }

  if (this->calibration_status_sensor_) {
    this->calibration_status_sensor_->publish_state(status_text);
  }
  ESP_LOGI(TAG, "[ORP] Calibration Status: %s (%d)", status_text.c_str(), cal_state);
}

void ORPSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[ORP] Custom response: '%s'", response.c_str());

  // Handle extended-scale response "?ORPEXT,n"
  const std::string ext_prefix = "?ORPEXT,";
  if (response.rfind(ext_prefix, 0) == 0) {
    this->parse_extended_scale_response_(response);
    return;
  }

  // Try common responses
  this->handle_common_responses_(response);
}

void ORPSensor::parse_extended_scale_response_(const std::string &response) {
  const std::string ext_prefix = "?ORPEXT,";
  std::string state_str = response.substr(ext_prefix.size());
  int state = parse_number<int>(state_str).value_or(0);

  bool enabled = (state == 1);
  this->extended_scale_callback_.call(enabled);

  ESP_LOGI(TAG, "[ORP] Extended scale %s", enabled ? "ENABLED" : "DISABLED");
}

void ORPSensor::set_extended_scale(bool enabled) {
  this->add_command_(enabled ? "ORPext,1" : "ORPext,0", ezo::EzoCommandType::EZO_CUSTOM, 300);
}

void TDSConversionFactorNumber::setup() {
  // Sync the displayed factor from the circuit on startup (read-only, no write-back).
  if (this->ec_sensor_ != nullptr) {
    this->ec_sensor_->request_tds_query();
  }
}

void TDSConversionFactorNumber::set_ec_sensor(ECSensor *ec_sensor) { ec_sensor_ = ec_sensor; }

void TDSConversionFactorNumber::control(float value) {
  if (ec_sensor_) {
    ec_sensor_->set_tds_conversion_factor(value);
    this->publish_state(value);
    ESP_LOGI(TAG, "[TDS CF] Set conversion factor to %.2f", value);
  }
}

bool EZOSensor::is_circuit_powered_() const {
  if (this->power_control_switch_ != nullptr) {
    return this->power_control_switch_->state;
  }
  // If no power control switch is configured, assume powered
  return true;
}

void EZOSensor::reinitialize() {
  if (this->is_circuit_powered_()) {
    this->send_custom("STATUS");
    this->get_device_information();
    this->request_calibration_status();
  }
}

void EZOSensor::request_calibration_status() {
  if (this->is_circuit_powered_() && this->calibration_status_sensor_) {
    this->add_command_("CAL,?", ezo::EzoCommandType::EZO_CUSTOM, 300);
    ESP_LOGD(TAG, "Requesting calibration status");
  }
}

void EZOSensor::send_compensated_read_() {
  std::string command = "R";  // Default to a normal read

  bool calibration_mode = this->calibration_mode_switch_ != nullptr && this->calibration_mode_switch_->state;
  bool temp_comp_enabled = this->temp_compensation_switch_ != nullptr && this->temp_compensation_switch_->state;

  if (calibration_mode) {
    // During calibration, we only send standard "R" commands.
    // The temperature is set once when calibration mode is enabled.
    command = "R";
  } else if (temp_comp_enabled && this->rtd_sensor_ != nullptr &&
             !std::isnan(this->rtd_sensor_->last_known_temperature_)) {
    // If temp comp is on and we have a valid temperature, use it
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.2f", this->rtd_sensor_->last_known_temperature_);
    command = "RT," + std::string(temp_str);
  }

  // Add the command to the queue
  this->add_command_(command.c_str(), ezo::EzoCommandType::EZO_READ, 900);
}

void EZOSensor::update_diagnostic_sensors_() {
  // Update queue size (only if changed)
  if (this->queue_size_sensor_) {
    size_t current_queue_size = this->commands_.size();
    if (current_queue_size != this->last_queue_size_) {
      this->queue_size_sensor_->publish_state(current_queue_size);
      this->last_queue_size_ = current_queue_size;
    }
  }

  // Update current command (only if changed)
  if (this->current_command_sensor_) {
    std::string current_cmd_info;

    // Check if we're in warm-up period
    if (this->initialization_pending_ && this->is_circuit_powered_()) {
      uint32_t time_since_power_on = millis() - this->power_on_time_;
      uint32_t remaining_ms = POWER_ON_DELAY_MS - time_since_power_on;
      current_cmd_info = "warming up (" + std::to_string(remaining_ms / 1000 + 1) + "s)";
    } else if (this->commands_.empty()) {
      current_cmd_info = "(empty)";
    } else {
      auto &current_cmd = this->commands_.front();
      current_cmd_info = current_cmd->command + " (";
      switch (current_cmd->command_type) {
        case ezo::EzoCommandType::EZO_READ:
          current_cmd_info += "READ";
          break;
        case ezo::EzoCommandType::EZO_CUSTOM:
          current_cmd_info += "CUSTOM";
          break;
        case ezo::EzoCommandType::EZO_CALIBRATION:
          current_cmd_info += "CAL";
          break;
        case ezo::EzoCommandType::EZO_T:
          current_cmd_info += "TEMP";
          break;
        case ezo::EzoCommandType::EZO_LED:
          current_cmd_info += "LED";
          break;
        case ezo::EzoCommandType::EZO_DEVICE_INFORMATION:
          current_cmd_info += "INFO";
          break;
        case ezo::EzoCommandType::EZO_SLOPE:
          current_cmd_info += "SLOPE";
          break;
        case ezo::EzoCommandType::EZO_SLEEP:
          current_cmd_info += "SLEEP";
          break;
        case ezo::EzoCommandType::EZO_I2C:
          current_cmd_info += "I2C";
          break;
        default:
          current_cmd_info += "UNKNOWN";
          break;
      }
      current_cmd_info += ")";
    }
    if (current_cmd_info != this->last_current_command_) {
      this->current_command_sensor_->publish_state(current_cmd_info);
      this->last_current_command_ = current_cmd_info;
    }
  }

  // Update next command (only if changed)
  if (this->next_command_sensor_) {
    std::string next_cmd_info;
    if (this->commands_.size() <= 1) {
      next_cmd_info = "(none)";
    } else {
      auto it = this->commands_.begin();
      ++it;
      auto &next_cmd = *it;
      next_cmd_info = next_cmd->command + " (";
      switch (next_cmd->command_type) {
        case ezo::EzoCommandType::EZO_READ:
          next_cmd_info += "READ";
          break;
        case ezo::EzoCommandType::EZO_CUSTOM:
          next_cmd_info += "CUSTOM";
          break;
        case ezo::EzoCommandType::EZO_CALIBRATION:
          next_cmd_info += "CAL";
          break;
        case ezo::EzoCommandType::EZO_T:
          next_cmd_info += "TEMP";
          break;
        case ezo::EzoCommandType::EZO_LED:
          next_cmd_info += "LED";
          break;
        case ezo::EzoCommandType::EZO_DEVICE_INFORMATION:
          next_cmd_info += "INFO";
          break;
        case ezo::EzoCommandType::EZO_SLOPE:
          next_cmd_info += "SLOPE";
          break;
        case ezo::EzoCommandType::EZO_SLEEP:
          next_cmd_info += "SLEEP";
          break;
        case ezo::EzoCommandType::EZO_I2C:
          next_cmd_info += "I2C";
          break;
        default:
          next_cmd_info += "UNKNOWN";
          break;
      }
      next_cmd_info += ")";
    }
    if (next_cmd_info != this->last_next_command_) {
      this->next_command_sensor_->publish_state(next_cmd_info);
      this->last_next_command_ = next_cmd_info;
    }
  }

  // Update last command (only if changed)
  if (this->last_command_sensor_) {
    std::string last_cmd_info = this->last_completed_command_.empty() ? "(none)" : this->last_completed_command_;
    if (last_cmd_info != this->last_last_command_) {
      this->last_command_sensor_->publish_state(last_cmd_info);
      this->last_last_command_ = last_cmd_info;
    }
  }
}

}  // namespace ezo_types
}  // namespace esphome
