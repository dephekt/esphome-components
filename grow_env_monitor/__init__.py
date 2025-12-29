import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32", "mlx90640"]

CONF_DISPLAY_BRIGHTNESS = "display_brightness"
CONF_DISPLAY_ROTATION = "display_rotation"
CONF_SENSORS = "sensors"
CONF_CO2 = "co2"
CONF_TEMPERATURE = "temperature"
CONF_HUMIDITY = "humidity"
CONF_THERMAL_MIN = "thermal_min"
CONF_THERMAL_MAX = "thermal_max"
CONF_THERMAL_AVG = "thermal_avg"
CONF_ROI_MIN = "roi_min"
CONF_ROI_MAX = "roi_max"
CONF_ROI_AVG = "roi_avg"
CONF_LIGHT_SENSOR = "light_sensor"
CONF_MLX90640_COMPONENT = "mlx90640_component"

# Alert binary sensors
CONF_TEMP_HIGH_ALERT = "temp_high_alert"
CONF_TEMP_LOW_ALERT = "temp_low_alert"
CONF_HUMIDITY_HIGH_ALERT = "humidity_high_alert"
CONF_HUMIDITY_LOW_ALERT = "humidity_low_alert"


grow_env_monitor_ns = cg.esphome_ns.namespace("grow_env_monitor")
GrowEnvMonitor = grow_env_monitor_ns.class_("GrowEnvMonitor", cg.Component)

# Import MLX90640 component
mlx90640_ns = cg.esphome_ns.namespace("mlx90640")
MLX90640Component = mlx90640_ns.class_("MLX90640Component", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GrowEnvMonitor),
        cv.Required(CONF_MLX90640_COMPONENT): cv.use_id(MLX90640Component),
        cv.Optional(CONF_DISPLAY_BRIGHTNESS, default=70): cv.int_range(min=0, max=100),
        cv.Optional(CONF_DISPLAY_ROTATION, default=3): cv.int_range(min=0, max=3),
        cv.Required(CONF_SENSORS): cv.Schema(
            {
                cv.Required(CONF_CO2): cv.use_id(sensor.Sensor),
                cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
                cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
                cv.Required(CONF_THERMAL_MIN): cv.use_id(sensor.Sensor),
                cv.Required(CONF_THERMAL_MAX): cv.use_id(sensor.Sensor),
                cv.Required(CONF_THERMAL_AVG): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_ROI_MIN): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_ROI_MAX): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_ROI_AVG): cv.use_id(sensor.Sensor),
                cv.Optional(CONF_LIGHT_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            }
        ),
        # Alert binary sensors (optional - connect to scd4x_alerts component)
        cv.Optional(CONF_TEMP_HIGH_ALERT): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_TEMP_LOW_ALERT): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_HUMIDITY_HIGH_ALERT): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_HUMIDITY_LOW_ALERT): cv.use_id(binary_sensor.BinarySensor),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set mlx90640 component dependency
    mlx90640_component = await cg.get_variable(config[CONF_MLX90640_COMPONENT])
    cg.add(var.set_mlx90640_component(mlx90640_component))

    # Set display configuration
    cg.add(var.set_display_brightness(config[CONF_DISPLAY_BRIGHTNESS]))
    cg.add(var.set_display_rotation(config[CONF_DISPLAY_ROTATION]))

    # Set sensor references
    sensors = config[CONF_SENSORS]

    co2_sensor = await cg.get_variable(sensors[CONF_CO2])
    cg.add(var.set_co2_sensor(co2_sensor))

    temperature_sensor = await cg.get_variable(sensors[CONF_TEMPERATURE])
    cg.add(var.set_temperature_sensor(temperature_sensor))

    humidity_sensor = await cg.get_variable(sensors[CONF_HUMIDITY])
    cg.add(var.set_humidity_sensor(humidity_sensor))

    thermal_min_sensor = await cg.get_variable(sensors[CONF_THERMAL_MIN])
    cg.add(var.set_thermal_min_sensor(thermal_min_sensor))

    thermal_max_sensor = await cg.get_variable(sensors[CONF_THERMAL_MAX])
    cg.add(var.set_thermal_max_sensor(thermal_max_sensor))

    thermal_avg_sensor = await cg.get_variable(sensors[CONF_THERMAL_AVG])
    cg.add(var.set_thermal_avg_sensor(thermal_avg_sensor))

    if CONF_ROI_MIN in sensors:
        roi_min_sensor = await cg.get_variable(sensors[CONF_ROI_MIN])
        cg.add(var.set_roi_min_sensor(roi_min_sensor))

    if CONF_ROI_MAX in sensors:
        roi_max_sensor = await cg.get_variable(sensors[CONF_ROI_MAX])
        cg.add(var.set_roi_max_sensor(roi_max_sensor))

    if CONF_ROI_AVG in sensors:
        roi_avg_sensor = await cg.get_variable(sensors[CONF_ROI_AVG])
        cg.add(var.set_roi_avg_sensor(roi_avg_sensor))

    if CONF_LIGHT_SENSOR in sensors:
        light_sensor = await cg.get_variable(sensors[CONF_LIGHT_SENSOR])
        cg.add(var.set_light_sensor(light_sensor))

    # Set alert binary sensor references (optional)
    if CONF_TEMP_HIGH_ALERT in config:
        temp_high_alert = await cg.get_variable(config[CONF_TEMP_HIGH_ALERT])
        cg.add(var.set_temp_high_alert_sensor(temp_high_alert))

    if CONF_TEMP_LOW_ALERT in config:
        temp_low_alert = await cg.get_variable(config[CONF_TEMP_LOW_ALERT])
        cg.add(var.set_temp_low_alert_sensor(temp_low_alert))

    if CONF_HUMIDITY_HIGH_ALERT in config:
        humidity_high_alert = await cg.get_variable(config[CONF_HUMIDITY_HIGH_ALERT])
        cg.add(var.set_humidity_high_alert_sensor(humidity_high_alert))

    if CONF_HUMIDITY_LOW_ALERT in config:
        humidity_low_alert = await cg.get_variable(config[CONF_HUMIDITY_LOW_ALERT])
        cg.add(var.set_humidity_low_alert_sensor(humidity_low_alert))
