# Seeed XIAO nRF52840 support

This branch adds the plain (non-Sense) Seeed Studio XIAO nRF52840 as a RejsaRubberTrac target.

## Arduino board package

Use Seeed's **Seeed nRF52 Boards** package (the non-mbed package), which provides the
Adafruit Bluefruit BLE API used by RejsaRubberTrac.

Boards Manager URL:

```
https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json
```

Board:

```
Seeed XIAO nRF52840
```

Arduino CLI FQBN:

```
Seeeduino:nrf52:xiaonRF52840
```

The firmware includes `Adafruit_TinyUSB.h` for native USB Serial, as recommended
by Seeed for this board package.

## XIAO defaults in this branch

When the XIAO board macro is detected, `Configuration.h` automatically selects:

- board: `BOARD_NRF52_XIAO`
- temperature sensor: `FIS_AMG8833`
- AMG8833 refresh rate: 10 FPS
- distance sensor: disabled
- display: disabled
- default wheel position: `FR`

The existing RejsaRubber BLE service (0x1FF7) and packet structures are retained,
so the XIAO target speaks the same RejsaRubber temperature protocol as the existing
nRF52 target.

The XIAO has no Wi-Fi. The ESP32 web-server code is therefore excluded from XIAO builds.

## Wiring

AMG8833 uses the XIAO's default I2C bus:

| XIAO | AMG8833 |
|---|---|
| 3V3 | VCC / 3V3 |
| GND | GND |
| D4 / SDA | SDA |
| D5 / SCL | SCL |

## Battery

The XIAO's onboard LiPo charger is used directly. Battery measurement uses the
board's built-in battery divider, with `VBAT_ENABLE` driven low only while sampling
the battery ADC input.

The firmware allows the high-impedance divider to settle, discards warm-up ADC
conversions, averages 16 samples, and disables the divider again after each
measurement. No external 5 V boost converter is required for the XIAO + AMG8833 node.

## Persistent settings

ESP32 builds continue to use ESP32 `Preferences`/NVS.

The XIAO target uses the nRF52 core's built-in `InternalFS` LittleFS region in
internal flash. `RRPreferences` keeps the same `getBool`, `getInt`,
`getString`, `putBool`, `putInt` and `putString` interface used by the
existing firmware, but persists each value in LittleFS so settings survive reset
and complete power loss.

The settings filesystem is separate from the XIAO's external QSPI flash. That
leaves the external flash available for future logging or diagnostics.

`DEFAULT_WHEEL_POS` in `main/Configuration.h` remains the fallback used when
no persisted wheel position has been stored.

For example, after flashing one board you can assign it permanently from Serial
or BLE UART:

```
set wheel_pos FL
reset
get wheel_pos
```

Valid car positions are `FL`, `FR`, `RL`, and `RR`. The stored value is
read on the next boot and is included in the RejsaRubber BLE device name.

## Build

From the repository root on Windows:

```bat
build_xiao.bat
```

Upload after building:

```bat
upload_xiao.bat COM5
```

Replace `COM5` with the XIAO's actual port.
