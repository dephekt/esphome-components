# MLX90640 Thermal Camera Component

ESPHome component for the Melexis MLX90640 thermal imaging sensor. Provides temperature measurement, thermal imaging, and region-of-interest (ROI) analysis.

## What it does

- Reads 32×24 thermal array from MLX90640 sensor
- Publishes temperature statistics as ESPHome sensors
- Provides JPEG thermal images via HTTP endpoint
- Supports configurable ROI for targeted temperature monitoring
- Generates runtime controls for thermal settings adjustment

## Quick Start

See [example.yaml](example.yaml) for a complete working configuration. Copy the relevant sections to your ESPHome config:

- `external_components:` — a git source must list both `mlx90640` and its shared
  base `thermal_camera_core` (a local source picks the base up automatically)
- `web_server_base:` with an ID
- `mlx90640:` component with temperature sensors, controls, and web server configuration

The sensor/number/select/switch platforms are pulled in automatically via the
component's `AUTO_LOAD`, so no empty platform stubs are required. The example
includes all control entities that will appear in Home Assistant.

Access thermal image at `http://device-ip/thermal.jpg`

## Hardware Requirements

- ESP32 microcontroller
- MLX90640 thermal sensor on I2C bus
- 3.3V power supply for sensor

## Configuration

### Hardware Settings

```yaml
mlx90640:
  id: thermal_camera
  refresh_rate: "16Hz"        # 0.5Hz to 64Hz (default: 16Hz)
  resolution: "18-bit"        # 16-bit to 19-bit (default: 18-bit)
  pattern: "chess"            # chess or interleaved (default: chess)
  update_interval: 20000      # Update frequency in milliseconds
```

### Temperature Sensors

Auto-generated sensors for temperature statistics:

```yaml
mlx90640:
  temperature_sensors:
    min:
      name: "Thermal Min"
    max:
      name: "Thermal Max"
    avg:
      name: "Thermal Average"
    roi_min:
      name: "ROI Min"
    roi_max:
      name: "ROI Max"
    roi_avg:
      name: "ROI Average"
```

### Web Server

HTTP endpoint for thermal images:

```yaml
mlx90640:
  web_server:
    enable: true
    path: "/thermal.jpg"
    quality: 85                # JPEG quality 10-100
    overlay_enabled: true      # Show ROI and temperature overlays
    html_page: true            # Also serve a viewer page (default: true)
```

With `html_page` enabled, the component also serves a small self-contained
HTML page that renders the JPEG with an auto-refreshing `<img>`, so you can
watch the camera in a browser without Home Assistant. Its path is derived from
`path` by swapping a trailing `.jpg` for `.html` (e.g. `/thermal.jpg` ->
`/thermal.html`), so with the default config it lives at
`http://device-ip/thermal.html`. The page refreshes at the component's
`update_interval`.

### Region of Interest (ROI)

Configure a specific area for temperature monitoring:

```yaml
mlx90640:
  roi:
    enabled: true
    center_row: 12             # Row 1-24
    center_col: 16             # Column 1-32
    size: 3                    # Creates 7x7 area (2*size+1)
```

### Thermography Parameters

Control the heat calculation accuracy:

```yaml
mlx90640:
  # Static config — applied at boot, overridden by runtime controls when wired
  emissivity: 0.95            # Surface IR emissivity 0.1–1.0 (default: 0.95)
                              # Use 0.95 for most surfaces; metals can be much lower
  # reflected_temperature: 22.0  # Foil-measured reflected temperature in °C
                              # Omit (or comment out) to use automatic mode
  ta_shift: 8.0               # Auto mode: reflected temp = ambient − ta_shift (default: 8.0)
```

- **emissivity**: A property of the measured surface. Most non-metal surfaces are close to 0.95. Bare metal can be 0.05–0.3. Getting this wrong shifts all temperature readings.
- **reflected_temperature**: The apparent temperature reflected into the sensor from the scene background, measured with a piece of crumpled aluminium foil placed at the scene. If omitted, the component computes it automatically as `Ta − ta_shift` (ambient minus 8°C by default, FLIR convention).
- **ta_shift**: Only used when `reflected_temperature` is omitted (auto mode). Increase for hotter environments; decrease for cooler ones.

### Runtime Controls

Auto-generated controls for runtime adjustment:

```yaml
mlx90640:
  update_interval_control:
    name: "Update Interval"
  thermal_palette_control:
    name: "Color Palette"
  roi_enabled_control:
    name: "ROI Enable"
  roi_center_row_control:
    name: "ROI Row"
  roi_center_col_control:
    name: "ROI Column"
  roi_size_control:
    name: "ROI Size"
  web_overlay_enabled_control:
    name: "Web Overlay"

  # Thermography runtime controls (NVS-persisted, take effect next frame)
  emissivity_control:
    name: "Emissivity"             # 0.1–1.0, step 0.01
  reflected_temperature_control:
    name: "Reflected Temperature"  # −40–300°C, step 0.5; only used when auto is OFF
  reflected_temperature_auto_control:
    name: "Reflected Temperature Auto"  # ON → auto (Ta−ta_shift); OFF → use number above
```

All controls persist settings across reboots.

## Configuration Notes

- Factory calibration is for 16Hz, 18-bit resolution, chess pattern
- Other settings may reduce temperature accuracy
- Refresh rates above 32Hz may cause hardware issues
- Single-frame mode reduces accuracy but improves motion performance

## Color Palettes

Available thermal color schemes: `rainbow`, `golden`, `grayscale`, `ironblack`, `cam`, `ironbow`, `arctic`, `lava`, `whitehot`, `blackhot`

## API for Custom Components

```cpp
// Get thermal data
const float *pixels = thermal_camera->get_thermal_pixels();        // 32x24 array
const float *interpolated = thermal_camera->get_interpolated_pixels(); // 64x48 array

// Temperature statistics
float min_temp = thermal_camera->get_min_temp();
float max_temp = thermal_camera->get_max_temp();
float avg_temp = thermal_camera->get_avg_temp();

// ROI data (when enabled)
float roi_min = thermal_camera->get_roi_min_temp();
float roi_max = thermal_camera->get_roi_max_temp();
float roi_avg = thermal_camera->get_roi_avg_temp();

// Color mapping
uint16_t color = thermal_camera->temp_to_color(temperature, min_temp, max_temp);
```
