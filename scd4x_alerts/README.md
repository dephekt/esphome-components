# SCD4x Alerts Component

A reusable ESPHome component that provides comprehensive threshold-based alerting for SCD4x CO2 sensors with configurable delays and user-adjustable thresholds.

## Features

- **Multi-Parameter Alerts**: CO2, temperature, humidity, and VPD threshold monitoring
- **Configurable Delays**: Prevents false alarms with adjustable on/off delays
- **User-Adjustable Thresholds**: Number entities for real-time threshold changes
- **Binary Sensor Alerts**: Clean on/off alert states for automation
- **VPD Integration**: Works with VPD sensors from `scd4x_stats` component

## Dependencies

- `sensor` - ESPHome sensor component
- `binary_sensor` - For alert outputs
- `number` - For threshold controls
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

scd4x_alerts:
  id: co2_alerts
  co2_sensor: co2_sensor
  temperature_sensor: temperature_sensor
  humidity_sensor: humidity_sensor
  co2_high_threshold: 1500
  co2_low_threshold: 800
  temp_high_threshold: 30
  temp_low_threshold: 18
  co2_high_alert:
    name: "CO2 High Alert"
    id: co2_high_alert
  co2_low_alert:
    name: "CO2 Low Alert" 
    id: co2_low_alert
```

### Full Configuration Example

```yaml
# First configure your sensors
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

# Optional: VPD stats component
scd4x_stats:
  id: co2_stats
  co2_sensor: co2_sensor
  temperature_sensor: temperature_sensor
  humidity_sensor: humidity_sensor
  vpd:
    name: "VPD"
    id: vpd_sensor

# Configure alerts component
scd4x_alerts:
  id: co2_alerts
  co2_sensor: co2_sensor
  temperature_sensor: temperature_sensor
  humidity_sensor: humidity_sensor
  vpd_sensor: vpd_sensor  # Optional: references VPD from scd4x_stats
  
  # Default thresholds
  co2_high_threshold: 1500
  co2_low_threshold: 800
  temp_high_threshold: 30
  temp_low_threshold: 18
  humidity_high_threshold: 70
  humidity_low_threshold: 40
  vpd_high_threshold: 1.5
  vpd_low_threshold: 0.4
  
  # Alert binary sensors
  co2_high_alert:
    name: "CO2 High Alert"
    id: co2_high_alert
  co2_low_alert:
    name: "CO2 Low Alert"
    id: co2_low_alert
  temp_high_alert:
    name: "Temperature High Alert"
    id: temp_high_alert
  temp_low_alert:
    name: "Temperature Low Alert"
    id: temp_low_alert
  humidity_high_alert:
    name: "Humidity High Alert"
    id: humidity_high_alert
  humidity_low_alert:
    name: "Humidity Low Alert"
    id: humidity_low_alert
  vpd_high_alert:
    name: "VPD High Alert"
    id: vpd_high_alert
  vpd_low_alert:
    name: "VPD Low Alert"
    id: vpd_low_alert
    
  # User-adjustable threshold controls (all optional)
  co2_high_threshold_number:
    name: "CO2 High Threshold"
  co2_low_threshold_number:
    name: "CO2 Low Threshold"
  temp_high_threshold_number:
    name: "Temperature High Threshold"
  temp_low_threshold_number:
    name: "Temperature Low Threshold"
  humidity_high_threshold_number:
    name: "Humidity High Threshold"
  humidity_low_threshold_number:
    name: "Humidity Low Threshold"
  vpd_high_threshold_number:
    name: "VPD High Threshold"
  vpd_low_threshold_number:
    name: "VPD Low Threshold"
```

## Configuration Variables

### Required Sensor References
- **co2_sensor** (**Required**): The ID of your CO2 sensor
- **temperature_sensor** (**Required**): The ID of your temperature sensor
- **humidity_sensor** (**Required**): The ID of your humidity sensor

### Optional Sensor Reference
- **vpd_sensor** (*Optional*): Reference to VPD sensor (typically from `scd4x_stats`)

### Default Thresholds
- **co2_high_threshold** (*Optional*, default: 1500): CO2 high threshold in ppm
- **co2_low_threshold** (*Optional*, default: 800): CO2 low threshold in ppm
- **temp_high_threshold** (*Optional*, default: 30): Temperature high threshold in °C
- **temp_low_threshold** (*Optional*, default: 18): Temperature low threshold in °C
- **humidity_high_threshold** (*Optional*, default: 70): Humidity high threshold in %
- **humidity_low_threshold** (*Optional*, default: 40): Humidity low threshold in %
- **vpd_high_threshold** (*Optional*, default: 1.5): VPD high threshold in kPa
- **vpd_low_threshold** (*Optional*, default: 0.4): VPD low threshold in kPa

### Alert Binary Sensors
All alert sensors support standard binary sensor options:
- **name**: Display name in Home Assistant
- **id**: ESPHome ID for referencing
- **device_class**: Optional device class
- **icon**: Optional icon override

Available alerts:
- **co2_high_alert**, **co2_low_alert**
- **temp_high_alert**, **temp_low_alert**
- **humidity_high_alert**, **humidity_low_alert**
- **vpd_high_alert**, **vpd_low_alert**

### Threshold Number Controls
User-adjustable threshold controls support:
- **name**: Display name in Home Assistant
- **id**: ESPHome ID for referencing
- **min_value**: Minimum allowed value
- **max_value**: Maximum allowed value
- **step**: Adjustment step size
- **unit_of_measurement**: Display unit
- **icon**: Display icon

Available controls:
- **co2_high_threshold_number**, **co2_low_threshold_number**
- **temp_high_threshold_number**, **temp_low_threshold_number**

## Important Notes

### Configuration Order
⚠️ **The `scd4x_alerts` component must be declared AFTER the sensor configuration AND any `scd4x_stats` component in your YAML configuration.**

**Correct Order:**
```yaml
sensor:
  - platform: scd4x
    id: co2_component
    co2:
      id: co2_sensor
    temperature:
      id: temperature_sensor
    humidity:
      id: humidity_sensor

scd4x_stats:  # If using VPD alerts
  id: co2_stats
  co2_sensor: co2_sensor
  temperature_sensor: temperature_sensor
  humidity_sensor: humidity_sensor
  vpd:
    id: vpd_sensor

scd4x_alerts:
  id: co2_alerts
  co2_sensor: co2_sensor
  temperature_sensor: temperature_sensor
  humidity_sensor: humidity_sensor
  vpd_sensor: vpd_sensor  # References VPD from stats component
  # ... alerts configuration
```

### Alert Delays
The component includes built-in delays to prevent false alarms:
- **CO2 alerts**: 30 seconds
- **Temperature alerts**: 60 seconds  
- **Humidity alerts**: 60 seconds
- **VPD alerts**: 60 seconds

Alerts will only trigger after the condition has been stable for the delay period.

## Usage Examples

### Basic Grow Room Monitoring
```yaml
sensor:
  - platform: scd4x
    co2:
      id: grow_co2
    temperature:
      id: grow_temp
    humidity:
      id: grow_humidity

scd4x_alerts:
  id: grow_alerts
  co2_sensor: grow_co2
  temperature_sensor: grow_temp
  humidity_sensor: grow_humidity
  co2_high_threshold: 1200  # Lower for plants
  co2_low_threshold: 600
  temp_high_threshold: 28
  temp_low_threshold: 20
  co2_high_alert:
    name: "Grow Room CO2 High"
  temp_high_alert:
    name: "Grow Room Too Hot"
```

### HVAC System Integration
```yaml
sensor:
  - platform: scd4x
    co2:
      id: hvac_co2
    temperature:
      id: hvac_temp
    humidity:
      id: hvac_humidity

scd4x_alerts:
  id: hvac_alerts
  co2_sensor: hvac_co2
  temperature_sensor: hvac_temp
  humidity_sensor: hvac_humidity
  co2_high_threshold: 1000
  temp_high_threshold: 25
  co2_high_alert:
    name: "Ventilation Needed"
  temp_high_alert:
    name: "Cooling Needed"
```

### With User Controls
```yaml
sensor:
  - platform: scd4x
    co2:
      id: room_co2
    temperature:
      id: room_temp
    humidity:
      id: room_humidity

scd4x_alerts:
  id: room_alerts
  co2_sensor: room_co2
  temperature_sensor: room_temp
  humidity_sensor: room_humidity
  co2_high_alert:
    name: "Room CO2 High"
  co2_high_threshold_number:
    name: "CO2 Alert Level"
```

## Complete Working Example

Here's a complete example configuration that includes all features:

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

# Optional: Add VPD calculation
scd4x_stats:
  id: co2_stats
  co2_sensor: co2_sensor
  temperature_sensor: temperature_sensor
  humidity_sensor: humidity_sensor
  vpd:
    name: "VPD"
    id: vpd_sensor

# Configure comprehensive alerts
scd4x_alerts:
  id: co2_alerts
  co2_sensor: co2_sensor
  temperature_sensor: temperature_sensor
  humidity_sensor: humidity_sensor
  vpd_sensor: vpd_sensor
  
  # Set initial thresholds
  co2_high_threshold: 1500.0
  co2_low_threshold: 800.0
  temp_high_threshold: 30.0
  temp_low_threshold: 18.0
  humidity_high_threshold: 70.0
  humidity_low_threshold: 40.0
  vpd_high_threshold: 1.5
  vpd_low_threshold: 0.4
  
  # Binary sensor alerts for automation
  co2_high_alert:
    name: "CO2 High Alert"
    id: co2_high_alert
  co2_low_alert:
    name: "CO2 Low Alert"
    id: co2_low_alert
  temp_high_alert:
    name: "Temperature High Alert"
    id: temp_high_alert
  temp_low_alert:
    name: "Temperature Low Alert"
    id: temp_low_alert
  humidity_high_alert:
    name: "Humidity High Alert"
    id: humidity_high_alert
  humidity_low_alert:
    name: "Humidity Low Alert"
    id: humidity_low_alert
  vpd_high_alert:
    name: "VPD High Alert"
    id: vpd_high_alert
  vpd_low_alert:
    name: "VPD Low Alert"
    id: vpd_low_alert
    
  # User-controllable thresholds
  co2_high_threshold_number:
    name: "CO2 High Threshold"
  co2_low_threshold_number:
    name: "CO2 Low Threshold"
  temp_high_threshold_number:
    name: "Temperature High Threshold"
  temp_low_threshold_number:
    name: "Temperature Low Threshold"
  humidity_high_threshold_number:
    name: "Humidity High Threshold"
  humidity_low_threshold_number:
    name: "Humidity Low Threshold"
  vpd_high_threshold_number:
    name: "VPD High Threshold"
  vpd_low_threshold_number:
    name: "VPD Low Threshold"
```

## Integration with Physical Displays

The alert binary sensors can be connected to display components like `grow_env_monitor` for visual alerts on physical screens:

```yaml
# Configure sensors and alerts as above, then:
grow_env_monitor:
  id: grow_monitor
  # ... other configuration
  sensors:
    co2: co2_sensor
    temperature: temperature_sensor
    humidity: humidity_sensor
    # ... other sensors
  # Connect alert binary sensors for configurable thresholds
  temp_high_alert: temp_high_alert
  temp_low_alert: temp_low_alert
  humidity_high_alert: humidity_high_alert
  humidity_low_alert: humidity_low_alert
```

This allows the physical display to show alerts based on the configurable thresholds instead of hardcoded values.

## Automation Integration

Use the alert binary sensors in Home Assistant automations:

```yaml
# Home Assistant automation example
automation:
  - alias: "High CO2 Notification"
    trigger:
      platform: state
      entity_id: binary_sensor.co2_high_alert
      to: 'on'
    action:
      service: notify.mobile_app
      data:
        message: "CO2 levels are too high!"
```

## Troubleshooting

### "Component scd4x_alerts requires component sensor" 
Ensure that the `scd4x_alerts` configuration is placed after your sensor configuration in the YAML file.

### "Failed to find sensor with id: co2_sensor"
Make sure the sensor IDs in `co2_sensor`, `temperature_sensor`, and `humidity_sensor` match the actual IDs from your sensor configuration.

### VPD alerts not working
Ensure you have:
1. A VPD sensor configured (usually from `scd4x_stats`)
2. The `vpd_sensor` parameter correctly references your VPD sensor ID
3. The VPD sensor is producing valid data

### Alerts not triggering
Check that:
1. The underlying sensors have valid data
2. The thresholds are set appropriately
3. The alert delay period has been met
4. The binary sensor is properly configured

### Threshold controls not working
Verify that:
1. The number entities are properly configured
2. The min/max values are reasonable
3. The step size allows for the desired precision