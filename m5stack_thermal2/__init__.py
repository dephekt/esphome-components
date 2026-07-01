import esphome.codegen as cg
from esphome.components import binary_sensor, button, i2c, number, switch
import esphome.config_validation as cv
from esphome.components.thermal_camera_core import (
    CONF_ROI,
    CONF_TEMPERATURE_SENSORS,
    CONF_THERMAL_PALETTE,
    CONF_UPDATE_INTERVAL,
    CONF_WEB_SERVER,
    PALETTES,
    REFRESH_RATES,
    ThermalCameraBase,
    ThermalNumber,
    ThermalSwitch,
    common_control_schemas,
    register_common_controls,
    register_roi,
    register_temperature_sensors,
    register_web_server,
    roi_schema,
    temperature_sensors_schema,
    web_server_schema,
)
from esphome.const import CONF_ID

CODEOWNERS = ["@dephekt"]
DEPENDENCIES = ["esp32", "i2c"]
AUTO_LOAD = ["thermal_camera_core", "binary_sensor", "button"]

# M5Stack Unit Thermal2 namespace and component classes
m5stack_thermal2_ns = cg.esphome_ns.namespace("m5stack_thermal2")
M5Thermal2Component = m5stack_thermal2_ns.class_(
    "M5Thermal2Component", ThermalCameraBase
)
M5Thermal2Button = m5stack_thermal2_ns.class_("M5Thermal2Button", button.Button)

# Device-specific control type values (see thermal_camera_core.h:
# THERMAL_CONTROL_TYPE_EXTRA_START)
M5Thermal2ControlType = m5stack_thermal2_ns.enum("M5Thermal2ControlType")
BUZZER_ENABLED = M5Thermal2ControlType.BUZZER_ENABLED
ALARM_HIGH_THRESHOLD = M5Thermal2ControlType.ALARM_HIGH_THRESHOLD
ALARM_LOW_THRESHOLD = M5Thermal2ControlType.ALARM_LOW_THRESHOLD

# Alarm enums (mirror the C++ enums)
AlarmSource = m5stack_thermal2_ns.enum("AlarmSource")
ALARM_SOURCES = {
    "average": AlarmSource.ALARM_SOURCE_AVERAGE,
    "median": AlarmSource.ALARM_SOURCE_MEDIAN,
    "min": AlarmSource.ALARM_SOURCE_MIN,
    "max": AlarmSource.ALARM_SOURCE_MAX,
}
AlarmRegion = m5stack_thermal2_ns.enum("AlarmRegion")
ALARM_REGIONS = {
    "active": AlarmRegion.ALARM_REGION_ACTIVE,
    "frame": AlarmRegion.ALARM_REGION_FRAME,
    "roi": AlarmRegion.ALARM_REGION_ROI,
}

CONF_REFRESH_RATE = "refresh_rate"
CONF_NOISE_FILTER = "noise_filter"
CONF_STATUS_LED = "status_led"

CONF_ALARM = "alarm"
CONF_ALARM_ENABLED = "enabled"
CONF_ALARM_SOURCE = "source"
CONF_ALARM_REGION = "region"
CONF_ALARM_HIGH_THRESHOLD = "high_threshold"
CONF_ALARM_LOW_THRESHOLD = "low_threshold"
CONF_ALARM_HYSTERESIS = "hysteresis"
CONF_ALARM_BUZZER_FREQUENCY = "buzzer_frequency"
CONF_ALARM_BUZZER_VOLUME = "buzzer_volume"
CONF_ALARM_BEEP_INTERVAL = "beep_interval"
CONF_ALARM_LED_COLOR = "led_color"

CONF_ALARM_ACTIVE = "alarm_active"
CONF_BUTTON = "button"
CONF_ALARM_TEST_BUTTON = "alarm_test_button"

CONF_BUZZER_ENABLED_CONTROL = "buzzer_enabled_control"
CONF_ALARM_HIGH_THRESHOLD_CONTROL = "alarm_high_threshold_control"
CONF_ALARM_LOW_THRESHOLD_CONTROL = "alarm_low_threshold_control"


def validate_rgb(value):
    value = cv.ensure_list(cv.int_range(min=0, max=255))(value)
    if len(value) != 3:
        raise cv.Invalid("led_color must be a list of 3 values: [r, g, b]")
    return value


def validate_alarm(config):
    if config[CONF_ALARM_LOW_THRESHOLD] >= config[CONF_ALARM_HIGH_THRESHOLD]:
        raise cv.Invalid(
            f"'{CONF_ALARM_LOW_THRESHOLD}' ({config[CONF_ALARM_LOW_THRESHOLD]}) must be "
            f"below '{CONF_ALARM_HIGH_THRESHOLD}' ({config[CONF_ALARM_HIGH_THRESHOLD]})"
        )
    return config


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Thermal2Component),
            cv.Optional(CONF_REFRESH_RATE, default="16Hz"): cv.one_of(*REFRESH_RATES),
            cv.Optional(CONF_NOISE_FILTER, default=8): cv.int_range(min=0, max=15),
            cv.Optional(CONF_UPDATE_INTERVAL, default=2000): cv.positive_int,
            cv.Optional(CONF_THERMAL_PALETTE, default="rainbow"): cv.one_of(*PALETTES),
            cv.Optional(CONF_STATUS_LED, default=True): cv.boolean,
            cv.Optional(CONF_ROI): roi_schema(),
            cv.Optional(CONF_ALARM): cv.All(
                cv.Schema(
                    {
                        cv.Optional(CONF_ALARM_ENABLED, default=True): cv.boolean,
                        cv.Optional(CONF_ALARM_SOURCE, default="average"): cv.enum(
                            ALARM_SOURCES, lower=True
                        ),
                        cv.Optional(CONF_ALARM_REGION, default="active"): cv.enum(
                            ALARM_REGIONS, lower=True
                        ),
                        cv.Optional(CONF_ALARM_HIGH_THRESHOLD, default=35.0): cv.float_,
                        cv.Optional(CONF_ALARM_LOW_THRESHOLD, default=5.0): cv.float_,
                        cv.Optional(CONF_ALARM_HYSTERESIS, default=0.5): cv.positive_float,
                        cv.Optional(
                            CONF_ALARM_BUZZER_FREQUENCY, default=4000
                        ): cv.int_range(min=0, max=20000),
                        cv.Optional(CONF_ALARM_BUZZER_VOLUME, default=96): cv.int_range(
                            min=0, max=255
                        ),
                        cv.Optional(CONF_ALARM_BEEP_INTERVAL, default=250): cv.int_range(
                            min=50, max=2550
                        ),
                        cv.Optional(
                            CONF_ALARM_LED_COLOR, default=[16, 0, 0]
                        ): validate_rgb,
                    }
                ),
                validate_alarm,
            ),
            cv.Optional(CONF_TEMPERATURE_SENSORS): temperature_sensors_schema(),
            cv.Optional(CONF_ALARM_ACTIVE): binary_sensor.binary_sensor_schema(
                device_class="problem"
            ),
            cv.Optional(CONF_BUTTON): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_ALARM_TEST_BUTTON): button.button_schema(M5Thermal2Button),
            cv.Optional(CONF_WEB_SERVER): web_server_schema(),
            # Device-specific auto-generated control entities
            cv.Optional(CONF_BUZZER_ENABLED_CONTROL): switch.switch_schema(
                ThermalSwitch
            ),
            cv.Optional(CONF_ALARM_HIGH_THRESHOLD_CONTROL): number.number_schema(
                ThermalNumber, unit_of_measurement="°C"
            ).extend(
                {
                    cv.Optional("min_value", default=-20.0): cv.float_,
                    cv.Optional("max_value", default=120.0): cv.float_,
                    cv.Optional("step", default=0.5): cv.positive_float,
                }
            ),
            cv.Optional(CONF_ALARM_LOW_THRESHOLD_CONTROL): number.number_schema(
                ThermalNumber, unit_of_measurement="°C"
            ).extend(
                {
                    cv.Optional("min_value", default=-40.0): cv.float_,
                    cv.Optional("max_value", default=100.0): cv.float_,
                    cv.Optional("step", default=0.5): cv.positive_float,
                }
            ),
        }
    )
    .extend(common_control_schemas())
    .extend(i2c.i2c_device_schema(0x32))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # JPEG encoder for the /thermal.jpg endpoint (no Wire/SPI: native ESPHome I2C)
    cg.add_platformio_option("lib_deps", ["https://github.com/bitbank2/JPEGENC.git"])

    # Basic configuration
    cg.add(var.set_refresh_rate(config[CONF_REFRESH_RATE]))
    cg.add(var.set_noise_filter(config[CONF_NOISE_FILTER]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_thermal_palette(config[CONF_THERMAL_PALETTE]))
    cg.add(var.set_status_led_enabled(config[CONF_STATUS_LED]))

    await register_roi(var, config)

    # Alarm configuration
    alarm = config.get(CONF_ALARM, {})
    alarm_high = alarm.get(CONF_ALARM_HIGH_THRESHOLD, 35.0)
    alarm_low = alarm.get(CONF_ALARM_LOW_THRESHOLD, 5.0)
    if CONF_ALARM in config:
        # Arm the alarm only when an `alarm:` block is present (and enabled), so a
        # camera-only config never beeps/flashes on its own.
        cg.add(var.set_alarm_enabled(alarm[CONF_ALARM_ENABLED]))
        cg.add(var.set_alarm_source(alarm[CONF_ALARM_SOURCE]))
        cg.add(var.set_alarm_region(alarm[CONF_ALARM_REGION]))
        cg.add(var.set_alarm_high_threshold(alarm_high))
        cg.add(var.set_alarm_low_threshold(alarm_low))
        cg.add(var.set_alarm_hysteresis(alarm[CONF_ALARM_HYSTERESIS]))
        cg.add(var.set_alarm_buzzer_frequency(alarm[CONF_ALARM_BUZZER_FREQUENCY]))
        cg.add(var.set_alarm_buzzer_volume(alarm[CONF_ALARM_BUZZER_VOLUME]))
        cg.add(var.set_alarm_beep_interval(alarm[CONF_ALARM_BEEP_INTERVAL]))
        led = alarm[CONF_ALARM_LED_COLOR]
        cg.add(var.set_alarm_led_color(led[0], led[1], led[2]))

    await register_temperature_sensors(var, config)

    # Status binary sensors
    if CONF_ALARM_ACTIVE in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_ALARM_ACTIVE])
        cg.add(var.set_alarm_active_sensor(bsens))
    if CONF_BUTTON in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_BUTTON])
        cg.add(var.set_button_sensor(bsens))

    if CONF_ALARM_TEST_BUTTON in config:
        btn = await button.new_button(config[CONF_ALARM_TEST_BUTTON])
        cg.add(btn.set_m5stack_thermal2_parent(var))

    await register_web_server(var, config)

    # --- Auto-generated control entities ---
    await register_common_controls(var, config)

    if CONF_BUZZER_ENABLED_CONTROL in config:
        switch_config = config[CONF_BUZZER_ENABLED_CONTROL]
        switch_var = cg.new_Pvariable(switch_config[CONF_ID])
        await cg.register_component(switch_var, switch_config)
        await switch.register_switch(switch_var, switch_config)
        cg.add(switch_var.set_thermal_parent(var))
        cg.add(switch_var.set_control_type(BUZZER_ENABLED))
        cg.add(
            switch_var.set_restore_mode(
                switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON
            )
        )
        cg.add(var.set_buzzer_enabled_control(switch_var))

    if CONF_ALARM_HIGH_THRESHOLD_CONTROL in config:
        num_config = config[CONF_ALARM_HIGH_THRESHOLD_CONTROL]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await cg.register_component(num, num_config)
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(num.set_thermal_parent(var))
        cg.add(num.set_control_type(ALARM_HIGH_THRESHOLD))
        cg.add(num.set_restore_value(True))
        cg.add(num.set_initial_value(alarm_high))
        cg.add(var.set_alarm_high_threshold_control(num))

    if CONF_ALARM_LOW_THRESHOLD_CONTROL in config:
        num_config = config[CONF_ALARM_LOW_THRESHOLD_CONTROL]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await cg.register_component(num, num_config)
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(num.set_thermal_parent(var))
        cg.add(num.set_control_type(ALARM_LOW_THRESHOLD))
        cg.add(num.set_restore_value(True))
        cg.add(num.set_initial_value(alarm_low))
        cg.add(var.set_alarm_low_threshold_control(num))
