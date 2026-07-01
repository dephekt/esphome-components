import esphome.codegen as cg
from esphome.components import i2c, number, switch
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
AUTO_LOAD = ["thermal_camera_core"]

# MLX90640 namespace and component class
mlx90640_ns = cg.esphome_ns.namespace("mlx90640")
MLX90640Component = mlx90640_ns.class_("MLX90640Component", ThermalCameraBase)

# Device-specific control type values (see thermal_camera_core.h:
# THERMAL_CONTROL_TYPE_EXTRA_START)
MLX90640ControlType = mlx90640_ns.enum("MLX90640ControlType")
EMISSIVITY = MLX90640ControlType.EMISSIVITY
REFLECTED_TEMPERATURE = MLX90640ControlType.REFLECTED_TEMPERATURE
REFLECTED_TEMPERATURE_AUTO = MLX90640ControlType.REFLECTED_TEMPERATURE_AUTO

CONF_EMISSIVITY = "emissivity"
CONF_REFLECTED_TEMPERATURE = "reflected_temperature"
CONF_TA_SHIFT = "ta_shift"
CONF_EMISSIVITY_CONTROL = "emissivity_control"
CONF_REFLECTED_TEMPERATURE_CONTROL = "reflected_temperature_control"
CONF_REFLECTED_TEMPERATURE_AUTO_CONTROL = "reflected_temperature_auto_control"

CONF_REFRESH_RATE = "refresh_rate"
CONF_RESOLUTION = "resolution"
CONF_PATTERN = "pattern"
CONF_SINGLE_FRAME = "single_frame"

RESOLUTIONS = ["16-bit", "17-bit", "18-bit", "19-bit"]
PATTERNS = ["chess", "interleaved"]

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

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MLX90640Component),
            cv.Optional(CONF_EMISSIVITY, default=0.95): cv.float_range(min=0.1, max=1.0),
            cv.Optional(CONF_REFLECTED_TEMPERATURE): cv.float_,
            cv.Optional(CONF_TA_SHIFT, default=8.0): cv.float_,
            cv.Optional(CONF_REFRESH_RATE, default="16Hz"): cv.one_of(*REFRESH_RATES),
            cv.Optional(CONF_RESOLUTION, default="18-bit"): cv.one_of(*RESOLUTIONS),
            cv.Optional(CONF_PATTERN, default="chess"): cv.one_of(*PATTERNS),
            cv.Optional(CONF_SINGLE_FRAME, default=False): cv.boolean,
            cv.Optional(CONF_UPDATE_INTERVAL, default=20000): cv.positive_int,
            cv.Optional(CONF_THERMAL_PALETTE, default="rainbow"): cv.one_of(*PALETTES),
            cv.Optional(CONF_ROI): roi_schema(),
            cv.Optional(CONF_TEMPERATURE_SENSORS): temperature_sensors_schema(),
            cv.Optional(CONF_WEB_SERVER): web_server_schema(),
            # Device-specific auto-generated control entities
            cv.Optional(CONF_EMISSIVITY_CONTROL): number.number_schema(
                ThermalNumber
            ).extend(
                {
                    cv.Optional("min_value", default=0.1): cv.float_,
                    cv.Optional("max_value", default=1.0): cv.float_,
                    cv.Optional("step", default=0.01): cv.float_,
                }
            ),
            cv.Optional(CONF_REFLECTED_TEMPERATURE_CONTROL): number.number_schema(
                ThermalNumber
            ).extend(
                {
                    cv.Optional("min_value", default=-40.0): cv.float_,
                    cv.Optional("max_value", default=300.0): cv.float_,
                    cv.Optional("step", default=0.5): cv.float_,
                }
            ),
            cv.Optional(CONF_REFLECTED_TEMPERATURE_AUTO_CONTROL): switch.switch_schema(
                ThermalSwitch
            ),
        }
    )
    .extend(common_control_schemas())
    .extend(i2c.i2c_device_schema(0x33))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # JPEG encoder for the /thermal.jpg endpoint, plus the Arduino I2C/SPI
    # libs the Melexis MLX90640 driver links against.
    cg.add_platformio_option("lib_deps", ["https://github.com/bitbank2/JPEGENC.git"])
    cg.add_library("Wire", None)
    cg.add_library("SPI", None)

    # Basic configuration
    cg.add(var.set_refresh_rate(config[CONF_REFRESH_RATE]))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    cg.add(var.set_pattern(config[CONF_PATTERN]))
    cg.add(var.set_single_frame(config[CONF_SINGLE_FRAME]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_thermal_palette(config[CONF_THERMAL_PALETTE]))

    # Thermography parameters
    cg.add(var.set_emissivity(config[CONF_EMISSIVITY]))
    cg.add(var.set_ta_shift(config[CONF_TA_SHIFT]))
    if CONF_REFLECTED_TEMPERATURE in config:
        cg.add(var.set_reflected_temperature(config[CONF_REFLECTED_TEMPERATURE]))

    await register_roi(var, config)
    await register_temperature_sensors(var, config)
    await register_web_server(var, config)

    # --- Auto-generated control entities ---
    await register_common_controls(var, config)

    if CONF_EMISSIVITY_CONTROL in config:
        num_config = config[CONF_EMISSIVITY_CONTROL]
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
        cg.add(num.set_control_type(EMISSIVITY))
        cg.add(num.set_restore_value(True))
        cg.add(num.set_initial_value(config[CONF_EMISSIVITY]))
        cg.add(var.set_emissivity_control(num))

    if CONF_REFLECTED_TEMPERATURE_CONTROL in config:
        num_config = config[CONF_REFLECTED_TEMPERATURE_CONTROL]
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
        cg.add(num.set_control_type(REFLECTED_TEMPERATURE))
        cg.add(num.set_restore_value(True))
        initial_tr = config.get(CONF_REFLECTED_TEMPERATURE, 22.0)
        cg.add(num.set_initial_value(initial_tr))
        cg.add(var.set_reflected_temperature_control(num))

    if CONF_REFLECTED_TEMPERATURE_AUTO_CONTROL in config:
        switch_config = config[CONF_REFLECTED_TEMPERATURE_AUTO_CONTROL]
        switch_var = cg.new_Pvariable(switch_config[CONF_ID])
        await cg.register_component(switch_var, switch_config)
        await switch.register_switch(switch_var, switch_config)
        cg.add(switch_var.set_thermal_parent(var))
        cg.add(switch_var.set_control_type(REFLECTED_TEMPERATURE_AUTO))
        cg.add(
            switch_var.set_restore_mode(
                switch.SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON
            )
        )
        cg.add(var.set_reflected_temperature_auto_control(switch_var))
