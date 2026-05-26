# Klipper MMU Integration

## Overview

This directory contains G-code macros and configuration for integrating the multi-material upgrade with Klipper on the 3D printer host.

## Files

- **mmu_tool_change.cfg**: T0..T3 tool-change macros
- **mmu_macros.cfg**: MMU_LOAD, MMU_EJECT, MMU_RESET, MMU_STATUS helper macros
- **mmu_integration.py**: (TODO) Custom Klipper module for serial communication with MMU master

## Installation

### 1. Add macros to your printer.cfg

```ini
[include /path/to/klipper/mmu_tool_change.cfg]
[include /path/to/klipper/mmu_macros.cfg]
```

### 2. Configure serial port in [mcu]

```ini
[mcu]
serial: /dev/ttyUSB0
baud: 115200

# Optional: if MMU uses a separate port
# [mcu mmu]
# serial: /dev/ttyMMU
# baud: 115200
```

### 3. Add MMU configuration section

```ini
[mmu]
# Port where MMU master is connected
port: /dev/ttyUSB0
# or if on separate MCU:
# port: mcu.gpiochip0
```

## Commands

### Tool Change

```gcode
T0    # Change to tool 0
T1    # Change to tool 1
# ... T2, T3
```

Waits for "ok" response before returning. Returns error if slave is offline or unreachable.

### Direct Load/Eject

```gcode
MMU_LOAD INDEX=0    # Load filament from slave 0 into hotend (no tool-change)
MMU_EJECT INDEX=0   # Eject filament from hotend (slave 0 stays loaded)
```

### Reset

```gcode
MMU_RESET   # Reset all slaves to initial state
```

### Status Query

```gcode
MMU_STATUS  # Query MMU status: coupled slave, tool index, etc.
```

Returns formatted status line: `state=<n> coupled=<idx|none> slaves=<count> tool=<idx|none>`

## Response Codes

- **ok**: Command completed successfully
- **algeladen**: Tool already loaded (for T<n> when n is already coupled)
- **busy**: MMU is busy (another command in flight)
- **fail<n>**: Error occurred, see error codes in [contracts/usb-serial.md](../contracts/usb-serial.md)

## Typical Gcode Sequence

```gcode
; Load tool 0
T0

; Perform printing with tool 0
G1 X0 Y0 Z0.2 F3000
...

; Change to tool 1
T1

; Continue printing with tool 1
...

; Reset all slaves
MMU_RESET
```

## Integration Architecture

```
Klipper (host)
    ↓
Serial port (115200 8N1)
    ↓
MMU Master (ESP32-C3)
    ↓
I2C bus
    ↓
MMU Slaves (2× ESP32-C3)
```

## Timeout Behavior

- **T<n> command**: Waits max 10 seconds for response
- **L<n> command**: Waits max 10 seconds for response
- **U<n> command**: Waits max 10 seconds for response
- **R command**: Waits max 5 seconds for response
- **S command**: Returns immediately (always allowed)

If timeout occurs, Klipper logs an error and continues. Use `MMU_STATUS` to check state.

## Troubleshooting

### "busy" responses

Check `MMU_STATUS` to see if MMU is still processing a previous command. Wait and retry.

### "fail<n>" responses

Refer to [contracts/usb-serial.md](../contracts/usb-serial.md) error table:
- fail1: Bad opcode
- fail2: Bad index
- fail3: Slave offline
- fail4: Illegal state
- fail16: Feed timeout
- fail32: Bus error
- fail48: Internal error

### No response / timeout

Check:
1. USB serial port is connected and accessible (`ls /dev/ttyUSB*`)
2. Baud rate is 115200 (must match master firmware)
3. MMU master is powered on
4. Slaves are enumerated (check LED indicator status)
5. Check [DiagnosticLog](../specs/001-multi-material-upgrade/contracts/usb-serial.md) for debug messages

## Future Enhancements

- Real-time status updates (broadcast mode for polling slaves)
- Automatic recovery on transient errors
- Material detection and dry-run mode
- Nozzle cleaning sequences per material
- Filament runout detection
- Pressure sensor integration for feed verification

## References

- [Specification](../specs/001-multi-material-upgrade/spec.md)
- [USB-serial protocol](../specs/001-multi-material-upgrade/contracts/usb-serial.md)
- [I2C frames](../specs/001-multi-material-upgrade/contracts/i2c-frames.md)
- [Klipper documentation](https://www.klipper3d.org/)
