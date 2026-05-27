# ColorChanger Wiring Guide

This document explains how to wire the current firmware setup for:
- Master ESP32-C3 board
- Slave ESP32-C3 board(s)
- Stepper driver + stepper motor
- Servo
- Filament switch (microswitch)
- I2C and enable-chain links

Important:
- Pin numbers below are GPIO numbers used in firmware.
- Several pin assignments are still marked TODO in code and should be treated as provisional.
- Slave pin definitions are currently inconsistent between `slave/include/Config.h` and some HAL `.cpp` files. The firmware uses the values in the HAL files where local `#define` values exist.

## 1) Master ESP32-C3 Pin Assignments (current firmware)

From `master/src/hal/Stepper.cpp` and `master/src/hal/EnableChain.cpp`:

| Function | GPIO | Used For |
|---|---:|---|
| Stepper EN | 5 | Stepper driver enable (active LOW) |
| Stepper STEP | 6 | Step pulse output |
| Stepper DIR | 7 | Direction output |
| EN_OUT | 10 | Enable-chain output to first slave (active LOW) |
| EN_IN | 11 | Enable-chain input from upstream (active LOW) |

I2C on master:
- The code calls `Wire.begin()` without explicit SDA/SCL pins.
- That means SDA/SCL use the board default pins from the ESP32 Arduino core for `seeed_xiao_esp32c3`.
- Check your board pinout and verify the default SDA/SCL before final harness build.

USB serial on master:
- USB-C connection is used for Klipper <-> master serial transport.

## 2) Slave ESP32-C3 Pin Assignments (current firmware)

### Effective pins actually used by HAL code

From `slave/src/hal/*.cpp`:

| Function | GPIO | Source File |
|---|---:|---|
| Servo signal | 9 | `slave/include/Config.h` via `Gripper.cpp` |
| Filament switch input | 12 | `slave/src/hal/FilamentSensor.cpp` local `#define` |
| LED data (WS2812) | 14 | `slave/src/hal/LedIndicator.cpp` local `#define` |
| EN_IN | 15 | `slave/src/hal/EnablePin.cpp` local `#define` |
| EN_OUT | 16 | `slave/src/hal/EnablePin.cpp` local `#define` |

### Values also present in `slave/include/Config.h` (not all currently active)

| Function | GPIO in Config.h |
|---|---:|
| LED_PIN | 10 |
| SERVO_PIN | 9 |
| SENSOR_PIN | 8 |
| EN_IN_PIN | 7 |
| EN_OUT_PIN | 6 |

Because of local `#define` statements in HAL files, the current runtime pins for sensor/LED/enable are 12/14/15/16, not 8/10/7/6.

## 3) Component Wiring

## 3.1 Master -> Stepper Driver -> Stepper Motor

Connect master GPIO to your stepper driver logic inputs:
- GPIO5 -> Driver EN
- GPIO6 -> Driver STEP
- GPIO7 -> Driver DIR
- Master GND -> Driver GND

Then wire driver to stepper motor and motor supply as required by your driver module.

Notes:
- Firmware currently uses EN/STEP/DIR only.
- TMC UART configuration is not wired/used in the current `Stepper.cpp` implementation.

## 3.2 Slave -> Servo

Connect:
- Slave GPIO9 -> Servo signal (PWM)
- External 5V supply -> Servo V+
- Servo GND -> Slave GND and power supply GND (common ground)

Notes:
- Servo timing is configured with `SERVO_OPEN_US` / `SERVO_GRIP_US` in `slave/include/Config.h`.
- Do not power most servos directly from the ESP32 3V3 pin.

## 3.3 Slave -> Filament Switch (microswitch)

Current code expects active-low with pull-up (`INPUT_PULLUP`):
- One switch terminal -> Slave GPIO12
- Other switch terminal -> GND

Behavior:
- Switch closed to GND = filament detected.
- Switch open = not detected.

## 3.4 Slave -> LED (WS2812 / NeoPixel)

Connect:
- Slave GPIO14 -> LED data in
- 5V -> LED V+
- GND -> LED GND (common with slave)

For long wires, add standard WS2812 best practices (series resistor on data line, bulk capacitor on 5V rail).

## 3.5 Master <-> Slave I2C Bus

Bus wiring between master and all slaves in parallel:
- Master SDA <-> all slave SDA
- Master SCL <-> all slave SCL
- GND common across all boards

Notes:
- Current firmware runs I2C at 100 kHz.
- Ensure proper 3.3V pull-ups on SDA/SCL if your setup does not already provide them.

## 3.6 Enable Chain (daisy chain)

Enable-chain is point-to-point between neighbors:
- Master EN_OUT (GPIO10) -> Slave1 EN_IN (GPIO15)
- Slave1 EN_OUT (GPIO16) -> Slave2 EN_IN (GPIO15)
- Continue for additional slaves

Signal is active LOW in current code.

## 4) Suggested Cleanup (recommended before final hardware harness)

1. Unify slave pin definitions so all HAL modules read from `slave/include/Config.h`.
2. Move master pin definitions from local `#define` in HAL files into `master/include/Config.h`.
3. Explicitly set SDA/SCL pins in `Wire.begin(sda, scl)` for both master and slave for unambiguous wiring docs.
4. Re-test enumeration and load/eject flows after pin unification.

## 5) Quick Bench Checklist

- Master USB is visible to host.
- Stepper driver EN/STEP/DIR follow 5/6/7.
- Servo signal is on slave GPIO9 with external 5V and common ground.
- Filament switch is on slave GPIO12 to GND (active-low).
- LED data is on slave GPIO14.
- I2C SDA/SCL and GND are common between all nodes.
- Enable-chain is daisy-chained via 10->15 and 16->15 from module to module.
