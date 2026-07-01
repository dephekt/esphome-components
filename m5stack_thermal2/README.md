# M5Stack Unit Thermal2 Component

ESPHome component for the **M5Stack Unit Thermal2** (MLX90640 behind an onboard
PICO-D4, I2C address `0x32`). Unlike the raw MLX90640, the Thermal2 computes
temperatures onboard and adds a passive buzzer and an RGB LED. This component
drives all of it: temperature statistics, an off-center region-of-interest, a
`/thermal.jpg` image, and a **software temperature alarm** that beeps the buzzer
and flashes the RGB red.

> This is a separate component from `mlx90640` in this repo. The original
> `mlx90640` component talks to the raw Melexis sensor at `0x33` and cannot drive
> the Thermal2; this one speaks the Thermal2's register protocol at `0x32`.

## What it does

- Reads the 32×24 thermal array (assembled from the unit's two 16×24 subpages)
  and publishes whole-frame **and** ROI min/mean/max/median as ESPHome sensors.
- **Temperature alarm** (evaluated on-device in software): when the monitored
  region's chosen statistic crosses a high/low threshold, the unit's buzzer
  beeps and its RGB flashes red at a configurable cadence. Hysteresis prevents
  chatter.
- **Buzzer mute** switch: silences the beep while keeping the red flash.
- **Off-center software ROI** (`center_row`/`center_col`/`size`) — the alarm and
  the `roi_*` sensors watch exactly this region (the unit's hardware ROI is
  centered-only, so we do it in software).
- **Button-driven ROI toggle**: a click on the unit's button toggles ROI mode.
- **Status LED**: green = healthy, blue = ROI mode, flashing red = alarm. A
  `status_led: false` knob keeps it dark in normal/ROI states (alarm still
  flashes).
- `/thermal.jpg` HTTP endpoint with 10 color palettes and optional overlays,
  plus a self-contained `/thermal.html` viewer page.

## Quick start

See [example.yaml](example.yaml) for a complete, compile-checked configuration.
Minimum wiring:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dephekt/esphome-components
      path: .
    components: [m5stack_thermal2]

i2c:
  sda: 2
  scl: 1
  frequency: 400kHz

m5stack_thermal2:
  id: thermal_camera
  address: 0x32
  alarm:
    high_threshold: 35.0
    low_threshold: 5.0
  temperature_sensors:
    avg:
      name: "Thermal Mean temp"
  alarm_active:
    name: "Thermal Alarm"
  buzzer_enabled_control:
    name: "Buzzer Enabled"
```

The entity platforms (`sensor`, `binary_sensor`, `number`, `select`, `switch`)
are auto-loaded — no empty stub blocks are required. For the `/thermal.jpg`
endpoint, add a `web_server_base:` with an id and reference it via
`web_server: { enable: true, web_server_base_id: ... }`.

## Configuration reference

### Top-level

| Key | Default | Notes |
|---|---|---|
| `address` | `0x32` | I2C address |
| `refresh_rate` | `16Hz` | `0.5Hz`..`64Hz` (register 0x0B) |
| `noise_filter` | `8` | 0 (off) .. 15 |
| `update_interval` | `2000` | frame/stat/publish cadence, ms (raw int) |
| `thermal_palette` | `rainbow` | one of the 10 palettes |
| `status_led` | `true` | green/blue status heartbeat; `false` = dark |

### `alarm:`

| Key | Default | Notes |
|---|---|---|
| `source` | `average` | `average` / `median` / `min` / `max` |
| `region` | `active` | `active` (follow ROI mode) / `frame` / `roi` |
| `high_threshold` | `35.0` | °C |
| `low_threshold` | `5.0` | °C |
| `hysteresis` | `0.5` | °C |
| `buzzer_frequency` | `4000` | Hz (0 = silent) |
| `buzzer_volume` | `96` | 0-255 |
| `beep_interval` | `250` | ms on/off cycle for beep + flash |
| `led_color` | `[16, 0, 0]` | RGB of the alarm flash |

### `roi:`

`enabled`, `center_row` (1-24), `center_col` (1-32), `size` (1-10, region is a
`(2*size+1)` square). The ROI can be off-center.

### Sensors and status

`temperature_sensors:` with `min` / `max` / `avg` / `median` / `roi_min` /
`roi_max` / `roi_avg`. Optional `alarm_active:` (binary_sensor), `button:`
(binary_sensor for the unit button), and `alarm_test_button:` (a momentary
`button` that sounds one alarm cycle — buzzer + red flash — as a one-shot test,
regardless of the mute switch).

### Controls (all persist across reboots)

`buzzer_enabled_control` (the mute switch), `alarm_high_threshold_control`,
`alarm_low_threshold_control`, `thermal_palette_control`, `roi_enabled_control`,
`roi_center_row_control`, `roi_center_col_control`, `roi_size_control`,
`update_interval_control`, `web_overlay_enabled_control`.

### `web_server:`

`enable`, `path` (default `/thermal.jpg`), `quality` (10-100),
`overlay_enabled`, `html_page`, `web_server_base_id` (required when enabled).

## Hardware notes

- ESP32 (this repo targets the AtomS3U / ESP32-S3). The unit answers at `0x32`;
  run `i2c: { scan: true }` to confirm.
- The alarm's beep/flash is driven from the ESPHome loop (not the unit's
  autonomous alarm engine) so the alarm can watch an off-center region; this is
  a deliberate trade-off documented in the component.
