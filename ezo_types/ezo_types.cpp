#include "ezo_types.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace ezo_types {

static const char *const TAG = "ezo_types";

// PHSensor Implementation
void PHSensor::setup() {
  EZOSensor::setup();
  this->add_custom_callback([this](std::string response) { this->handle_custom_response_(response); });
}

void PHSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "pH EZO Sensor:");
  LOG_SENSOR("", "pH", this);
  if (this->voltage_sensor_) {
    LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  }
  if (this->reset_reason_sensor_) {
    LOG_TEXT_SENSOR("", "Reset Reason", this->reset_reason_sensor_);
  }
}

void PHSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[PH] Custom response: '%s'", response.c_str());

  // Handle ?STATUS,<reason>,<voltage>
  const std::string status_prefix = "?STATUS,";
  if (response.rfind(status_prefix, 0) == 0) {
    this->parse_status_response_(response);
    return;
  }

  // Handle calibration status response "?CAL,n"
  const std::string cal_prefix = "?CAL,";
  if (response.rfind(cal_prefix, 0) == 0) {
    this->parse_calibration_response_(response);
    return;
  }
}

void PHSensor::parse_status_response_(const std::string &response) {
  const std::string status_prefix = "?STATUS,";
  std::string payload = response.substr(status_prefix.size());
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

    ESP_LOGI(TAG, "[PH] Last Reset: %s | Vcc: %.3f V", reason.c_str(), voltage);
  } else {
    ESP_LOGW(TAG, "[PH] Malformed status response: %s", response.c_str());
  }
}

void PHSensor::parse_calibration_response_(const std::string &response) {
  const std::string cal_prefix = "?CAL,";
  std::string cal_state_str = response.substr(cal_prefix.size());
  int cal_state = parse_number<int>(cal_state_str).value_or(0);
  ESP_LOGI(TAG, "[PH] Calibration state: %d", cal_state);
}

// ECSensor Implementation
void ECSensor::setup() {
  EZOSensor::setup();
  this->add_custom_callback([this](std::string response) { this->handle_custom_response_(response); });
}

void ECSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "EC EZO Sensor:");
  LOG_SENSOR("", "EC", this);
  if (this->voltage_sensor_) {
    LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  }
  if (this->reset_reason_sensor_) {
    LOG_TEXT_SENSOR("", "Reset Reason", this->reset_reason_sensor_);
  }
  if (this->tds_sensor_) {
    LOG_SENSOR("", "TDS", this->tds_sensor_);
  }
  if (this->salinity_sensor_) {
    LOG_SENSOR("", "Salinity", this->salinity_sensor_);
  }
  if (this->relative_density_sensor_) {
    LOG_SENSOR("", "Relative Density", this->relative_density_sensor_);
  }
}

void ECSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[EC] Custom response: '%s'", response.c_str());

  // Handle Cell Constant query response "?K,n.n"
  const std::string k_prefix = "?K,";
  if (response.rfind(k_prefix, 0) == 0) {
    this->parse_cell_constant_response_(response);
    return;
  }

  // Handle ?STATUS,<reason>,<voltage>
  const std::string status_prefix = "?STATUS,";
  if (response.rfind(status_prefix, 0) == 0) {
    this->parse_status_response_(response);
    return;
  }

  // Handle calibration status response "?CAL,n"
  const std::string cal_prefix = "?CAL,";
  if (response.rfind(cal_prefix, 0) == 0) {
    this->parse_calibration_response_(response);
    return;
  }

  // Handle output parameters response "?O,EC,TDS"
  const std::string output_prefix = "?O,";
  if (response.rfind(output_prefix, 0) == 0) {
    this->parse_output_params_response_(response);
    return;
  }

  // Handle multi-value read response
  if (response.find(',') != std::string::npos || (response.find('.') != std::string::npos && response != "1") ||
      (response.find('.') == std::string::npos && response.length() > 0 && isdigit(response[0]))) {
    this->parse_multi_value_response_(response);
    return;
  }
}

void ECSensor::parse_status_response_(const std::string &response) {
  const std::string status_prefix = "?STATUS,";
  std::string payload = response.substr(status_prefix.size());
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

    ESP_LOGI(TAG, "[EC] Last Reset: %s | Vcc: %.3f V", reason.c_str(), voltage);
  } else {
    ESP_LOGW(TAG, "[EC] Malformed status response: %s", response.c_str());
  }
}

void ECSensor::parse_calibration_response_(const std::string &response) {
  const std::string cal_prefix = "?CAL,";
  std::string cal_state_str = response.substr(cal_prefix.size());
  int cal_state = parse_number<int>(cal_state_str).value_or(0);
  ESP_LOGI(TAG, "[EC] Calibration state: %d", cal_state);
}

void ECSensor::parse_cell_constant_response_(const std::string &response) {
  const std::string k_prefix = "?K,";
  std::string cell_constant_str = response.substr(k_prefix.size());
  float cell_constant = parse_number<float>(cell_constant_str).value_or(1.0f);

  this->cell_constant_callback_.call(cell_constant);
  ESP_LOGI(TAG, "[EC] Cell Constant: %.2f", cell_constant);
}

void ECSensor::parse_output_params_response_(const std::string &response) {
  const std::string output_prefix = "?O,";
  std::string params_str = response.substr(output_prefix.size());

  // Parse enabled parameters
  bool ec_enabled = params_str.find("EC") != std::string::npos;
  bool tds_enabled = params_str.find("TDS") != std::string::npos;
  bool rd_enabled = params_str.find("SG") != std::string::npos;

  // Check for standalone "S" parameter (salinity)
  bool s_enabled = (params_str.find(",S,") != std::string::npos) ||
                   (params_str.length() > 1 && params_str.substr(params_str.length() - 2) == ",S");

  this->output_params_callback_.call(ec_enabled, tds_enabled, s_enabled, rd_enabled);

  ESP_LOGI(TAG, "[EC] Output parameters - EC:%s TDS:%s S:%s RD:%s", ec_enabled ? "ON" : "OFF",
           tds_enabled ? "ON" : "OFF", s_enabled ? "ON" : "OFF", rd_enabled ? "ON" : "OFF");
}

void ECSensor::parse_multi_value_response_(const std::string &response) {
  // Split by comma
  std::vector<std::string> values;
  std::string remaining = response;
  size_t pos = 0;
  while ((pos = remaining.find(',')) != std::string::npos) {
    values.push_back(remaining.substr(0, pos));
    remaining = remaining.substr(pos + 1);
  }
  if (!remaining.empty()) {
    values.push_back(remaining);
  }

  ESP_LOGI(TAG, "[EC] Parsing %d values (ignoring EC): tds=%s s=%s rd=%s", values.size(),
           output_tds_enabled_ ? "ON" : "OFF", output_salinity_enabled_ ? "ON" : "OFF",
           output_rd_enabled_ ? "ON" : "OFF");

  // Skip EC value if present, parse remaining values in order: TDS, S, RD
  int idx = output_ec_enabled_ ? 1 : 0;

  if (output_tds_enabled_ && idx < values.size() && this->tds_sensor_) {
    float tds_val = parse_number<float>(values[idx]).value_or(0.0f);
    this->tds_sensor_->publish_state(tds_val);
    ESP_LOGI(TAG, "[EC] TDS: %.0f ppm", tds_val);
    idx++;
  }

  if (output_salinity_enabled_ && idx < values.size() && this->salinity_sensor_) {
    float s_val = parse_number<float>(values[idx]).value_or(0.0f);
    this->salinity_sensor_->publish_state(s_val);
    ESP_LOGI(TAG, "[EC] Salinity: %.2f ppt", s_val);
    idx++;
  }

  if (output_rd_enabled_ && idx < values.size() && this->relative_density_sensor_) {
    float rd_val = parse_number<float>(values[idx]).value_or(0.0f);
    this->relative_density_sensor_->publish_state(rd_val);
    ESP_LOGI(TAG, "[EC] Relative Density: %.3f", rd_val);
    idx++;
  }
}

// RTDSensor Implementation
void RTDSensor::setup() {
  EZOSensor::setup();
  this->add_custom_callback([this](std::string response) { this->handle_custom_response_(response); });
}

void RTDSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "RTD EZO Sensor:");
  LOG_SENSOR("", "RTD", this);
  if (this->voltage_sensor_) {
    LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  }
  if (this->reset_reason_sensor_) {
    LOG_TEXT_SENSOR("", "Reset Reason", this->reset_reason_sensor_);
  }
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

  // Handle ?STATUS,<reason>,<voltage>
  const std::string status_prefix = "?STATUS,";
  if (response.rfind(status_prefix, 0) == 0) {
    this->parse_status_response_(response);
    return;
  }

  // Handle calibration status response "?CAL,n"
  const std::string cal_prefix = "?CAL,";
  if (response.rfind(cal_prefix, 0) == 0) {
    this->parse_calibration_response_(response);
    return;
  }

  // Handle datalogger interval response "?D,n"
  const std::string dl_prefix = "?D,";
  if (response.rfind(dl_prefix, 0) == 0) {
    this->parse_datalogger_response_(response);
    return;
  }
}

void RTDSensor::parse_status_response_(const std::string &response) {
  const std::string status_prefix = "?STATUS,";
  std::string payload = response.substr(status_prefix.size());
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

    ESP_LOGI(TAG, "[RTD] Last Reset: %s | Vcc: %.3f V", reason.c_str(), voltage);
  } else {
    ESP_LOGW(TAG, "[RTD] Malformed status response: %s", response.c_str());
  }
}

void RTDSensor::parse_calibration_response_(const std::string &response) {
  const std::string cal_prefix = "?CAL,";
  std::string cal_state_str = response.substr(cal_prefix.size());
  int cal_state = parse_number<int>(cal_state_str).value_or(0);
  ESP_LOGI(TAG, "[RTD] Calibration state: %d", cal_state);
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

// ORPSensor Implementation
void ORPSensor::setup() {
  EZOSensor::setup();
  this->add_custom_callback([this](std::string response) { this->handle_custom_response_(response); });
}

void ORPSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "ORP EZO Sensor:");
  LOG_SENSOR("", "ORP", this);
  if (this->voltage_sensor_) {
    LOG_SENSOR("", "Voltage", this->voltage_sensor_);
  }
  if (this->reset_reason_sensor_) {
    LOG_TEXT_SENSOR("", "Reset Reason", this->reset_reason_sensor_);
  }
}

void ORPSensor::handle_custom_response_(const std::string &response) {
  ESP_LOGI(TAG, "[ORP] Custom response: '%s'", response.c_str());

  // Handle ?STATUS,<reason>,<voltage>
  const std::string status_prefix = "?STATUS,";
  if (response.rfind(status_prefix, 0) == 0) {
    this->parse_status_response_(response);
    return;
  }

  // Handle calibration status response "?CAL,n"
  const std::string cal_prefix = "?CAL,";
  if (response.rfind(cal_prefix, 0) == 0) {
    this->parse_calibration_response_(response);
    return;
  }

  // Handle extended-scale response "?ORPEXT,n"
  const std::string ext_prefix = "?ORPEXT,";
  if (response.rfind(ext_prefix, 0) == 0) {
    this->parse_extended_scale_response_(response);
    return;
  }
}

void ORPSensor::parse_status_response_(const std::string &response) {
  const std::string status_prefix = "?STATUS,";
  std::string payload = response.substr(status_prefix.size());
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

    ESP_LOGI(TAG, "[ORP] Last Reset: %s | Vcc: %.3f V", reason.c_str(), voltage);
  } else {
    ESP_LOGW(TAG, "[ORP] Malformed status response: %s", response.c_str());
  }
}

void ORPSensor::parse_calibration_response_(const std::string &response) {
  const std::string cal_prefix = "?CAL,";
  std::string cal_state_str = response.substr(cal_prefix.size());
  int cal_state = parse_number<int>(cal_state_str).value_or(0);
  ESP_LOGI(TAG, "[ORP] Calibration state: %d", cal_state);
}

void ORPSensor::parse_extended_scale_response_(const std::string &response) {
  const std::string ext_prefix = "?ORPEXT,";
  std::string state_str = response.substr(ext_prefix.size());
  int state = parse_number<int>(state_str).value_or(0);

  bool enabled = (state == 1);
  this->extended_scale_callback_.call(enabled);

  ESP_LOGI(TAG, "[ORP] Extended scale %s", enabled ? "ENABLED" : "DISABLED");
}

}  // namespace ezo_types
}  // namespace esphome