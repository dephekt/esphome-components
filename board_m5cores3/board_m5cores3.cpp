#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "board_m5cores3.h"

namespace esphome {
namespace board_m5cores3 {

static const char *TAG = "board.m5cores3";

void BoardM5CoreS3::setup() {
  ESP_LOGI(TAG, "setup");

  // Give time for serial connection to be established after reset
  ESP_LOGI(TAG, "Waiting 10 seconds for serial connection...");
  for (int i = 0; i < 100; i++) {
    delay(100);      // 100ms delays to avoid watchdog timeout
    App.feed_wdt();  // Feed the watchdog
  }
  ESP_LOGI(TAG, "Starting M5 initialization with proper config");

  // Configure M5Unified properly for Core S3
  auto cfg = M5.config();

  // Disable external displays we don't have
  cfg.external_display.module_display = false;
  cfg.external_display.atom_display = false;
  cfg.external_display.unit_glass = false;
  cfg.external_display.unit_glass2 = false;
  cfg.external_display.unit_oled = false;
  cfg.external_display.unit_mini_oled = false;
  cfg.external_display.unit_lcd = false;
  cfg.external_display.unit_rca = false;
  cfg.external_display.module_rca = false;

  // Initialize M5 with configuration
  M5.begin(cfg);
  ESP_LOGI(TAG, "M5 system initialized with config");

  ESP_LOGI(TAG, "Enabling 5V power output on the external ports");
  M5.Power.setExtOutput(true);

  ESP_LOGI(TAG, "Setting initial display brightness to 0 (will be controlled by display component)");
  M5.Display.setBrightness(0);
}

void BoardM5CoreS3::loop() {}

void BoardM5CoreS3::dump_config() { ESP_LOGCONFIG(TAG, "config"); }

}  // namespace board_m5cores3
}  // namespace esphome
