import esphome.codegen as cg
from esphome.components import sensor, time
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = ["sensor"]

CONF_SCD4X_ID = "scd4x_id"
CONF_VPD = "vpd"
CONF_DAILY_MAX_CO2 = "daily_max_co2"
CONF_DAILY_MIN_TEMP = "daily_min_temp"
CONF_DAILY_MAX_TEMP = "daily_max_temp"
CONF_CO2_MOVING_AVG = "co2_moving_avg"
CONF_TEMP_MOVING_AVG = "temp_moving_avg"

scd4x_stats_ns = cg.esphome_ns.namespace("scd4x_stats")
SCD4xStats = scd4x_stats_ns.class_("SCD4xStats", cg.Component)


def sensor_schema(class_name):
    return sensor.sensor_schema(class_name).extend(
        {
            cv.Optional(CONF_NAME): cv.string,
        }
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SCD4xStats),
        cv.Required("co2_sensor"): cv.use_id(sensor.Sensor),
        cv.Required("temperature_sensor"): cv.use_id(sensor.Sensor),
        cv.Required("humidity_sensor"): cv.use_id(sensor.Sensor),
        cv.Optional("time_id"): cv.use_id(time.RealTimeClock),
        cv.Optional(CONF_VPD): sensor_schema(sensor.Sensor),
        cv.Optional(CONF_DAILY_MAX_CO2): sensor_schema(sensor.Sensor),
        cv.Optional(CONF_DAILY_MIN_TEMP): sensor_schema(sensor.Sensor),
        cv.Optional(CONF_DAILY_MAX_TEMP): sensor_schema(sensor.Sensor),
        cv.Optional(CONF_CO2_MOVING_AVG): sensor_schema(sensor.Sensor),
        cv.Optional(CONF_TEMP_MOVING_AVG): sensor_schema(sensor.Sensor),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set sensor references
    co2_sensor = await cg.get_variable(config["co2_sensor"])
    cg.add(var.set_co2_sensor(co2_sensor))

    temperature_sensor = await cg.get_variable(config["temperature_sensor"])
    cg.add(var.set_temperature_sensor(temperature_sensor))

    humidity_sensor = await cg.get_variable(config["humidity_sensor"])
    cg.add(var.set_humidity_sensor(humidity_sensor))

    # Set time component if provided
    if "time_id" in config:
        time_component = await cg.get_variable(config["time_id"])
        cg.add(var.set_time_component(time_component))

    # Configure VPD sensor
    if CONF_VPD in config:
        sens = await sensor.new_sensor(config[CONF_VPD])
        cg.add(var.set_vpd_sensor(sens))

    # Configure daily max CO2 sensor
    if CONF_DAILY_MAX_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_DAILY_MAX_CO2])
        cg.add(var.set_daily_max_co2_sensor(sens))

    # Configure daily min temperature sensor
    if CONF_DAILY_MIN_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_DAILY_MIN_TEMP])
        cg.add(var.set_daily_min_temp_sensor(sens))

    # Configure daily max temperature sensor
    if CONF_DAILY_MAX_TEMP in config:
        sens = await sensor.new_sensor(config[CONF_DAILY_MAX_TEMP])
        cg.add(var.set_daily_max_temp_sensor(sens))

    # Configure CO2 moving average sensor
    if CONF_CO2_MOVING_AVG in config:
        sens = await sensor.new_sensor(config[CONF_CO2_MOVING_AVG])
        cg.add(var.set_co2_moving_avg_sensor(sens))

    # Configure temperature moving average sensor
    if CONF_TEMP_MOVING_AVG in config:
        sens = await sensor.new_sensor(config[CONF_TEMP_MOVING_AVG])
        cg.add(var.set_temp_moving_avg_sensor(sens))
