#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include <M5Unified.h>

namespace esphome {
namespace m5cores3_touch {

struct TouchPoint {
  uint16_t x;
  uint16_t y;
  uint8_t id;
  bool is_pressed;
};

class TouchTrigger;

class M5CoreS3Touch : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  // Automation triggers
  void add_on_touch_trigger(TouchTrigger *trigger) { this->touch_triggers_.push_back(trigger); }
  void add_on_release_trigger(Trigger<> *trigger) { this->release_triggers_.push_back(trigger); }

 protected:
  std::vector<TouchTrigger *> touch_triggers_;
  std::vector<Trigger<> *> release_triggers_;

  bool last_touch_state_{false};
  TouchPoint last_touch_point_;
  uint32_t last_touch_time_{0};
};

class TouchTrigger : public Trigger<TouchPoint> {
 public:
  explicit TouchTrigger(M5CoreS3Touch *parent) { parent->add_on_touch_trigger(this); }
};

}  // namespace m5cores3_touch
}  // namespace esphome