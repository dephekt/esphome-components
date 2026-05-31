#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace ezo_types {

// Forward declarations
class ORPSensor;

class ExtendedScaleSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_orp_sensor(ORPSensor *orp_sensor) { orp_sensor_ = orp_sensor; }

 protected:
  void write_state(bool state) override;

  ORPSensor *orp_sensor_{nullptr};
};

}  // namespace ezo_types
}  // namespace esphome
