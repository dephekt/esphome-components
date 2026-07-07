// Atlas Scientific EZO command/state machine, vendored for ezo_types.
// See ezo_base.h for provenance and the reason this exists.
//
// Per the ESPHome License, the C++ sources (.c/.cpp/.h/.hpp/.tcc/.ino) of
// this component are licensed under the GNU General Public License v3. See
// the LICENSE file in this directory for the full text.
//
// Copyright (c) 2019 ESPHome

#include "ezo_base.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <cmath>

namespace esphome {
namespace ezo_types {

static const char *const TAG = "ezo_types.base";

void EZOSensorBase::update() {
  // Check if a read is in there already and if not insert one in the second position

  if (!this->commands_.empty() && this->commands_.front()->command_type != EzoCommandType::EZO_READ &&
      this->commands_.size() > 1) {
    bool found = false;

    for (auto &i : this->commands_) {
      if (i->command_type == EzoCommandType::EZO_READ) {
        found = true;
        break;
      }
    }

    if (!found) {
      auto ezo_command = make_unique<EzoCommand>();
      ezo_command->command = "R";
      ezo_command->command_type = EzoCommandType::EZO_READ;
      ezo_command->delay_ms = 900;

      auto it = this->commands_.begin();
      ++it;
      this->commands_.insert(it, std::move(ezo_command));
    }

    return;
  }

  this->get_state();
}

void EZOSensorBase::loop() {
  if (this->commands_.empty()) {
    return;
  }

  EzoCommand *to_run = this->commands_.front().get();

  if (!to_run->command_sent) {
    const uint8_t *data = reinterpret_cast<const uint8_t *>(to_run->command.c_str());
    ESP_LOGVV(TAG, "Sending command \"%s\"", data);

    // A failed write means the device never got the command; arming the read
    // timer would just burn delay+grace on a doomed transaction.
    if (this->write(data, to_run->command.length()) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "write failed for command \"%s\"; dropping", to_run->command.c_str());
      this->finish_command_(false);
      return;
    }

    if (to_run->command_type == EzoCommandType::EZO_SLEEP ||
        to_run->command_type == EzoCommandType::EZO_I2C) {  // Commands with no return data
      bool update_address = to_run->command_type == EzoCommandType::EZO_I2C;
      this->commands_.pop_front();
      if (update_address)
        this->address_ = this->new_address_;
      return;
    }

    this->start_time_ = millis();
    to_run->command_sent = true;
    return;
  }

  if (millis() - this->start_time_ < to_run->delay_ms)
    return;

  // One extra byte guarantees NUL termination even if the device fills all 32.
  uint8_t buf[33];

  buf[0] = 0;
  buf[sizeof(buf) - 1] = 0;

  if (!this->read_bytes_raw(buf, 32)) {
    ESP_LOGE(TAG, "read error");
    this->finish_command_(false);
    return;
  }

  switch (buf[0]) {
    case 1:
      break;
    case 2:
      ESP_LOGE(TAG, "device returned a syntax error for \"%s\"", to_run->command.c_str());
      this->finish_command_(false);
      return;
    case 254:
      // Still processing. Atlas's own Ezo_I2c_lib fails the cycle after a single
      // attempt; we allow a short bounded grace past the processing delay, then
      // drop the command. NEVER wait unboundedly: a firmware-locked circuit
      // answers 254 forever and would pin the queue head permanently (the stock
      // esphome ezo bug this vendored copy exists to fix).
      if (millis() - this->start_time_ > (uint32_t) to_run->delay_ms + COMMAND_TIMEOUT_MS) {
        ESP_LOGW(TAG, "command \"%s\" still processing %ums after its %ums delay; dropping",
                 to_run->command.c_str(), COMMAND_TIMEOUT_MS, to_run->delay_ms);
        this->finish_command_(false);
      }
      return;
    case 255:
      ESP_LOGE(TAG, "device returned no data for \"%s\"", to_run->command.c_str());
      this->finish_command_(false);
      return;
    default:
      // Handle unknown codes explicitly as failures so a misbehaving device can
      // never leave stale state behind (the vendor lib's default-less switch bug).
      ESP_LOGE(TAG, "device returned an unknown response: %d", buf[0]);
      this->finish_command_(false);
      return;
  }

  ESP_LOGV(TAG, "Received buffer \"%s\" for command \"%s\"", &buf[1], to_run->command.c_str());

  std::string payload = reinterpret_cast<char *>(&buf[1]);
  if (!payload.empty()) {
    auto start_location = payload.find(',');
    switch (to_run->command_type) {
      case EzoCommandType::EZO_READ:
        this->handle_read_payload_(payload);
        break;
      case EzoCommandType::EZO_LED:
        this->led_callback_.call(payload.back() == '1');
        break;
      case EzoCommandType::EZO_DEVICE_INFORMATION:
        if (start_location != std::string::npos) {
          this->device_infomation_callback_.call(payload.substr(start_location + 1));
        }
        break;
      case EzoCommandType::EZO_SLOPE:
        if (start_location != std::string::npos) {
          this->slope_callback_.call(payload.substr(start_location + 1));
        }
        break;
      case EzoCommandType::EZO_CALIBRATION:
        if (start_location != std::string::npos) {
          this->calibration_callback_.call(payload.substr(start_location + 1));
        }
        break;
      case EzoCommandType::EZO_T:
        if (start_location != std::string::npos) {
          this->t_callback_.call(payload.substr(start_location + 1));
        }
        break;
      case EzoCommandType::EZO_CUSTOM:
        this->custom_callback_.call(payload);
        break;
      default:
        break;
    }
  }

  this->finish_command_(true);
}

void EZOSensorBase::finish_command_(bool success) {
  EzoCommand *cmd = this->commands_.front().get();
  if (cmd->command_type == EzoCommandType::EZO_READ) {
    if (success) {
      this->consecutive_read_failures_ = 0;
    } else if (this->consecutive_read_failures_ < UINT8_MAX) {
      this->consecutive_read_failures_++;
      if (this->consecutive_read_failures_ >= MAX_READ_FAILURES) {
        ESP_LOGW(TAG, "%u consecutive failed reads; marking value unavailable", this->consecutive_read_failures_);
        this->publish_read_failure_();
      }
    }
  }
  this->commands_.pop_front();
}

void EZOSensorBase::handle_read_payload_(const std::string &payload_in) {
  // Some sensors return multiple comma-separated values; keep only the first.
  std::string payload = payload_in;
  auto start_location = payload.find(',');
  if (start_location != std::string::npos) {
    payload.erase(start_location);
  }
  auto val = parse_number<float>(payload);
  if (!val.has_value()) {
    ESP_LOGW(TAG, "Can't convert '%s' to number!", payload.c_str());
    return;
  }
  this->publish_state(*val);
}

void EZOSensorBase::publish_read_failure_() { this->publish_state(NAN); }

void EZOSensorBase::add_command_(const char *command, EzoCommandType command_type, uint16_t delay_ms) {
  auto ezo_command = make_unique<EzoCommand>();
  ezo_command->command = command;
  ezo_command->command_type = command_type;
  ezo_command->delay_ms = delay_ms;
  this->commands_.push_back(std::move(ezo_command));
}

void EZOSensorBase::set_calibration_point_(EzoCalibrationType type, float value) {
  static const char *const EZO_CALIBRATION_TYPE_STRINGS[] = {"LOW", "MID", "HIGH"};
  // max 21: "Cal,"(4) + type(4) + ","(1) + float(11) + null; use 24 for safety
  char payload[24];
  snprintf(payload, sizeof(payload), "Cal,%s,%0.2f", EZO_CALIBRATION_TYPE_STRINGS[type], value);
  this->add_command_(payload, EzoCommandType::EZO_CALIBRATION, 900);
}

void EZOSensorBase::set_address(uint8_t address) {
  if (address > 0 && address < 128) {
    // max 8: "I2C,"(4) + uint8(3) + null
    char payload[8];
    snprintf(payload, sizeof(payload), "I2C,%u", address);
    this->new_address_ = address;
    this->add_command_(payload, EzoCommandType::EZO_I2C);
  } else {
    ESP_LOGE(TAG, "Invalid I2C address");
  }
}

void EZOSensorBase::get_device_information() { this->add_command_("i", EzoCommandType::EZO_DEVICE_INFORMATION); }

void EZOSensorBase::set_sleep() { this->add_command_("Sleep", EzoCommandType::EZO_SLEEP); }

void EZOSensorBase::get_state() { this->add_command_("R", EzoCommandType::EZO_READ, 900); }

void EZOSensorBase::get_slope() { this->add_command_("Slope,?", EzoCommandType::EZO_SLOPE); }

void EZOSensorBase::get_t() { this->add_command_("T,?", EzoCommandType::EZO_T); }

void EZOSensorBase::set_t(float value) {
  // max 14 bytes: "T,"(2) + float with "%0.2f" (up to 11 chars) + null(1); use 16 for alignment
  char payload[16];
  snprintf(payload, sizeof(payload), "T,%0.2f", value);
  this->add_command_(payload, EzoCommandType::EZO_T);
}

void EZOSensorBase::set_tempcomp_value(float temp) { this->set_t(temp); }

void EZOSensorBase::get_calibration() { this->add_command_("Cal,?", EzoCommandType::EZO_CALIBRATION); }

void EZOSensorBase::set_calibration_point_low(float value) {
  this->set_calibration_point_(EzoCalibrationType::EZO_CAL_LOW, value);
}

void EZOSensorBase::set_calibration_point_mid(float value) {
  this->set_calibration_point_(EzoCalibrationType::EZO_CAL_MID, value);
}

void EZOSensorBase::set_calibration_point_high(float value) {
  this->set_calibration_point_(EzoCalibrationType::EZO_CAL_HIGH, value);
}

void EZOSensorBase::set_calibration_generic(float value) {
  // exact 16 bytes: "Cal," (4) + float with "%0.2f" (up to 11 chars, e.g. "-9999999.99") + null (1) = 16
  char payload[16];
  snprintf(payload, sizeof(payload), "Cal,%0.2f", value);
  this->add_command_(payload, EzoCommandType::EZO_CALIBRATION, 900);
}

void EZOSensorBase::clear_calibration() { this->add_command_("Cal,clear", EzoCommandType::EZO_CALIBRATION); }

void EZOSensorBase::get_led_state() { this->add_command_("L,?", EzoCommandType::EZO_LED); }

void EZOSensorBase::set_led_state(bool on) { this->add_command_(on ? "L,1" : "L,0", EzoCommandType::EZO_LED); }

void EZOSensorBase::send_custom(const std::string &to_send) {
  this->add_command_(to_send.c_str(), EzoCommandType::EZO_CUSTOM);
}

}  // namespace ezo_types
}  // namespace esphome
