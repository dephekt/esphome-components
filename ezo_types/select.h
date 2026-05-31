#pragma once

#include "esphome/core/component.h"
#include "esphome/components/select/select.h"

namespace esphome {
namespace ezo_types {

// Forward declaration
class ECSensor;

class CellConstantSelect : public select::Select, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_ec_sensor(ECSensor *ec_sensor);

 protected:
  void control(const std::string &value) override;

  ECSensor *ec_sensor_{nullptr};
};

}  // namespace ezo_types
}  // namespace esphome
