# timezone

A `text` platform holding a **runtime POSIX-TZ override** for a `time`
component. The control system writes the timezone as ordinary desired state
(an entity) instead of it being baked immutably into the firmware.

- **Empty state = no override.** The timezone compiled into the firmware
  (`time:` → `timezone:`) stays in effect. Writing an empty string clears a
  stored override and restores the build default immediately.
- **Values are POSIX TZ strings**, e.g. `CST6CDT,M3.2.0,M11.1.0/2`. There is
  no IANA database on-chip — the string itself carries the base offset, DST
  offset, and DST transition rules, so DST flips happen autonomously on the
  device with no controller involvement. Convert IANA → POSIX server-side.
- **Invalid input is rejected**, not applied: the value is checked with the
  platform's POSIX TZ parser (over-length and embedded-NUL input is rejected
  too) and the previous state is re-published so an MQTT observer can see the
  write did not take. One exception: writes longer than 255 characters never
  reach the component — ESPHome's text core drops them against the entity's
  max-length trait without a re-publish, so writers should bound their own
  input (valid POSIX TZ strings top out around 40 chars).
- **Persisted to flash** and re-applied on boot (after the build-time default,
  so the override wins). On ESP8266, set `esp8266: restore_from_flash: true`
  — preferences default to RTC memory there and do not survive power loss.
- **The override is global.** ESPHome keeps a single process-wide timezone,
  so the override affects every `time` component in the build, not just the
  one referenced by `time_id`.

## Compatibility

- Requires ESPHome **2026.3.0 or newer** (`time/posix_tz.h`).
- The validation entry point, `time::parse_posix_tz`, is upstream bridge code
  scheduled for removal before 2026.9.0 (esphome/backlog#91). This component
  must migrate to whatever replaces it before tracking core past that.
- The `homeassistant` time platform re-asserts Home Assistant's timezone on
  every time sync unless it has an explicit `timezone:` in its config. The
  component re-applies the override right after each sync as a defense, but
  prefer setting an explicit `timezone:` there to avoid the churn.

## Usage

```yaml
time:
  - platform: sntp
    id: sntp_time
    timezone: America/Chicago   # build-time default

text:
  - platform: timezone
    name: "Time Zone"
    time_id: sntp_time
```

`time_id` may be omitted when there is exactly one time component.
