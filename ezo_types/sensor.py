import esphome.codegen as cg
from esphome.components import i2c, sensor, text_sensor
from esphome.components.ezo.sensor import EZOSensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_VOLTAGE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

CODEOWNERS = ["@terpasaurus-midwest"]
DEPENDENCIES = ["i2c"]

# Custom configuration constants
CONF_TYPE = "type"
CONF_RESET_REASON = "reset_reason"
CONF_TDS = "tds"
CONF_SALINITY = "salinity"
CONF_RELATIVE_DENSITY = "relative_density"
CONF_OUTPUT_EC_ENABLED = "output_ec_enabled"
CONF_OUTPUT_TDS_ENABLED = "output_tds_enabled"
CONF_OUTPUT_SALINITY_ENABLED = "output_salinity_enabled"
CONF_OUTPUT_RELATIVE_DENSITY_ENABLED = "output_relative_density_enabled"

ezo_types_ns = cg.esphome_ns.namespace("ezo_types")

PHSensor = ezo_types_ns.class_("PHSensor", EZOSensor)
ECSensor = ezo_types_ns.class_("ECSensor", EZOSensor)
RTDSensor = ezo_types_ns.class_("RTDSensor", EZOSensor)
ORPSensor = ezo_types_ns.class_("ORPSensor", EZOSensor)


def _voltage_sensor_schema():
    return sensor.sensor_schema(
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
    )


def _reset_reason_schema():
    return text_sensor.text_sensor_schema()


CONFIG_SCHEMA = cv.typed_schema(
    {
        "ph": sensor.sensor_schema(PHSensor)
        .extend(
            {
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "ec": sensor.sensor_schema(ECSensor)
        .extend(
            {
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
                cv.Optional(CONF_TDS): sensor.sensor_schema(
                    unit_of_measurement="ppm",
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_SALINITY): sensor.sensor_schema(
                    unit_of_measurement="ppt",
                    accuracy_decimals=2,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_RELATIVE_DENSITY): sensor.sensor_schema(
                    accuracy_decimals=3,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_OUTPUT_EC_ENABLED, default=True): cv.boolean,
                cv.Optional(CONF_OUTPUT_TDS_ENABLED, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT_SALINITY_ENABLED, default=False): cv.boolean,
                cv.Optional(
                    CONF_OUTPUT_RELATIVE_DENSITY_ENABLED, default=False
                ): cv.boolean,
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "rtd": sensor.sensor_schema(RTDSensor)
        .extend(
            {
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
        "orp": sensor.sensor_schema(ORPSensor)
        .extend(
            {
                cv.Optional(CONF_VOLTAGE): _voltage_sensor_schema(),
                cv.Optional(CONF_RESET_REASON): _reset_reason_schema(),
            }
        )
        .extend(cv.polling_component_schema("60s"))
        .extend(i2c.i2c_device_schema(None)),
    },
    key=CONF_TYPE,
)


async def to_code(config):
    sensor_type = config[CONF_TYPE]

    if sensor_type == "ph":
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await sensor.register_sensor(var, config)
        await i2c.register_i2c_device(var, config)

        if voltage_config := config.get(CONF_VOLTAGE):
            voltage_sensor = await sensor.new_sensor(voltage_config)
            cg.add(var.set_voltage_sensor(voltage_sensor))

        if reset_reason_config := config.get(CONF_RESET_REASON):
            reset_reason_sensor = await text_sensor.new_text_sensor(reset_reason_config)
            cg.add(var.set_reset_reason_sensor(reset_reason_sensor))

    elif sensor_type == "ec":
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await sensor.register_sensor(var, config)
        await i2c.register_i2c_device(var, config)

        if voltage_config := config.get(CONF_VOLTAGE):
            voltage_sensor = await sensor.new_sensor(voltage_config)
            cg.add(var.set_voltage_sensor(voltage_sensor))

        if reset_reason_config := config.get(CONF_RESET_REASON):
            reset_reason_sensor = await text_sensor.new_text_sensor(reset_reason_config)
            cg.add(var.set_reset_reason_sensor(reset_reason_sensor))

        if tds_config := config.get(CONF_TDS):
            tds_sensor = await sensor.new_sensor(tds_config)
            cg.add(var.set_tds_sensor(tds_sensor))

        if salinity_config := config.get(CONF_SALINITY):
            salinity_sensor = await sensor.new_sensor(salinity_config)
            cg.add(var.set_salinity_sensor(salinity_sensor))

        if relative_density_config := config.get(CONF_RELATIVE_DENSITY):
            relative_density_sensor = await sensor.new_sensor(relative_density_config)
            cg.add(var.set_relative_density_sensor(relative_density_sensor))

        # Set output parameter flags
        cg.add(var.set_output_ec_enabled(config[CONF_OUTPUT_EC_ENABLED]))
        cg.add(var.set_output_tds_enabled(config[CONF_OUTPUT_TDS_ENABLED]))
        cg.add(var.set_output_salinity_enabled(config[CONF_OUTPUT_SALINITY_ENABLED]))
        cg.add(
            var.set_output_relative_density_enabled(
                config[CONF_OUTPUT_RELATIVE_DENSITY_ENABLED]
            )
        )

    elif sensor_type == "rtd":
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await sensor.register_sensor(var, config)
        await i2c.register_i2c_device(var, config)

        if voltage_config := config.get(CONF_VOLTAGE):
            voltage_sensor = await sensor.new_sensor(voltage_config)
            cg.add(var.set_voltage_sensor(voltage_sensor))

        if reset_reason_config := config.get(CONF_RESET_REASON):
            reset_reason_sensor = await text_sensor.new_text_sensor(reset_reason_config)
            cg.add(var.set_reset_reason_sensor(reset_reason_sensor))

    elif sensor_type == "orp":
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await sensor.register_sensor(var, config)
        await i2c.register_i2c_device(var, config)

        if voltage_config := config.get(CONF_VOLTAGE):
            voltage_sensor = await sensor.new_sensor(voltage_config)
            cg.add(var.set_voltage_sensor(voltage_sensor))

        if reset_reason_config := config.get(CONF_RESET_REASON):
            reset_reason_sensor = await text_sensor.new_text_sensor(reset_reason_config)
            cg.add(var.set_reset_reason_sensor(reset_reason_sensor))
