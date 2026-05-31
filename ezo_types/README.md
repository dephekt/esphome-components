# EZO Types Component

A typed, batteries-included ESPHome component for **Atlas Scientific EZO** circuits
(pH, EC/conductivity, RTD/temperature, ORP). It extends the stock `ezo` component
(via `AUTO_LOAD: ["ezo"]`) and adds first-class entities for the things the bare
`ezo` platform leaves you to wire up by hand: calibration, temperature
compensation, cell constant, TDS, and per-circuit diagnostics.

Each circuit `type` is its own self-contained universe — the component is generic
and makes no application-specific assumptions. Opinionation (which probe
compensates which, what units you display, how you alert) belongs in *your* YAML,
not in the component.

## Features

- **Four circuit types**: `ph`, `ec`, `rtd`, `orp`, selected with the `type:` key.
- **Temperature compensation**: point a pH/EC/ORP circuit at an `rtd_sensor:` and the
  component feeds its temperature into the circuit's compensated read, gated by a switch.
- **Component-owned config entities** that *read back* the circuit's actual stored
  state instead of being write-only: EC cell-constant `select`, TDS conversion-factor
  `number`, ORP extended-scale `switch`.
- **Rich diagnostics** per circuit: voltage, reset reason, firmware version,
  calibration status, the live command queue (current/next/last command, queue size),
  and pH probe-health sensors (slope quality, asymmetry potential).
- **Calibration helpers**: typed `lambda` methods for every circuit's calibration
  points, clear-calibration, and factory reset.

## Dependencies

- `i2c` — the EZO circuits are I²C devices.
- `select`, `switch` — used by the component-owned config entities.
- `ezo` — auto-loaded; provides the underlying `EZOSensor` C++ base.

## Basic configuration

```yaml
external_components:
  - source:
      type: local
      path: ..            # or a git source pointing at this repo
    components: ["ezo_types"]

i2c:
  sda: 23
  scl: 22
  scan: true

sensor:
  - platform: ezo_types
    type: rtd
    id: rtd_sensor
    name: "Water Temperature"
    address: 102
    update_interval: 5s

  - platform: ezo_types
    type: ph
    id: ph_sensor
    name: "Water pH"
    address: 99
    update_interval: 5s
    accuracy_decimals: 3
    rtd_sensor: rtd_sensor               # temperature source for compensation
    temp_compensation_switch: ph_temp_comp_enable
    calibration_mode_switch: ph_cal_mode_enable

  - platform: ezo_types
    type: ec
    id: ec_sensor
    name: "Water EC"
    address: 100
    update_interval: 5s
    rtd_sensor: rtd_sensor
    temp_compensation_switch: ec_temp_comp_enable
    cell_constant:               # EC-only: K select (0.1 / 1.0 / 10.0)
      name: "EC Cell Constant"
    tds:                         # EC-only: derived TDS output
      name: "Water TDS"
    tds_conversion_factor:       # EC-only: TDS factor (0.01–2.0; default 0.54)
      name: "EC TDS Conversion Factor"

  - platform: ezo_types
    type: orp
    id: orp_sensor
    name: "Water ORP"
    address: 98
    update_interval: 5s
    extended_scale:              # ORP-only: widen range to ±2040 mV (±2 mV accuracy)
      name: "ORP Extended Scale"

switch:
  - platform: template
    id: ph_temp_comp_enable
    name: "pH Temperature Compensation"
    optimistic: true
  - platform: template
    id: ph_cal_mode_enable
    name: "pH Calibration Mode"
    optimistic: true
  - platform: template
    id: ec_temp_comp_enable
    name: "EC Temperature Compensation"
    optimistic: true
```

A complete, working reference config (all four circuits, every diagnostic, GPIO
power control, calibration buttons, web-server sorting groups) lives at
[`configs/test-ezo-types.yaml`](../configs/test-ezo-types.yaml).

## Configuration variables

### Common to all types

- **type** (**Required**): one of `ph`, `ec`, `rtd`, `orp`.
- **address** (*Optional*): I²C address (defaults to the circuit's factory address).
- **power_control_switch** (*Optional*): id of a `switch` that powers the circuit's
  port. The component skips commands while the circuit is powered off, so a sleeping
  port doesn't flood the I²C bus with errors.
- **rtd_sensor** (*Optional*): id of an `ezo_types` `rtd` sensor to use as the
  temperature source for this circuit's compensation.
- **temp_compensation_switch** (*Optional*): id of a `switch` gating whether the
  `rtd_sensor` temperature is sent with each read.
- **calibration_mode_switch** (*Optional*): id of a `switch` that, while on, switches
  the circuit to plain uncompensated reads (`R`) and pins compensation at 25 °C — use
  it during calibration so a drifting temperature can't perturb the procedure.
- Diagnostic sub-sensors (all *Optional*): **voltage**, **reset_reason**,
  **firmware_version**, **calibration_status**, **temperature_compensation**,
  **current_command**, **next_command**, **last_command**, **queue_size**.

### `ph`

- **on_slope** (*Optional*): automation fired with the probe slope string.
- **acid_slope_quality** / **alkaline_slope_quality** / **asymmetry_potential**
  (*Optional*): probe-health diagnostic sensors.

### `ec`

- **cell_constant** (*Optional*): a `select` exposing the circuit's K value
  (`0.1` / `1.0` / `10.0`).
- **tds** (*Optional*): derived total-dissolved-solids sensor (ppm).
- **salinity** (*Optional*): derived salinity sensor (PSU).
- **relative_density** (*Optional*): derived specific-gravity sensor.
- **tds_conversion_factor** (*Optional*): a `number` (0.01–2.0) for the TDS scale.
  Default 0.54; use 0.50 for the ppm-500/NaCl scale common to US hydroponics, 0.70 for
  ppm-700/KCl.

### `rtd`

No type-specific configuration keys; only the common options above apply.

### `orp`

- **extended_scale** (*Optional*): a `switch` toggling the ORP circuit's extended
  reading range. Off (the default) the circuit reads ±1020 mV at ±1 mV accuracy; on,
  the range widens to ±2040 mV but accuracy drops to ±2 mV. Leave it off unless you
  need to read beyond ±1020 mV.

## Temperature compensation and units

The pH, EC, and ORP circuits compensate against temperature, and the Atlas
compensation command (`T` / `RT`) is **always Celsius** — those circuits have no
notion of a scale. The RTD circuit therefore reports in **°C**, and that °C value is
what the component sends to the other circuits for compensation. This is a hard
requirement of the EZO protocol, not a preference.

### Displaying temperature in °F or K

The component deliberately does **not** expose a control to change the RTD circuit's
own scale. Doing so would only invite a casual user to flip the circuit to °F from the
web UI and silently corrupt every pH/EC reading that depends on °C compensation — and
it solves a problem that isn't really a circuit problem. Unit *display* is a
presentation choice, so handle it the ESPHome way: derive a converted sensor in your
config, or let Home Assistant convert it. The authoritative reading and the
compensation stay in °C.

```yaml
sensor:
  # ... your `type: rtd` sensor with id: rtd_sensor ...

  # Optional: a Fahrenheit copy for display only.
  - platform: copy
    source_id: rtd_sensor
    name: "Water Temperature (°F)"
    unit_of_measurement: "°F"
    device_class: temperature
    state_class: measurement
    accuracy_decimals: 2
    filters:
      - lambda: return x * (9.0 / 5.0) + 32.0;

  # Optional: a Kelvin copy for display only.
  - platform: copy
    source_id: rtd_sensor
    name: "Water Temperature (K)"
    unit_of_measurement: "K"
    device_class: temperature
    state_class: measurement
    accuracy_decimals: 2
    filters:
      - offset: 273.15
```

## State readback

The component-owned config entities mirror the circuit's *actual* stored state rather
than being write-only. On startup each one queries the circuit (`K,?` for cell
constant, `TDS,?` for the conversion factor, `ORPext,?` for extended scale) and
publishes what it gets back, so the web UI and Home Assistant reflect reality instead
of a default. Writing to the entity still sends the corresponding command to the circuit.

The **ORP extended scale** is **not power-retained** by the EZO firmware — it resets
to off on every power cut. Because the EZO circuits power-cycle whenever the ESP
reboots, it always reads off at boot; that's the true state, not a bug. The **cell
constant** and **TDS conversion factor** *are* retained and survive reboots.

### Re-syncing on demand

Boot-time readback can be hard to observe (the circuits answer a couple of seconds
into boot, before logs/API reconnect). A template button that calls the public query
methods lets you re-sync all readback entities at any time — handy after changing
something at runtime:

```yaml
button:
  - platform: template
    name: "Refresh Circuit State"
    entity_category: diagnostic
    on_press:
      - lambda: |-
          id(ec_sensor).request_cell_constant_query();
          id(ec_sensor).request_tds_query();
          id(orp_sensor).request_extended_scale_query();
```

## Notes

- **Declare the `rtd_sensor` before the circuits that reference it.** ESPHome resolves
  the `rtd_sensor:` id at config time, so the RTD must appear first.
- **Calibration:** use the typed `lambda` helpers (`set_calibration_point_*`,
  `set_calibration_generic`, `clear_calibration`, `factory_reset`) from template
  buttons — see the reference config for the full set.
- **Powered-off circuits:** if you gate a circuit with `power_control_switch`, the
  component suspends its command queue while the port is off.
