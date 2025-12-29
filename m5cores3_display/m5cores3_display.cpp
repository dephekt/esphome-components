#include "esphome/core/log.h"
#include "m5cores3_display.h"
#include "../m5unit_co2l/m5unit_co2l.h"
#include "../mlx90640_thermal/mlx90640_thermal.h"
#include <cmath>

namespace esphome {
namespace m5cores3_display {

static const char *TAG = "m5cores3_display";

// Thermal color palette (similar to MLX90640 component)
const uint16_t M5CoreS3Display::thermal_colors_[256] = {
    0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 0x3810, 0x3810, 0x3010, 0x3010, 0x3010, 0x2810, 0x2810,
    0x2810, 0x2810, 0x2010, 0x2010, 0x2010, 0x1810, 0x1810, 0x1811, 0x1811, 0x1011, 0x1011, 0x1011, 0x0811, 0x0811,
    0x0811, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0031, 0x0031, 0x0051, 0x0072, 0x0072, 0x0092, 0x00B2, 0x00B2,
    0x00D2, 0x00F2, 0x00F2, 0x0112, 0x0132, 0x0152, 0x0152, 0x0172, 0x0192, 0x0192, 0x01B2, 0x01D2, 0x01F3, 0x01F3,
    0x0213, 0x0233, 0x0253, 0x0253, 0x0273, 0x0293, 0x02B3, 0x02D3, 0x02D3, 0x02F3, 0x0313, 0x0333, 0x0333, 0x0353,
    0x0373, 0x0394, 0x03B4, 0x03D4, 0x03D4, 0x03F4, 0x0414, 0x0434, 0x0454, 0x0474, 0x0474, 0x0494, 0x04B4, 0x04D4,
    0x04F4, 0x0514, 0x0534, 0x0534, 0x0554, 0x0554, 0x0574, 0x0574, 0x0573, 0x0573, 0x0573, 0x0572, 0x0572, 0x0572,
    0x0571, 0x0591, 0x0591, 0x0590, 0x0590, 0x058F, 0x058F, 0x058F, 0x058E, 0x05AE, 0x05AE, 0x05AD, 0x05AD, 0x05AD,
    0x05AC, 0x05AC, 0x05AB, 0x05CB, 0x05CB, 0x05CA, 0x05CA, 0x05CA, 0x05C9, 0x05C9, 0x05C8, 0x05E8, 0x05E8, 0x05E7,
    0x05E7, 0x05E6, 0x05E6, 0x05E6, 0x05E5, 0x05E5, 0x0604, 0x0604, 0x0604, 0x0603, 0x0603, 0x0602, 0x0602, 0x0601,
    0x0621, 0x0621, 0x0620, 0x0620, 0x0620, 0x0620, 0x0E20, 0x0E20, 0x0E40, 0x1640, 0x1640, 0x1E40, 0x1E40, 0x2640,
    0x2640, 0x2E40, 0x2E60, 0x3660, 0x3660, 0x3E60, 0x3E60, 0x3E60, 0x4660, 0x4660, 0x4E60, 0x4E80, 0x5680, 0x5680,
    0x5E80, 0x5E80, 0x6680, 0x6680, 0x6E80, 0x6EA0, 0x76A0, 0x76A0, 0x7EA0, 0x7EA0, 0x86A0, 0x86A0, 0x8EA0, 0x8EC0,
    0x96C0, 0x96C0, 0x9EC0, 0x9EC0, 0xA6C0, 0xAEC0, 0xAEC0, 0xB6E0, 0xB6E0, 0xBEE0, 0xBEE0, 0xC6E0, 0xC6E0, 0xCEE0,
    0xCEE0, 0xD6E0, 0xD700, 0xDF00, 0xDEE0, 0xDEC0, 0xDEA0, 0xDE80, 0xDE80, 0xE660, 0xE640, 0xE620, 0xE600, 0xE5E0,
    0xE5C0, 0xE5A0, 0xE580, 0xE560, 0xE540, 0xE520, 0xE500, 0xE4E0, 0xE4C0, 0xE4A0, 0xE480, 0xE460, 0xEC40, 0xEC20,
    0xEC00, 0xEBE0, 0xEBC0, 0xEBA0, 0xEB80, 0xEB60, 0xEB40, 0xEB20, 0xEB00, 0xEAE0, 0xEAC0, 0xEAA0, 0xEA80, 0xEA60,
    0xEA40, 0xF220, 0xF200, 0xF1E0, 0xF1C0, 0xF1A0, 0xF180, 0xF160, 0xF140, 0xF100, 0xF0E0, 0xF0C0, 0xF0A0, 0xF080,
    0xF060, 0xF040, 0xF020, 0xF800,
};

void M5CoreS3Display::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5CoreS3 Display...");

  // M5.Display is already initialized by board_m5cores3 component
  // Just configure our display settings
  M5.Display.setRotation(3);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setBrightness(70);
  M5.Display.setFont(&fonts::Font4);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextSize(1);

  initialized_ = true;
  ESP_LOGCONFIG(TAG, "M5CoreS3 Display setup complete");
}

void M5CoreS3Display::loop() {
  if (!initialized_)
    return;

  // Check if we need to update the display
  uint32_t now = millis();
  if (now - last_update_time_ > 1000) {  // Check every second
    bool should_update = false;

    if (co2l_component_ != nullptr) {
      auto &unit = co2l_component_->unit_;

      // Update if sensor data has been updated or if it's been too long
      if (unit.updated() || display_needs_update_ || now - last_update_time_ > 10000) {
        should_update = true;
        display_needs_update_ = false;
        ESP_LOGD(TAG, "Display update - CO2: %u, Temp: %.1f, Humidity: %.1f", unit.co2(), unit.temperature(),
                 unit.humidity());
      }
    } else {
      // No sensor, but still update periodically
      if (now - last_update_time_ > 10000) {
        should_update = true;
      }
    }

    if (should_update) {
      draw_environmental_data_();
      last_update_time_ = now;
    }
  }
}

void M5CoreS3Display::draw_environmental_data_() {
  // Clear the screen
  M5.Display.fillScreen(TFT_WHITE);

  // Set text properties
  M5.Display.setFont(&fonts::Font4);  // Smaller font to fit more content
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(TL_DATUM);  // Top Left alignment

  int y_pos = 10;
  int line_height = M5.Display.fontHeight() + 8;      // Compact spacing between sections
  int label_value_gap = M5.Display.fontHeight() + 1;  // Minimal gap between label and value

  // Get data from CO2L component if available
  if (co2l_component_ != nullptr) {
    // Access the unit directly to get current readings
    auto &unit = co2l_component_->unit_;

    // CO2 reading - vertical format
    M5.Display.drawString("CO2", 10, y_pos);
    y_pos += label_value_gap;
    String co2_value;
    if (unit.updated() || unit.co2() > 0) {
      co2_value = String(unit.co2()) + " ppm";
    } else {
      co2_value = "--- ppm";
    }
    M5.Display.drawString(co2_value, 10, y_pos);
    y_pos += line_height;

    // Temperature reading - vertical format
    M5.Display.drawString("Temp", 10, y_pos);
    y_pos += label_value_gap;
    if (unit.updated() || !isnan(unit.temperature())) {
      String temp_value = String(unit.temperature(), 1);
      int temp_width = M5.Display.textWidth(temp_value.c_str());
      M5.Display.drawString(temp_value.c_str(), 10, y_pos);
      // Draw a small circle for degree symbol
      M5.Display.drawCircle(10 + temp_width + 2, y_pos + 2, 2, TFT_BLACK);
      M5.Display.drawString("C", 10 + temp_width + 8, y_pos);
    } else {
      int temp_width = M5.Display.textWidth("---");
      M5.Display.drawString("---", 10, y_pos);
      M5.Display.drawCircle(10 + temp_width + 2, y_pos + 2, 2, TFT_BLACK);
      M5.Display.drawString("C", 10 + temp_width + 8, y_pos);
    }
    y_pos += line_height;

    // Humidity reading - vertical format
    M5.Display.drawString("RH", 10, y_pos);
    y_pos += label_value_gap;
    String hum_value;
    if (unit.updated() || !isnan(unit.humidity())) {
      hum_value = String(unit.humidity(), 1) + " %";
    } else {
      hum_value = "--- %";
    }
    M5.Display.drawString(hum_value, 10, y_pos);
    y_pos += line_height;

    // VPD calculation - vertical format
    M5.Display.drawString("VPD", 10, y_pos);
    y_pos += label_value_gap;
    String vpd_value;
    if (!isnan(unit.temperature()) && !isnan(unit.humidity())) {
      float temp = unit.temperature();
      float humidity = unit.humidity();
      float svp = 0.61078f * exp((17.27f * temp) / (temp + 237.3f));
      float vpd = svp * (1.0f - (humidity / 100.0f));
      vpd_value = String(vpd, 2) + " kPa";
    } else {
      vpd_value = "--- kPa";
    }
    M5.Display.drawString(vpd_value, 10, y_pos);

  } else {
    // No component connected
    M5.Display.setTextColor(TFT_RED);
    M5.Display.drawString("No CO2L sensor", 10, y_pos);
  }

  // Draw thermal camera on the right side
  draw_thermal_camera_();
}

void M5CoreS3Display::draw_thermal_camera_() {
  // Position thermal camera on the right side
  int thermal_x = 160;  // Right side of 320px screen
  int thermal_y = 20;   // Top position

  // Scale the 32x24 image to 128x96 (4x scaling)
  int scaled_width = 128;  // 32 * 4
  int scaled_height = 96;  // 24 * 4

  if (thermal_component_ == nullptr) {
    // Draw placeholder if no thermal component
    M5.Display.setTextColor(TFT_RED);
    M5.Display.drawRect(thermal_x, thermal_y, scaled_width, scaled_height, TFT_RED);
    M5.Display.drawString("No Thermal", thermal_x + 10, thermal_y + 40);
    M5.Display.drawString("Camera", thermal_x + 10, thermal_y + 60);
    return;
  }

  // Get thermal data from the component
  const float *thermal_pixels = thermal_component_->get_thermal_pixels();
  if (thermal_pixels == nullptr || !thermal_component_->has_valid_data()) {
    // Draw placeholder if no data available
    M5.Display.setTextColor(TFT_ORANGE);
    M5.Display.drawRect(thermal_x, thermal_y, scaled_width, scaled_height, TFT_ORANGE);
    M5.Display.drawString("Thermal", thermal_x + 10, thermal_y + 40);
    M5.Display.drawString("Loading...", thermal_x + 10, thermal_y + 60);
    return;
  }

  // Draw the thermal image
  draw_thermal_image_(thermal_x, thermal_y, scaled_width, scaled_height);
}

void M5CoreS3Display::draw_thermal_image_(int x, int y, int width, int height) {
  const float *thermal_pixels = thermal_component_->get_thermal_pixels();
  if (thermal_pixels == nullptr) {
    return;
  }

  // Get temperature statistics from MLX90640 component
  float min_temp = thermal_component_->get_min_temperature();
  float max_temp = thermal_component_->get_max_temperature();

  // Get temperature range for color mapping
  float temp_range = max_temp - min_temp;

  // Ensure we have a reasonable temperature range
  if (temp_range < 5.0f) {
    float center = (min_temp + max_temp) / 2.0f;
    min_temp = center - 2.5f;
    max_temp = center + 2.5f;
  }

  // Create image buffer for thermal data (32x24 pixels)
  // Use 16-bit color depth for hardware-accelerated DMA transfer
  uint16_t thermal_image[32 * 24];

  // Generate thermal image data
  for (int row = 0; row < 24; row++) {
    for (int col = 0; col < 32; col++) {
      int pixel_index = row * 32 + col;
      int buffer_index = row * 32 + col;

      float pixel_temp = thermal_pixels[pixel_index];

      // Only use valid temperatures
      if (pixel_temp > -50.0f && pixel_temp < 200.0f) {
        thermal_image[buffer_index] = temperature_to_color_(pixel_temp, min_temp, max_temp);
      } else {
        thermal_image[buffer_index] = 0x0000;  // Black for invalid data
      }
    }
  }

  // Push the thermal image using DMA for maximum performance
  // Hardware-accelerated transfer: 768 fillRect calls → 1 DMA transfer
  M5.Display.pushImageDMA(x, y, 32, 24, thermal_image);

  // Draw temperature values
  M5.Display.setTextColor(0xFFFF);  // White text
  M5.Display.setTextSize(1);
  M5.Display.setCursor(x + 5, y + 5);
  M5.Display.print("Low: ");
  M5.Display.print(min_temp, 1);
  M5.Display.print("°C");

  M5.Display.setCursor(x + 5, y + 15);
  M5.Display.print("High: ");
  M5.Display.print(max_temp, 1);
  M5.Display.print("°C");

  M5.Display.setCursor(x + 5, y + 25);
  M5.Display.print("Avg: ");
  M5.Display.print(thermal_component_->get_average_temperature(), 1);
  M5.Display.print("°C");

  ESP_LOGV(TAG, "Thermal image drawn - Range: %.1f°C to %.1f°C", min_temp, max_temp);
}

uint16_t M5CoreS3Display::temperature_to_color_(float temperature, float min_temp, float max_temp) {
  // Map temperature to color index (0-255)
  int color_index = (int) map(temperature, min_temp, max_temp, 0, 255);
  color_index = constrain(color_index, 0, 255);

  return thermal_colors_[color_index];
}

void M5CoreS3Display::dump_config() {
  ESP_LOGCONFIG(TAG, "M5CoreS3 Display:");
  ESP_LOGCONFIG(TAG, "  Width: %d", initialized_ ? M5.Display.width() : 320);
  ESP_LOGCONFIG(TAG, "  Height: %d", initialized_ ? M5.Display.height() : 240);
  ESP_LOGCONFIG(TAG, "  CO2L Component: %s", co2l_component_ != nullptr ? "Connected" : "Not connected");
  ESP_LOGCONFIG(TAG, "  Thermal Component: %s", thermal_component_ != nullptr ? "Connected" : "Not connected");
}

}  // namespace m5cores3_display
}  // namespace esphome
