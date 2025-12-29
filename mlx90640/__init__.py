import esphome.codegen as cg
from esphome.components import i2c, number, select, sensor, switch
from esphome.components.sensor import StateClasses
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Conditional import for web server functionality
try:
    from esphome.components import web_server_base

    WEB_SERVER_BASE_AVAILABLE = True
except ImportError:
    WEB_SERVER_BASE_AVAILABLE = False

DEPENDENCIES = ["esp32", "sensor", "number", "select", "switch", "i2c"]

# Define the MLX90640 namespace and component classes
mlx90640_ns = cg.esphome_ns.namespace("mlx90640")
MLX90640Component = mlx90640_ns.class_("MLX90640Component", cg.Component, i2c.I2CDevice)

# Define MLX90640-specific component classes
MLX90640Number = mlx90640_ns.class_("MLX90640Number", number.Number, cg.Component)
MLX90640Select = mlx90640_ns.class_("MLX90640Select", select.Select, cg.Component)
MLX90640Switch = mlx90640_ns.class_("MLX90640Switch", switch.Switch, cg.Component)

# Control type enum
MLX90640ControlType = mlx90640_ns.enum("MLX90640ControlType")
UPDATE_INTERVAL = MLX90640ControlType.UPDATE_INTERVAL
ROI_CENTER_ROW = MLX90640ControlType.ROI_CENTER_ROW
ROI_CENTER_COL = MLX90640ControlType.ROI_CENTER_COL
ROI_SIZE = MLX90640ControlType.ROI_SIZE
THERMAL_PALETTE = MLX90640ControlType.THERMAL_PALETTE
ROI_ENABLED = MLX90640ControlType.ROI_ENABLED
WEB_OVERLAY_ENABLED = MLX90640ControlType.WEB_OVERLAY_ENABLED

CONF_REFRESH_RATE = "refresh_rate"
CONF_RESOLUTION = "resolution"
CONF_PATTERN = "pattern"
CONF_SINGLE_FRAME = "single_frame"
CONF_UPDATE_INTERVAL = "update_interval"
CONF_THERMAL_PALETTE = "thermal_palette"
CONF_ROI = "roi"
CONF_ROI_ENABLED = "enabled"
CONF_ROI_CENTER_ROW = "center_row"
CONF_ROI_CENTER_COL = "center_col"
CONF_ROI_SIZE = "size"
CONF_TEMPERATURE_MIN = "temperature_min"
CONF_TEMPERATURE_MAX = "temperature_max"
CONF_TEMPERATURE_AVG = "temperature_avg"
CONF_ROI_MIN = "roi_min"
CONF_ROI_MAX = "roi_max"
CONF_ROI_AVG = "roi_avg"
CONF_WEB_SERVER = "web_server"
CONF_WEB_ENABLE = "enable"
CONF_WEB_PATH = "path"
CONF_WEB_QUALITY = "quality"
CONF_WEB_OVERLAY_ENABLED = "overlay_enabled"
CONF_WEB_SERVER_BASE_ID = "web_server_base_id"

# User control configuration
CONF_UPDATE_INTERVAL_CONTROL = "update_interval_control"
CONF_THERMAL_PALETTE_CONTROL = "thermal_palette_control"
CONF_ROI_ENABLED_CONTROL = "roi_enabled_control"
CONF_ROI_CENTER_ROW_CONTROL = "roi_center_row_control"
CONF_ROI_CENTER_COL_CONTROL = "roi_center_col_control"
CONF_ROI_SIZE_CONTROL = "roi_size_control"
CONF_WEB_OVERLAY_ENABLED_CONTROL = "web_overlay_enabled_control"
CONF_TEMPERATURE_SENSORS = "temperature_sensors"

# Component configuration documentation:
#
# REFRESH RATES:
# - Lower rates (0.5Hz-8Hz): Better for static scenes, less I2C bus load
# - Higher rates (16Hz-64Hz): Better for motion detection, requires more I2C bandwidth
#
# PATTERNS:
# - "chess": Chess pattern readout (default) - vendor recommended, better for even temperature distribution
# - "interleaved": Interleaved pattern readout - different readout pattern
#
# SINGLE FRAME:
# - false (default): Read both subpages for better image quality
# - true: Read only one frame to reduce motion artifacts (checkerboard pattern)
#
# THERMAL PALETTES:
# - "rainbow": Rainbow color palette (default) - most intuitive color mapping, blue=cold, red=hot
# - "golden": Golden color palette - warm tones, good for general thermal viewing
# - "grayscale": Grayscale palette - professional look, good for analysis
# - "ironblack": Iron-black palette - high contrast, good for highlighting temperature differences
# - "cam": CAM palette - specialized thermal camera palette
# - "ironbow": FLIR Ironbow palette - black through blue, magenta, orange, yellow to white
# - "arctic": FLIR Arctic palette - blues for cold to golden yellows/red for warm
# - "lava": FLIR Lava palette - similar to ironbow, hot objects in warm colors, cold in blue
# - "whitehot": FLIR White Hot palette - grayscale with hot=white, cold=black
# - "blackhot": FLIR Black Hot palette - inverted grayscale with hot=black, cold=white

ROIConfig = mlx90640_ns.struct("ROIConfig")


def validate_web_server_config(config):
    if config.get(CONF_WEB_ENABLE, False) and CONF_WEB_SERVER_BASE_ID not in config:
        raise cv.Invalid(
            f"'{CONF_WEB_SERVER_BASE_ID}' is required when web server is enabled"
        )
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MLX90640Component),
        cv.Optional(CONF_REFRESH_RATE, default="16Hz"): cv.one_of(
            "0.5Hz", "1Hz", "2Hz", "4Hz", "8Hz", "16Hz", "32Hz", "64Hz"
        ),
        cv.Optional(CONF_RESOLUTION, default="18-bit"): cv.one_of(
            "16-bit", "17-bit", "18-bit", "19-bit"
        ),
        cv.Optional(CONF_PATTERN, default="chess"): cv.one_of("chess", "interleaved"),
        cv.Optional(CONF_SINGLE_FRAME, default=False): cv.boolean,
        cv.Optional(CONF_UPDATE_INTERVAL, default=20000): cv.positive_int,
        cv.Optional(CONF_THERMAL_PALETTE, default="rainbow"): cv.one_of(
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
        ),
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
        cv.Optional(CONF_TEMPERATURE_MIN): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_TEMPERATURE_MAX): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_TEMPERATURE_AVG): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ROI_MIN): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ROI_MAX): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_ROI_AVG): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_WEB_SERVER): cv.All(
            cv.Schema(
                {
                    cv.Optional(CONF_WEB_ENABLE, default=False): cv.boolean,
                    cv.Optional(CONF_WEB_PATH, default="/thermal.jpg"): cv.string,
                    cv.Optional(CONF_WEB_QUALITY, default=85): cv.int_range(
                        min=10, max=100
                    ),
                    cv.Optional(CONF_WEB_OVERLAY_ENABLED, default=True): cv.boolean,
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
            MLX90640Number
        ).extend(
            {
                cv.Optional("min_value", default=100): cv.positive_int,
                cv.Optional("max_value", default=30000): cv.positive_int,
                cv.Optional("step", default=100): cv.positive_int,
            }
        ),
        cv.Optional(CONF_THERMAL_PALETTE_CONTROL): select.select_schema(MLX90640Select),
        cv.Optional(CONF_ROI_ENABLED_CONTROL): switch.switch_schema(MLX90640Switch),
        cv.Optional(CONF_ROI_CENTER_ROW_CONTROL): number.number_schema(
            MLX90640Number
        ).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=24): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_ROI_CENTER_COL_CONTROL): number.number_schema(
            MLX90640Number
        ).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=32): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_ROI_SIZE_CONTROL): number.number_schema(MLX90640Number).extend(
            {
                cv.Optional("min_value", default=1): cv.positive_int,
                cv.Optional("max_value", default=10): cv.positive_int,
                cv.Optional("step", default=1): cv.positive_int,
            }
        ),
        cv.Optional(CONF_WEB_OVERLAY_ENABLED_CONTROL): switch.switch_schema(
            MLX90640Switch
        ),
        # Temperature sensors (auto-generated by component)
        cv.Optional(CONF_TEMPERATURE_SENSORS): cv.Schema(
            {
                cv.Optional("min"): sensor.sensor_schema(),
                cv.Optional("max"): sensor.sensor_schema(),
                cv.Optional("avg"): sensor.sensor_schema(),
                cv.Optional("roi_min"): sensor.sensor_schema(),
                cv.Optional("roi_max"): sensor.sensor_schema(),
                cv.Optional("roi_avg"): sensor.sensor_schema(),
            }
        ),
    }
).extend(i2c.i2c_device_schema(0x33))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Add external libraries for JPEG generation only
    # Skip Adafruit GFX due to dependency issues - we'll implement simple text rendering
    cg.add_platformio_option("lib_deps", ["https://github.com/bitbank2/JPEGENC.git"])

    # Add Arduino framework libraries
    cg.add_library("Wire", None)
    cg.add_library("SPI", None)

    # Set basic configuration
    cg.add(var.set_refresh_rate(config[CONF_REFRESH_RATE]))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    cg.add(var.set_pattern(config[CONF_PATTERN]))
    cg.add(var.set_single_frame(config[CONF_SINGLE_FRAME]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_thermal_palette(config[CONF_THERMAL_PALETTE]))

    # Set ROI configuration if provided
    if CONF_ROI in config:
        roi_config = config[CONF_ROI]
        # Create ROI config struct
        roi_struct = cg.StructInitializer(
            ROIConfig,
            ("enabled", roi_config[CONF_ROI_ENABLED]),
            ("center_row", roi_config[CONF_ROI_CENTER_ROW]),
            ("center_col", roi_config[CONF_ROI_CENTER_COL]),
            ("size", roi_config[CONF_ROI_SIZE]),
        )
        cg.add(var.set_roi_config(roi_struct))

    # Wire up temperature sensors if provided
    if CONF_TEMPERATURE_MIN in config:
        sensor_var = await cg.get_variable(config[CONF_TEMPERATURE_MIN])
        cg.add(var.set_temperature_min_sensor(sensor_var))

    if CONF_TEMPERATURE_MAX in config:
        sensor_var = await cg.get_variable(config[CONF_TEMPERATURE_MAX])
        cg.add(var.set_temperature_max_sensor(sensor_var))

    if CONF_TEMPERATURE_AVG in config:
        sensor_var = await cg.get_variable(config[CONF_TEMPERATURE_AVG])
        cg.add(var.set_temperature_avg_sensor(sensor_var))

    if CONF_ROI_MIN in config:
        sensor_var = await cg.get_variable(config[CONF_ROI_MIN])
        cg.add(var.set_roi_min_sensor(sensor_var))

    if CONF_ROI_MAX in config:
        sensor_var = await cg.get_variable(config[CONF_ROI_MAX])
        cg.add(var.set_roi_max_sensor(sensor_var))

    if CONF_ROI_AVG in config:
        sensor_var = await cg.get_variable(config[CONF_ROI_AVG])
        cg.add(var.set_roi_avg_sensor(sensor_var))

    # Configure web server if enabled
    if CONF_WEB_SERVER in config:
        web_config = config[CONF_WEB_SERVER]
        if web_config.get(CONF_WEB_ENABLE, False):
            # Web server is enabled, so web_server_base_id must be provided (enforced by validation)
            if CONF_WEB_SERVER_BASE_ID in web_config:
                # Get the web server base instance
                web_server_base_var = await cg.get_variable(
                    web_config[CONF_WEB_SERVER_BASE_ID]
                )
                cg.add(var.set_web_server_base(web_server_base_var))
            else:
                raise ValueError(
                    "web_server_base_id is required when web server is enabled"
                )
            cg.add(var.set_web_server_enabled(True))
            cg.add(var.set_web_server_path(web_config[CONF_WEB_PATH]))
            cg.add(var.set_web_server_quality(web_config[CONF_WEB_QUALITY]))
            cg.add(var.set_web_overlay_enabled(web_config[CONF_WEB_OVERLAY_ENABLED]))

    # Create auto-generated user control entities
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
        cg.add(num.set_mlx90640_parent(var))
        cg.add(num.set_control_type(UPDATE_INTERVAL))
        cg.add(num.set_restore_value(True))
        cg.add(num.set_initial_value(config[CONF_UPDATE_INTERVAL]))
        cg.add(var.set_update_interval_control(num))

    if CONF_THERMAL_PALETTE_CONTROL in config:
        sel_config = config[CONF_THERMAL_PALETTE_CONTROL]
        sel = cg.new_Pvariable(sel_config[CONF_ID])
        await cg.register_component(sel, sel_config)
        await select.register_select(
            sel,
            sel_config,
            options=[
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
            ],
        )
        cg.add(sel.set_mlx90640_parent(var))
        cg.add(sel.set_restore_value(True))
        cg.add(sel.set_initial_option(config[CONF_THERMAL_PALETTE]))
        cg.add(var.set_thermal_palette_control(sel))

    if CONF_ROI_ENABLED_CONTROL in config:
        switch_config = config[CONF_ROI_ENABLED_CONTROL]
        switch_var = cg.new_Pvariable(switch_config[CONF_ID])
        await cg.register_component(switch_var, switch_config)
        await switch.register_switch(switch_var, switch_config)
        cg.add(switch_var.set_mlx90640_parent(var))
        cg.add(
            switch_var.set_restore_mode(
                switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_OFF
            )
        )
        # Set initial state from ROI config if available
        if CONF_ROI in config:
            cg.add(
                switch_var.set_restore_mode(
                    switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON
                    if config[CONF_ROI][CONF_ROI_ENABLED]
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
        cg.add(num.set_mlx90640_parent(var))
        cg.add(num.set_control_type(ROI_CENTER_ROW))
        cg.add(num.set_restore_value(True))
        # Set initial value from ROI config if available
        initial_row = (
            config[CONF_ROI][CONF_ROI_CENTER_ROW] if CONF_ROI in config else 12
        )
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
        cg.add(num.set_mlx90640_parent(var))
        cg.add(num.set_control_type(ROI_CENTER_COL))
        cg.add(num.set_restore_value(True))
        initial_col = (
            config[CONF_ROI][CONF_ROI_CENTER_COL] if CONF_ROI in config else 16
        )
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
        cg.add(num.set_mlx90640_parent(var))
        cg.add(num.set_control_type(ROI_SIZE))
        cg.add(num.set_restore_value(True))
        initial_size = config[CONF_ROI][CONF_ROI_SIZE] if CONF_ROI in config else 2
        cg.add(num.set_initial_value(initial_size))
        cg.add(var.set_roi_size_control(num))

    # Create web overlay enabled control
    if CONF_WEB_OVERLAY_ENABLED_CONTROL in config:
        switch_config = config[CONF_WEB_OVERLAY_ENABLED_CONTROL]
        switch_var = cg.new_Pvariable(switch_config[CONF_ID])
        await cg.register_component(switch_var, switch_config)
        await switch.register_switch(switch_var, switch_config)
        cg.add(switch_var.set_mlx90640_parent(var))
        cg.add(switch_var.set_control_type(WEB_OVERLAY_ENABLED))
        # Set restore mode for persistence across reboots
        restore_mode = switch_config.get("restore_mode", "RESTORE_DEFAULT_ON")
        if restore_mode == "RESTORE_DEFAULT_ON":
            cg.add(
                switch_var.set_restore_mode(
                    switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON
                )
            )
        elif restore_mode == "RESTORE_DEFAULT_OFF":
            cg.add(
                switch_var.set_restore_mode(
                    switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_OFF
                )
            )
        elif restore_mode == "RESTORE_INVERTED_DEFAULT_ON":
            cg.add(
                switch_var.set_restore_mode(
                    switch.SwitchRestoreMode.SWITCH_RESTORE_INVERTED_DEFAULT_ON
                )
            )
        elif restore_mode == "RESTORE_INVERTED_DEFAULT_OFF":
            cg.add(
                switch_var.set_restore_mode(
                    switch.SwitchRestoreMode.SWITCH_RESTORE_INVERTED_DEFAULT_OFF
                )
            )
        else:
            # Default to always on if no valid restore mode
            cg.add(
                switch_var.set_restore_mode(switch.SwitchRestoreMode.SWITCH_ALWAYS_ON)
            )
        cg.add(var.set_web_overlay_enabled_control(switch_var))

    # Create auto-generated temperature sensors
    if CONF_TEMPERATURE_SENSORS in config:
        temp_sensors = config[CONF_TEMPERATURE_SENSORS]

        if "min" in temp_sensors:
            sens = cg.new_Pvariable(temp_sensors["min"][CONF_ID])
            await sensor.register_sensor(sens, temp_sensors["min"])
            cg.add(sens.set_unit_of_measurement("°C"))
            cg.add(sens.set_device_class(sensor.DEVICE_CLASS_TEMPERATURE))
            cg.add(sens.set_state_class(StateClasses.STATE_CLASS_MEASUREMENT))
            cg.add(sens.set_accuracy_decimals(1))
            cg.add(var.set_temperature_min_sensor(sens))

        if "max" in temp_sensors:
            sens = cg.new_Pvariable(temp_sensors["max"][CONF_ID])
            await sensor.register_sensor(sens, temp_sensors["max"])
            cg.add(sens.set_unit_of_measurement("°C"))
            cg.add(sens.set_device_class(sensor.DEVICE_CLASS_TEMPERATURE))
            cg.add(sens.set_state_class(StateClasses.STATE_CLASS_MEASUREMENT))
            cg.add(sens.set_accuracy_decimals(1))
            cg.add(var.set_temperature_max_sensor(sens))

        if "avg" in temp_sensors:
            sens = cg.new_Pvariable(temp_sensors["avg"][CONF_ID])
            await sensor.register_sensor(sens, temp_sensors["avg"])
            cg.add(sens.set_unit_of_measurement("°C"))
            cg.add(sens.set_device_class(sensor.DEVICE_CLASS_TEMPERATURE))
            cg.add(sens.set_state_class(StateClasses.STATE_CLASS_MEASUREMENT))
            cg.add(sens.set_accuracy_decimals(1))
            cg.add(var.set_temperature_avg_sensor(sens))

        if "roi_min" in temp_sensors:
            sens = cg.new_Pvariable(temp_sensors["roi_min"][CONF_ID])
            await sensor.register_sensor(sens, temp_sensors["roi_min"])
            cg.add(sens.set_unit_of_measurement("°C"))
            cg.add(sens.set_device_class(sensor.DEVICE_CLASS_TEMPERATURE))
            cg.add(sens.set_state_class(StateClasses.STATE_CLASS_MEASUREMENT))
            cg.add(sens.set_accuracy_decimals(1))
            cg.add(var.set_roi_min_sensor(sens))

        if "roi_max" in temp_sensors:
            sens = cg.new_Pvariable(temp_sensors["roi_max"][CONF_ID])
            await sensor.register_sensor(sens, temp_sensors["roi_max"])
            cg.add(sens.set_unit_of_measurement("°C"))
            cg.add(sens.set_device_class(sensor.DEVICE_CLASS_TEMPERATURE))
            cg.add(sens.set_state_class(StateClasses.STATE_CLASS_MEASUREMENT))
            cg.add(sens.set_accuracy_decimals(1))
            cg.add(var.set_roi_max_sensor(sens))

        if "roi_avg" in temp_sensors:
            sens = cg.new_Pvariable(temp_sensors["roi_avg"][CONF_ID])
            await sensor.register_sensor(sens, temp_sensors["roi_avg"])
            cg.add(sens.set_unit_of_measurement("°C"))
            cg.add(sens.set_device_class(sensor.DEVICE_CLASS_TEMPERATURE))
            cg.add(sens.set_state_class(StateClasses.STATE_CLASS_MEASUREMENT))
            cg.add(sens.set_accuracy_decimals(1))
            cg.add(var.set_roi_avg_sensor(sens))
