# CYD-TESTER

Cheap Yellow Display Modbus tester for the RIOT controller: a Modbus TCP server on an ESP32-2432S028R that emulates field devices described by RIOT Device Library TOML profiles. `PROMPT.md` has the full specification.

## Status

| Phase | Scope | State |
|---|---|---|
| 1 | Board bring-up: display, touch and calibration, RGB LED, BOOT button | Done, tested on hardware |
| 2 | Wi-Fi station, setup AP, captive portal, mDNS, BOOT 3 s / 10 s | Done, tested on hardware |
| 3 | Profile parser, address map, codec, host tests | Done: 8/8 host tests, all reference profiles load |
| 4 | Modbus TCP server with static values | Done: 19/19 host tests, live checks over Wi-Fi |
| 5 | Simulation engine | Next |
| 6-8 | Web UI, faults/diagnostics, OTA/SD | Not started |

Sizes (env `cyd`): flash was 1.11 MB after phase 1, 1.24 MB after phase 2 and 1.27 MB after phase 3. Phase 4 uses RAM 54.0 KB of 320 KB and flash 1.29 MB of the 1600 KB app slot.

## Build and flash

```
pio run -e cyd -t upload        # board on COM14 (CH340)
pio device monitor -e cyd
```

- Host tests: `pio test -e native`. These need a host GCC, by default WinLibs at `C:\Users\scada\tools\winlibs-gcc-16.2.0` (set `CYD_HOST_GCC` to override). They parse every profile in `test/fixtures/` (copied from MutliFlowOTA/toml), check the spec's address and codec vectors, and exercise every Modbus function code and write rule.
- On first boot, LittleFS formats itself and touch calibration runs (tap three crosses). Hold the screen during power-up to recalibrate.
- Calibration is stored in `/config/touch.toml`.
- The built-in `test_bench` profile and sim are embedded in the firmware and written to `/profiles/` and `/sims/` only when missing, so a fresh unit works without a filesystem upload.

## Hardware notes

- All pins are in `include/board_pins.h`. `build_flags` force-includes it, so TFT_eSPI is configured without editing the library.
- Partitions (`partitions.csv`): two 1600 KB OTA app slots, 768 KB LittleFS, nvs, otadata and coredump.

## Assumptions

Each item is to be confirmed with the user (see PROMPT.md §14), except where marked confirmed.

- Confirmed: the RIOT polls this unit using Modbus TCP only (port 502 over Wi-Fi), even though the reference profiles say `transport = "rtu"`.
- The Modbus TCP server is our own small AsyncTCP implementation, not eModbus `ModbusServerTCPasync` as the spec suggests. eModbus answers unknown unit IDs with an error, and sends a malformed zero-length frame for "no reply". Its synchronous workers also can't delay replies, alter transaction IDs, or close after N requests, which the phase 7 faults need. Request handling is transport-free and covered by host tests.
- A `read_write` tag with no `write_function` accepts any suitable write function code, even with `strict_function_codes = true`. Strict mode only enforces a declared `write_function`.
- A broadcast write (unit ID 0) is applied to every enabled device, and never answered.
- Each tag starts at its sim `initial` value, or else the middle of `min`/`max`, or else 0.
- `modbus-test` / `riotwifi` are the default station credentials (the network the unit joins). The `riot-XXXX` setup AP is open.
- Profiles that state no address basis use the register number unchanged on the wire, and are flagged "address basis not stated".
- The first test unit has no light sensor fitted (`HAS_LDR 0`). The `ldr` simulation mode falls back to a static value with a warning. Set `HAS_LDR 1` on boards that have one.
- Arduino core 2.0.14 (espressif32 6.5.0) is used, because TFT_eSPI is not reliable on core 3.x.
- The portal's "Save & connect" tries the new network before saving it. A failed attempt reports the reason and keeps the previous settings.
- A setup AP opened from the BOOT button or touch while the station is connected stays up while a phone is connected, and closes after 10 idle minutes. After a successful portal setup the AP always closes after 60 s, as the spec says.
- Station retries pause while a phone is connected to the setup AP, because retries change channel and would drop the phone.
