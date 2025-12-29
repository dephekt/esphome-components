import esphome.codegen as cg
from esphome.components import binary_sensor, number, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = ["sensor", "binary_sensor"]

CONF_CO2_SENSOR = "co2_sensor"
CONF_TEMPERATURE_SENSOR = "temperature_sensor"
CONF_HUMIDITY_SENSOR = "humidity_sensor"
CONF_VPD_SENSOR = "vpd_sensor"

# Threshold configuration
CONF_CO2_HIGH_THRESHOLD = "co2_high_threshold"
CONF_CO2_LOW_THRESHOLD = "co2_low_threshold"
CONF_TEMP_HIGH_THRESHOLD = "temp_high_threshold"
CONF_TEMP_LOW_THRESHOLD = "temp_low_threshold"
CONF_HUMIDITY_HIGH_THRESHOLD = "humidity_high_threshold"
CONF_HUMIDITY_LOW_THRESHOLD = "humidity_low_threshold"
CONF_VPD_HIGH_THRESHOLD = "vpd_high_threshold"
CONF_VPD_LOW_THRESHOLD = "vpd_low_threshold"

# Alert sensor configuration
CONF_CO2_HIGH_ALERT = "co2_high_alert"
CONF_CO2_LOW_ALERT = "co2_low_alert"
CONF_TEMP_HIGH_ALERT = "temp_high_alert"
CONF_TEMP_LOW_ALERT = "temp_low_alert"
CONF_HUMIDITY_HIGH_ALERT = "humidity_high_alert"
CONF_HUMIDITY_LOW_ALERT = "humidity_low_alert"
CONF_VPD_HIGH_ALERT = "vpd_high_alert"
CONF_VPD_LOW_ALERT = "vpd_low_alert"

# Threshold number entities
CONF_CO2_HIGH_THRESHOLD_NUMBER = "co2_high_threshold_number"
CONF_CO2_LOW_THRESHOLD_NUMBER = "co2_low_threshold_number"
CONF_TEMP_HIGH_THRESHOLD_NUMBER = "temp_high_threshold_number"
CONF_TEMP_LOW_THRESHOLD_NUMBER = "temp_low_threshold_number"
CONF_HUMIDITY_HIGH_THRESHOLD_NUMBER = "humidity_high_threshold_number"
CONF_HUMIDITY_LOW_THRESHOLD_NUMBER = "humidity_low_threshold_number"
CONF_VPD_HIGH_THRESHOLD_NUMBER = "vpd_high_threshold_number"
CONF_VPD_LOW_THRESHOLD_NUMBER = "vpd_low_threshold_number"

scd4x_alerts_ns = cg.esphome_ns.namespace("scd4x_alerts")
SCD4xAlerts = scd4x_alerts_ns.class_("SCD4xAlerts", cg.Component)

# Import TemplateNumber from template namespace
template_ns = cg.esphome_ns.namespace("template_")
TemplateNumber = template_ns.class_("TemplateNumber")


def binary_sensor_schema(class_name):
    return binary_sensor.binary_sensor_schema(class_name).extend(
        {
            cv.Optional(CONF_NAME): cv.string,
        }
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SCD4xAlerts),
        cv.Required(CONF_CO2_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_TEMPERATURE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_HUMIDITY_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_VPD_SENSOR): cv.use_id(sensor.Sensor),
        # Default thresholds
        cv.Optional(CONF_CO2_HIGH_THRESHOLD, default=1500): cv.positive_float,
        cv.Optional(CONF_CO2_LOW_THRESHOLD, default=800): cv.positive_float,
        cv.Optional(CONF_TEMP_HIGH_THRESHOLD, default=30): cv.positive_float,
        cv.Optional(CONF_TEMP_LOW_THRESHOLD, default=18): cv.positive_float,
        cv.Optional(CONF_HUMIDITY_HIGH_THRESHOLD, default=70): cv.positive_float,
        cv.Optional(CONF_HUMIDITY_LOW_THRESHOLD, default=40): cv.positive_float,
        cv.Optional(CONF_VPD_HIGH_THRESHOLD, default=1.5): cv.positive_float,
        cv.Optional(CONF_VPD_LOW_THRESHOLD, default=0.4): cv.positive_float,
        # Alert binary sensors
        cv.Optional(CONF_CO2_HIGH_ALERT): binary_sensor_schema(
            binary_sensor.BinarySensor
        ),
        cv.Optional(CONF_CO2_LOW_ALERT): binary_sensor_schema(
            binary_sensor.BinarySensor
        ),
        cv.Optional(CONF_TEMP_HIGH_ALERT): binary_sensor_schema(
            binary_sensor.BinarySensor
        ),
        cv.Optional(CONF_TEMP_LOW_ALERT): binary_sensor_schema(
            binary_sensor.BinarySensor
        ),
        cv.Optional(CONF_HUMIDITY_HIGH_ALERT): binary_sensor_schema(
            binary_sensor.BinarySensor
        ),
        cv.Optional(CONF_HUMIDITY_LOW_ALERT): binary_sensor_schema(
            binary_sensor.BinarySensor
        ),
        cv.Optional(CONF_VPD_HIGH_ALERT): binary_sensor_schema(
            binary_sensor.BinarySensor
        ),
        cv.Optional(CONF_VPD_LOW_ALERT): binary_sensor_schema(
            binary_sensor.BinarySensor
        ),
        # Threshold number entities
        cv.Optional(CONF_CO2_HIGH_THRESHOLD_NUMBER): number.number_schema(
            TemplateNumber
        ).extend(
            {
                cv.Optional("min_value", default=500): cv.float_,
                cv.Optional("max_value", default=2000): cv.float_,
                cv.Optional("step", default=50): cv.positive_float,
                cv.Optional("optimistic", default=True): cv.boolean,
                cv.Optional("restore_value", default=True): cv.boolean,
            }
        ),
        cv.Optional(CONF_CO2_LOW_THRESHOLD_NUMBER): number.number_schema(
            TemplateNumber
        ).extend(
            {
                cv.Optional("min_value", default=400): cv.float_,
                cv.Optional("max_value", default=1000): cv.float_,
                cv.Optional("step", default=50): cv.positive_float,
                cv.Optional("optimistic", default=True): cv.boolean,
                cv.Optional("restore_value", default=True): cv.boolean,
            }
        ),
        cv.Optional(CONF_TEMP_HIGH_THRESHOLD_NUMBER): number.number_schema(
            TemplateNumber
        ).extend(
            {
                cv.Optional("min_value", default=15): cv.float_,
                cv.Optional("max_value", default=40): cv.float_,
                cv.Optional("step", default=0.5): cv.positive_float,
                cv.Optional("optimistic", default=True): cv.boolean,
                cv.Optional("restore_value", default=True): cv.boolean,
            }
        ),
        cv.Optional(CONF_TEMP_LOW_THRESHOLD_NUMBER): number.number_schema(
            TemplateNumber
        ).extend(
            {
                cv.Optional("min_value", default=10): cv.float_,
                cv.Optional("max_value", default=25): cv.float_,
                cv.Optional("step", default=0.5): cv.positive_float,
                cv.Optional("optimistic", default=True): cv.boolean,
                cv.Optional("restore_value", default=True): cv.boolean,
            }
        ),
        cv.Optional(CONF_HUMIDITY_HIGH_THRESHOLD_NUMBER): number.number_schema(
            TemplateNumber
        ).extend(
            {
                cv.Optional("min_value", default=30): cv.float_,
                cv.Optional("max_value", default=90): cv.float_,
                cv.Optional("step", default=1): cv.positive_float,
                cv.Optional("optimistic", default=True): cv.boolean,
                cv.Optional("restore_value", default=True): cv.boolean,
            }
        ),
        cv.Optional(CONF_HUMIDITY_LOW_THRESHOLD_NUMBER): number.number_schema(
            TemplateNumber
        ).extend(
            {
                cv.Optional("min_value", default=10): cv.float_,
                cv.Optional("max_value", default=60): cv.float_,
                cv.Optional("step", default=1): cv.positive_float,
                cv.Optional("optimistic", default=True): cv.boolean,
                cv.Optional("restore_value", default=True): cv.boolean,
            }
        ),
        cv.Optional(CONF_VPD_HIGH_THRESHOLD_NUMBER): number.number_schema(
            TemplateNumber
        ).extend(
            {
                cv.Optional("min_value", default=0.5): cv.float_,
                cv.Optional("max_value", default=3.0): cv.float_,
                cv.Optional("step", default=0.1): cv.positive_float,
                cv.Optional("optimistic", default=True): cv.boolean,
                cv.Optional("restore_value", default=True): cv.boolean,
            }
        ),
        cv.Optional(CONF_VPD_LOW_THRESHOLD_NUMBER): number.number_schema(
            TemplateNumber
        ).extend(
            {
                cv.Optional("min_value", default=0.1): cv.float_,
                cv.Optional("max_value", default=1.0): cv.float_,
                cv.Optional("step", default=0.1): cv.positive_float,
                cv.Optional("optimistic", default=True): cv.boolean,
                cv.Optional("restore_value", default=True): cv.boolean,
            }
        ),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set sensor references
    co2_sensor = await cg.get_variable(config[CONF_CO2_SENSOR])
    cg.add(var.set_co2_sensor(co2_sensor))

    temperature_sensor = await cg.get_variable(config[CONF_TEMPERATURE_SENSOR])
    cg.add(var.set_temperature_sensor(temperature_sensor))

    humidity_sensor = await cg.get_variable(config[CONF_HUMIDITY_SENSOR])
    cg.add(var.set_humidity_sensor(humidity_sensor))

    # Set VPD sensor reference if provided
    if CONF_VPD_SENSOR in config:
        vpd_sensor = await cg.get_variable(config[CONF_VPD_SENSOR])
        cg.add(var.set_vpd_sensor(vpd_sensor))

    # Set threshold values
    cg.add(var.set_co2_high_threshold(config[CONF_CO2_HIGH_THRESHOLD]))
    cg.add(var.set_co2_low_threshold(config[CONF_CO2_LOW_THRESHOLD]))
    cg.add(var.set_temp_high_threshold(config[CONF_TEMP_HIGH_THRESHOLD]))
    cg.add(var.set_temp_low_threshold(config[CONF_TEMP_LOW_THRESHOLD]))
    cg.add(var.set_humidity_high_threshold(config[CONF_HUMIDITY_HIGH_THRESHOLD]))
    cg.add(var.set_humidity_low_threshold(config[CONF_HUMIDITY_LOW_THRESHOLD]))
    cg.add(var.set_vpd_high_threshold(config[CONF_VPD_HIGH_THRESHOLD]))
    cg.add(var.set_vpd_low_threshold(config[CONF_VPD_LOW_THRESHOLD]))

    # Configure alert binary sensors
    if CONF_CO2_HIGH_ALERT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_CO2_HIGH_ALERT])
        cg.add(var.set_co2_high_alert_sensor(sens))

    if CONF_CO2_LOW_ALERT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_CO2_LOW_ALERT])
        cg.add(var.set_co2_low_alert_sensor(sens))

    if CONF_TEMP_HIGH_ALERT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_TEMP_HIGH_ALERT])
        cg.add(var.set_temp_high_alert_sensor(sens))

    if CONF_TEMP_LOW_ALERT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_TEMP_LOW_ALERT])
        cg.add(var.set_temp_low_alert_sensor(sens))

    if CONF_HUMIDITY_HIGH_ALERT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HUMIDITY_HIGH_ALERT])
        cg.add(var.set_humidity_high_alert_sensor(sens))

    if CONF_HUMIDITY_LOW_ALERT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HUMIDITY_LOW_ALERT])
        cg.add(var.set_humidity_low_alert_sensor(sens))

    if CONF_VPD_HIGH_ALERT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_VPD_HIGH_ALERT])
        cg.add(var.set_vpd_high_alert_sensor(sens))

    if CONF_VPD_LOW_ALERT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_VPD_LOW_ALERT])
        cg.add(var.set_vpd_low_alert_sensor(sens))

    # Configure threshold number entities
    if CONF_CO2_HIGH_THRESHOLD_NUMBER in config:
        num_config = config[CONF_CO2_HIGH_THRESHOLD_NUMBER]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(var.set_co2_high_threshold_number(num))
        cg.add(num.set_initial_value(config[CONF_CO2_HIGH_THRESHOLD]))
        cg.add(num.set_optimistic(num_config["optimistic"]))
        cg.add(num.set_restore_value(num_config["restore_value"]))
        cg.add(num.traits.set_unit_of_measurement("ppm"))

    if CONF_CO2_LOW_THRESHOLD_NUMBER in config:
        num_config = config[CONF_CO2_LOW_THRESHOLD_NUMBER]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(var.set_co2_low_threshold_number(num))
        cg.add(num.set_initial_value(config[CONF_CO2_LOW_THRESHOLD]))
        cg.add(num.set_optimistic(num_config["optimistic"]))
        cg.add(num.set_restore_value(num_config["restore_value"]))
        cg.add(num.traits.set_unit_of_measurement("ppm"))

    if CONF_TEMP_HIGH_THRESHOLD_NUMBER in config:
        num_config = config[CONF_TEMP_HIGH_THRESHOLD_NUMBER]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(var.set_temp_high_threshold_number(num))
        cg.add(num.set_initial_value(config[CONF_TEMP_HIGH_THRESHOLD]))
        cg.add(num.set_optimistic(num_config["optimistic"]))
        cg.add(num.set_restore_value(num_config["restore_value"]))
        cg.add(num.traits.set_unit_of_measurement("°C"))

    if CONF_TEMP_LOW_THRESHOLD_NUMBER in config:
        num_config = config[CONF_TEMP_LOW_THRESHOLD_NUMBER]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(var.set_temp_low_threshold_number(num))
        cg.add(num.set_initial_value(config[CONF_TEMP_LOW_THRESHOLD]))
        cg.add(num.set_optimistic(num_config["optimistic"]))
        cg.add(num.set_restore_value(num_config["restore_value"]))
        cg.add(num.traits.set_unit_of_measurement("°C"))

    if CONF_HUMIDITY_HIGH_THRESHOLD_NUMBER in config:
        num_config = config[CONF_HUMIDITY_HIGH_THRESHOLD_NUMBER]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(var.set_humidity_high_threshold_number(num))
        cg.add(num.set_initial_value(config[CONF_HUMIDITY_HIGH_THRESHOLD]))
        cg.add(num.set_optimistic(num_config["optimistic"]))
        cg.add(num.set_restore_value(num_config["restore_value"]))
        cg.add(num.traits.set_unit_of_measurement("%"))

    if CONF_HUMIDITY_LOW_THRESHOLD_NUMBER in config:
        num_config = config[CONF_HUMIDITY_LOW_THRESHOLD_NUMBER]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(var.set_humidity_low_threshold_number(num))
        cg.add(num.set_initial_value(config[CONF_HUMIDITY_LOW_THRESHOLD]))
        cg.add(num.set_optimistic(num_config["optimistic"]))
        cg.add(num.set_restore_value(num_config["restore_value"]))
        cg.add(num.traits.set_unit_of_measurement("%"))

    if CONF_VPD_HIGH_THRESHOLD_NUMBER in config:
        num_config = config[CONF_VPD_HIGH_THRESHOLD_NUMBER]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(var.set_vpd_high_threshold_number(num))
        cg.add(num.set_initial_value(config[CONF_VPD_HIGH_THRESHOLD]))
        cg.add(num.set_optimistic(num_config["optimistic"]))
        cg.add(num.set_restore_value(num_config["restore_value"]))
        cg.add(num.traits.set_unit_of_measurement("kPa"))

    if CONF_VPD_LOW_THRESHOLD_NUMBER in config:
        num_config = config[CONF_VPD_LOW_THRESHOLD_NUMBER]
        num = cg.new_Pvariable(num_config[CONF_ID])
        await number.register_number(
            num,
            num_config,
            min_value=num_config["min_value"],
            max_value=num_config["max_value"],
            step=num_config["step"],
        )
        cg.add(var.set_vpd_low_threshold_number(num))
        cg.add(num.set_initial_value(config[CONF_VPD_LOW_THRESHOLD]))
        cg.add(num.set_optimistic(num_config["optimistic"]))
        cg.add(num.set_restore_value(num_config["restore_value"]))
        cg.add(num.traits.set_unit_of_measurement("kPa"))
