# Quickstart — Multi-material Upgrade

This quickstart lists the bench-side steps to validate the three User Stories
from `spec.md` once both firmware images are flashed and a Klipper instance is
configured. Each step references the FRs and SCs it exercises.

## Prerequisites

- Hardware:
  - 1× SEEED XIAO ESP32-C3 wired as master, with a TMC2209 driver and the
    shared drive shaft + NEMA stepper.
  - ≥ 2× SEEED XIAO ESP32-C3 wired as slaves, each with: clamp servo, output
    microswitch (filament sensor), WS2812 LED, EN_IN/EN_OUT pogo pads.
  - Daisy-chained 5-pin pogo connections (`5V`, `GND`, `SCL`, `SDA`, `EN`)
    between master and slaves.
  - Raspberry Pi running Klipper ≥ 0.11.
- Firmware:
  - `master/` flashed onto the master board.
  - `slave/` flashed onto every slave board (single image, identity is
    positional).
- Klipper config:
  - `klipper/mmu_macros.cfg` included from `printer.cfg`, with the master's
    USB-serial device path configured.

## Bring-up checks (do these once)

1. Power master only. Open a serial monitor at 115200; verify the master prints
   a single `# … boot ok` diagnostic line and that an `S` command returns a
   state line with `slaves=0`. Validates: USB-serial transport, contract
   `S`-response format.
2. Power one slave. Within ≈ 1 s the slave's LED should briefly blink white,
   then settle (state-coded LED per FR-010). Running `S` on master should now
   show `slaves=1`. Validates: enumeration (FR-008/014/015) and contract
   `usb-serial.md` `S` line.
3. Add a second slave (hot-plug while master is idle). After at most one
   broadcast interval, `S` should report `slaves=2`. Validates: FR-008, FR-013,
   FR-013a.

## User Story 1 — Material selection during print (FR-001…FR-007, SC-002, SC-005)

1. Load filament into slave 1 manually via the `MMU_LOAD INDEX=0` macro
   (sends `L0`). Operator inserts filament until the slave's microswitch
   activates; servo clamps autonomously; LED turns green; master responds `ok`.
2. Repeat with `MMU_LOAD INDEX=1` for slave 2.
3. Start a small dual-material G-code print. The first `T0` is issued in the
   start sequence: master executes Process 3 (LOAD_PRINTER), responds `ok`;
   slave 1's LED turns blue.
4. At a planned tool change, Klipper issues `T1`. Master executes Process 4
   (UNLOAD_PRINTER) on slave 1, then Process 3 on slave 2, then responds `ok`.
   Slave 1 LED → green, slave 2 LED → blue.
5. Print completes normally.

**Expected**: every `T<nr>` is acknowledged with `ok` (or `algeladen` if the
target tool is already loaded). At no point are two slaves coupled. The
print's progress counter never resets (FR-012).

## User Story 2 — Hot-plug a module (FR-008, FR-013, FR-014, FR-015)

1. With master idle and one slave attached, run `S` — note `slaves=1`.
2. Mechanically and electrically attach a second slave (sliding it onto the
   pogo-pin chain). Wait at most one broadcast interval (≈ 1 s).
3. Run `S` — verify `slaves=2`.
4. Verify the new slave's LED indicates `EMPTY` (off) since no filament is
   loaded. Run `MMU_LOAD INDEX=1`; verify the load completes successfully.
5. Detach the second slave (master still idle). Run `S` — verify `slaves=1`
   again and that the remaining slave is unaffected.

## User Story 3 — Feed-path fault detection (FR-006, FR-007, FR-011, FR-011a)

1. Disable the filament sensor on one slave (e.g. wedge the microswitch open
   so it never reports filament) or remove its filament mid-load.
2. Issue `T<n>` for that slave during an idle-print scenario.
3. Observe: master executes Process 3 (LOAD_PRINTER), feed times out after
   `FEED_TIMEOUT_MS = 5000` ms. Master auto-retries the failing sub-step
   3× with backoffs 1/2/4 s (FR-011/011a). Slave LED stays/blinks yellow
   during retries.
4. After 3 failed retries: master responds `fail16` (`ERR_FEED_TIMEOUT`); the
   Klipper macro invokes `PAUSE` (FR-003c) and emits a `RESPOND` line with
   the failure description.
5. Operator clears the obstruction, runs `MMU_STATUS` then `RESUME` (or
   re-issues `T<n>`); verify recovery completes successfully.

## I2C bus-fault recovery (FR-017)

1. Briefly disconnect one slave's SDA pogo contact during a `PING` cycle (or
   simulate via removing the slave's I2C library on a test build).
2. Observe via diagnostic lines: master attempts the failing command 3× with
   `10/20/40 ms` backoff, then sets the channel to `FAULT` (red LED) and
   sends `fail32` (`ERR_BUS_TIMEOUT`).
3. Verify that other channels remain operational and the operator is paused
   (via Klipper `PAUSE`) per FR-003c.

## Success-criteria mapping

- **SC-001** (hot-plug usable < 60 s): timed in User Story 2 steps 2–4.
- **SC-002** (≥ 95 % material-changes succeed): exercised by repeated User
  Story 1 cycles (≥ 20 cycles).
- **SC-003** (feed-fault detection within 5 s): exercised by User Story 3
  step 3.
- **SC-004** (operator recovery < 2 min): exercised by User Story 3 step 5.
- **SC-005** (wrong-channel rate < 1/1000): exercised by long-running User
  Story 1 cycles with explicit channel-activation logging via FR-020
  diagnostics.

## Teardown

1. Issue `MMU_RESET` (`R`). All slaves return to their best-known clean state
   (per Process Reset in `TECHNICAL_DESIGN.md` § 6).
2. Power off the master; slaves are unpowered automatically (single supply).
