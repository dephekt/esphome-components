#include "m5cores3_touch.h"
#include "esphome/core/log.h"

namespace esphome {
namespace m5cores3_touch {

static const char *TAG = "m5cores3_touch";

void M5CoreS3Touch::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5CoreS3 Touch...");

  // M5.Touch is already initialized by board_m5cores3 component through M5.begin()
  // No additional setup needed

  ESP_LOGCONFIG(TAG, "M5CoreS3 Touch setup complete");
}

void M5CoreS3Touch::loop() {
  // Update M5 touch state - this reads the latest touch data
  M5.update();

  uint32_t now = millis();
  int touch_count = M5.Touch.getCount();
  bool current_touch_state = (touch_count > 0);

  if (current_touch_state && !last_touch_state_) {
    // Touch just started
    if (touch_count > 0) {
      auto touch_detail = M5.Touch.getDetail(0);  // Get first touch point

      TouchPoint point;
      point.x = touch_detail.x;
      point.y = touch_detail.y;
      point.id = 0;
      point.is_pressed = true;

      last_touch_point_ = point;
      last_touch_time_ = now;

      ESP_LOGD(TAG, "Touch started at (%d, %d)", point.x, point.y);

      // Trigger all touch automation
      for (auto *trigger : this->touch_triggers_) {
        trigger->trigger(point);
      }
    }
    last_touch_state_ = true;

  } else if (!current_touch_state && last_touch_state_) {
    // Touch just released
    ESP_LOGD(TAG, "Touch released after %dms", now - last_touch_time_);

    // Trigger all release automation
    for (auto *trigger : this->release_triggers_) {
      trigger->trigger();
    }
    last_touch_state_ = false;

  } else if (current_touch_state && last_touch_state_) {
    // Touch is continuing - update position if it moved significantly
    if (touch_count > 0) {
      auto touch_detail = M5.Touch.getDetail(0);

      // Check if touch moved significantly (more than 5 pixels)
      int dx = abs(touch_detail.x - last_touch_point_.x);
      int dy = abs(touch_detail.y - last_touch_point_.y);

      if (dx > 5 || dy > 5) {
        TouchPoint point;
        point.x = touch_detail.x;
        point.y = touch_detail.y;
        point.id = 0;
        point.is_pressed = true;

        last_touch_point_ = point;

        ESP_LOGV(TAG, "Touch moved to (%d, %d)", point.x, point.y);

        // Trigger touch automation for movement
        for (auto *trigger : this->touch_triggers_) {
          trigger->trigger(point);
        }
      }
    }
  }
}

void M5CoreS3Touch::dump_config() {
  ESP_LOGCONFIG(TAG, "M5CoreS3 Touch:");
  ESP_LOGCONFIG(TAG, "  Display Width: %d", M5.Display.width());
  ESP_LOGCONFIG(TAG, "  Display Height: %d", M5.Display.height());
  ESP_LOGCONFIG(TAG, "  Touch Triggers: %zu", this->touch_triggers_.size());
  ESP_LOGCONFIG(TAG, "  Release Triggers: %zu", this->release_triggers_.size());
}

}  // namespace m5cores3_touch
}  // namespace esphome