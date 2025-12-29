// Ref: https://github.com/thegroove/esphome-custom-component-examples.git

#pragma once

#include <M5Unified.h>
#include "esphome/core/component.h"

// Power mode constants for M5CoreS3
#define POWER_MODE_USB_IN_BUS_IN 0
#define POWER_MODE_USB_IN_BUS_OUT 1
#define POWER_MODE_USB_OUT_BUS_IN 2
#define POWER_MODE_USB_OUT_BUS_OUT 3

namespace esphome {
namespace board_m5cores3 {

class BoardM5CoreS3 : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Set high priority to run before I2C setup
  float get_setup_priority() const override { return setup_priority::BUS + 1.0f; }
};

}  // namespace board_m5cores3
}  // namespace esphome
