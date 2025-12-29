# SCD4x Statistics Component

A reusable ESPHome component that provides advanced statistical calculations and derived sensors for SCD4x CO2 sensors.

## Features

- **VPD Calculation**: Automatic Vapor Pressure Deficit calculation from temperature and humidity
- **Daily Statistics**: Track daily min/max values with automatic midnight reset
- **Moving Averages**: Smooth out sensor readings with configurable averaging windows
- **Time-based Reset**: Automatically resets daily statistics at midnight using ESPHome's time component

## Dependencies

- `sensor` - ESPHome sensor component
- `scd4x` - An SCD4x sensor must be configured in your YAML

## Configuration

### Basic Configuration

```yaml
sensor:
  - platform: scd4x
    id: co2_component
    co2:
      name: "CO2"
      id: co2_sensor
    temperature:
      name: "Temperature"
      id: temperature_sensor
    humidity:
      name: "Humidity"
      id: humidity_sensor

scd4x_stats:
  id: co2_stats
  scd4x_id: co2_component
  vpd:
    name: "VPD"
    id: vpd_sensor
  daily_max_co2:
    name: "Daily Max CO2"
    id: daily_max_co2
  daily_min_temp:
    name: "Daily Min Temperature"
    id: daily_min_temp
  daily_max_temp:
    name: "Daily Max Temperature"
    id: daily_max_temp
  co2_moving_avg:
    name: "CO2 Moving Average"
    id: co2_moving_avg
  temp_moving_avg:
    name: "Temperature Moving Average"
    id: temp_moving_avg
```

### Configuration Variables

- **scd4x_id** (**Required**): The ID of your SCD4x sensor component
- **vpd** (*Optional*): VPD sensor configuration
  - **name**: Sensor name in Home Assistant
  - **id**: ESPHome sensor ID for referencing
- **daily_max_co2** (*Optional*): Daily maximum CO2 sensor
- **daily_min_temp** (*Optional*): Daily minimum temperature sensor
- **daily_max_temp** (*Optional*): Daily maximum temperature sensor
- **co2_moving_avg** (*Optional*): CO2 moving average sensor (1-minute window)
- **temp_moving_avg** (*Optional*): Temperature moving average sensor (1-minute window)

## Important Notes

### Configuration Order
⚠️ **The `scd4x_stats` component must be declared AFTER the `scd4x` sensor in your YAML configuration.** This is because it needs to reference the SCD4x component that must exist first.

**Correct Order:**
```yaml
sensor:
  - platform: scd4x
    id: co2_component
    # ... sensor configuration

scd4x_stats:
  id: co2_stats
  scd4x_id: co2_component
  # ... stats configuration
```

### Time Component Required
For daily statistics to work properly, you need a time component configured:

```yaml
time:
  - platform: homeassistant
    id: homeassistant_time
```

## Technical Details

### VPD Calculation
The component calculates Vapor Pressure Deficit using the standard formula:
- `SVP = 0.6108 * exp((17.27 * T) / (T + 237.3))`
- `VPD = SVP * (1 - RH/100)`

Units: kPa (kilopascals)

### Moving Averages
- **Window Size**: 12 samples (1-minute average at 5-second intervals)
- **Update Frequency**: Every 5 seconds
- **Memory**: Uses circular buffer for efficient memory usage

### Daily Statistics
- **Reset Time**: Automatically resets at midnight (00:00)
- **Persistence**: Values persist until next midnight reset
- **Update Frequency**: Every 30 seconds

## Example Use Cases

### Grow Room Monitoring
```yaml
scd4x_stats:
  id: grow_stats
  scd4x_id: grow_sensor
  vpd:
    name: "Grow Room VPD"
    id: grow_vpd
  daily_max_co2:
    name: "Daily Peak CO2"
    id: daily_peak_co2
```

### HVAC Monitoring
```yaml
scd4x_stats:
  id: hvac_stats
  scd4x_id: hvac_sensor
  co2_moving_avg:
    name: "Smooth CO2"
    id: smooth_co2
  temp_moving_avg:
    name: "Smooth Temperature"
    id: smooth_temp
```

## Sensor IDs for Reference

All configured sensors can be referenced by their IDs in other components:

```yaml
# Use VPD sensor in alerts
scd4x_alerts:
  vpd_id: vpd_sensor
  
# Use in display component
grow_env_monitor:
  sensors:
    vpd: vpd_sensor
    daily_max_co2: daily_max_co2
```

## Troubleshooting

### "Component scd4x_stats requires component scd4x"
This error occurs when `scd4x_stats` is declared before the `scd4x` sensor. Move the `scd4x_stats` configuration after your sensor configuration.

### Daily stats not resetting
Ensure you have a time component configured and that it's receiving time updates from Home Assistant or NTP.

### Missing sensor data
Check that your SCD4x sensor is properly configured and producing data. The stats component will return NaN (Not a Number) if the underlying sensor has no data.