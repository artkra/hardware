# Tilt Platform

A 2-axis (pan/tilt) servo platform driven by an ESP32.

## Bill of materials

- ESP32 dev board (e.g. ESP32-DevKitC / NodeMCU-32S)
- 2x hobby servo (SG90 or similar, 5V)
- 5-pin analog joystick module (VRx, VRy, SW, +5V, GND)
- External 5V power supply for the servos (2-3A, do **not** power servos from the ESP32 5V pin)
- Common ground wire between the external supply and the ESP32

## Connection scheme

```
                 ┌─────────────────────────┐
                 │          ESP32          │
                 │                         │
   Pan servo ◄───┤ GPIO13 (signal)         │
                 │                         │
  Tilt servo ◄───┤ GPIO12 (signal)         │
                 │                         │
                 │                     GND ├──────┬───────────────┐
                 └─────────────────────────┘      │               │
                                                   │               │
                                             ┌─────┴─────┐   ┌─────┴─────┐
                                             │ Pan servo │   │Tilt servo │
                                             │  GND/PWR  │   │  GND/PWR  │
                                             └─────┬─────┘   └─────┬─────┘
                                                   │               │
                                                   └───────┬───────┘
                                                            │
                                                    ┌───────┴───────┐
                                                    │ External 5V PSU│
                                                    │  (+5V and GND) │
                                                    └────────────────┘
```

| Signal              | ESP32 pin | Notes                                  |
|---------------------|-----------|-----------------------------------------|
| Pan servo signal    | GPIO13    | PWM-capable pin                         |
| Tilt servo signal   | GPIO12    | PWM-capable pin                         |
| Servo power (+5V)   | External PSU | Not from ESP32 5V/3V3 pin           |
| Servo ground        | External PSU GND, tied to ESP32 GND | Common ground is required |
| Joystick VRx        | GPIO34    | ADC1 channel (analog input)             |
| Joystick VRy        | GPIO35    | ADC1 channel (analog input)             |
| Joystick SW         | GPIO25    | Digital input, uses internal pull-up    |
| Joystick +5V / GND  | ESP32 5V / GND | Joystick module draws negligible current, fine off the ESP32 |

ADC1 (GPIO32–39) is used rather than ADC2 because ADC2 becomes unusable once WiFi is active.

Pin numbers are defined as `PAN_PIN`/`TILT_PIN` in `src/main.cpp` — change them if you wire differently. The joystick wiring above is left in place for reference but isn't read by the current firmware (see below) — a joystick-controlled version is saved at `main.cpp.joystick-backup` if you want to swap back.

## Firmware

`src/main.cpp` now receives head orientation from [`../head-tracker`](../head-tracker) over **ESP-NOW** (WiFi-based, no router needed) and drives the servos to match: yaw → pan, pitch → tilt. It uses ESP-NOW **broadcast**, so no MAC address pairing between the two boards is required — this board just listens for any matching packet on the same WiFi channel.

The servo eases toward the received target at a limited rate (`MAX_STEP_DEGREES` per tick) rather than jumping straight there, for smooth motion. If the head-tracker goes silent for more than `SIGNAL_TIMEOUT_MS`, this board recenters both axes to 90° rather than holding a stale position indefinitely.

**Range limits:** `ANGLE_MIN`/`ANGLE_MAX` default to a conservative 30°–150° rather than the servo's full 0°–180°, since most brackets/mounts don't have clearance across the servo's entire theoretical range — commanding an angle the mount can't physically reach makes the servo strain against the obstruction (same buzzing as a stalled servo). Narrow or widen these to match whatever range your specific mount can clear without straining.

**Note on yaw drift:** the head-tracker's yaw has no absolute reference (no magnetometer), so it slowly drifts even when still. Over time this can push pan toward one edge of its range. There's no fix for this at the firmware level yet — restarting the head-tracker board re-zeros yaw.

## Build & deploy (PlatformIO / VS Code)

1. Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) in VS Code.
2. Open this folder (`tilt-platform/`) in VS Code — PlatformIO will detect `platformio.ini` automatically.
3. Connect the ESP32 via USB.
4. Build and upload using the PlatformIO toolbar (checkmark = build, right-arrow = upload), or via CLI:

   ```bash
   pio run             # build
   pio run -t upload   # build + flash
   pio device monitor  # serial monitor (115200 baud)
   ```
