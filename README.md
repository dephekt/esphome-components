# esphome-components

Reusable [ESPHome](https://esphome.io) external components for grow and
environmental monitoring hardware — a thermal imaging camera, derived
statistics and threshold alerting for CO₂ sensors, typed support for liquid
chemistry probes, and M5Stack CoreS3 board support.

This repository owns **components only**. It contains no device firmware — real
per-device configs live in a separate fleet repository (`grow-fleet`) and pull
these components in by reference. Each component is independent and can be
adopted à la carte.

## Component catalog

| Component | What it does | Docs |
|---|---|---|
| **`mlx90640`** | Melexis MLX90640 32×24 thermal camera: per-pixel temperatures, min/max/avg and region-of-interest (ROI) stats, configurable emissivity/refresh/resolution, color palettes, and a self-contained thermal-viewer web page (plus on-demand JPEG renders) served over HTTP. | [README](mlx90640/README.md) · [example](mlx90640/example.yaml) |
| **`scd4x_stats`** | Derived sensors for an SCD4x CO₂ sensor: VPD (vapor-pressure deficit), daily min/max, and moving averages, with a midnight reset. | [README](scd4x_stats/README.md) |
| **`scd4x_alerts`** | Debounced threshold alerting over CO₂, temperature, humidity, and VPD. Emits `binary_sensor` alert outputs with user-adjustable `number` thresholds and configurable on/off delays. | [README](scd4x_alerts/README.md) |
| **`ezo_types`** | Typed support for Atlas Scientific **EZO** circuits (pH, EC, RTD, ORP). A multi-platform component (`sensor`/`select`/`number`/`switch`) with diagnostic sensors, calibration controls, and temperature-compensated reads. | [README](ezo_types/README.md) |
| **`board_m5cores3`** | M5Stack CoreS3 board bring-up: powers the AXP2101 PMIC, configures the AW9523 GPIO expander, and enables the display backlight — prerequisites before anything renders. | — |
| **`m5cores3_power`** | Polls the AXP2101 PMIC for battery / power state. | — |
| **`m5cores3_touch`** | Drives the CoreS3 touchscreen and fires touch/release automation triggers. | — |
| **`m5cores3_display`** | Renders environmental data and a color-mapped thermal image to the built-in CoreS3 screen, refreshing only changed values each loop. | — |
| **`grow_env_monitor`** | Integrative component that composes the others — thermal camera, CO₂/temp/humidity with derived VPD, threshold alerts, light status, and ROI stats — and renders the full picture to the CoreS3 display. | — |

## Architecture

Every component follows ESPHome's external-component convention and is built in
two layers:

- **Python codegen** (`__init__.py` and the platform modules `sensor.py`,
  `select.py`, `number.py`, `switch.py`). These define a `CONFIG_SCHEMA` that
  validates user YAML and a `to_code()` that emits the C++ wiring. This Python
  **runs on your build machine**, not on the device — it generates firmware
  source.
- **C++ firmware** (`*.h` / `*.cpp`). The on-device implementation the codegen
  instantiates and wires together — polling sensors, computing derived values,
  driving peripherals, and serving HTTP.

The `mlx90640` component additionally delegates to **vendored** Melexis driver
code (`MLX90640_API.*` for frame acquisition, EEPROM calibration, and
temperature math) behind a thin I²C shim (`MLX90640_I2C_Driver.*`) that adapts
the upstream driver onto an ESPHome `i2c::I2CBus`. The API files are essentially
untouched upstream code; the shim is the integration seam this repo owns.

The components are intentionally decoupled — there are no cross-component
imports. `grow_env_monitor` composes the others through ESPHome configuration,
not code dependencies, so you can use any single component on its own.

## Repository layout

```
<component>/
  __init__.py          # ESPHome codegen entry point (CONFIG_SCHEMA + to_code)
  sensor.py …          # additional platform modules, where applicable
  <component>.h/.cpp   # on-device C++ implementation
  README.md            # per-component config reference (where present)
docker/                # local ESPHome dev harness (see below)
```

## Using these components

Components live at the **repository root** (not in a `components/` subfolder),
which matters for how you reference them.

### Local source (recommended)

How the dev harness and the fleet repo consume them — vendor this repo (clone or
git submodule) and point ESPHome at the checkout:

```yaml
external_components:
  - source:
      type: local
      path: /path/to/esphome-components
    components: [mlx90640, scd4x_stats, scd4x_alerts]   # only what you use
```

### Git source

For external projects pulling directly from GitHub. Because components sit at the
repo root, set `path: .`:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/dephekt/esphome-components
      path: .
    components: [mlx90640]
```

Most components are I²C devices, so a standard ESPHome `i2c:` bus is a
prerequisite. See each component's README and `mlx90640/example.yaml` for
complete, copy-pasteable device configs.

## Local development

A Dockerized ESPHome dev loop lives in `docker/`. It pins the ESPHome image
(`ghcr.io/esphome/esphome:2026.5`), mounts this repo at `/config`, and exposes
USB and host networking for flashing, OTA, and mDNS. Scratch configs reference
the in-tree components via a `type: local`, `path: /config` source.

The `docker/esphome` wrapper runs the ESPHome CLI inside that container against
the local Docker daemon (run from anywhere; config paths are relative to the
repo root):

```bash
./docker/esphome config  mlx90640/example.yaml          # validate
./docker/esphome compile path/to/scratch.yaml           # build firmware
./docker/esphome run     path/to/scratch.yaml --device /dev/ttyACM0   # flash
```

Or run the ESPHome web dashboard:

```bash
docker compose -f docker/esphome-dev.yml up -d          # http://localhost:6052
```

## License

Vendored Melexis MLX90640 driver code under `mlx90640/` (`MLX90640_API.*`,
`MLX90640_I2C_Driver.*`) is third-party upstream code and retains its original
license.
