from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, number, select, sensor, switch, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_VOLTAGE,
    DEVICE_CLASS_CONDUCTIVITY,
    DEVICE_CLASS_PH,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_MILLIVOLT,
    UNIT_PH,
    UNIT_VOLT,
)

from .select import CONF_CELL_CONSTANT, _cell_constant_select_schema
from .switch import (
    CONF_EXTENDED_SCALE,
    _extended_scale_switch_schema,
)

CODEOWNERS = ["@dephekt"]
DEPENDENCIES = ["i2c", "select", "switch"]

CONF_TYPE = "type"
CONF_POWER_CONTROL_SWITCH = "power_control_switch"
CONF_RESET_REASON = "reset_reason"
CONF_FIRMWARE_VERSION = "firmware_version"
CONF_CALIBRATION_STATUS = "calibration_status"
CONF_TEMPERATURE_COMPENSATION = "temperature_compensation"
CONF_TDS = "tds"
CONF_TDS_CONVERSION_FACTOR = "tds_conversion_factor"
CONF_SALINITY = "salinity"
CONF_RELATIVE_DENSITY = "relative_density"
CONF_ACID_SLOPE_QUALITY = "acid_slope_quality"
CONF_ALKALINE_SLOPE_QUALITY = "alkaline_slope_quality"
CONF_ASYMMETRY_POTENTIAL = "asymmetry_potential"
CONF_CURRENT_COMMAND = "current_command"
CONF_NEXT_COMMAND = "next_command"
CONF_LAST_COMMAND = "last_command"
CONF_QUEUE_SIZE = "queue_size"
CONF_RTD_SENSOR = "rtd_sensor"
CONF_TEMP_COMPENSATION_SWITCH = "temp_compensation_switch"
CONF_CALIBRATION_MODE_SWITCH = "calibration_mode_switch"


# The EZO command/state machine is vendored into this component (ezo_base.h),
# so everything lives in the ezo_types namespace — the stock `ezo` component is
# not loaded at all. Note: on_slope uses the CallbackAutomation pattern via
# add_slope_callback (there is no SlopeTrigger class).
CONF_ON_SLOPE = "on_slope"

ezo_types_ns = cg.esphome_ns.namespace("ezo_types")
EZOSensor = ezo_types_ns.class_(
    "EZOSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

PHSensor = ezo_types_ns.class_("PHSensor", EZOSensor)
ECSensor = ezo_types_ns.class_("ECSensor", EZOSensor)
RTDSensor = ezo_types_ns.class_("RTDSensor", EZOSensor)
ORPSensor = ezo_types_ns.class_("ORPSensor", EZOSensor)
TDSConversionFactorNumber = ezo_types_ns.class_(
    "TDSConversionFactorNumber", number.Number, cg.Component
)


def _voltage_sensor_schema():
    return sensor.sensor_schema(
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        icon="mdi:flash",
    )


def _reset_reason_schema():
    return text_sensor.text_sensor_schema(
        icon="mdi:information-outline",
    )


def _firmware_version_schema():
    return text_sensor.text_sensor_schema(
        icon="mdi:chip",
    )


def _calibration_status_schema():
    return text_sensor.text_sensor_schema(
        icon="mdi:check-circle",
    )


def _current_command_schema():
    return text_sensor.text_sensor_schema(
        icon="mdi:console-line",
    )


def _next_command_schema():
    return text_sensor.text_sensor_schema(
        icon="mdi:console-network",
    )


def _last_command_schema():
    return text_sensor.text_sensor_schema(
        icon="mdi:history",
    )


def _queue_size_schema():
    return sensor.sensor_schema(
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon="mdi:counter",
    )


def _temperature_compensation_sensor_schema():
    return sensor.sensor_schema(
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        icon="mdi:tune",
    )


def _tds_conversion_factor_number_schema():
    return number.number_schema(
        TDSConversionFactorNumber,
        icon="mdi:water-percent",
        entity_category=ENTITY_CATEGORY_CONFIG,
    )


def _tds_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement="ppm",
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:water-opacity",
    )


def _salinity_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement="PSU",
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:shaker-outline",
    )


def _relative_density_sensor_schema():
    return sensor.sensor_schema(
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:water",
    )


def _acid_slope_quality_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement="%",
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:beaker-minus",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


def _alkaline_slope_quality_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement="%",
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:beaker-plus",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


def _asymmetry_potential_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement="mV",
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:sine-wave",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        "ph": sensor.sensor_schema(
            PHSensor,
            unit_of_measurement=UNIT_PH,
            device_class=DEVICE_CLASS_PH,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:ph",
        )
        .extend(
            {
                cv.Optional(CONF_POWER_CONTROL_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_RTD_SENSOR): cv.use_id(RTDSensor),
                cv.Optional(CONF_TEMP_COMPENSATION_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_CALIBRATION_MODE_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_FIRMWARE_VERSION): _firmware_version_schema(),
                cv.Optional(CONF_CALIBRATION_STATUS): _calibration_status_schema(),
                cv.Optional(CONF_CURRENT_COMMAND): _current_command_schema(),
                cv.Optional(CONF_NEXT_COMMAND): _next_command_schema(),
                cv.Optional(CONF_LAST_COMMAND): _last_command_schema(),
                cv.Optional(CONF_QUEUE_SIZE): _queue_size_schema(),
                cv.Optional(
                    CONF_TEMPERATURE_COMPENSATION
                ): _temperature_compensation_sensor_schema(),
                cv.Optional(CONF_ON_SLOPE): automation.validate_automation({}),
                cv.Optional(
                    CONF_ACID_SLOPE_QUALITY
                ): _acid_slope_quality_sensor_schema(),
                cv.Optional(
                    CONF_ALKALINE_SLOPE_QUALITY
                ): _alkaline_slope_quality_sensor_schema(),
                cv.Optional(
                    CONF_ASYMMETRY_POTENTIAL
                ): _asymmetry_potential_sensor_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "ec": sensor.sensor_schema(
            ECSensor,
            unit_of_measurement="µS/cm",
            device_class=DEVICE_CLASS_CONDUCTIVITY,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:water-check",
        )
        .extend(
            {
                cv.Optional(CONF_POWER_CONTROL_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_RTD_SENSOR): cv.use_id(RTDSensor),
                cv.Optional(CONF_TEMP_COMPENSATION_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_CALIBRATION_MODE_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_FIRMWARE_VERSION): _firmware_version_schema(),
                cv.Optional(CONF_CALIBRATION_STATUS): _calibration_status_schema(),
                cv.Optional(CONF_CURRENT_COMMAND): _current_command_schema(),
                cv.Optional(CONF_NEXT_COMMAND): _next_command_schema(),
                cv.Optional(CONF_LAST_COMMAND): _last_command_schema(),
                cv.Optional(CONF_QUEUE_SIZE): _queue_size_schema(),
                cv.Optional(
                    CONF_TEMPERATURE_COMPENSATION
                ): _temperature_compensation_sensor_schema(),
                cv.Optional(CONF_CELL_CONSTANT): _cell_constant_select_schema(),
                cv.Optional(CONF_TDS): _tds_sensor_schema(),
                cv.Optional(CONF_SALINITY): _salinity_sensor_schema(),
                cv.Optional(CONF_RELATIVE_DENSITY): _relative_density_sensor_schema(),
                cv.Optional(
                    CONF_TDS_CONVERSION_FACTOR
                ): _tds_conversion_factor_number_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "rtd": sensor.sensor_schema(
            RTDSensor,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:thermometer",
        )
        .extend(
            {
                cv.Optional(CONF_POWER_CONTROL_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_RTD_SENSOR): cv.use_id(RTDSensor),
                cv.Optional(CONF_TEMP_COMPENSATION_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_CALIBRATION_MODE_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_FIRMWARE_VERSION): _firmware_version_schema(),
                cv.Optional(CONF_CALIBRATION_STATUS): _calibration_status_schema(),
                cv.Optional(CONF_CURRENT_COMMAND): _current_command_schema(),
                cv.Optional(CONF_NEXT_COMMAND): _next_command_schema(),
                cv.Optional(CONF_LAST_COMMAND): _last_command_schema(),
                cv.Optional(CONF_QUEUE_SIZE): _queue_size_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "orp": sensor.sensor_schema(
            ORPSensor,
            unit_of_measurement=UNIT_MILLIVOLT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:lightning-bolt",
        )
        .extend(
            {
                cv.Optional(CONF_POWER_CONTROL_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_RTD_SENSOR): cv.use_id(RTDSensor),
                cv.Optional(CONF_TEMP_COMPENSATION_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_CALIBRATION_MODE_SWITCH): cv.use_id(switch.Switch),
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_FIRMWARE_VERSION): _firmware_version_schema(),
                cv.Optional(CONF_CALIBRATION_STATUS): _calibration_status_schema(),
                cv.Optional(CONF_CURRENT_COMMAND): _current_command_schema(),
                cv.Optional(CONF_NEXT_COMMAND): _next_command_schema(),
                cv.Optional(CONF_LAST_COMMAND): _last_command_schema(),
                cv.Optional(CONF_QUEUE_SIZE): _queue_size_schema(),
                cv.Optional(CONF_EXTENDED_SCALE): _extended_scale_switch_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
    },
    key=CONF_TYPE,
)


async def setup_atlas_sensor_base(var, config):
    """Common setup for all Atlas EZO sensors"""
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await i2c.register_i2c_device(var, config)

    if voltage_config := config.get(CONF_VOLTAGE):
        voltage_sensor = await sensor.new_sensor(voltage_config)
        cg.add(var.set_voltage_sensor(voltage_sensor))

    if reset_reason_config := config.get(CONF_RESET_REASON):
        reset_reason_sensor = await text_sensor.new_text_sensor(reset_reason_config)
        cg.add(var.set_reset_reason_sensor(reset_reason_sensor))

    if firmware_version_config := config.get(CONF_FIRMWARE_VERSION):
        firmware_version_sensor = await text_sensor.new_text_sensor(
            firmware_version_config
        )
        cg.add(var.set_firmware_version_sensor(firmware_version_sensor))

    if calibration_status_config := config.get(CONF_CALIBRATION_STATUS):
        calibration_status_sensor = await text_sensor.new_text_sensor(
            calibration_status_config
        )
        cg.add(var.set_calibration_status_sensor(calibration_status_sensor))

    if current_command_config := config.get(CONF_CURRENT_COMMAND):
        current_command_sensor = await text_sensor.new_text_sensor(
            current_command_config
        )
        cg.add(var.set_current_command_sensor(current_command_sensor))

    if next_command_config := config.get(CONF_NEXT_COMMAND):
        next_command_sensor = await text_sensor.new_text_sensor(next_command_config)
        cg.add(var.set_next_command_sensor(next_command_sensor))

    if last_command_config := config.get(CONF_LAST_COMMAND):
        last_command_sensor = await text_sensor.new_text_sensor(last_command_config)
        cg.add(var.set_last_command_sensor(last_command_sensor))

    if queue_size_config := config.get(CONF_QUEUE_SIZE):
        queue_size_sensor = await sensor.new_sensor(queue_size_config)
        cg.add(var.set_queue_size_sensor(queue_size_sensor))

    if temperature_compensation_config := config.get(CONF_TEMPERATURE_COMPENSATION):
        temperature_compensation_sensor = await sensor.new_sensor(
            temperature_compensation_config
        )
        cg.add(var.set_temperature_compensation_sensor(temperature_compensation_sensor))

    if power_control_switch_config := config.get(CONF_POWER_CONTROL_SWITCH):
        power_switch = await cg.get_variable(power_control_switch_config)
        cg.add(var.set_power_control_switch(power_switch))

    if rtd_sensor_config := config.get(CONF_RTD_SENSOR):
        rtd_sensor = await cg.get_variable(rtd_sensor_config)
        cg.add(var.set_rtd_sensor(rtd_sensor))

    if temp_compensation_switch_config := config.get(CONF_TEMP_COMPENSATION_SWITCH):
        temp_compensation_switch = await cg.get_variable(
            temp_compensation_switch_config
        )
        cg.add(var.set_temp_compensation_switch(temp_compensation_switch))

    if calibration_mode_switch_config := config.get(CONF_CALIBRATION_MODE_SWITCH):
        calibration_mode_switch = await cg.get_variable(calibration_mode_switch_config)
        cg.add(var.set_calibration_mode_switch(calibration_mode_switch))


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_atlas_sensor_base(var, config)

    if sensor_type == "ph":
        await automation.build_callback_automations(
            var,
            config,
            (automation.CallbackAutomation(CONF_ON_SLOPE, "add_slope_callback", [(cg.std_string, "x")]),),
        )

        if acid_slope_config := config.get(CONF_ACID_SLOPE_QUALITY):
            acid_slope_sensor = await sensor.new_sensor(acid_slope_config)
            cg.add(var.set_acid_slope_quality_sensor(acid_slope_sensor))

        if alkaline_slope_config := config.get(CONF_ALKALINE_SLOPE_QUALITY):
            alkaline_slope_sensor = await sensor.new_sensor(alkaline_slope_config)
            cg.add(var.set_alkaline_slope_quality_sensor(alkaline_slope_sensor))

        if asymmetry_potential_config := config.get(CONF_ASYMMETRY_POTENTIAL):
            asymmetry_potential_sensor = await sensor.new_sensor(
                asymmetry_potential_config
            )
            cg.add(var.set_asymmetry_potential_sensor(asymmetry_potential_sensor))

    elif sensor_type == "ec":
        if cell_constant_config := config.get(CONF_CELL_CONSTANT):
            cell_constant_select = await select.new_select(
                cell_constant_config,
                options=["0.1", "1.0", "10.0"],
            )
            await cg.register_component(cell_constant_select, cell_constant_config)
            cg.add(var.set_cell_constant_select(cell_constant_select))
            cg.add(cell_constant_select.set_ec_sensor(var))

        if tds_config := config.get(CONF_TDS):
            tds_sensor = await sensor.new_sensor(tds_config)
            cg.add(var.set_tds_sensor(tds_sensor))

        if salinity_config := config.get(CONF_SALINITY):
            salinity_sensor = await sensor.new_sensor(salinity_config)
            cg.add(var.set_salinity_sensor(salinity_sensor))

        if relative_density_config := config.get(CONF_RELATIVE_DENSITY):
            relative_density_sensor = await sensor.new_sensor(relative_density_config)
            cg.add(var.set_relative_density_sensor(relative_density_sensor))

        if tds_conversion_factor_config := config.get(CONF_TDS_CONVERSION_FACTOR):
            num = await number.new_number(
                tds_conversion_factor_config,
                min_value=0.01,
                max_value=2.0,
                step=0.01,
            )
            await cg.register_component(num, tds_conversion_factor_config)
            cg.add(num.set_ec_sensor(var))
            cg.add(var.set_tds_conversion_factor_number(num))

    elif sensor_type == "orp":
        if extended_scale_config := config.get(CONF_EXTENDED_SCALE):
            sw = await switch.new_switch(extended_scale_config)
            await cg.register_component(sw, extended_scale_config)
            cg.add(sw.set_orp_sensor(var))
            cg.add(var.set_extended_scale_switch(sw))
