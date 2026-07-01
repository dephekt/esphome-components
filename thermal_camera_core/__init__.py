"""Shared base for thermal-camera components (M5Stack Thermal2, bare MLX90640,
...). Compile-only: this component has no CONFIG_SCHEMA / YAML key of its
own (mirrors the esphome-core `climate_ir` precedent) -- device components
`AUTO_LOAD` it, subclass `ThermalCameraBase` in C++, and reuse the schema
fragments + codegen helpers below.
"""

import esphome.codegen as cg
from esphome.components import i2c, number, select, sensor, switch
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Conditional import for web server functionality
try:
    from esphome.components import web_server_base

    WEB_SERVER_BASE_AVAILABLE = True
except ImportError:
    WEB_SERVER_BASE_AVAILABLE = False

CODEOWNERS = ["@dephekt"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "number", "select", "switch"]

thermal_camera_core_ns = cg.esphome_ns.namespace("thermal_camera_core")
ThermalCameraBase = thermal_camera_core_ns.class_(
    "ThermalCameraBase", cg.Component, i2c.I2CDevice
)
ThermalNumber = thermal_camera_core_ns.class_(
    "ThermalNumber", number.Number, cg.Component
)
ThermalSelect = thermal_camera_core_ns.class_(
    "ThermalSelect", select.Select, cg.Component
)
ThermalSwitch = thermal_camera_core_ns.class_(
    "ThermalSwitch", switch.Switch, cg.Component
)

# Control type enum (the 7 values common to every thermal-camera device;
# device-specific extras start at ThermalControlType::THERMAL_CONTROL_TYPE_EXTRA_START)
ThermalControlType = thermal_camera_core_ns.enum("ThermalControlType")
UPDATE_INTERVAL = ThermalControlType.UPDATE_INTERVAL
ROI_CENTER_ROW = ThermalControlType.ROI_CENTER_ROW
ROI_CENTER_COL = ThermalControlType.ROI_CENTER_COL
ROI_SIZE = ThermalControlType.ROI_SIZE
THERMAL_PALETTE = ThermalControlType.THERMAL_PALETTE
ROI_ENABLED = ThermalControlType.ROI_ENABLED
WEB_OVERLAY_ENABLED = ThermalControlType.WEB_OVERLAY_ENABLED

ROIConfig = thermal_camera_core_ns.struct("ROIConfig")

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

# Refresh-rate values shared by every device; the register/API call that
# applies the value is device-specific (each component keeps its own
# parse_refresh_rate_()).
REFRESH_RATES = ["0.5Hz", "1Hz", "2Hz", "4Hz", "8Hz", "16Hz", "32Hz", "64Hz"]

CONF_UPDATE_INTERVAL = "update_interval"
CONF_THERMAL_PALETTE = "thermal_palette"

CONF_ROI = "roi"
CONF_ROI_ENABLED = "enabled"
CONF_ROI_CENTER_ROW = "center_row"
CONF_ROI_CENTER_COL = "center_col"
CONF_ROI_SIZE = "size"

CONF_TEMPERATURE_SENSORS = "temperature_sensors"

CONF_WEB_SERVER = "web_server"
CONF_WEB_ENABLE = "enable"
CONF_WEB_PATH = "path"
CONF_WEB_QUALITY = "quality"
CONF_WEB_OVERLAY_ENABLED = "overlay_enabled"
CONF_WEB_HTML_PAGE = "html_page"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"

# User control configuration (the 7 common controls)
CONF_UPDATE_INTERVAL_CONTROL = "update_interval_control"
CONF_THERMAL_PALETTE_CONTROL = "thermal_palette_control"
CONF_ROI_ENABLED_CONTROL = "roi_enabled_control"
CONF_ROI_CENTER_ROW_CONTROL = "roi_center_row_control"
CONF_ROI_CENTER_COL_CONTROL = "roi_center_col_control"
CONF_ROI_SIZE_CONTROL = "roi_size_control"
CONF_WEB_OVERLAY_ENABLED_CONTROL = "web_overlay_enabled_control"


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


def roi_schema():
    """The `roi:` sub-schema shared by every thermal-camera device."""
    return cv.Schema(
        {
            cv.Optional(CONF_ROI_ENABLED, default=False): cv.boolean,
            cv.Optional(CONF_ROI_CENTER_ROW, default=12): cv.int_range(min=1, max=24),
            cv.Optional(CONF_ROI_CENTER_COL, default=16): cv.int_range(min=1, max=32),
            cv.Optional(CONF_ROI_SIZE, default=2): cv.int_range(min=1, max=10),
        }
    )


def web_server_schema():
    """The `web_server:` sub-schema shared by every thermal-camera device."""
    return cv.All(
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
    )


def temperature_sensors_schema():
    """The `temperature_sensors:` sub-schema shared by every thermal-camera device."""
    return cv.Schema(
        {
            cv.Optional("min"): temperature_sensor_schema(),
            cv.Optional("max"): temperature_sensor_schema(),
            cv.Optional("avg"): temperature_sensor_schema(),
            cv.Optional("median"): temperature_sensor_schema(),
            cv.Optional("roi_min"): temperature_sensor_schema(),
            cv.Optional("roi_max"): temperature_sensor_schema(),
            cv.Optional("roi_avg"): temperature_sensor_schema(),
        }
    )


def common_control_schemas():
    """The 7 common auto-generated control entities, keyed the same way in
    every thermal-camera device's CONFIG_SCHEMA."""
    return {
        cv.Optional(CONF_UPDATE_INTERVAL_CONTROL): number.number_schema(
            ThermalNumber
        ).extend(
            {
                cv.Optional("min_value", default=100): cv.positive_int,
                cv.Optional("max_value", default=30000): cv.positive_int,
                cv.Optional("step", default=100): cv.positive_int,
            }
        ),
        cv.Optional(CONF_THERMAL_PALETTE_CONTROL): select.select_schema(
            ThermalSelect
        ),
        cv.Optional(CONF_ROI_ENABLED_CONTROL): switch.switch_schema(ThermalSwitch),
        cv.Optional(CONF_ROI_CENTER_ROW_CONTROL): number.number_schema(
            ThermalNumber
        ).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=24): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_ROI_CENTER_COL_CONTROL): number.number_schema(
            ThermalNumber
        ).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=32): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_ROI_SIZE_CONTROL): number.number_schema(
            ThermalNumber
        ).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=10): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_WEB_OVERLAY_ENABLED_CONTROL): switch.switch_schema(
            ThermalSwitch
        ),
    }


async def register_roi(var, config):
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


async def register_temperature_sensors(var, config):
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


async def register_web_server(var, config):
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


async def register_common_controls(var, config):
    """Wires up the 7 auto-generated common control entities. Call after
    `register_roi`/`register_temperature_sensors`/`register_web_server`."""
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
        cg.add(num.set_thermal_parent(var))
        cg.add(num.set_control_type(UPDATE_INTERVAL))
        cg.add(num.set_restore_value(True))
        cg.add(num.set_initial_value(config[CONF_UPDATE_INTERVAL]))
        cg.add(var.set_update_interval_control(num))

    if CONF_THERMAL_PALETTE_CONTROL in config:
        sel_config = config[CONF_THERMAL_PALETTE_CONTROL]
        sel = cg.new_Pvariable(sel_config[CONF_ID])
        await cg.register_component(sel, sel_config)
        await select.register_select(sel, sel_config, options=PALETTES)
        cg.add(sel.set_thermal_parent(var))
        cg.add(sel.set_restore_value(True))
        cg.add(sel.set_initial_option(config[CONF_THERMAL_PALETTE]))
        cg.add(var.set_thermal_palette_control(sel))

    if CONF_ROI_ENABLED_CONTROL in config:
        switch_config = config[CONF_ROI_ENABLED_CONTROL]
        switch_var = cg.new_Pvariable(switch_config[CONF_ID])
        await cg.register_component(switch_var, switch_config)
        await switch.register_switch(switch_var, switch_config)
        cg.add(switch_var.set_thermal_parent(var))
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
        cg.add(num.set_thermal_parent(var))
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
        cg.add(num.set_thermal_parent(var))
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
        cg.add(num.set_thermal_parent(var))
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
        cg.add(switch_var.set_thermal_parent(var))
        cg.add(switch_var.set_control_type(WEB_OVERLAY_ENABLED))
        cg.add(
            switch_var.set_restore_mode(
                switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON
            )
        )
        cg.add(var.set_web_overlay_enabled_control(switch_var))
