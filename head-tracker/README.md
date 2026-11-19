# Head Tracker

A head-mounted motion sensor (MPU6050) on its own ESP32 that broadcasts
orientation over ESP-NOW to [`../tilt-platform`](../tilt-platform), which
drives the pan/tilt servos to match.

## Bill of materials

- ESP32 dev board (separate board from the one used in `tilt-platform`)
- GY-521 breakout module (MPU6050 6-axis accelerometer/gyroscope, I2C)

## Connection scheme

```
                 ┌─────────────────────────┐
                 │          ESP32          │
                 │                         │
   MPU6050 VCC ◄─┤ 3.3V                    │
   MPU6050 GND ◄─┤ GND                     │
   MPU6050 SCL ◄─┤ GPIO22 (I2C SCL)        │
   MPU6050 SDA ◄─┤ GPIO21 (I2C SDA)        │
                 │                         │
                 └─────────────────────────┘
```

| Signal       | ESP32 pin | Notes                                      |
|--------------|-----------|---------------------------------------------|
| MPU6050 VCC  | 3.3V      | Power the module from 3.3V, not 5V, so I2C logic levels match the ESP32 |
| MPU6050 GND  | GND       |                                               |
| MPU6050 SCL  | GPIO22    | Default I2C clock pin (Arduino framework)    |
| MPU6050 SDA  | GPIO21    | Default I2C data pin (Arduino framework)     |

`XDA`, `XCL`, `AD0`, and `INT` are unused for this basic test. `AD0` sets the I2C address (floating/low = `0x68`, the default the firmware expects); only pull it high if you need a second MPU6050 on the same bus.

## Firmware

`src/main.cpp` initializes the MPU6050 and fuses the accelerometer and gyroscope into roll/pitch/yaw angles (a complementary filter). These angles are both printed as CSV over serial (115200 baud) — `roll,pitch,yaw` in degrees, for `scripts/visualize.py` — and broadcast over **ESP-NOW** to the tilt-platform board. If the sensor isn't detected, it prints a wiring warning and halts — a quick way to confirm the I2C connection is good before building anything on top of it.

The accelerometer and gyroscope measure fundamentally different things, which is why they're combined:
- **Accelerometer** tracks gravity's direction relative to the sensor — stable at rest, settles at a new value once you tilt and hold, but noisy under real motion.
- **Gyroscope** tracks rotation *rate*, not position — responsive and smooth during movement, but has no absolute reference, so it drifts if used alone.

The filter uses the gyro for short-term smoothness and the accelerometer to correct long-term drift on roll/pitch. Yaw has no accelerometer correction available (the MPU6050 has no magnetometer), so it will still slowly drift even when still — that's a sensor hardware limitation, not a bug. This means pan (driven by yaw) can drift toward one edge of its range over a long session; restart this board to re-zero yaw if that happens.

## Wireless link to the tilt-platform

Both boards use ESP-NOW **broadcast** — no MAC address pairing is needed, and no WiFi router/network is involved. Each board just needs to be running and within radio range (same rough range as regular WiFi, tens of meters) of the other; ESP-NOW handles the rest automatically once both are flashed and powered on. Each board prints its own MAC address at boot for reference/debugging, though it isn't required for the link to work.

If you want to confirm the link is working: watch the tilt-platform's serial monitor — it should print `signal: ok` and yaw/pitch values that track this board's motion once both are powered on nearby.

## Visualizing orientation

`scripts/visualize.py` reads the roll/pitch/yaw serial output and rotates a 3D box to match in real time, so you can see the fused orientation directly (red face = front).

```bash
pip install pyserial matplotlib numpy
python3 scripts/visualize.py /dev/ttyUSB0
```

Close any open serial monitor first — only one program can hold the serial port at a time.

## Build & deploy (PlatformIO / VS Code)

1. Install the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) in VS Code.
2. Open this folder (`head-tracker/`) in VS Code — PlatformIO will detect `platformio.ini` automatically.
3. Connect the ESP32 via USB.
4. Build and upload using the PlatformIO toolbar (checkmark = build, right-arrow = upload), or via CLI:

   ```bash
   pio run             # build
   pio run -t upload   # build + flash
   pio device monitor  # serial monitor (115200 baud)
   ```
