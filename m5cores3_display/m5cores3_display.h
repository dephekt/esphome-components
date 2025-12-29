#pragma once

#include "esphome/core/component.h"
#include <M5Unified.h>

// Forward declarations
namespace esphome {
namespace m5unit_co2l {
class M5UnitCO2L;
}
namespace mlx90640_thermal {
class MLX90640Thermal;
}
}  // namespace esphome

namespace esphome {
namespace m5cores3_display {

class M5CoreS3Display : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_co2l_component(m5unit_co2l::M5UnitCO2L *co2l_component) { co2l_component_ = co2l_component; }
  void set_thermal_component(mlx90640_thermal::MLX90640Thermal *thermal_component) {
    thermal_component_ = thermal_component;
  }

 protected:
  void draw_environmental_data_();
  void draw_thermal_camera_();
  void draw_thermal_image_(int x, int y, int width, int height);
  uint16_t temperature_to_color_(float temperature, float min_temp, float max_temp);

  bool initialized_{false};
  m5unit_co2l::M5UnitCO2L *co2l_component_{nullptr};
  mlx90640_thermal::MLX90640Thermal *thermal_component_{nullptr};

  // Track last update to avoid unnecessary redraws
  uint32_t last_update_time_{0};
  bool display_needs_update_{true};

  // Thermal color palette
  static const uint16_t thermal_colors_[256];
};

}  // namespace m5cores3_display
}  // namespace esphome