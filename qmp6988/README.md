# QMP6988 (hardened)

A drop-in replacement for ESPHome's built-in [`qmp6988`](https://esphome.io/components/sensor/qmp6988.html)
barometric pressure sensor that adds initialization and runtime reliability the
stock driver lacks. The YAML schema is **identical** to upstream — listing
`qmp6988` under `external_components` makes ESPHome use this version instead of
the built-in one, with no config changes required.

## Why this exists

On some M5Stack BPS (QMP6988) units the stock driver intermittently publishes a
fixed, physically impossible value — e.g. **-3418 hPa / -61.7 °C** — that never
recovers until the device is rebooted. The chip is healthy; the driver is the
problem:

- **It never verifies the sensor actually entered measuring mode.** `setup()`
  writes power mode and oversampling as three separate, unchecked I²C
  transactions. If any one is dropped, the device silently stays in sleep, its
  data registers never update, and the compensation math turns the stale
  registers into a constant garbage value.
- **It ignores return values.** The calibration read result is discarded, and
  `software_reset_()` compared `write_byte()` (a `bool`) against `i2c::ERROR_OK`,
  logging a spurious *"Software Reset failed"* on every *successful* reset.
- **It publishes whatever it computes**, so garbage propagates straight to MQTT /
  Home Assistant / downstream apps.

## What this version changes

- **Verified configuration with retries.** Power mode + both oversampling fields
  are written in a single `CTRLMEAS` (0xF4) transaction, read back, and retried
  (up to 3×) until the device is confirmed in NORMAL mode.
- **Runtime self-heal.** Each `update()` checks the reading for plausibility; if
  it fails (or the raw registers read all-zero, i.e. "not converting"), the
  component re-initializes and re-reads once, recovering without a reboot.
- **Output guard.** Values outside roughly **300–1200 hPa** or **-50–100 °C** are
  never published as data — the sensor reports *unavailable* (NaN) instead of
  garbage.
- **Correct return-value handling** for the reset and calibration reads (no more
  spurious error log).

These are additive hardening; the calibration and compensation math is unchanged
from upstream.

## Configuration

Same as the upstream component:

```yaml
i2c:
  sda: 2
  scl: 1
  frequency: 400kHz

external_components:
  - source:
      type: git
      url: https://github.com/dephekt/esphome-components
      path: .
    components: [qmp6988]

sensor:
  - platform: qmp6988
    address: 0x70          # QMP6988 default (SDO high); use 0x56 for SDO low
    update_interval: 60s
    iir_filter: 2x
    temperature:
      name: "BPS Temperature"
    pressure:
      name: "Barometric Pressure"
```

### Notes

- **Address.** `0x70` is the component default and a valid QMP6988 address (SDO
  pulled high). `0x56` is the other valid address (SDO low). The stock M5Stack
  BPS unit answers at `0x70`.
- **Unavailable readings.** When the sensor cannot produce a valid reading the
  component publishes NaN, which surfaces as *unavailable* in Home Assistant. A
  downstream consumer that renders raw MQTT strings should treat `nan` as "no
  data" rather than a number.

See [`example.yaml`](example.yaml) for a complete, copy-pasteable config.
