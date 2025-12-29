#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "m5cores3_power.h"

namespace esphome {
namespace m5cores3_power {

static const char *TAG = "m5cores3_power";

void M5CoreS3Power::log_axp2101_diagnostics() {
  ESP_LOGI(TAG, "=== AXP2101 Diagnostic Information ===");

  // Power states
  ESP_LOGI(TAG, "Battery present: %s", M5.Power.Axp2101.getBatState() ? "YES" : "NO");
  ESP_LOGI(TAG, "VBUS present: %s", M5.Power.Axp2101.isVBUS() ? "YES" : "NO");
  ESP_LOGI(TAG, "Charging: %s", M5.Power.Axp2101.isCharging() ? "YES" : "NO");
  ESP_LOGI(TAG, "Charge status: %d", M5.Power.Axp2101.getChargeStatus());
  ESP_LOGI(TAG, "Battery level: %d%%", M5.Power.Axp2101.getBatteryLevel());

  // Voltage readings
  ESP_LOGI(TAG, "Battery voltage: %.3fV", M5.Power.Axp2101.getBatteryVoltage());
  ESP_LOGI(TAG, "VBUS voltage: %.3fV", M5.Power.Axp2101.getVBUSVoltage());
  ESP_LOGI(TAG, "Internal temperature: %.1f°C", M5.Power.Axp2101.getInternalTemperature());
  ESP_LOGI(TAG, "TS voltage: %.3fV", M5.Power.Axp2101.getTSVoltage());

  // LDO states
  ESP_LOGI(TAG, "ALDO1 enabled: %s", M5.Power.Axp2101.getALDO1Enabled() ? "YES" : "NO");
  ESP_LOGI(TAG, "ALDO2 enabled: %s", M5.Power.Axp2101.getALDO2Enabled() ? "YES" : "NO");
  ESP_LOGI(TAG, "ALDO3 enabled: %s", M5.Power.Axp2101.getALDO3Enabled() ? "YES" : "NO");
  ESP_LOGI(TAG, "ALDO4 enabled: %s", M5.Power.Axp2101.getALDO4Enabled() ? "YES" : "NO");
  ESP_LOGI(TAG, "BLDO1 enabled: %s", M5.Power.Axp2101.getBLDO1Enabled() ? "YES" : "NO");
  ESP_LOGI(TAG, "BLDO2 enabled: %s", M5.Power.Axp2101.getBLDO2Enabled() ? "YES" : "NO");

  // IRQ status before clearing
  uint64_t irq_status = M5.Power.Axp2101.getIRQStatuses();
  ESP_LOGI(TAG, "IRQ status registers: 0x%016llx", irq_status);

  // Check specific IRQ states
  ESP_LOGI(TAG, "VBUS insert IRQ: %s", M5.Power.Axp2101.isVbusInsertIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "VBUS remove IRQ: %s", M5.Power.Axp2101.isVbusRemoveIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Battery insert IRQ: %s", M5.Power.Axp2101.isBatInsertIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Battery remove IRQ: %s", M5.Power.Axp2101.isBatRemoveIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "LDO over current IRQ: %s", M5.Power.Axp2101.isLdoOverCurrentIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "BATFET over current IRQ: %s", M5.Power.Axp2101.isBatfetOverCurrentIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Die over temperature IRQ: %s", M5.Power.Axp2101.isBatDieOverTemperatureIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Watchdog expire IRQ: %s", M5.Power.Axp2101.isWdtExpireIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Battery over voltage IRQ: %s", M5.Power.Axp2101.isBatOverVoltageIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Charger done IRQ: %s", M5.Power.Axp2101.isBatChagerDoneIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Charger start IRQ: %s", M5.Power.Axp2101.isBatChagerStartIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Charger timeout IRQ: %s", M5.Power.Axp2101.isChagerOverTimeoutIrq() ? "ACTIVE" : "inactive");

  // Power button states
  uint8_t pek_press = M5.Power.Axp2101.getPekPress();
  ESP_LOGI(TAG, "Power button press: %d", pek_press);
  ESP_LOGI(TAG, "Power button short press IRQ: %s", M5.Power.Axp2101.isPekeyShortPressIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Power button long press IRQ: %s", M5.Power.Axp2101.isPekeyLongPressIrq() ? "ACTIVE" : "inactive");

  // Temperature related IRQs
  ESP_LOGI(TAG, "Battery under temp (work) IRQ: %s",
           M5.Power.Axp2101.isBatWorkUnderTemperatureIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Battery over temp (work) IRQ: %s",
           M5.Power.Axp2101.isBatWorkOverTemperatureIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Battery under temp (charge) IRQ: %s",
           M5.Power.Axp2101.isBatChargerUnderTemperatureIrq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Battery over temp (charge) IRQ: %s",
           M5.Power.Axp2101.isBatChargerOverTemperatureIrq() ? "ACTIVE" : "inactive");

  // Warning levels
  ESP_LOGI(TAG, "Warning level 1 IRQ: %s", M5.Power.Axp2101.isDropWarningLevel1Irq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Warning level 2 IRQ: %s", M5.Power.Axp2101.isDropWarningLevel2Irq() ? "ACTIVE" : "inactive");
  ESP_LOGI(TAG, "Gauge WDT timeout IRQ: %s", M5.Power.Axp2101.isGaugeWdtTimeoutIrq() ? "ACTIVE" : "inactive");

  ESP_LOGI(TAG, "=== End AXP2101 Diagnostic ===");
}

void M5CoreS3Power::setup() {
  ESP_LOGI(TAG, "Setting up M5CoreS3 Power component");

  // M5 should already be initialized by board_m5cores3 component
  // Initialize update timer
  last_battery_update_ = 0;

  ESP_LOGI(TAG, "M5CoreS3 Power component setup complete");
}

void M5CoreS3Power::loop() {
  // Update battery sensors every 30 seconds
  uint32_t now = millis();
  if (now - last_battery_update_ >= 30000) {
    if (battery_sensor_ != nullptr) {
      float battery_level = M5.Power.Axp2101.getBatteryLevel();
      battery_sensor_->publish_state(battery_level);
    }

    if (battery_present_sensor_ != nullptr) {
      bool battery_present = M5.Power.Axp2101.getBatState();
      battery_present_sensor_->publish_state(battery_present);
    }

    if (battery_charging_sensor_ != nullptr) {
      bool battery_charging = M5.Power.Axp2101.isCharging();
      battery_charging_sensor_->publish_state(battery_charging);
    }

    last_battery_update_ = now;
  }
}

void M5CoreS3Power::dump_config() {
  ESP_LOGCONFIG(TAG, "M5CoreS3 Power:");
  if (battery_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Battery Level Sensor: %s", battery_sensor_->get_name().c_str());
  }
  if (battery_present_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Battery Present Sensor: %s", battery_present_sensor_->get_name().c_str());
  }
  if (battery_charging_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Battery Charging Sensor: %s", battery_charging_sensor_->get_name().c_str());
  }
}

}  // namespace m5cores3_power
}  // namespace esphome