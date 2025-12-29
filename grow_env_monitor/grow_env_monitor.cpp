#include "grow_env_monitor.h"
#include "esphome/core/log.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <cmath>
#include <algorithm>  // For std::sort

namespace esphome {
namespace grow_env_monitor {

static const char *TAG = "grow_env_monitor";

void GrowEnvMonitor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Grow Environment Monitor...");

  M5.Display.begin();
  M5.Display.setRotation(display_rotation_);
  M5.Display.setBrightness(display_brightness_);
  M5.Display.fillScreen(COLOR_BACKGROUND);

  // Note: Thermal camera initialization is now handled by the mlx90640 component

  // Draw initial screen layout and labels
  draw_screen_();

  initialized_ = true;
  ESP_LOGCONFIG(TAG, "Grow Environment Monitor setup complete");
}

void GrowEnvMonitor::loop() {
  if (!initialized_)
    return;

  uint32_t now = millis();

  // Draw thermal image at a reasonable rate (every 1 second max)
  static uint32_t last_thermal_draw = 0;
  const uint32_t THERMAL_DRAW_INTERVAL = 1000;  // 1 second

  if (mlx90640_component_ && mlx90640_component_->is_initialized() &&
      (now - last_thermal_draw > THERMAL_DRAW_INTERVAL)) {
    draw_thermal_image_();
    last_thermal_draw = now;
  }

  // Check for sensor value changes and selectively update display areas
  check_and_update_sensor_values_();
}

void GrowEnvMonitor::dump_config() {
  ESP_LOGCONFIG(TAG, "Grow Environment Monitor:");
  ESP_LOGCONFIG(TAG, "  Display Brightness: %d", display_brightness_);
  ESP_LOGCONFIG(TAG, "  Display Rotation: %d", display_rotation_);
  ESP_LOGCONFIG(TAG, "  Sensors configured: %s",
                (co2_sensor_ && temperature_sensor_ && humidity_sensor_) ? "Yes" : "No");
}

void GrowEnvMonitor::draw_screen_() {
  M5.Display.fillScreen(COLOR_BACKGROUND);

  // Draw header
  M5.Display.setTextColor(COLOR_HEADER);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(MARGIN, MARGIN);
  M5.Display.println("Grow Monitor");

  // Draw main sensor data
  draw_sensor_data_();

  // Draw alerts/status
  draw_alerts_();

  // Note: thermal image drawn separately at higher frequency
}

void GrowEnvMonitor::draw_sensor_data_() {
  int y = MARGIN + 30;

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_TEXT);

  // CO2 Data
  if (co2_sensor_ && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    uint16_t co2_color = (co2 > 1500) ? COLOR_ALERT : (co2 > 1000) ? COLOR_WARNING : COLOR_GOOD;

    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print("CO2: ");
    M5.Display.setTextColor(co2_color);
    M5.Display.print(co2, 0);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.println(" ppm");
  } else {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.println("CO2: No data");
  }
  y += LINE_HEIGHT;

  // Temperature Data
  if (temperature_sensor_ && temperature_sensor_->has_state()) {
    float temp = temperature_sensor_->state;
    uint16_t temp_color = COLOR_GOOD;

    // Use binary sensors for color if available
    if (temp_high_alert_sensor_ && temp_high_alert_sensor_->has_state() && temp_high_alert_sensor_->state) {
      temp_color = COLOR_ALERT;
    } else if (temp_low_alert_sensor_ && temp_low_alert_sensor_->has_state() && temp_low_alert_sensor_->state) {
      temp_color = COLOR_ALERT;
    } else if (!temp_high_alert_sensor_ && !temp_low_alert_sensor_) {
      // Fallback to hardcoded thresholds if no binary sensors
      temp_color = (temp > 30 || temp < 18) ? COLOR_ALERT : (temp > 28 || temp < 20) ? COLOR_WARNING : COLOR_GOOD;
    }

    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print("Temp: ");
    M5.Display.setTextColor(temp_color);
    M5.Display.print(temp, 1);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.println(" C");
  } else {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.println("Temp: No data");
  }
  y += LINE_HEIGHT;

  // Humidity Data
  if (humidity_sensor_ && humidity_sensor_->has_state()) {
    float humidity = humidity_sensor_->state;
    uint16_t humidity_color = COLOR_GOOD;

    // Use binary sensors for color if available
    if (humidity_high_alert_sensor_ && humidity_high_alert_sensor_->has_state() && humidity_high_alert_sensor_->state) {
      humidity_color = COLOR_ALERT;
    } else if (humidity_low_alert_sensor_ && humidity_low_alert_sensor_->has_state() &&
               humidity_low_alert_sensor_->state) {
      humidity_color = COLOR_ALERT;
    } else if (!humidity_high_alert_sensor_ && !humidity_low_alert_sensor_) {
      // Fallback to hardcoded thresholds if no binary sensors
      humidity_color = (humidity > 70 || humidity < 40)   ? COLOR_ALERT
                       : (humidity > 65 || humidity < 45) ? COLOR_WARNING
                                                          : COLOR_GOOD;
    }

    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print("Humidity: ");
    M5.Display.setTextColor(humidity_color);
    M5.Display.print(humidity, 1);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.println(" %");
  } else {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.println("Humidity: No data");
  }
  y += LINE_HEIGHT;

  // VPD Calculation
  if (temperature_sensor_ && temperature_sensor_->has_state() && humidity_sensor_ && humidity_sensor_->has_state()) {
    float vpd = calculate_vpd_(temperature_sensor_->state, humidity_sensor_->state);
    uint16_t vpd_color = (vpd > 1.5 || vpd < 0.4) ? COLOR_ALERT : (vpd > 1.2 || vpd < 0.6) ? COLOR_WARNING : COLOR_GOOD;

    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print("VPD: ");
    M5.Display.setTextColor(vpd_color);
    M5.Display.print(vpd, 2);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.println(" kPa");
  } else {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.println("VPD: No data");
  }
}

void GrowEnvMonitor::draw_thermal_data_() {
  int y = MARGIN + 135;  // Moved up 15 pixels to prevent bottom cutoff

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_HEADER);
  M5.Display.setCursor(MARGIN, y);
  M5.Display.println("Thermal Camera");
  y += LINE_HEIGHT;

  // Thermal Min
  if (thermal_min_sensor_ && thermal_min_sensor_->has_state()) {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print("Min: ");
    M5.Display.print(thermal_min_sensor_->state, 1);
    M5.Display.println(" C");
  } else {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.println("Min: No data");
  }
  y += LINE_HEIGHT;

  // Thermal Max
  if (thermal_max_sensor_ && thermal_max_sensor_->has_state()) {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print("Max: ");
    M5.Display.print(thermal_max_sensor_->state, 1);
    M5.Display.println(" C");
  } else {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.println("Max: No data");
  }

  // Thermal Average (on same line as Max)
  if (thermal_avg_sensor_ && thermal_avg_sensor_->has_state()) {
    M5.Display.setCursor(MARGIN + 120, y);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print("Avg: ");
    M5.Display.print(thermal_avg_sensor_->state, 1);
    M5.Display.println(" C");
  } else {
    M5.Display.setCursor(MARGIN + 120, y);
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.println("Avg: No data");
  }
  y += LINE_HEIGHT;

  // Show temperature range and median (using mlx90640 component)
  if (mlx90640_component_ && mlx90640_component_->is_initialized() && thermal_max_sensor_ && thermal_min_sensor_ &&
      thermal_max_sensor_->has_state() && thermal_min_sensor_->has_state()) {
    float temp_range = thermal_max_sensor_->state - thermal_min_sensor_->state;

    // Color-code based on thermal activity (wider range = more thermal activity)
    uint16_t range_color = COLOR_TEXT;
    if (temp_range > 10.0) {
      range_color = COLOR_ALERT;  // High thermal activity
    } else if (temp_range > 5.0) {
      range_color = COLOR_WARNING;  // Moderate thermal activity
    } else {
      range_color = COLOR_GOOD;  // Low thermal activity
    }

    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print("Range: ");
    M5.Display.setTextColor(range_color);
    M5.Display.print(temp_range, 1);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print(" C  Med: ");
    M5.Display.print(mlx90640_component_->get_median_temp(), 1);
    M5.Display.println(" C");
  }
}

void GrowEnvMonitor::draw_alerts_() {
  // Detailed alerts section on the left (where thermal data used to be)
  int y = MARGIN + 135;

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_HEADER);
  M5.Display.setCursor(MARGIN, y);
  M5.Display.println("Alerts");
  y += LINE_HEIGHT;

  bool has_alerts = false;

  // CO2 Alerts
  if (co2_sensor_ && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    if (co2 > 1500) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_ALERT);
      M5.Display.println("CO2 Critical High");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (co2 > 1000) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_WARNING);
      M5.Display.println("CO2 High");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (co2 < 400) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_WARNING);
      M5.Display.println("CO2 Low");
      y += LINE_HEIGHT;
      has_alerts = true;
    }
  }

  // Temperature Alerts - use binary sensors if available, otherwise fallback to hardcoded thresholds
  if (temp_high_alert_sensor_ && temp_high_alert_sensor_->has_state() && temp_high_alert_sensor_->state) {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_ALERT);
    M5.Display.println("Temperature High");
    y += LINE_HEIGHT;
    has_alerts = true;
  } else if (temp_low_alert_sensor_ && temp_low_alert_sensor_->has_state() && temp_low_alert_sensor_->state) {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_ALERT);
    M5.Display.println("Temperature Low");
    y += LINE_HEIGHT;
    has_alerts = true;
  } else if (!temp_high_alert_sensor_ && !temp_low_alert_sensor_ && temperature_sensor_ &&
             temperature_sensor_->has_state()) {
    // Fallback to hardcoded thresholds only if no alert sensors are configured
    float temp = temperature_sensor_->state;
    if (temp > 30) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_ALERT);
      M5.Display.println("Temperature High");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (temp < 18) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_ALERT);
      M5.Display.println("Temperature Low");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (temp > 28) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_WARNING);
      M5.Display.println("Temperature Warm");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (temp < 20) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_WARNING);
      M5.Display.println("Temperature Cool");
      y += LINE_HEIGHT;
      has_alerts = true;
    }
  }

  // Humidity Alerts - use binary sensors if available, otherwise fallback to hardcoded thresholds
  if (humidity_high_alert_sensor_ && humidity_high_alert_sensor_->has_state() && humidity_high_alert_sensor_->state) {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_ALERT);
    M5.Display.println("Humidity High");
    y += LINE_HEIGHT;
    has_alerts = true;
  } else if (humidity_low_alert_sensor_ && humidity_low_alert_sensor_->has_state() &&
             humidity_low_alert_sensor_->state) {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_ALERT);
    M5.Display.println("Humidity Low");
    y += LINE_HEIGHT;
    has_alerts = true;
  } else if (!humidity_high_alert_sensor_ && !humidity_low_alert_sensor_ && humidity_sensor_ &&
             humidity_sensor_->has_state()) {
    // Fallback to hardcoded thresholds only if no alert sensors are configured
    float humidity = humidity_sensor_->state;
    if (humidity > 70) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_ALERT);
      M5.Display.println("Humidity High");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (humidity < 40) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_ALERT);
      M5.Display.println("Humidity Low");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (humidity > 60) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_WARNING);
      M5.Display.println("Humidity Elevated");
      y += LINE_HEIGHT;
      has_alerts = true;
    } else if (humidity < 50) {
      M5.Display.setCursor(MARGIN, y);
      M5.Display.setTextColor(COLOR_WARNING);
      M5.Display.println("Humidity Dry");
      y += LINE_HEIGHT;
      has_alerts = true;
    }
  }

  // If no alerts, show "All Good"
  if (!has_alerts) {
    M5.Display.setCursor(MARGIN, y);
    M5.Display.setTextColor(COLOR_GOOD);
    M5.Display.println("All Good");
  }
}

float GrowEnvMonitor::calculate_vpd_(float temperature, float humidity) {
  // Calculate VPD using standard formula
  float svp = 0.6108 * exp((17.27 * temperature) / (temperature + 237.3));
  float vpd = svp * (1 - humidity / 100);
  return vpd;
}

uint16_t GrowEnvMonitor::get_status_color_(float co2, float temp, float humidity) {
  // Check for alert conditions using binary sensors if available, otherwise use hardcoded thresholds
  bool temp_alert = false;
  bool humidity_alert = false;

  // Check temperature alerts
  if (temp_high_alert_sensor_ && temp_high_alert_sensor_->has_state()) {
    temp_alert = temp_high_alert_sensor_->state;
  } else if (temp_low_alert_sensor_ && temp_low_alert_sensor_->has_state()) {
    temp_alert = temp_low_alert_sensor_->state;
  } else {
    // Fallback to hardcoded thresholds
    temp_alert = (temp > 30 || temp < 18);
  }

  // Check humidity alerts
  if (humidity_high_alert_sensor_ && humidity_high_alert_sensor_->has_state()) {
    humidity_alert = humidity_high_alert_sensor_->state;
  } else if (humidity_low_alert_sensor_ && humidity_low_alert_sensor_->has_state()) {
    humidity_alert = humidity_low_alert_sensor_->state;
  } else {
    // Fallback to hardcoded thresholds
    humidity_alert = (humidity > 70 || humidity < 40);
  }

  if (co2 > 1500 || temp_alert || humidity_alert) {
    return COLOR_ALERT;
  }

  // Check for warning conditions (using hardcoded thresholds for now)
  if (co2 > 1000 || temp > 28 || temp < 20 || humidity > 65 || humidity < 45) {
    return COLOR_WARNING;
  }

  return COLOR_GOOD;
}

// M5 Rainbow thermal color palette (256 colors)

void GrowEnvMonitor::draw_thermal_image_() {
  if (!mlx90640_component_ || !mlx90640_component_->is_initialized())
    return;

  // Position thermal image in right middle center area
  int image_x = SCREEN_WIDTH - 180;   // 180 pixels from right edge (good horizontal position)
  int image_y = SCREEN_HEIGHT - 170;  // 170 pixels from bottom (moved down slightly)
  int image_w = 160;                  // Double the width (was 80)
  int image_h = 120;                  // Double the height (was 60)

  // Ensure we don't go off screen
  if (image_x < 0)
    image_x = SCREEN_WIDTH - image_w - 5;
  if (image_y < 0)
    image_y = SCREEN_HEIGHT - image_h - 5;

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_HEADER);
  M5.Display.setCursor(image_x, image_y - 15);
  M5.Display.println("Thermal");

  // Draw thermal temperature info below the image
  int info_y = image_y + image_h + 5;

  // Clear the thermal temperature text area before drawing
  M5.Display.fillRect(image_x, info_y, image_w, 24, COLOR_BACKGROUND);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_TEXT);

  // Display ROI or full-frame temperatures based on ROI enabled state
  if (mlx90640_component_->is_roi_enabled() && mlx90640_component_->get_roi_pixel_count() > 0) {
    // Show ROI temperatures
    // Min temp
    M5.Display.setCursor(image_x, info_y);
    M5.Display.print("Min: ");
    M5.Display.print(mlx90640_component_->get_roi_min_temp(), 1);
    M5.Display.print("C");

    // Max temp (same line, offset)
    M5.Display.setCursor(image_x + 80, info_y);
    M5.Display.print("Max: ");
    M5.Display.print(mlx90640_component_->get_roi_max_temp(), 1);
    M5.Display.print("C");

    // Avg temp (next line)
    M5.Display.setCursor(image_x, info_y + 12);
    M5.Display.print("Avg: ");
    M5.Display.print(mlx90640_component_->get_roi_avg_temp(), 1);
    M5.Display.print("C");

    // ROI indicator label (right of Avg temp)
    M5.Display.setCursor(image_x + 80, info_y + 12);
    M5.Display.setTextColor(COLOR_GOOD);  // Green color
    M5.Display.print("ROI Mode");
    M5.Display.setTextColor(COLOR_TEXT);  // Back to white
  } else {
    // Show full-frame temperatures (original behavior)
    // Min temp
    M5.Display.setCursor(image_x, info_y);
    M5.Display.print("Min: ");
    if (thermal_min_sensor_ && thermal_min_sensor_->has_state()) {
      M5.Display.print(thermal_min_sensor_->state, 1);
      M5.Display.print("C");
    } else {
      M5.Display.print("--");
    }

    // Max temp (same line, offset)
    M5.Display.setCursor(image_x + 80, info_y);
    M5.Display.print("Max: ");
    if (thermal_max_sensor_ && thermal_max_sensor_->has_state()) {
      M5.Display.print(thermal_max_sensor_->state, 1);
      M5.Display.print("C");
    } else {
      M5.Display.print("--");
    }

    // Avg temp (next line)
    M5.Display.setCursor(image_x, info_y + 12);
    M5.Display.print("Avg: ");
    if (thermal_avg_sensor_ && thermal_avg_sensor_->has_state()) {
      M5.Display.print(thermal_avg_sensor_->state, 1);
      M5.Display.print("C");
    } else {
      M5.Display.print("--");
    }
  }

  // Draw thermal image with color mapping - using interpolated data for smoother display
  const float *interpolated_pixels = mlx90640_component_->get_interpolated_pixels();
  if (interpolated_pixels) {
    for (int y = 0; y < 48; y++) {
      for (int x = 0; x < 64; x++) {
        int pixel_idx = y * 64 + x;
        // Bounds check for array access safety
        if (pixel_idx >= 0 && pixel_idx < (64 * 48)) {
          float temperature = interpolated_pixels[pixel_idx];

          // Get color from thermal palette via mlx90640 component
          uint16_t color = mlx90640_component_->temp_to_color(temperature, mlx90640_component_->get_min_temp(),
                                                              mlx90640_component_->get_max_temp());

          // Calculate exact pixel dimensions to fill entire area
          int draw_x = image_x + (x * image_w / 64);
          int draw_y = image_y + (y * image_h / 48);
          int next_x = image_x + ((x + 1) * image_w / 64);
          int next_y = image_y + ((y + 1) * image_h / 48);

          // Fill rectangle to next pixel boundary to avoid gaps
          int width = next_x - draw_x;
          int height = next_y - draw_y;

          M5.Display.fillRect(draw_x, draw_y, width, height, color);
        }
      }
    }
  }

  // Draw ROI overlay if enabled
  if (mlx90640_component_->is_roi_enabled()) {
    draw_roi_overlay_(image_x, image_y, image_w, image_h);
  }

  // Draw border around thermal image
  M5.Display.drawRect(image_x - 1, image_y - 1, image_w + 2, image_h + 2, COLOR_TEXT);
}

void GrowEnvMonitor::draw_roi_overlay_(int image_x, int image_y, int image_w, int image_h) {
  if (!mlx90640_component_ || !mlx90640_component_->is_roi_enabled()) {
    return;
  }

  // Get ROI overlay bounds from mlx90640 component
  int roi_display_x1, roi_display_y1, roi_display_x2, roi_display_y2;
  if (!mlx90640_component_->get_roi_overlay_bounds(image_x, image_y, image_w, image_h, roi_display_x1, roi_display_y1,
                                                   roi_display_x2, roi_display_y2)) {
    return;
  }

  int roi_width = roi_display_x2 - roi_display_x1;
  int roi_height = roi_display_y2 - roi_display_y1;

  // Draw cyan border around ROI selection - stands out well against thermal colors
  uint16_t roi_color = 0x07FF;  // Bright cyan (RGB565) - good contrast against thermal palettes
  M5.Display.drawRect(roi_display_x1, roi_display_y1, roi_width, roi_height, roi_color);

  // Draw a second border 1 pixel inset for better visibility
  M5.Display.drawRect(roi_display_x1 + 1, roi_display_y1 + 1, roi_width - 2, roi_height - 2, roi_color);

  // Draw crosshairs at center point to help identify the exact ROI center
  int center_display_x, center_display_y;
  if (mlx90640_component_->get_roi_crosshair_coords(image_x, image_y, image_w, image_h, center_display_x,
                                                    center_display_y)) {
    int crosshair_size = 6;  // Length of crosshair lines in pixels

    // Horizontal crosshair line
    M5.Display.drawLine(center_display_x - crosshair_size, center_display_y, center_display_x + crosshair_size,
                        center_display_y, roi_color);
    // Vertical crosshair line
    M5.Display.drawLine(center_display_x, center_display_y - crosshair_size, center_display_x,
                        center_display_y + crosshair_size, roi_color);
  }
}

// Selective redraw functions for efficient display updates
void GrowEnvMonitor::check_and_update_sensor_values_() {
  bool changed = false;

  // Check CO2 sensor
  if (co2_sensor_ && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    if (isnan(prev_co2_) || fabs(co2 - prev_co2_) > 5.0) {  // 5 ppm threshold
      prev_co2_ = co2;
      draw_co2_value_();
      changed = true;
    }
  }

  // Check temperature sensor
  if (temperature_sensor_ && temperature_sensor_->has_state()) {
    float temp = temperature_sensor_->state;
    if (isnan(prev_temperature_) || fabs(temp - prev_temperature_) > 0.1) {  // 0.1°C threshold
      prev_temperature_ = temp;
      draw_temperature_value_();
      changed = true;
    }
  }

  // Check humidity sensor
  if (humidity_sensor_ && humidity_sensor_->has_state()) {
    float humidity = humidity_sensor_->state;
    if (isnan(prev_humidity_) || fabs(humidity - prev_humidity_) > 0.5) {  // 0.5% threshold
      prev_humidity_ = humidity;
      draw_humidity_value_();
      changed = true;
    }
  }

  // Update VPD if temp or humidity changed
  if (changed && temperature_sensor_ && temperature_sensor_->has_state() && humidity_sensor_ &&
      humidity_sensor_->has_state()) {
    draw_vpd_value_();
  }

  // Check light sensor
  if (light_sensor_ && light_sensor_->has_state()) {
    bool light_on = light_sensor_->state;
    if (light_on != prev_light_on_) {
      prev_light_on_ = light_on;
      draw_light_status_();
      changed = true;
    }
  }

  // Check thermal sensor values
  bool thermal_changed = false;
  if (thermal_min_sensor_ && thermal_min_sensor_->has_state()) {
    float thermal_min = thermal_min_sensor_->state;
    if (isnan(prev_thermal_min_) || fabs(thermal_min - prev_thermal_min_) > 0.1) {
      prev_thermal_min_ = thermal_min;
      thermal_changed = true;
    }
  }

  if (thermal_max_sensor_ && thermal_max_sensor_->has_state()) {
    float thermal_max = thermal_max_sensor_->state;
    if (isnan(prev_thermal_max_) || fabs(thermal_max - prev_thermal_max_) > 0.1) {
      prev_thermal_max_ = thermal_max;
      thermal_changed = true;
    }
  }

  if (thermal_avg_sensor_ && thermal_avg_sensor_->has_state()) {
    float thermal_avg = thermal_avg_sensor_->state;
    if (isnan(prev_thermal_avg_) || fabs(thermal_avg - prev_thermal_avg_) > 0.1) {
      prev_thermal_avg_ = thermal_avg;
      thermal_changed = true;
    }
  }

  // Status drawing removed - using detailed alerts section instead
}

void GrowEnvMonitor::draw_co2_value_() {
  int y = MARGIN + 30;

  // Clear the CO2 value area
  M5.Display.fillRect(MARGIN + 30, y, 100, LINE_HEIGHT, COLOR_BACKGROUND);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(MARGIN, y);
  M5.Display.setTextColor(COLOR_TEXT);
  M5.Display.print("CO2: ");

  if (co2_sensor_ && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    uint16_t co2_color = (co2 > 1500) ? COLOR_ALERT : (co2 > 1000) ? COLOR_WARNING : COLOR_GOOD;
    M5.Display.setTextColor(co2_color);
    M5.Display.print(co2, 0);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print(" ppm");
  } else {
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.print("No data");
  }
}

void GrowEnvMonitor::draw_temperature_value_() {
  int y = MARGIN + 55;

  // Clear the temperature value area
  M5.Display.fillRect(MARGIN + 42, y, 80, LINE_HEIGHT, COLOR_BACKGROUND);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(MARGIN, y);
  M5.Display.setTextColor(COLOR_TEXT);
  M5.Display.print("Temp: ");

  if (temperature_sensor_ && temperature_sensor_->has_state()) {
    float temp = temperature_sensor_->state;
    uint16_t temp_color = COLOR_GOOD;

    // Use binary sensors for color if available
    if (temp_high_alert_sensor_ && temp_high_alert_sensor_->has_state() && temp_high_alert_sensor_->state) {
      temp_color = COLOR_ALERT;
    } else if (temp_low_alert_sensor_ && temp_low_alert_sensor_->has_state() && temp_low_alert_sensor_->state) {
      temp_color = COLOR_ALERT;
    } else if (!temp_high_alert_sensor_ && !temp_low_alert_sensor_) {
      // Fallback to hardcoded thresholds if no binary sensors
      temp_color = (temp > 30 || temp < 18) ? COLOR_ALERT : (temp > 28 || temp < 20) ? COLOR_WARNING : COLOR_GOOD;
    }

    M5.Display.setTextColor(temp_color);
    M5.Display.print(temp, 1);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print(" C");
  } else {
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.print("No data");
  }
}

void GrowEnvMonitor::draw_humidity_value_() {
  int y = MARGIN + 80;

  // Clear the humidity value area
  M5.Display.fillRect(MARGIN + 66, y, 80, LINE_HEIGHT, COLOR_BACKGROUND);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(MARGIN, y);
  M5.Display.setTextColor(COLOR_TEXT);
  M5.Display.print("Humidity: ");

  if (humidity_sensor_ && humidity_sensor_->has_state()) {
    float humidity = humidity_sensor_->state;
    uint16_t humidity_color = COLOR_GOOD;

    // Use binary sensors for color if available
    if (humidity_high_alert_sensor_ && humidity_high_alert_sensor_->has_state() && humidity_high_alert_sensor_->state) {
      humidity_color = COLOR_ALERT;
    } else if (humidity_low_alert_sensor_ && humidity_low_alert_sensor_->has_state() &&
               humidity_low_alert_sensor_->state) {
      humidity_color = COLOR_ALERT;
    } else if (!humidity_high_alert_sensor_ && !humidity_low_alert_sensor_) {
      // Fallback to hardcoded thresholds if no binary sensors
      humidity_color = (humidity > 70 || humidity < 40)   ? COLOR_ALERT
                       : (humidity > 65 || humidity < 45) ? COLOR_WARNING
                                                          : COLOR_GOOD;
    }

    M5.Display.setTextColor(humidity_color);
    M5.Display.print(humidity, 1);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print(" %");
  } else {
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.print("No data");
  }
}

void GrowEnvMonitor::draw_vpd_value_() {
  int y = MARGIN + 105;

  // Clear the VPD value area
  M5.Display.fillRect(MARGIN + 30, y, 100, LINE_HEIGHT, COLOR_BACKGROUND);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(MARGIN, y);
  M5.Display.setTextColor(COLOR_TEXT);
  M5.Display.print("VPD: ");

  if (temperature_sensor_ && temperature_sensor_->has_state() && humidity_sensor_ && humidity_sensor_->has_state()) {
    float vpd = calculate_vpd_(temperature_sensor_->state, humidity_sensor_->state);
    uint16_t vpd_color = (vpd > 1.5 || vpd < 0.4) ? COLOR_ALERT : (vpd > 1.2 || vpd < 0.6) ? COLOR_WARNING : COLOR_GOOD;
    M5.Display.setTextColor(vpd_color);
    M5.Display.print(vpd, 2);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.print(" kPa");
  } else {
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.print("No data");
  }
}

void GrowEnvMonitor::draw_light_status_() {
  int y = MARGIN + 130;

  // Clear the light status area
  M5.Display.fillRect(MARGIN, y, 150, LINE_HEIGHT, COLOR_BACKGROUND);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(MARGIN, y);
  M5.Display.setTextColor(COLOR_TEXT);
  M5.Display.print("Light: ");

  if (light_sensor_ && light_sensor_->has_state()) {
    bool light_on = light_sensor_->state;
    uint16_t light_color = light_on ? COLOR_GOOD : COLOR_WARNING;
    M5.Display.setTextColor(light_color);
    M5.Display.print(light_on ? "ON" : "OFF");
  } else {
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.print("No data");
  }
}

void GrowEnvMonitor::draw_thermal_values_() {
  int y = MARGIN + 160;

  // Clear thermal values area
  M5.Display.fillRect(MARGIN, y, 300, LINE_HEIGHT * 3, COLOR_BACKGROUND);

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_TEXT);

  // Thermal Min
  M5.Display.setCursor(MARGIN, y);
  M5.Display.print("Min: ");
  if (thermal_min_sensor_ && thermal_min_sensor_->has_state()) {
    M5.Display.print(thermal_min_sensor_->state, 1);
    M5.Display.print(" C");
  } else {
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.print("No data");
    M5.Display.setTextColor(COLOR_TEXT);
  }
  y += LINE_HEIGHT;

  // Thermal Max
  M5.Display.setCursor(MARGIN, y);
  M5.Display.print("Max: ");
  if (thermal_max_sensor_ && thermal_max_sensor_->has_state()) {
    M5.Display.print(thermal_max_sensor_->state, 1);
    M5.Display.print(" C");
  } else {
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.print("No data");
    M5.Display.setTextColor(COLOR_TEXT);
  }

  // Thermal Average (on same line as Max)
  M5.Display.setCursor(MARGIN + 120, y);
  M5.Display.print("Avg: ");
  if (thermal_avg_sensor_ && thermal_avg_sensor_->has_state()) {
    M5.Display.print(thermal_avg_sensor_->state, 1);
    M5.Display.print(" C");
  } else {
    M5.Display.setTextColor(COLOR_WARNING);
    M5.Display.print("No data");
    M5.Display.setTextColor(COLOR_TEXT);
  }
}

}  // namespace grow_env_monitor
}  // namespace esphome
