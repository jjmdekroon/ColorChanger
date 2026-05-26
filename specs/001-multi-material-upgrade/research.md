# Phase 0 Research — Multi-material Upgrade

This document resolves the dependency, integration, and best-practice questions
implied by `plan.md`'s Technical Context. There are no `NEEDS CLARIFICATION`
markers remaining in `spec.md`; this research locks in the *technical* choices
that the spec deliberately left to implementation.

Format for each entry: **Decision** → **Rationale** → **Alternatives considered**.

---

## R-001 — Build system & toolchain

- **Decision**: PlatformIO Core with `platform = espressif32` (pinned to a
  specific stable release, e.g. `espressif32@6.5.0` or current LTS at task
  generation time), `framework = arduino`, three environments in one
  `platformio.ini`: `master`, `slave`, `native`.
- **Rationale**: PlatformIO is the de-facto tooling for multi-target Arduino-style
  ESP32 projects; multi-env in a single config keeps shared headers trivially
  resolvable and lets `native` host-tests reuse domain code via dependency
  injection. Pinning the platform satisfies Principle IV (pinned reuse).
- **Alternatives considered**:
  - *ESP-IDF directly*: more capable but Principle I/III/IV are served equally
    well by the Arduino-layer wrappers we need (`Wire`, `Servo`, GPIO), and the
    Arduino layer keeps the codebase familiar and the libraries off-the-shelf.
  - *Arduino IDE*: no multi-env, no proper test runner, no `lib_deps` pinning.

## R-002 — Stepper driver (TMC2209) interface

- **Decision**: `TMCStepper` library (TMC2209 UART configuration) combined with
  step/dir/enable GPIOs. Microstepping, current and StealthChop are set once in
  `setup()`; motion is driven step-pulse-based from the main loop. No
  StallGuard / stall-detection is enabled (constitution: no stall detection).
- **Rationale**: `TMCStepper` is the maintained reference library for Trinamic
  drivers; UART config keeps wiring count low. Step generation is done in the
  loop with `millis()`/`micros()`-paced pulses, deliberately avoiding a blocking
  AccelStepper-style integrator on the main thread.
- **Alternatives considered**:
  - *AccelStepper*: convenient but blocking-API-ish; step-rate ceiling on
    ESP32-C3 in a non-blocking pattern is fine for the modest feed rates
    (`STEPPER_FEED_RATE_HZ = 2000`) the design needs.
  - *Hand-rolled TMC UART*: rejected — Principle IV requires preferring a
    maintained library unless materially smaller or principle-violating.

## R-003 — Servo control

- **Decision**: `ESP32Servo` library, one servo per slave. PWM positions
  `SERVO_OPEN_US = 1000` and `SERVO_GRIP_US = 2000`, with a `SERVO_TRAVEL_MS`
  settling deadline tracked via `millis()` (no `delay()` while the servo
  settles).
- **Rationale**: `ESP32Servo` is the canonical maintained library for ESP32 PWM
  servo control and works on ESP32-C3. Treating travel time as a non-blocking
  deadline keeps Principle II's no-`delay()` rule.
- **Alternatives considered**:
  - *LEDC PWM hand-roll*: feasible but reinvents the wheel.

## R-004 — WS2812 LED driver

- **Decision**: `Adafruit_NeoPixel` (pinned). One pixel per slave, refreshed
  only on state changes plus a 20 Hz "tick" for blink/pulse patterns; refresh
  is gated so the bit-bang only runs when needed.
- **Rationale**: Adafruit_NeoPixel is the small, well-known library and works
  on ESP32-C3 RMT. Its bit-bang is a permitted Principle II "vetted library"
  exception. Refresh gating prevents the ~30 µs bit-bang from running every
  loop iteration.
- **Alternatives considered**:
  - *FastLED*: larger, more features than needed; either is acceptable but
    NeoPixel is smaller.

## R-005 — I2C addressing scheme & enumeration

- **Decision**: Master is sole bus-master on Wire at 100 kHz. During
  enumeration, the master uses a fixed *default* address `0x60` that every
  unaddressed slave listens on. The EN-chain ensures only one slave is
  unaddressed-and-listening at any time. Final addresses are assigned
  sequentially starting at `0x50` (slave 1) through `0x50 + (MAX_SLAVES - 1)`
  (slave N).
- **Rationale**: Matches the design already documented in `TECHNICAL_DESIGN.md`
  § 3 and `ARCHITECTURE.md` "EN-Chain". `0x50…0x5F` is a 16-address range that
  avoids common reserved I2C addresses and keeps `MAX_SLAVES = 16` as the cap.
- **Alternatives considered**:
  - *Per-slave hardware DIP switch*: forbidden by constitution
    ("electronically-fused identities are forbidden").

## R-006 — I2C frame format

- **Decision**: Fixed-length, 4-byte master→slave command frames:
  `[opcode, payload0, payload1, payload2]`. Slave→master reply on `PING`:
  4-byte `[mode, sensor_b, last_event, error_code]`. Opcodes per
  `TECHNICAL_DESIGN.md` § 8.2.
- **Rationale**: Smallest stable wire format that carries all needed data;
  fixed length simplifies parsing and removes the need for delimiters or
  variable-length state. Matches existing design.
- **Alternatives considered**:
  - *Variable length + length-byte*: more bytes, more failure modes.

## R-007 — USB-serial line protocol

- **Decision**: Line-oriented ASCII, `\n`-terminated. Commands: `T<nr>`,
  `L<n>`, `U<n>`, `R`, `S`. Responses: `ok`, `algeladen`, `busy`, `fail<n>`.
  Strict request/response per FR-019. Diagnostic lines per FR-020 are
  emitted as separate lines prefixed `# ` so Klipper safely ignores them.
- **Rationale**: Klipper macros can read/write a serial port with simple
  string matching; an ASCII line protocol is debuggable from any terminal.
  The `# `-prefixed diagnostic lines piggyback on the same channel without
  confusing the response parser.
- **Alternatives considered**:
  - *Binary protocol*: marginally faster but harder to debug and not warranted
    given the slow physical timescale of material-changes.

## R-008 — Klipper-side macro shape

- **Decision**: A single `klipper/mmu_macros.cfg` file defines
  `[gcode_macro T0]`, `[gcode_macro T1]`, … up to `MAX_KLIPPER_TOOLS` (default
  4) plus `MMU_LOAD`, `MMU_EJECT`, `MMU_RESET`, `MMU_STATUS`. Each macro uses a
  `[respond]`/`SERIAL_SEND`-style mechanism (or a small helper macro that
  writes to the configured USB-serial device) and blocks until either an `ok`
  or a `fail<n>` line is received, with `busy` triggering a Klipper-side
  retry (delay then re-send) up to `BUSY_RETRY_MAX`. On `fail<n>` the macro
  invokes `PAUSE` and emits the failure description via `RESPOND` (FR-003c).
- **Rationale**: Tool-change `T<nr>` macros are the standard Klipper hook for
  filament selection; the spec requires minimal Klipper changes. Klipper
  ships with `[pause_resume]` and `RESPOND`, which are sufficient.
- **Alternatives considered**:
  - *A custom Klipper extras module (Python plugin)*: more capable but
    requires shipping Python code to the operator's Klipper installation;
    rejected as overkill — the spec explicitly says "minimale Klipper-
    wijzigingen".

## R-009 — Filament sensor (microswitch) debounce

- **Decision**: 5 ms software debounce on the microswitch input, implemented
  as a stable-edge filter in `FilamentSensor::tick()`. Active state is
  logically `filamentPresent = !switchPressed` or vice versa depending on
  wiring, made explicit in `Config.h` via `SENSOR_ACTIVE_LOW`.
- **Rationale**: Microswitches bounce on the order of 1–3 ms; 5 ms is a safe
  margin without lengthening the FR-007 5 s feed timeout noticeably. The
  edge filter avoids spurious state transitions feeding the FSM.
- **Alternatives considered**:
  - *Hardware RC debounce only*: adds parts and is uncalibrated. Software
    debounce is the standard practice.

## R-010 — Compile-time configuration surface

- **Decision**: All tunables live in `master/include/Config.h` and
  `slave/include/Config.h` as `constexpr` with units in the name (e.g.
  `FEED_TIMEOUT_MS`, `BUS_RETRY_BACKOFF_MS[3] = {10, 20, 40}`,
  `FEED_RETRY_BACKOFF_MS[3] = {1000, 2000, 4000}`). The header carries a
  block comment listing the FR that motivates each value. Changing any value
  requires re-building and re-flashing.
- **Rationale**: FR-007 and the constitution forbid runtime configuration;
  one canonical header per image is the smallest discoverable surface.
- **Alternatives considered**:
  - *Per-feature `Config*.h` shards*: harder to audit; one file per image is
    the right granularity.

## R-011 — Diagnostic logging

- **Decision**: `DiagnosticLog` on master writes `# <ts_ms> <chan> <op> <code> <retry>\n`
  lines on USB-serial. Strings are flash-resident (`F()` / `PROGMEM`). A
  small in-RAM ringbuffer (256 B) stores recent events so `S` can include the
  last N events in its response.
- **Rationale**: FR-020 requires this stream for development/support; the
  ringbuffer satisfies the existing note in `TECHNICAL_DESIGN.md` § 13's
  "logging-target" discussion point. 256 B is justified by the constitution's
  "buffer > 256 B requires comment" rule (exactly at the threshold, with
  rationale here).
- **Alternatives considered**:
  - *Separate UART for logging*: forbidden by 5-pin pogo interface.

## R-012 — Host-side test strategy

- **Decision**: PlatformIO `env:native` builds the *pure* domain code
  (parsers, FSM, retry policy, frame encode/decode) against the host stdlib
  via small "platform shims" (`millis()` replaced with a deterministic
  clock; `Wire` and serial mocked via test doubles). Tests run with Unity.
- **Rationale**: Principle V mandates host-side testability. Keeping the
  domain layer free of Arduino-specific calls (encapsulated in `transport/`
  and `hal/`) makes this straightforward; the layering required by
  Principle I directly enables it.
- **Alternatives considered**:
  - *Hardware-in-the-loop only*: too slow, not reproducible in CI.
  - *Wokwi simulation as the primary test*: useful for board bring-up but
    ESP32-C3 modelling is uneven; not a replacement for unit tests.

## R-013 — Reference sources (authority)

- **Decision**: Authoritative references cited in code comments where
  non-obvious:
  - SEEED XIAO ESP32-C3 board datasheet & pinout (SEEED wiki).
  - Espressif ESP32-C3 Technical Reference Manual.
  - Arduino-ESP32 core API reference for `Wire`, `Serial`, `attachInterrupt`.
  - TMC2209 datasheet (Trinamic).
  - TMCStepper, ESP32Servo, Adafruit_NeoPixel READMEs at the pinned versions.
  - Klipper documentation (`Config_Reference.md`, `G-Codes.md`,
    `pause_resume`, `respond`).
- **Rationale**: Principle V "de-risking changes" requires authoritative
  citations for non-obvious peripheral or platform behaviour.
- **Alternatives considered**: none — this is process discipline, not a
  choice.

---

## Open questions explicitly deferred (not blocking implementation)

These are recorded in `TECHNICAL_DESIGN.md` § 13 and remain non-blocking:

- Exact micro-stepping value for the TMC2209 (will be tuned on bench in
  Phase 2 tasks).
- Specific pinout assignment per board (deferred to the bench wiring step in
  Phase 2; will be captured in `Config.h` once boards are wired).
- Maximum number of slaves beyond the firmware cap of 16 (cap is sufficient
  for the foreseeable use case and revisitable via a `Config.h` change).

No `NEEDS CLARIFICATION` markers remain. Phase 0 is complete.
