import esphome.codegen as cg
from esphome.components import binary_sensor, i2c, number, select, sensor, switch
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Conditional import for web server functionality
try:
    from esphome.components import web_server_base

    WEB_SERVER_BASE_AVAILABLE = True
except ImportError:
    WEB_SERVER_BASE_AVAILABLE = False

CODEOWNERS = ["@dephekt"]
DEPENDENCIES = ["esp32", "i2c"]
AUTO_LOAD = ["sensor", "binary_sensor", "number", "select", "switch"]

# M5Stack Unit Thermal2 namespace and component classes
m5stack_thermal2_ns = cg.esphome_ns.namespace("m5stack_thermal2")
M5Thermal2Component = m5stack_thermal2_ns.class_(
    "M5Thermal2Component", cg.Component, i2c.I2CDevice
)
M5Thermal2Number = m5stack_thermal2_ns.class_(
    "M5Thermal2Number", number.Number, cg.Component
)
M5Thermal2Select = m5stack_thermal2_ns.class_(
    "M5Thermal2Select", select.Select, cg.Component
)
M5Thermal2Switch = m5stack_thermal2_ns.class_(
    "M5Thermal2Switch", switch.Switch, cg.Component
)

# Control type enum
M5Thermal2ControlType = m5stack_thermal2_ns.enum("M5Thermal2ControlType")
UPDATE_INTERVAL = M5Thermal2ControlType.UPDATE_INTERVAL
ROI_CENTER_ROW = M5Thermal2ControlType.ROI_CENTER_ROW
ROI_CENTER_COL = M5Thermal2ControlType.ROI_CENTER_COL
ROI_SIZE = M5Thermal2ControlType.ROI_SIZE
ROI_ENABLED = M5Thermal2ControlType.ROI_ENABLED
WEB_OVERLAY_ENABLED = M5Thermal2ControlType.WEB_OVERLAY_ENABLED
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

PALETTES = [
    "rainbow",
    "golden",
    "grayscale",
    "ironblack",
    "cam",
    "ironbow",
    "arctic",
    "lava",
    "whitehot",
    "blackhot",
]

CONF_REFRESH_RATE = "refresh_rate"
CONF_NOISE_FILTER = "noise_filter"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_THERMAL_PALETTE = "thermal_palette"
CONF_STATUS_LED = "status_led"

CONF_ROI = "roi"
CONF_ROI_ENABLED = "enabled"
CONF_ROI_CENTER_ROW = "center_row"
CONF_ROI_CENTER_COL = "center_col"
CONF_ROI_SIZE = "size"

CONF_ALARM = "alarm"
CONF_ALARM_SOURCE = "source"
CONF_ALARM_REGION = "region"
CONF_ALARM_HIGH_THRESHOLD = "high_threshold"
CONF_ALARM_LOW_THRESHOLD = "low_threshold"
CONF_ALARM_HYSTERESIS = "hysteresis"
CONF_ALARM_BUZZER_FREQUENCY = "buzzer_frequency"
CONF_ALARM_BUZZER_VOLUME = "buzzer_volume"
CONF_ALARM_BEEP_INTERVAL = "beep_interval"
CONF_ALARM_LED_COLOR = "led_color"

CONF_TEMPERATURE_SENSORS = "temperature_sensors"
CONF_ALARM_ACTIVE = "alarm_active"
CONF_BUTTON = "button"

CONF_WEB_SERVER = "web_server"
CONF_WEB_ENABLE = "enable"
CONF_WEB_PATH = "path"
CONF_WEB_QUALITY = "quality"
CONF_WEB_OVERLAY_ENABLED = "overlay_enabled"
CONF_WEB_HTML_PAGE = "html_page"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"

# User control configuration
CONF_UPDATE_INTERVAL_CONTROL = "update_interval_control"
CONF_THERMAL_PALETTE_CONTROL = "thermal_palette_control"
CONF_ROI_ENABLED_CONTROL = "roi_enabled_control"
CONF_ROI_CENTER_ROW_CONTROL = "roi_center_row_control"
CONF_ROI_CENTER_COL_CONTROL = "roi_center_col_control"
CONF_ROI_SIZE_CONTROL = "roi_size_control"
CONF_WEB_OVERLAY_ENABLED_CONTROL = "web_overlay_enabled_control"
CONF_BUZZER_ENABLED_CONTROL = "buzzer_enabled_control"
CONF_ALARM_HIGH_THRESHOLD_CONTROL = "alarm_high_threshold_control"
CONF_ALARM_LOW_THRESHOLD_CONTROL = "alarm_low_threshold_control"

ROIConfig = m5stack_thermal2_ns.struct("ROIConfig")


def validate_rgb(value):
    value = cv.ensure_list(cv.int_range(min=0, max=255))(value)
    if len(value) != 3:
        raise cv.Invalid("led_color must be a list of 3 values: [r, g, b]")
    return value


def validate_web_server_config(config):
    if config.get(CONF_WEB_ENABLE, False) and CONF_WEB_SERVER_BASE_ID not in config:
        raise cv.Invalid(
            f"'{CONF_WEB_SERVER_BASE_ID}' is required when web server is enabled"
        )
    return config


def temperature_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement="°C",
        device_class="temperature",
        state_class="measurement",
        accuracy_decimals=1,
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5Thermal2Component),
        cv.Optional(CONF_REFRESH_RATE, default="16Hz"): cv.one_of(
            "0.5Hz", "1Hz", "2Hz", "4Hz", "8Hz", "16Hz", "32Hz", "64Hz"
        ),
        cv.Optional(CONF_NOISE_FILTER, default=8): cv.int_range(min=0, max=15),
        cv.Optional(CONF_UPDATE_INTERVAL, default=2000): cv.positive_int,
        cv.Optional(CONF_THERMAL_PALETTE, default="rainbow"): cv.one_of(*PALETTES),
        cv.Optional(CONF_STATUS_LED, default=True): cv.boolean,
        cv.Optional(CONF_ROI): cv.Schema(
            {
                cv.Optional(CONF_ROI_ENABLED, default=False): cv.boolean,
                cv.Optional(CONF_ROI_CENTER_ROW, default=12): cv.int_range(
                    min=1, max=24
                ),
                cv.Optional(CONF_ROI_CENTER_COL, default=16): cv.int_range(
                    min=1, max=32
                ),
                cv.Optional(CONF_ROI_SIZE, default=2): cv.int_range(min=1, max=10),
            }
        ),
        cv.Optional(CONF_ALARM): cv.Schema(
            {
                cv.Optional(CONF_ALARM_SOURCE, default="average"): cv.enum(
                    ALARM_SOURCES, lower=True
                ),
                cv.Optional(CONF_ALARM_REGION, default="active"): cv.enum(
                    ALARM_REGIONS, lower=True
                ),
                cv.Optional(CONF_ALARM_HIGH_THRESHOLD, default=35.0): cv.float_,
                cv.Optional(CONF_ALARM_LOW_THRESHOLD, default=5.0): cv.float_,
                cv.Optional(CONF_ALARM_HYSTERESIS, default=0.5): cv.positive_float,
                cv.Optional(CONF_ALARM_BUZZER_FREQUENCY, default=4000): cv.int_range(
                    min=0, max=20000
                ),
                cv.Optional(CONF_ALARM_BUZZER_VOLUME, default=96): cv.int_range(
                    min=0, max=255
                ),
                cv.Optional(CONF_ALARM_BEEP_INTERVAL, default=250): cv.int_range(
                    min=50, max=2550
                ),
                cv.Optional(CONF_ALARM_LED_COLOR, default=[16, 0, 0]): validate_rgb,
            }
        ),
        cv.Optional(CONF_TEMPERATURE_SENSORS): cv.Schema(
            {
                cv.Optional("min"): temperature_sensor_schema(),
                cv.Optional("max"): temperature_sensor_schema(),
                cv.Optional("avg"): temperature_sensor_schema(),
                cv.Optional("median"): temperature_sensor_schema(),
                cv.Optional("roi_min"): temperature_sensor_schema(),
                cv.Optional("roi_max"): temperature_sensor_schema(),
                cv.Optional("roi_avg"): temperature_sensor_schema(),
            }
        ),
        cv.Optional(CONF_ALARM_ACTIVE): binary_sensor.binary_sensor_schema(
            device_class="problem"
        ),
        cv.Optional(CONF_BUTTON): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_WEB_SERVER): cv.All(
            cv.Schema(
                {
                    cv.Optional(CONF_WEB_ENABLE, default=False): cv.boolean,
                    cv.Optional(CONF_WEB_PATH, default="/thermal.jpg"): cv.string,
                    cv.Optional(CONF_WEB_QUALITY, default=85): cv.int_range(
                        min=10, max=100
                    ),
                    cv.Optional(CONF_WEB_OVERLAY_ENABLED, default=True): cv.boolean,
                    cv.Optional(CONF_WEB_HTML_PAGE, default=True): cv.boolean,
                    cv.Optional(CONF_WEB_SERVER_BASE_ID): cv.All(
                        cv.requires_component("web_server_base")
                        if WEB_SERVER_BASE_AVAILABLE
                        else cv.valid,
                        cv.use_id(web_server_base.WebServerBase)
                        if WEB_SERVER_BASE_AVAILABLE
                        else cv.string,
                    ),
                }
            ),
            validate_web_server_config,
        ),
        # User control entities (auto-generated by component)
        cv.Optional(CONF_UPDATE_INTERVAL_CONTROL): number.number_schema(
            M5Thermal2Number
        ).extend(
            {
                cv.Optional("min_value", default=100): cv.positive_int,
                cv.Optional("max_value", default=30000): cv.positive_int,
                cv.Optional("step", default=100): cv.positive_int,
            }
        ),
        cv.Optional(CONF_THERMAL_PALETTE_CONTROL): select.select_schema(
            M5Thermal2Select
        ),
        cv.Optional(CONF_ROI_ENABLED_CONTROL): switch.switch_schema(M5Thermal2Switch),
        cv.Optional(CONF_ROI_CENTER_ROW_CONTROL): number.number_schema(
            M5Thermal2Number
        ).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=24): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_ROI_CENTER_COL_CONTROL): number.number_schema(
            M5Thermal2Number
        ).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=32): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_ROI_SIZE_CONTROL): number.number_schema(
            M5Thermal2Number
        ).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=10): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_WEB_OVERLAY_ENABLED_CONTROL): switch.switch_schema(
            M5Thermal2Switch
        ),
        cv.Optional(CONF_BUZZER_ENABLED_CONTROL): switch.switch_schema(
            M5Thermal2Switch
        ),
        cv.Optional(CONF_ALARM_HIGH_THRESHOLD_CONTROL): number.number_schema(
            M5Thermal2Number, unit_of_measurement="°C"
        ).extend(
            {
                cv.Optional("min_value", default=-20.0): cv.float_,
                cv.Optional("max_value", default=120.0): cv.float_,
                cv.Optional("step", default=0.5): cv.positive_float,
            }
        ),
        cv.Optional(CONF_ALARM_LOW_THRESHOLD_CONTROL): number.number_schema(
            M5Thermal2Number, unit_of_measurement="°C"
        ).extend(
            {
                cv.Optional("min_value", default=-40.0): cv.float_,
                cv.Optional("max_value", default=100.0): cv.float_,
                cv.Optional("step", default=0.5): cv.positive_float,
            }
        ),
    }
).extend(i2c.i2c_device_schema(0x32))


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

    # ROI configuration
    if CONF_ROI in config:
        roi_config = config[CONF_ROI]
        roi_struct = cg.StructInitializer(
            ROIConfig,
            ("enabled", roi_config[CONF_ROI_ENABLED]),
            ("center_row", roi_config[CONF_ROI_CENTER_ROW]),
            ("center_col", roi_config[CONF_ROI_CENTER_COL]),
            ("size", roi_config[CONF_ROI_SIZE]),
        )
        cg.add(var.set_roi_config(roi_struct))

    # Alarm configuration
    alarm = config.get(CONF_ALARM, {})
    alarm_high = alarm.get(CONF_ALARM_HIGH_THRESHOLD, 35.0)
    alarm_low = alarm.get(CONF_ALARM_LOW_THRESHOLD, 5.0)
    if CONF_ALARM in config:
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

    # Temperature sensors
    if CONF_TEMPERATURE_SENSORS in config:
        temp_sensors = config[CONF_TEMPERATURE_SENSORS]
        if "min" in temp_sensors:
            sens = await sensor.new_sensor(temp_sensors["min"])
            cg.add(var.set_temperature_min_sensor(sens))
        if "max" in temp_sensors:
            sens = await sensor.new_sensor(temp_sensors["max"])
            cg.add(var.set_temperature_max_sensor(sens))
        if "avg" in temp_sensors:
            sens = await sensor.new_sensor(temp_sensors["avg"])
            cg.add(var.set_temperature_avg_sensor(sens))
        if "median" in temp_sensors:
            sens = await sensor.new_sensor(temp_sensors["median"])
            cg.add(var.set_median_sensor(sens))
        if "roi_min" in temp_sensors:
            sens = await sensor.new_sensor(temp_sensors["roi_min"])
            cg.add(var.set_roi_min_sensor(sens))
        if "roi_max" in temp_sensors:
            sens = await sensor.new_sensor(temp_sensors["roi_max"])
            cg.add(var.set_roi_max_sensor(sens))
        if "roi_avg" in temp_sensors:
            sens = await sensor.new_sensor(temp_sensors["roi_avg"])
            cg.add(var.set_roi_avg_sensor(sens))

    # Status binary sensors
    if CONF_ALARM_ACTIVE in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_ALARM_ACTIVE])
        cg.add(var.set_alarm_active_sensor(bsens))
    if CONF_BUTTON in config:
        bsens = await binary_sensor.new_binary_sensor(config[CONF_BUTTON])
        cg.add(var.set_button_sensor(bsens))

    # Web server
    if CONF_WEB_SERVER in config:
        web_config = config[CONF_WEB_SERVER]
        if web_config.get(CONF_WEB_ENABLE, False):
            web_server_base_var = await cg.get_variable(
                web_config[CONF_WEB_SERVER_BASE_ID]
            )
            cg.add(var.set_web_server_base(web_server_base_var))
            cg.add(var.set_web_server_enabled(True))
            cg.add(var.set_web_server_path(web_config[CONF_WEB_PATH]))
            cg.add(var.set_web_server_quality(web_config[CONF_WEB_QUALITY]))
            cg.add(var.set_web_overlay_enabled(web_config[CONF_WEB_OVERLAY_ENABLED]))
            cg.add(var.set_web_html_page_enabled(web_config[CONF_WEB_HTML_PAGE]))

    # --- Auto-generated control entities ---
    if CONF_UPDATE_INTERVAL_CONTROL in config:
        num_config = config[CONF_UPDATE_INTERVAL_CONTROL]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await cg.register_component(num, num_config)
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(num.set_m5stack_thermal2_parent(var))
        cg.add(num.set_control_type(UPDATE_INTERVAL))
        cg.add(num.set_restore_value(True))
        cg.add(num.set_initial_value(config[CONF_UPDATE_INTERVAL]))
        cg.add(var.set_update_interval_control(num))

    if CONF_THERMAL_PALETTE_CONTROL in config:
        sel_config = config[CONF_THERMAL_PALETTE_CONTROL]
        sel = cg.new_Pvariable(sel_config[CONF_ID])
        await cg.register_component(sel, sel_config)
        await select.register_select(sel, sel_config, options=PALETTES)
        cg.add(sel.set_m5stack_thermal2_parent(var))
        cg.add(sel.set_restore_value(True))
        cg.add(sel.set_initial_option(config[CONF_THERMAL_PALETTE]))
        cg.add(var.set_thermal_palette_control(sel))

    if CONF_ROI_ENABLED_CONTROL in config:
        switch_config = config[CONF_ROI_ENABLED_CONTROL]
        switch_var = cg.new_Pvariable(switch_config[CONF_ID])
        await cg.register_component(switch_var, switch_config)
        await switch.register_switch(switch_var, switch_config)
        cg.add(switch_var.set_m5stack_thermal2_parent(var))
        cg.add(switch_var.set_control_type(ROI_ENABLED))
        roi_default_on = CONF_ROI in config and config[CONF_ROI][CONF_ROI_ENABLED]
        cg.add(
            switch_var.set_restore_mode(
                switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON
                if roi_default_on
                else switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_OFF
            )
        )
        cg.add(var.set_roi_enabled_control(switch_var))

    if CONF_ROI_CENTER_ROW_CONTROL in config:
        num_config = config[CONF_ROI_CENTER_ROW_CONTROL]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await cg.register_component(num, num_config)
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(num.set_m5stack_thermal2_parent(var))
        cg.add(num.set_control_type(ROI_CENTER_ROW))
        cg.add(num.set_restore_value(True))
        initial_row = config[CONF_ROI][CONF_ROI_CENTER_ROW] if CONF_ROI in config else 12
        cg.add(num.set_initial_value(initial_row))
        cg.add(var.set_roi_center_row_control(num))

    if CONF_ROI_CENTER_COL_CONTROL in config:
        num_config = config[CONF_ROI_CENTER_COL_CONTROL]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await cg.register_component(num, num_config)
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(num.set_m5stack_thermal2_parent(var))
        cg.add(num.set_control_type(ROI_CENTER_COL))
        cg.add(num.set_restore_value(True))
        initial_col = config[CONF_ROI][CONF_ROI_CENTER_COL] if CONF_ROI in config else 16
        cg.add(num.set_initial_value(initial_col))
        cg.add(var.set_roi_center_col_control(num))

    if CONF_ROI_SIZE_CONTROL in config:
        num_config = config[CONF_ROI_SIZE_CONTROL]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await cg.register_component(num, num_config)
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(num.set_m5stack_thermal2_parent(var))
        cg.add(num.set_control_type(ROI_SIZE))
        cg.add(num.set_restore_value(True))
        initial_size = config[CONF_ROI][CONF_ROI_SIZE] if CONF_ROI in config else 2
        cg.add(num.set_initial_value(initial_size))
        cg.add(var.set_roi_size_control(num))

    if CONF_WEB_OVERLAY_ENABLED_CONTROL in config:
        switch_config = config[CONF_WEB_OVERLAY_ENABLED_CONTROL]
        switch_var = cg.new_Pvariable(switch_config[CONF_ID])
        await cg.register_component(switch_var, switch_config)
        await switch.register_switch(switch_var, switch_config)
        cg.add(switch_var.set_m5stack_thermal2_parent(var))
        cg.add(switch_var.set_control_type(WEB_OVERLAY_ENABLED))
        cg.add(
            switch_var.set_restore_mode(
                switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON
            )
        )
        cg.add(var.set_web_overlay_enabled_control(switch_var))

    if CONF_BUZZER_ENABLED_CONTROL in config:
        switch_config = config[CONF_BUZZER_ENABLED_CONTROL]
        switch_var = cg.new_Pvariable(switch_config[CONF_ID])
        await cg.register_component(switch_var, switch_config)
        await switch.register_switch(switch_var, switch_config)
        cg.add(switch_var.set_m5stack_thermal2_parent(var))
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
        cg.add(num.set_m5stack_thermal2_parent(var))
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
        cg.add(num.set_m5stack_thermal2_parent(var))
        cg.add(num.set_control_type(ALARM_LOW_THRESHOLD))
        cg.add(num.set_restore_value(True))
        cg.add(num.set_initial_value(alarm_low))
        cg.add(var.set_alarm_low_threshold_control(num))
