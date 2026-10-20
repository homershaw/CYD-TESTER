# CYD-TESTER

Cheap Yellow Display Modbus tester for the RIOT controller: a Modbus TCP server on an ESP32-2432S028R that emulates field devices described by RIOT Device Library TOML profiles. `PROMPT.md` has the full specification.

## Status

| Phase | Scope | State |
|---|---|---|
| 1 | Board bring-up: display, touch and calibration, RGB LED, BOOT button | Done, tested on hardware |
| 2 | Wi-Fi station, setup AP, captive portal, mDNS, BOOT 3 s / 10 s | Done, tested on hardware |
| 3 | Profile parser and native tests | Next |
| 4-8 | Modbus server, simulation, web UI, faults/diagnostics, OTA/SD | Not started |

Sizes (env `cyd`): phase 1 used RAM 49.6 KB and flash 1.11 MB. Phase 2 uses RAM 53.7 KB of 320 KB and flash 1.24 MB of the 1600 KB app slot.

## Build and flash

```
pio run -e cyd -t upload        # board on COM14 (CH340)
pio device monitor -e cyd
```

- On first boot, LittleFS formats itself and touch calibration runs (tap three crosses). Hold the screen during power-up to recalibrate.
- Calibration is stored in `/config/touch.toml`.

## Hardware notes

- All pins are in `include/board_pins.h`. `build_flags` force-includes it, so TFT_eSPI is configured without editing the library.
- Partitions (`partitions.csv`): two 1600 KB OTA app slots, 768 KB LittleFS, nvs, otadata and coredump.

## Assumptions

Each item is to be confirmed with the user (see PROMPT.md §14).

- The RIOT polls this unit using Modbus TCP on port 502 over Wi-Fi, even though the reference profiles say `transport = "rtu"`.
- `modbus-test` / `riotwifi` are the default station credentials (the network the unit joins). The `riot-XXXX` setup AP is open.
- Profiles that state no address basis use the register number unchanged on the wire, and are flagged "address basis not stated".
- The first test unit has no light sensor fitted (`HAS_LDR 0`). The `ldr` simulation mode falls back to a static value with a warning. Set `HAS_LDR 1` on boards that have one.
- Arduino core 2.0.14 (espressif32 6.5.0) is used, because TFT_eSPI is not reliable on core 3.x.
- The portal's "Save & connect" tries the new network before saving it. A failed attempt reports the reason and keeps the previous settings.
- A setup AP opened from the BOOT button or touch while the station is connected stays up while a phone is connected, and closes after 10 idle minutes. After a successful portal setup the AP always closes after 60 s, as the spec says.
- Station retries pause while a phone is connected to the setup AP, because retries change channel and would drop the phone.
