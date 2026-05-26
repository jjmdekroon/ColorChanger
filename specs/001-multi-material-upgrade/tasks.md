---
description: "Tasks for Multi-material Upgrade voor Klipper 3D-printer"
---

# Tasks: Multi-material Upgrade voor Klipper 3D-printer

**Input**: Design documents from [specs/001-multi-material-upgrade/](.)

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md), [data-model.md](data-model.md), [contracts/usb-serial.md](contracts/usb-serial.md), [contracts/i2c-frames.md](contracts/i2c-frames.md), [quickstart.md](quickstart.md)

**Tests**: Included. The plan (Constitution Check, Principle V) and `research.md` (R-012) explicitly mandate test-first contract tests and host-side FSM/retry/parser tests on the PlatformIO `native` environment before production code is written.

**Organization**: Tasks are grouped by user story (US1 — material change, US2 — hot-plug, US3 — fault detection) to enable independent implementation and bench validation per [quickstart.md](quickstart.md).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Different file, no dependency on an incomplete earlier task — safe to parallelise.
- **[Story]**: `[US1]`, `[US2]`, `[US3]` for story-scoped tasks. Setup / Foundational / Polish phases carry no story label.
- Each task names the exact file path it touches.

## Path Conventions

Embedded firmware project per [plan.md](plan.md) §"Project Structure":

- Two PlatformIO firmware images: `master/` and `slave/` (Arduino-framework on ESP32-C3).
- Shared headers under `shared/`.
- Klipper-side config fragments under `klipper/`.
- Host-side tests under `test/native/` (PlatformIO `native` env, Unity).
- Single `platformio.ini` at repository root with three envs: `master`, `slave`, `native`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish the PlatformIO multi-env scaffold, pinned libraries, shared headers, and the directory skeleton that every later phase relies on.

- [ ] T001 Create the top-level repository layout per [plan.md](plan.md) §"Source Code": create empty directories `master/src/transport/`, `master/src/protocol/`, `master/src/domain/`, `master/src/hal/`, `master/include/`, `slave/src/transport/`, `slave/src/protocol/`, `slave/src/domain/`, `slave/src/hal/`, `slave/include/`, `shared/`, `klipper/`, `test/native/`, `test/integration/`. Add a `.gitkeep` to each so the empty tree is committable.
- [ ] T002 Create [platformio.ini](../../platformio.ini) at repo root with three environments (`[env:master]`, `[env:slave]`, `[env:native]`). Configure `platform = espressif32@<pinned>` and `framework = arduino` for `master` and `slave`; pin `lib_deps` for `TMCStepper`, `ESP32Servo`, `adafruit/Adafruit NeoPixel` with explicit versions or git tags (per [research.md](research.md) R-001/R-002/R-003/R-004). For `[env:native]` set `platform = native`, `test_framework = unity`, and `build_flags = -DNATIVE_TEST -std=c++17`. Add `board = seeed_xiao_esp32c3` to master/slave envs.
- [ ] T003 [P] Create [shared/Protocol.h](../../shared/Protocol.h) with the shared types per [data-model.md](data-model.md) §1: `I2cOpcode` (enum class : uint8_t), `SlaveMode` (enum class : uint8_t), `LastEvent` enum, `ErrorCode` (enum class : uint8_t), `I2cStatusFrame` (packed, 4 bytes) and `I2cCommandFrame` (packed, 4 bytes), each with the `static_assert(sizeof(...) == 4)` guard. Include an include-guard and the exact opcode/mode/error values from [data-model.md](data-model.md) §1.1, §1.2, §1.5.
- [ ] T004 [P] Create [master/include/Config.h](../../master/include/Config.h) with `constexpr` constants exactly as enumerated in [research.md](research.md) R-010 and referenced from [contracts/usb-serial.md](contracts/usb-serial.md) / [contracts/i2c-frames.md](contracts/i2c-frames.md): `MAX_SLAVES = 16`, `FEED_TIMEOUT_MS = 5000`, `FEED_RETRY_BACKOFF_MS[3] = {1000,2000,4000}`, `BUS_RETRY_BACKOFF_MS[3] = {10,20,40}`, `MAX_FEED_RETRIES = 3`, `MAX_BUS_RETRIES = 3`, `POLL_SLAVE_INTERVAL_MS = 50`, `BROADCAST_INTERVAL_MS = 1000`, `ENUM_DEFAULT_PING_TIMEOUT_MS = 50`, `BUSY_RETRY_HINT_MS = 500`, `BUSY_RETRY_MAX = 60`, `SERIAL_LINE_MAX = 63`, `I2C_DEFAULT_ADDR = 0x60`, `I2C_BASE_ADDR = 0x50`, `I2C_CLOCK_HZ = 100000`, `STEPPER_FEED_RATE_HZ = 2000`. Add a block comment listing the FR motivating each value (note: `BROADCAST_INTERVAL_MS` motivated by FR-013/FR-013a — idle-only topology poll cadence; SC-001 60 s hot-plug budget requires this ≤ ~2000 ms).
- [ ] T005 [P] Create [master/include/ErrorCodes.h](../../master/include/ErrorCodes.h) mapping `ErrorCode` enum values (from [shared/Protocol.h](../../shared/Protocol.h)) to the `fail<n>` numeric table in [contracts/usb-serial.md](contracts/usb-serial.md) §"Error code table": `fail1=ERR_BAD_OPCODE`, `fail2=ERR_BAD_INDEX`, `fail3=ERR_SLAVE_OFFLINE`, `fail4=ERR_ILLEGAL_STATE`, `fail16=ERR_FEED_TIMEOUT`, `fail17=ERR_SENSOR_STUCK`, `fail32=ERR_BUS_TIMEOUT`, `fail48=ERR_INVARIANT`, `fail255=ERR_INTERNAL`. Provide `constexpr const char* failTextFor(ErrorCode)` and `constexpr uint8_t failNumFor(ErrorCode)`.
- [ ] T006 [P] Create [slave/include/Config.h](../../slave/include/Config.h) with slave-side `constexpr` constants: `FEED_TIMEOUT_MS = 5000`, `SERVO_OPEN_US = 1000`, `SERVO_GRIP_US = 2000`, `SERVO_TRAVEL_MS = 300`, `SENSOR_DEBOUNCE_MS = 5`, `SENSOR_ACTIVE_LOW = true|false` (wiring placeholder with TODO comment), `LED_PIN`, `SERVO_PIN`, `SENSOR_PIN`, `EN_IN_PIN`, `EN_OUT_PIN` as placeholder constants with a TODO referring to the bench-wiring step in [research.md](research.md) "Open questions explicitly deferred".
- [ ] T007 [P] Configure PlatformIO `check_tool = clangtidy` and `check_flags = clangtidy: --checks=-*,readability-*,bugprone-*,cppcoreguidelines-*` in [platformio.ini](../../platformio.ini) for master/slave envs; add a `.clang-format` at repo root with `BasedOnStyle: LLVM, IndentWidth: 4, ColumnLimit: 100` for consistent formatting across [master/](../../master/) and [slave/](../../slave/).

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Build the shared protocol/transport/HAL layers, the enumeration mechanism, and the host-side contract tests that ALL three user stories build on. Test-first per Constitution Principle V.

**⚠️ CRITICAL**: No user-story work (Phase 3+) may begin until this phase is complete. Every host-side contract test in this phase MUST FAIL initially (no implementation), then pass once the foundational code lands.

### Foundational contract tests (host-side, write FIRST)

- [ ] T008 [P] Create [test/native/test_i2c_frame/test_i2c_frame.cpp](../../test/native/test_i2c_frame/test_i2c_frame.cpp) implementing the **frame round-trip** contract test from [contracts/i2c-frames.md](contracts/i2c-frames.md) §"Required contract tests" #1: encode any `I2cCommandFrame` to 4 bytes and decode back to an equal struct; same for `I2cStatusFrame`. Cover all opcodes and all `SlaveMode`/`ErrorCode` values. Use Unity asserts. MUST FAIL on first run (no encoder/decoder yet).
- [ ] T009 [P] Create [test/native/test_serial_protocol/test_parser.cpp](../../test/native/test_serial_protocol/test_parser.cpp) implementing the **parser positive + parser negative** contract tests from [contracts/usb-serial.md](contracts/usb-serial.md) §"Required contract tests" #1 and #2: well-formed `T<nr>`, `L<n>`, `U<n>`, `R`, `S` parse to the expected `SerialCommand`; malformed lines (`t0`, `T`, `T-1`, `TX`, line > 63 B) yield `fail<bad_command>` / `fail<bad_index>` / `fail<line_overflow>`. MUST FAIL on first run.
- [ ] T010 [P] Create [test/native/test_serial_protocol/test_responder.cpp](../../test/native/test_serial_protocol/test_responder.cpp) implementing the **responder serialisation** contract test (#3 in [contracts/usb-serial.md](contracts/usb-serial.md) §"Required contract tests"): given a synthetic `MasterContext`, the serialiser emits exactly `ok\n`, `algeladen\n`, `busy\n`, `fail<n>\n`, and the `S` state line per the documented format. MUST FAIL on first run.
- [ ] T011 [P] Create [test/native/test_enumeration/test_enumeration.cpp](../../test/native/test_enumeration/test_enumeration.cpp) implementing the **enumeration model** contract test (#3 in [contracts/i2c-frames.md](contracts/i2c-frames.md) §"Required contract tests"): with a simulated EN-chain of N=1, 3, and 16 slaves, the enumeration FSM assigns addresses `0x50..0x50+N-1` and ids `1..N` in chain order. MUST FAIL on first run.

### Foundational implementation — shared & transport

- [ ] T012 [P] Implement [master/src/protocol/I2CFrame.cpp](../../master/src/protocol/I2CFrame.cpp) and [master/src/protocol/I2CFrame.h](../../master/src/protocol/I2CFrame.h): pure encode/decode helpers (`encodeCommand(const I2cCommandFrame&, uint8_t out[4])`, `decodeStatus(const uint8_t in[4], I2cStatusFrame&)`) operating on the structs from [shared/Protocol.h](../../shared/Protocol.h). No Arduino dependencies — must compile under `env:native`. Makes T008 pass.
- [ ] T013 [P] Implement [slave/src/protocol/I2CFrame.cpp](../../slave/src/protocol/I2CFrame.cpp) and [slave/src/protocol/I2CFrame.h](../../slave/src/protocol/I2CFrame.h) mirroring the master-side encode/decode but for the slave's perspective (status frame produced, command frame consumed). Same Arduino-free pure code so it can be unit-tested.
- [ ] T014 Implement [master/src/protocol/SerialProtocol.h](../../master/src/protocol/SerialProtocol.h) declaring `SerialCommand` (per [data-model.md](data-model.md) §2.4), the parser entry point `parseLine(const char* line, size_t len, SerialCommand& out, ErrorCode& err)`, and the responder `serializeResponse(...)` for `ok`/`algeladen`/`busy`/`fail<n>`/`S`-state. Pure interface; no Arduino includes.
- [ ] T015 Implement the parser in [master/src/protocol/SerialProtocol.cpp](../../master/src/protocol/SerialProtocol.cpp): tokenise the line, validate command letter (`T`/`L`/`U`/`R`/`S` only — uppercase per [contracts/usb-serial.md](contracts/usb-serial.md)), parse the integer argument bounded by `slaveCount`, enforce `SERIAL_LINE_MAX = 63`. Makes T009 pass. Depends on T014.
- [ ] T016 Implement the responder in [master/src/protocol/SerialProtocol.cpp](../../master/src/protocol/SerialProtocol.cpp): serialise `ok`, `algeladen`, `busy`, `fail<n>` (using `failNumFor` from [master/include/ErrorCodes.h](../../master/include/ErrorCodes.h)), and the `S` state line `state=<MasterState> coupled=<int|none> slaves=<n> tool=<int|none>` exactly per [contracts/usb-serial.md](contracts/usb-serial.md) §"Response to `S`". Makes T010 pass. Depends on T014, T015.
- [ ] T017 [P] Implement [master/src/transport/SerialTransport.cpp](../../master/src/transport/SerialTransport.cpp) and [master/src/transport/SerialTransport.h](../../master/src/transport/SerialTransport.h): line-buffered USB-serial I/O wrapping `Serial` at 115200 8N1; non-blocking `pollLine(char* buf, size_t cap, size_t& outLen)`; `writeLine(const char* s)`. No echo. Bound by `SERIAL_LINE_MAX`. Arduino-only file; isolated from domain so `native` tests stub it.
- [ ] T018 [P] Implement [master/src/transport/I2CBus.cpp](../../master/src/transport/I2CBus.cpp) and [master/src/transport/I2CBus.h](../../master/src/transport/I2CBus.h): wraps Arduino `Wire` as bus-master at 100 kHz; provides `write(addr, const uint8_t buf[4])` and `request(addr, uint8_t out[4])` returning a transport-level result code (`OK`/`NACK`/`TIMEOUT`/`SHORT_READ`); enforces ≤1 ms clock-stretch tolerance.
- [ ] T019 [P] Implement [slave/src/transport/I2CSlave.cpp](../../slave/src/transport/I2CSlave.cpp) and [slave/src/transport/I2CSlave.h](../../slave/src/transport/I2CSlave.h): wraps Arduino `Wire` in slave-mode on the default address `0x60` and on the assigned address; provides callbacks `onReceive(const uint8_t buf[4], size_t len)` and `onRequest()` (returns the cached 4-byte status frame). Re-binds address on `SET_ID`.

### Foundational implementation — HAL layer

- [ ] T020 [P] Implement [master/src/hal/Stepper.cpp](../../master/src/hal/Stepper.cpp) and [master/src/hal/Stepper.h](../../master/src/hal/Stepper.h): TMC2209 wrapper via `TMCStepper` UART for one-time configuration in `setup()`; non-blocking step-pulse generation on `millis()`/`micros()` cadence at `STEPPER_FEED_RATE_HZ`; `start(direction)`, `stop()`, `isRunning()`. No stall detection (per [research.md](research.md) R-002).
- [ ] T021 [P] Implement [master/src/hal/EnableChain.cpp](../../master/src/hal/EnableChain.cpp) and [master/src/hal/EnableChain.h](../../master/src/hal/EnableChain.h): drives the master's `EN_OUT` GPIO; `deassertAll()` and `assert()` operations used during enumeration.
- [ ] T022 [P] Implement [master/src/hal/DiagnosticLog.cpp](../../master/src/hal/DiagnosticLog.cpp) and [master/src/hal/DiagnosticLog.h](../../master/src/hal/DiagnosticLog.h): emits `# <ts_ms> <channel> <opcode> <error> <retry>\n` lines via `SerialTransport::writeLine` per [contracts/usb-serial.md](contracts/usb-serial.md) §"Diagnostic lines". Strings via `F()`/`PROGMEM` per [research.md](research.md) R-011. In-RAM ringbuffer `DiagLogEvent[16]` (per [data-model.md](data-model.md) §2.6).
- [ ] T023 [P] Implement [slave/src/hal/FilamentSensor.cpp](../../slave/src/hal/FilamentSensor.cpp) and [slave/src/hal/FilamentSensor.h](../../slave/src/hal/FilamentSensor.h): microswitch read with 5 ms software debounce per [research.md](research.md) R-009; reports `sensorBStable` and `sensorEdgeMs`; honours `SENSOR_ACTIVE_LOW`.
- [ ] T024 [P] Implement [slave/src/hal/Gripper.cpp](../../slave/src/hal/Gripper.cpp) and [slave/src/hal/Gripper.h](../../slave/src/hal/Gripper.h): `ESP32Servo` wrapper with non-blocking travel; `close()`/`open()` set targets at `SERVO_GRIP_US`/`SERVO_OPEN_US` and arm a `SERVO_TRAVEL_MS` deadline; `isSettled()` reports completion.
- [ ] T025 [P] Implement [slave/src/hal/LedIndicator.cpp](../../slave/src/hal/LedIndicator.cpp) and [slave/src/hal/LedIndicator.h](../../slave/src/hal/LedIndicator.h): `Adafruit_NeoPixel` wrapper; refresh-gated 20 Hz tick per [research.md](research.md) R-004; `setState(SlaveMode, ErrorCode)` maps to the LED scheme in FR-010 (off / white blink / green / blue / yellow blink / red).
- [ ] T026 [P] Implement [slave/src/hal/EnablePin.cpp](../../slave/src/hal/EnablePin.cpp) and [slave/src/hal/EnablePin.h](../../slave/src/hal/EnablePin.h): reads `EN_IN`, drives `EN_OUT` (used in enumeration so the downstream slave wakes up).

### Foundational implementation — domain skeleton & enumeration

- [ ] T027 Create [master/src/domain/MasterContext.h](../../master/src/domain/MasterContext.h) per [data-model.md](data-model.md) §2.3 (`MasterContext` aggregating `state`, `slaves[MAX_SLAVES]`, `slaveCount`, `coupledSlaveIdx`, `currentToolIdx`, `inFlightCommand`, `busyDeadlineMs`, `lastBroadcastMs`, `broadcastSuspended`) and the per-slave `Slave` struct per §2.2. Declare `MasterState` enum per §2.1.
- [ ] T028 Create [slave/src/domain/SlaveContext.h](../../slave/src/domain/SlaveContext.h) per [data-model.md](data-model.md) §3.1 (`SlaveContext` with `mode`, `id`, `i2cAddress`, `sensorBStable`, `sensorEdgeMs`, `servoTargetReachedMs`, `servoClosedTarget`, `feedDeadlineMs`, `lastError`, `lastEvent`, `enInActive`, `enOutDriven`) and the `EnumState` enum from §3.2.
- [ ] T029 Implement [master/src/domain/Enumerator.cpp](../../master/src/domain/Enumerator.cpp) and [master/src/domain/Enumerator.h](../../master/src/domain/Enumerator.h): EN-chain enumeration FSM per [contracts/i2c-frames.md](contracts/i2c-frames.md) §"Enumeration sequence". Uses `EnableChain` + `I2CBus`; emits `SET_ID` to `0x60`, confirms via `PING`; assigns `0x50..0x5F`, ids `1..N`; stops on `ENUM_DEFAULT_PING_TIMEOUT_MS` timeout. Run-to-completion ticks driven from a host-injectable clock so the host test can drive it. Makes T011 pass.
- [ ] T030 Implement [slave/src/domain/EnumerationResponder.cpp](../../slave/src/domain/EnumerationResponder.cpp) and [slave/src/domain/EnumerationResponder.h](../../slave/src/domain/EnumerationResponder.h): slave-side enumeration handler. On EN_IN rising edge, registers as default-address listener on `0x60`; on receiving `SET_ID`, re-binds to assigned address and asserts `EN_OUT`. Per [contracts/i2c-frames.md](contracts/i2c-frames.md) §"Enumeration sequence" steps 4-7.
- [ ] T031 Implement [master/src/domain/SlaveBus.cpp](../../master/src/domain/SlaveBus.cpp) and [master/src/domain/SlaveBus.h](../../master/src/domain/SlaveBus.h): periodic per-slave PING loop at `POLL_SLAVE_INTERVAL_MS`; updates `Slave::mode`/`sensorB`/`lastError`/`lastPingMs`. Provides `sendCommand(slaveIdx, I2cCommandFrame&)` returning a transport result. Polling pauseable for bursty material-change activity per [contracts/i2c-frames.md](contracts/i2c-frames.md) §"Polling cadence".
- [ ] T032 Create [master/src/main.cpp](../../master/src/main.cpp) with `setup()` (init Serial, Wire, Stepper, EnableChain, DiagnosticLog; run Enumerator until complete) and a lean `loop()` of ≤30 lines per [plan.md](plan.md) Principle I: `serviceTransport(); serviceProtocol(); tickFsm(); driveHardware(); tickLed(); tickPolling();`. FSM tick body remains a TODO until per-story FSM tasks.
- [ ] T033 Create [slave/src/main.cpp](../../slave/src/main.cpp) with `setup()` (init Wire slave on `0x60`, Servo at `SERVO_OPEN_US`, FilamentSensor, LedIndicator, EnablePin). The slave MUST perform NO mechanical movement at boot — it has no stepper driver (the shared TMC2209 is master-owned). Per the amended FR-010a: after `SENSOR_DEBOUNCE_MS` stabilises, classify sensor-active → `READY` (LED green) and sensor-inactive → `EMPTY` / `IDLE_AWAITING_LOAD` (LED off). Any post-enumeration normalisation (retract/feed cycle to land in a known position) is the master's responsibility via GRIP + master-driven stepper and is OUT OF SCOPE for this task. Then a lean `loop()` of ≤30 lines: `serviceI2c(); tickSensor(); tickServo(); tickFsm(); tickLed();`. FSM tick body remains a TODO until per-story FSM tasks. Add a code comment at the boot-classification site citing FR-010a (amended).

**Checkpoint**: All foundational contract tests (T008–T011) PASS. Master enumerates a chain of slaves; slaves report status via PING. `S` command returns a well-formed state line. No tool-change, no broadcast loop, no fault-handling yet — those land in Phase 3+.

---

## Phase 3: User Story 1 — Materiaal kiezen tijdens print (Priority: P1) 🎯 MVP

**Goal**: An operator can issue `T<nr>` from Klipper and the master orchestrates the full sequential material-change sequence (FR-004a) on the addressed slave, returning `ok` / `algeladen` once the new filament is in the hotend. Implements FR-001…FR-005, FR-005a, FR-006, FR-012, FR-019, FR-003a, FR-003b.

**Independent Test**: Per [quickstart.md](quickstart.md) §"User Story 1": with two slaves loaded via `MMU_LOAD INDEX=0/1`, run a dual-material G-code with a `T0` then `T1`; verify both tool changes complete with `ok`, exactly one slave is `COUPLED` at a time, and LEDs follow green→blue→green per FR-010.

### Tests for User Story 1 (test-first)

- [ ] T034 [P] [US1] Create [test/native/test_master_fsm/test_tool_change.cpp](../../test/native/test_master_fsm/test_tool_change.cpp): exercises `MasterStateMachine` transitions for `T<nr>` per [STATE_TRANSITIONS.md](STATE_TRANSITIONS.md). Cases: T0 from `IDLE` with no tool loaded → traverses `VALIDATE → LOAD_SLAVE → LOAD_PRINTER → IDLE` and emits `ok`; T1 with T0 already loaded → traverses `UNLOAD_PRINTER → EJECT_SLAVE → LOAD_SLAVE → LOAD_PRINTER → IDLE`; T0 when T0 is already loaded → emits `algeladen` and stays in `IDLE`; T9 with `slaveCount=2` → emits `fail2` (`ERR_BAD_INDEX`); T0 against an offline slave → emits `fail3` (`ERR_SLAVE_OFFLINE`). MUST FAIL on first run.
- [ ] T035 [P] [US1] Create [test/native/test_slave_fsm/test_load_unload.cpp](../../test/native/test_slave_fsm/test_load_unload.cpp): exercises `SlaveStateMachine` per [SLAVE_STATE_TRANSITIONS.md](SLAVE_STATE_TRANSITIONS.md). Cases: `IDLE_AWAITING_LOAD` + sensor `0→1` → `LOADING` → `READY` (FR-005a autonomous load); `READY` + `GRIP` → `COUPLED`; `COUPLED` + `MODE_IN_PRINTER` → `IN_TRANSIT` → `IN_PRINTER`; `IN_PRINTER` + sensor `1→0` → `CATCHING` (autonomous catch); `IN_PRINTER` + `MODE_EJECTING` → `EJECTING` → `IDLE_AWAITING_LOAD` on sensor `1→0`. Illegal opcode in current state → `ERR_ILLEGAL_STATE`, mode unchanged. MUST FAIL on first run.
- [ ] T036 [P] [US1] Create [test/native/test_serial_protocol/test_request_response.cpp](../../test/native/test_serial_protocol/test_request_response.cpp): exercises the strict request/response contract (FR-019/FR-024/FR-025, [contracts/usb-serial.md](contracts/usb-serial.md) §"Required contract tests" #4): a `T0` is accepted; a `T1` arriving before terminal response returns `busy`; an `S` arriving during the same in-flight `T0` returns the full state line (`state=… coupled=… slaves=… tool=…`), NOT `busy`, and does NOT alter master state or counters (FR-024 exception to FR-025); after `T0` terminates with `ok`, a subsequent `T1` is accepted. MUST FAIL on first run.
- [ ] T036a [P] [US1] Create [test/native/test_master_fsm/test_load_eject.cpp](../../test/native/test_master_fsm/test_load_eject.cpp): exercises the master-side `L<n>` and `U<n>` command flows end-to-end through `MasterStateMachine` per FR-021 and FR-022. Cases: (a) `L<n>` on an `EMPTY` slave → GRIP + master-driven feed → sensor activates → respond `ok`; (b) `L<n>` on a slave already `READY` or `LOADED` → respond `ok` immediately with NO mechanical action (no GRIP, no stepper, no sensor verify), `coupledSlaveIdx` unchanged (FR-021 idempotency); (c) `L<n>` on an unregistered index → `fail2 ERR_BAD_INDEX`; (d) `L<n>` on a slave whose `lastError != OK` (FAULT) → `fail3 ERR_SLAVE_OFFLINE` (FR-009); (e) `L<n>` whose feed-sensor verification times out → retries 3× on the SAME slave with backoffs `1000/2000/4000` ms (FR-011/FR-011a), then `fail16 ERR_FEED_TIMEOUT` and channel FAULT; (f) `U<n>` on a `READY`/`LOADED` slave → master-driven retract until sensor deactivates → RELEASE → respond `ok`; (g) `U<n>` on a slave already `EMPTY` → respond `ok` immediately with NO mechanical action (FR-022 idempotency); (h) bus-level NACK on the GRIP frame of an `L<n>` → 3× bus-retry with `10/20/40` ms backoff (FR-017), bus-retry counter MUST NOT increment the feed-retry counter. MUST FAIL on first run.

### Implementation for User Story 1

- [ ] T037 [US1] Implement [master/src/domain/MasterStateMachine.h](../../master/src/domain/MasterStateMachine.h) declaring the state-transition table for `MasterState` per [STATE_TRANSITIONS.md](STATE_TRANSITIONS.md), with `tick(MasterContext&, uint32_t now_ms)` and command-acceptance helpers `handleSerialCommand(MasterContext&, const SerialCommand&)`.
- [ ] T038 [US1] Implement [master/src/domain/MasterStateMachine.cpp](../../master/src/domain/MasterStateMachine.cpp) — Process 1 (Validate) and Process 2 (LOAD_SLAVE / EJECT_SLAVE coupling) per [TECHNICAL_DESIGN.md](TECHNICAL_DESIGN.md) §6. Enforces the *max-1-coupled* invariant (FR-004) and emits the `GRIP` / `RELEASE` I2C opcodes via `SlaveBus`. Depends on T031, T037.
- [ ] T039 [US1] Extend [master/src/domain/MasterStateMachine.cpp](../../master/src/domain/MasterStateMachine.cpp) with Process 3 (LOAD_PRINTER) and Process 4 (UNLOAD_PRINTER) — the sequential FR-004a sequence: deactivate old (RELEASE on previously-coupled slave) → verify `sensor_b=0` via polling → activate new (GRIP) → verify `sensor_b=1`. Drives `Stepper::start/stop` between GRIP and sensor-verify. Emits `MODE_IN_PRINTER` / `MODE_EJECTING` to slaves at the appropriate point. Depends on T038.
- [ ] T040 [US1] Implement the strict request/response gate in [master/src/domain/MasterStateMachine.cpp](../../master/src/domain/MasterStateMachine.cpp): while `state != IDLE` and an in-flight `T<nr>`/`L<n>`/`U<n>`/`R` exists, any newly-parsed command **other than `S`** returns `busy` via the responder (FR-019/FR-025, makes T036 pass). The `busy` answer is written immediately; the in-flight command is not preempted. `S` is explicitly excluded per FR-024/FR-025: it MUST be dispatched directly to the responder (`serializeResponse` for the state line) without state change, without mechanical action, and without altering retry counters or `coupledSlaveIdx`/`currentToolIdx`, even mid-flight.
- [ ] T041 [US1] Implement [master/src/domain/Slave.cpp](../../master/src/domain/Slave.cpp) and [master/src/domain/Slave.h](../../master/src/domain/Slave.h): per-slave helpers (`isOnline`, `markCoupled`, `markReleased`, `setMode`) that maintain `Slave::mode` and the master's `coupledSlaveIdx`, with assertions enforcing the max-1-coupled invariant (returns `fail48 ERR_INVARIANT` to caller on violation).
- [ ] T042 [US1] Implement [slave/src/domain/SlaveStateMachine.h](../../slave/src/domain/SlaveStateMachine.h) and [slave/src/domain/SlaveStateMachine.cpp](../../slave/src/domain/SlaveStateMachine.cpp): encode the legality matrix `(SlaveMode × I2cOpcode)` from [SLAVE_STATE_TRANSITIONS.md](SLAVE_STATE_TRANSITIONS.md) and [contracts/i2c-frames.md](contracts/i2c-frames.md) §"State-machine integration"; handle GRIP/RELEASE/MODE_* opcodes; drive `Gripper` and update `mode`. Implements the FR-005a autonomous load (sensor `0→1` in `IDLE_AWAITING_LOAD` triggers `GRIP` autonomously). Updates `LedIndicator` on every mode change. Makes T035 pass. Depends on T024, T025, T028.
- [ ] T043 [US1] Implement the IN_PRINTER autonomous-catch sub-FSM in [slave/src/domain/SlaveStateMachine.cpp](../../slave/src/domain/SlaveStateMachine.cpp): on `mode == IN_PRINTER` and sensor edge `1→0`, autonomously close the servo and transition to `CATCHING`, then await `MODE_EJECTING` or `MODE_IN_PRINTER` confirmation per the slave state diagram.
- [ ] T044 [US1] Wire the parser → MasterStateMachine → responder in [master/src/main.cpp](../../master/src/main.cpp) `loop()` helpers: `serviceTransport()` calls `SerialTransport::pollLine`; `serviceProtocol()` calls `parseLine` and feeds the command into `MasterStateMachine::handleSerialCommand`; the machine eventually calls `serializeResponse` and `SerialTransport::writeLine`. Makes T034 pass. Depends on T037–T040.
- [ ] T045 [US1] Emit diagnostic lines via `DiagnosticLog` from `MasterStateMachine` at each state entry and each I2C command issue, per FR-020 and [contracts/usb-serial.md](contracts/usb-serial.md) §"Diagnostic lines". Format: `# <ts_ms> <channel> <opcode> <error> <retry>`.
- [ ] T046 [US1] Create [klipper/mmu_macros.cfg](../../klipper/mmu_macros.cfg) with `[gcode_macro T0]`, `[gcode_macro T1]`, `[gcode_macro T2]`, `[gcode_macro T3]` (4 tools default per [research.md](research.md) R-008). Each macro writes `T<n>\n` to the master USB-serial device and blocks on either `ok` / `algeladen` (continue) or `fail<n>` (invoke `PAUSE` + `RESPOND MSG="MMU fail<n>"` per FR-003c). On `busy` it waits `BUSY_RETRY_HINT_MS = 500` ms and retries up to `BUSY_RETRY_MAX = 60` times. The blocking-on-`ok` discipline is what preserves G-code line-counter continuity across a material change (FR-012 — the print resumes at the next G-code line without job re-initialisation).
- [ ] T047 [US1] Add `[gcode_macro MMU_LOAD]`, `[gcode_macro MMU_EJECT]`, `[gcode_macro MMU_RESET]`, `[gcode_macro MMU_STATUS]` to [klipper/mmu_macros.cfg](../../klipper/mmu_macros.cfg). (NOT parallelisable with T046 — same file; must run after T046.) `MMU_LOAD INDEX=<n>` → sends `L<n>\n`; `MMU_EJECT INDEX=<n>` → `U<n>\n`; `MMU_RESET` → `R\n`; `MMU_STATUS` → `S\n` and echoes the returned state line via `RESPOND`.
- [ ] T048 [P] [US1] Create [klipper/README.md](../../klipper/README.md) documenting how to include `mmu_macros.cfg` from `printer.cfg`, the required `[mcu mmu_master] serial: …` stanza for the master's USB-serial device, and the `BUSY_RETRY_*` variables.

**Checkpoint**: User Story 1 is independently testable per [quickstart.md](quickstart.md) §"User Story 1". `T<nr>` works end-to-end. MVP candidate.

---

## Phase 4: User Story 2 — Hot-plug van modules (Priority: P2)

**Goal**: A slave can be added or removed while the master is idle and the master detects this within ≤ one broadcast cycle, performs full re-enumeration, and updates `slaves[]` accordingly. Implements FR-008, FR-013, FR-013a, FR-014, FR-015, FR-016.

**Independent Test**: Per [quickstart.md](quickstart.md) §"User Story 2": start with one slave, hot-plug a second while master is `IDLE`, run `S` and verify `slaves=2`; detach it, verify `slaves=1` again; remaining slave is unaffected.

### Tests for User Story 2 (test-first)

- [ ] T049 [P] [US2] Create [test/native/test_enumeration/test_hotplug.cpp](../../test/native/test_enumeration/test_hotplug.cpp): simulate an idle master with a 1-slave chain; inject a topology change (chain now reports 2 slaves on next broadcast); assert that within one broadcast cycle the master triggers a full re-enumeration that yields `slaves=2`, ids `1..2`, addresses `0x50..0x51`. Then simulate removal of slave 2 and assert `slaves=1` and the removed slot is invalidated (FR-014/015). MUST FAIL on first run.
- [ ] T050 [P] [US2] Create [test/native/test_master_fsm/test_broadcast_gating.cpp](../../test/native/test_master_fsm/test_broadcast_gating.cpp): assert that `broadcastSuspended = true` while `state ∈ {LOAD_SLAVE, EJECT_SLAVE, LOAD_PRINTER, UNLOAD_PRINTER, VALIDATE}` and that `lastBroadcastMs` is NOT updated during these states (FR-013a); resumes within one tick of returning to `IDLE`. MUST FAIL on first run.

### Implementation for User Story 2

- [ ] T051 [US2] Add the periodic broadcast loop to [master/src/domain/MasterStateMachine.cpp](../../master/src/domain/MasterStateMachine.cpp): when `state == IDLE && !broadcastSuspended` and `now_ms - lastBroadcastMs >= BROADCAST_INTERVAL_MS`, invoke `Enumerator::detectTopologyChange()` which probes both the existing slave-set AND the default address `0x60` for a newly-arrived slave. On change → trigger full re-enumeration via `Enumerator::run(...)`. Suspend during non-idle states; resume on entering `IDLE`. Makes T050 pass. Depends on T029, T037.
- [ ] T052 [US2] Extend [master/src/domain/Enumerator.cpp](../../master/src/domain/Enumerator.cpp) with a `detectTopologyChange()` method: PINGs each known slave address with bus-retry budget = 1 (to distinguish removal from transient bus error — full FR-017 retries happen later); also writes a probe `PING` to `0x60` to detect a new unaddressed slave. Returns one of `NO_CHANGE`, `SLAVE_ADDED`, `SLAVE_REMOVED`, `MULTIPLE_CHANGES`.
- [ ] T053 [US2] Extend [master/src/domain/Enumerator.cpp](../../master/src/domain/Enumerator.cpp): on a detected change, perform full re-enumeration — drop all known addresses, deassert EN, then re-run the chain handshake. After re-enumeration update `slaveCount` and zero out per-slave retry counters / errors / `coupledSlaveIdx` (FR-016 reset semantics for `BOOT` apply here too). Makes T049 pass.
- [ ] T054 [US2] Implement the master-idle-only insertion gate (FR-005a back-half): when the master leaves `IDLE`, broadcast `MODE_READY` to every `IDLE_AWAITING_LOAD` slave so an operator-insertion during a print is NOT picked up. On return to `IDLE`, broadcast `MODE_AWAITING_LOAD` to every `EMPTY` slave. Implemented as transition hooks in [master/src/domain/MasterStateMachine.cpp](../../master/src/domain/MasterStateMachine.cpp).
- [ ] T055 [US2] Implement the slave boot classification in [slave/src/main.cpp](../../slave/src/main.cpp) `setup()` per FR-010a (amended): the slave performs NO mechanical movement at boot (it has no stepper driver — the shared TMC2209 is master-owned). After sensor debounce stabilises, classify sensor-active → `READY` (LED green) and sensor-inactive → `EMPTY` / `IDLE_AWAITING_LOAD` (LED off). The master is responsible for any post-enumeration normalisation via GRIP + master-driven stepper. Add a code comment at the boot-classification site citing FR-010a.
- [ ] T056 [P] [US2] Append a diagnostic line `# <ts> M ENUM OK <slaveCount>` after every completed enumeration via `DiagnosticLog` (FR-020) in [master/src/domain/Enumerator.cpp](../../master/src/domain/Enumerator.cpp), so the operator can observe re-enumeration events from the same USB-serial channel as `S`.

**Checkpoint**: User Story 2 testable independently. `S` reflects topology changes within ≤ one broadcast cycle while master is idle; topology stays frozen during prints.

---

## Phase 5: User Story 3 — Foutdetectie op filamentpad (Priority: P3)

**Goal**: Feed errors (FR-007) trigger automatic 3× retry with 1/2/4 s backoff (FR-011/011a); I2C bus errors (FR-017) trigger 3× retry with 10/20/40 ms backoff; on exhaustion the channel goes `FAULT` (red LED), the master returns `fail<n>` to Klipper, and the Klipper macro invokes `PAUSE`. Implements FR-007, FR-009, FR-010a fault leg, FR-011, FR-011a, FR-016 sticky-FAULT, FR-017, FR-020 retry-attempt diagnostic.

**Independent Test**: Per [quickstart.md](quickstart.md) §"User Story 3" and §"I2C bus-fault recovery": wedge the microswitch open on one slave, issue `T<n>`, observe 3× retry then `fail16` + Klipper `PAUSE`; separately disconnect a slave's SDA briefly, observe 3× bus-retry then `fail32` + Klipper `PAUSE`; other channels remain operational.

### Tests for User Story 3 (test-first)

- [X] T057 [P] [US3] Create [test/native/test_retry_policy/test_retry_policy.cpp](../../test/native/test_retry_policy/test_retry_policy.cpp): unit-test the `RetryPolicy` helper per [data-model.md](data-model.md) §2.5: `nextBusBackoff(attempt)` returns `{10,20,40}`; `nextFeedBackoff(attempt)` returns `{1000,2000,4000}`; `MAX_BUS_RETRIES = 3`; `MAX_FEED_RETRIES = 3`; attempt counter resets on success. MUST FAIL on first run.
- [X] T058 [P] [US3] Create [test/native/test_master_fsm/test_feed_retry.cpp](../../test/native/test_master_fsm/test_feed_retry.cpp): with a simulated slave that fails to register `sensor_b=1` within `FEED_TIMEOUT_MS`, drive `LOAD_PRINTER` via the FSM and assert the master retries ONLY the failing sub-step (FR-011a) on the SAME slave 3× with backoffs `1000/2000/4000` ms; the previously-deactivated slave is NOT touched; on exhaustion the channel state goes `FAULT`, the master responds `fail16`, and the slave's LED state is `FAULT`. MUST FAIL on first run.
- [X] T059 [P] [US3] Create [test/native/test_master_fsm/test_bus_retry.cpp](../../test/native/test_master_fsm/test_bus_retry.cpp): per [contracts/i2c-frames.md](contracts/i2c-frames.md) §"Required contract tests" #4 — inject 1/2/3 simulated NACKs then ACK; the master retries with backoffs `10/20/40` ms and succeeds. Then inject 4 NACKs in a row; assert the channel transitions to `FAULT`, the master emits `fail32`, and the bus-retry counter does NOT increment the feed-retry counter (FR-017 last sentence). MUST FAIL on first run.
- [X] T060 [P] [US3] Create [test/native/test_slave_fsm/test_fault_sticky.cpp](../../test/native/test_slave_fsm/test_fault_sticky.cpp): assert that once a slave reaches `FAULT` with `ERR_FEED_TIMEOUT` it is sticky — subsequent valid opcodes return `ERR_ILLEGAL_STATE` (state unchanged) until a master `R` (reset) procedure clears it (FR-016). Also assert the sensor-stuck edge-case (sensor reports filament while clamp open) → `FAULT` with `ERR_SENSOR_STUCK`. MUST FAIL on first run.

### Implementation for User Story 3

- [X] T061 [US3] Implement [master/src/domain/RetryPolicy.h](../../master/src/domain/RetryPolicy.h) and [master/src/domain/RetryPolicy.cpp](../../master/src/domain/RetryPolicy.cpp) per [data-model.md](data-model.md) §2.5: pure functions returning the backoff arrays and the max counts from [master/include/Config.h](../../master/include/Config.h). Makes T057 pass.
- [X] T062 [US3] Add the feed-retry logic to the LOAD_PRINTER / EJECT_SLAVE branches of [master/src/domain/MasterStateMachine.cpp](../../master/src/domain/MasterStateMachine.cpp): on `FEED_TIMEOUT_MS` deadline reached without `sensor_b` transitioning, increment `Slave::feedRetryCount`; if `< MAX_FEED_RETRIES`, arm a `FEED_RETRY_BACKOFF_MS[attempt]` delay then re-issue ONLY the failing sub-step (re-GRIP + re-start stepper) on the SAME slave (FR-011a); the deactivated slave is not touched. On exhaustion → set channel `FAULT`, respond `fail16`. Makes T058 pass. Depends on T039, T061.
- [X] T063 [US3] Add bus-retry logic to [master/src/domain/SlaveBus.cpp](../../master/src/domain/SlaveBus.cpp): each `write`/`request` is wrapped with up to `MAX_BUS_RETRIES = 3` retries on `NACK`/`TIMEOUT`/`SHORT_READ` with `BUS_RETRY_BACKOFF_MS[attempt] = {10,20,40}` ms; on exhaustion → return `BUS_FAULT` to caller. Caller (MasterStateMachine) maps `BUS_FAULT` to a channel-FAULT + `fail32 ERR_BUS_TIMEOUT` response. Bus-retry counter is per-command and does NOT touch `Slave::feedRetryCount` (FR-017). Makes T059 pass. Depends on T018, T031.
- [X] T064 [US3] Add `FAULT`-channel guards to `MasterStateMachine::handleSerialCommand` in [master/src/domain/MasterStateMachine.cpp](../../master/src/domain/MasterStateMachine.cpp): a `T<nr>` / `L<n>` / `U<n>` targeting a slave whose `Slave::lastError != OK` is rejected with `fail3 ERR_SLAVE_OFFLINE` (FR-009) until either a successful re-enumeration or an explicit `R` reset clears the fault.
- [X] T065 [US3] Add the sticky-FAULT logic to [slave/src/domain/SlaveStateMachine.cpp](../../slave/src/domain/SlaveStateMachine.cpp): once `mode == FAULT`, every opcode except an implicit re-enumeration (loss of EN_IN power-cycle) returns `ERR_ILLEGAL_STATE` and leaves state unchanged. Add the sensor-stuck detection: in any non-`COUPLED`/`IN_TRANSIT` mode where the servo target is OPEN, if `sensorBStable == true` for > `FEED_TIMEOUT_MS`, transition to `FAULT` with `ERR_SENSOR_STUCK`. Update `LedIndicator` to red. Makes T060 pass.
- [X] T066 [US3] Implement the `R` (reset) handler in [master/src/domain/MasterStateMachine.cpp](../../master/src/domain/MasterStateMachine.cpp) per [TECHNICAL_DESIGN.md](TECHNICAL_DESIGN.md) Process Reset: on `R`, broadcast `RELEASE` + `MODE_AWAITING_LOAD` to every slave, clear all `Slave::feedRetryCount`/`busRetryCount`/`lastError`, set `coupledSlaveIdx = -1`, set `currentToolIdx = -1`, then return to `IDLE` and respond `ok`. Equivalent semantics to the `BOOT` clean-start of FR-016 but without re-enumeration.
- [X] T067 [P] [US3] Emit per-retry diagnostic lines via `DiagnosticLog` (FR-020): on each retry attempt, write `# <ts_ms> <channel> <opcode> <error> <attempt>` with the actual `<attempt>` number (1..3 for feed retries, 1..3 for bus retries) so operators can correlate Klipper `PAUSE` events with the underlying fault.
- [X] T068 [P] [US3] Extend [klipper/mmu_macros.cfg](../../klipper/mmu_macros.cfg) `fail<n>` handling: map the specific numeric codes to human-readable `RESPOND TYPE=error MSG="..."` strings using a `{% if n == 16 %}feed timeout{% elif n == 32 %}I2C bus error{% elif n == 48 %}invariant violated{% else %}error <n>{% endif %}` block before invoking `PAUSE`. Per FR-003c the macro does NOT auto-retry on `fail<n>`; the operator resumes manually.

**Checkpoint**: All three user stories are independently testable per their [quickstart.md](quickstart.md) scenario. Feed faults and bus faults both behave as specified; LEDs reflect the fault state; Klipper pauses cleanly.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final hardening, complete contract-test coverage, integration documentation, and bench validation.

- [ ] T069 [P] Create [test/native/test_i2c_frame/test_opcode_legality.cpp](../../test/native/test_i2c_frame/test_opcode_legality.cpp) implementing the full **opcode legality matrix** (#2 in [contracts/i2c-frames.md](contracts/i2c-frames.md) §"Required contract tests") covering every `(SlaveMode × I2cOpcode)` pair; documented outcomes (transition / `ERR_ILLEGAL_STATE` / `ERR_BAD_OPCODE`) verified. Final coverage gate.
- [ ] T070 [P] Create [test/native/test_serial_protocol/test_diagnostic_noninterference.cpp](../../test/native/test_serial_protocol/test_diagnostic_noninterference.cpp) implementing the **diagnostic line non-interference** test (#5 in [contracts/usb-serial.md](contracts/usb-serial.md) §"Required contract tests"): a stream of `# `-prefixed diagnostic lines interleaved with the terminal response leaves the terminal response correctly extractable by a Klipper-style line matcher that filters `# `.
- [ ] T071 [P] Create [test/native/test_enumeration/test_polling_suspension.cpp](../../test/native/test_enumeration/test_polling_suspension.cpp) implementing the **polling suspension** test (#5 in [contracts/i2c-frames.md](contracts/i2c-frames.md) §"Required contract tests"): under simulated material-change activity, no `PING` is emitted; within one `POLL_SLAVE_INTERVAL_MS` after activity drops, polling resumes.
- [ ] T072 Create [test/integration/README.md](../../test/integration/README.md) documenting the bench-side execution of each [quickstart.md](quickstart.md) scenario; map each step to its FRs/SCs; provide a checklist for SC-001/SC-002/SC-003/SC-004/SC-005 measurement (e.g. "execute ≥ 20 material-change cycles for SC-002"). Include an explicit FR-012 check: after a `T<nr>` returns `ok` mid-print, verify the next G-code line is executed without job re-initialisation and that the printed layer count advances continuously across the tool change.
- [ ] T073 [P] Sweep the codebase for any `delay()` / `String` / `new`/`malloc` on hot paths (Principle II/III) and replace with `millis()` deadlines / fixed buffers / static storage. Document any remaining exceptions in a top-of-file comment block in [master/src/main.cpp](../../master/src/main.cpp) and [slave/src/main.cpp](../../slave/src/main.cpp).
- [ ] T074 [P] Add CI configuration at [.github/workflows/ci.yml](../../.github/workflows/ci.yml) (or document the equivalent manual command in [README.md](../../README.md)) to build `master`, `slave`, and `native` envs and run `pio test -e native` on every PR — guarantees the Principle V contract tests stay green.
- [ ] T075 [P] Add a top-level [README.md](../../README.md) describing the project structure (links to [specs/001-multi-material-upgrade/](.) and key contracts), how to flash master/slave, how to include `mmu_macros.cfg` in `printer.cfg`, and how to run host tests.
- [ ] T076 Run the full [quickstart.md](quickstart.md) on bench hardware: bring-up checks (3 steps), User Story 1 (5 steps), User Story 2 (5 steps), User Story 3 (5 steps), I2C bus-fault recovery (3 steps), teardown. Capture results against SC-001…SC-005 and the FR-012 print-progress check (T072) in [test/integration/README.md](../../test/integration/README.md).

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: T001 first (creates the directory tree). T002 depends on T001. T003–T007 depend on T002 (need [platformio.ini](../../platformio.ini) and `shared/` folder).
- **Foundational (Phase 2)**: depends on Phase 1 complete. Tests T008–T011 are written first and MUST FAIL. Implementation T012–T033 follows. T029/T031/T032/T033 close the foundation.
- **User Story 1 (Phase 3)**: depends on Phase 2 complete.
- **User Story 2 (Phase 4)**: depends on Phase 2 complete. Independent of US1's tool-change FSM but shares `Enumerator` (T029) and `MasterStateMachine` (T037).
- **User Story 3 (Phase 5)**: depends on Phase 2 complete and on US1's `MasterStateMachine` (T037–T039) to attach retry logic. US3 is NOT independent of US1 at the code level (retries wrap the LOAD_PRINTER flow) but IS independent at the test/observable level.
- **Polish (Phase 6)**: depends on all three user stories.

### Within Each User Story

- Tests (T034–T036, T049–T050, T057–T060) MUST be written and FAIL before implementation.
- Domain headers (`MasterStateMachine.h`, `SlaveStateMachine.h`) before .cpp implementations.
- I2C / Serial drivers (Phase 2) before any FSM that calls them.
- Klipper macros (T046–T048) can be written in parallel with master FSM code since they're separate files in [klipper/](../../klipper/).

### Parallel Opportunities

- **Phase 1**: T003, T004, T005, T006, T007 are all separate files → all parallelisable.
- **Phase 2 tests**: T008, T009, T010, T011 are 4 separate test files → all parallelisable.
- **Phase 2 HAL**: T020 (Stepper), T021 (EnableChain), T022 (DiagnosticLog), T023 (FilamentSensor), T024 (Gripper), T025 (LedIndicator), T026 (EnablePin) are all separate files → all parallelisable. Transport-layer T017/T018/T019 likewise.
- **Phase 3 tests**: T034, T035, T036, T036a separate files → parallelisable.
- **Phase 3 Klipper**: T046 and T047 both edit `klipper/mmu_macros.cfg` and are NOT mutually parallelisable; T048 (separate file) IS parallelisable with both. All Klipper tasks are parallelisable with master-firmware tasks T037–T045.
- **Phase 4 tests**: T049, T050 → parallelisable.
- **Phase 5 tests**: T057, T058, T059, T060 → parallelisable.
- **Phase 6**: T069, T070, T071, T073, T074, T075 → parallelisable.

---

## Parallel Example: Phase 2 foundational tests

```
# Launch all foundational contract tests together (all must FAIL initially):
Task T008: I2C frame round-trip → test/native/test_i2c_frame/test_i2c_frame.cpp
Task T009: USB-serial parser positive/negative → test/native/test_serial_protocol/test_parser.cpp
Task T010: USB-serial responder serialisation → test/native/test_serial_protocol/test_responder.cpp
Task T011: Enumeration model → test/native/test_enumeration/test_enumeration.cpp
```

## Parallel Example: User Story 1 tests + Klipper macros

```
# Tests (written first):
Task T034: Master FSM tool-change → test/native/test_master_fsm/test_tool_change.cpp
Task T035: Slave FSM load/unload → test/native/test_slave_fsm/test_load_unload.cpp
Task T036: Strict request/response → test/native/test_serial_protocol/test_request_response.cpp

# Klipper-side macros (independent of master firmware code).
# NOTE: T046 and T047 touch the SAME file (klipper/mmu_macros.cfg) — run sequentially.
Task T046: T0..T3 macros → klipper/mmu_macros.cfg
Task T047: MMU_LOAD/EJECT/RESET/STATUS macros → klipper/mmu_macros.cfg (after T046)
Task T048: Klipper README → klipper/README.md (parallelisable with T046/T047)
```

---

## Implementation Strategy

### MVP First (User Story 1 only)

1. Complete Phase 1: Setup (T001–T007).
2. Complete Phase 2: Foundational (T008–T033). Contract tests T008–T011 PASS at the end.
3. Complete Phase 3: User Story 1 (T034–T048). Bench-test per [quickstart.md](quickstart.md) §"User Story 1".
4. **STOP and VALIDATE**: bench-print a dual-material G-code with 2 slaves.

### Incremental Delivery

1. Phase 1 + Phase 2 → foundation ready, enumeration works.
2. Phase 3 (US1) → MVP shipped (single tool change with two slaves).
3. Phase 4 (US2) → hot-plug works, broadcast properly gated.
4. Phase 5 (US3) → robust against feed faults and bus faults.
5. Phase 6 → polished, CI-protected, bench-validated against SC-001…SC-005.

### Parallel Team Strategy

After Phase 2 completes:
- Developer A → US1 (firmware: T037–T045, T034–T036).
- Developer B → US1 Klipper-side (T046–T048) in parallel with A.
- Developer C → US2 (T049–T056) in parallel.
- Developer D → US3 (T057–T068) in parallel once US1's `MasterStateMachine` skeleton (T037–T039) exists.

---

## Task Count Summary

- **Phase 1 — Setup**: 7 tasks (T001–T007).
- **Phase 2 — Foundational**: 26 tasks (T008–T033).
- **Phase 3 — US1 Material change (P1 / MVP)**: 16 tasks (T034–T048, incl. T036a).
- **Phase 4 — US2 Hot-plug (P2)**: 8 tasks (T049–T056).
- **Phase 5 — US3 Fault detection (P3)**: 12 tasks (T057–T068).
- **Phase 6 — Polish & validation**: 8 tasks (T069–T076).
- **Total**: **77 tasks**.

### Per-Story Task Allocation

| Story | Phase | Tasks | Count |
|-------|-------|-------|-------|
| US1 — Material change (P1 / MVP) | Phase 3 | T034–T048 (incl. T036a) | 16 |
| US2 — Hot-plug (P2)              | Phase 4 | T049–T056 | 8  |
| US3 — Fault detection (P3)       | Phase 5 | T057–T068 | 12 |

### Parallel Opportunities Identified

- 5 of 7 Setup tasks (T003–T007) parallelisable.
- All 4 Foundational contract tests (T008–T011) parallelisable.
- 10 of 26 Foundational implementation tasks parallelisable (HAL + transport).
- 4 of 4 US1 tests parallelisable; 3 of 16 US1 implementation tasks (Klipper-side) parallelisable with the rest.
- 2 of 2 US2 tests parallelisable.
- 4 of 4 US3 tests parallelisable; final retry/diagnostic tasks parallelisable.
- 6 of 8 Polish tasks parallelisable.

### Suggested MVP Scope

Phases 1 → 2 → 3 (T001–T048) deliver the P1 user story end-to-end: a working dual-material print driven by `T<nr>` from Klipper, exactly the scope that [quickstart.md](quickstart.md) §"User Story 1" validates. Stop and demo here before continuing.

### Format Validation

All 77 tasks follow the strict checklist format: every line begins with `- [ ]`, carries a sequential `T<NNN>` id (with T036a as an inserted addendum for FR-021/FR-022 master-flow coverage), includes a `[P]` marker on parallelisable tasks, carries a `[US1]` / `[US2]` / `[US3]` story label on every Phase 3/4/5 task (and NO story label on Setup / Foundational / Polish tasks), and names an exact file path. Verified.

---

## Extension Hooks

**Optional Hook**: git
Command: `/speckit.git.commit`
Description: Auto-commit after task generation

Prompt: Commit task changes?
To execute: `/speckit.git.commit`
