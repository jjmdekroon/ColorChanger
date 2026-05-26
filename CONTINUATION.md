# Implementation Continuation Guide

**Status**: Phase 1 (Setup) complete. Phase 2 (Foundational) 42% complete.  
**Remaining Tasks**: ~55 (Phase 2 finish + Phases 3–6)  
**Recommended Next Step**: Complete Phase 2 foundational layer (T010–T026), then run `pio test -e native` to verify contract tests pass.

---

## Immediate Next Tasks (Phase 2 Continuation)

### 1. Complete Contract Tests (T010, T011)

These define the exact expected behavior. Write them to FAIL initially, then implementation satisfies them.

#### T010: USB-Serial Responder Test
**File**: `test/native/test_serial_responder.cpp`

Requirements from `contracts/usb-serial.md` §"Required contract tests" #3:
- Given a synthetic `MasterContext`, serializer emits exactly:
  - `ok\n` (success)
  - `algeladen\n` (tool already loaded)
  - `busy\n` (command in flight)
  - `fail<n>\n` (error with numeric code)
  - `state=... coupled=... slaves=... tool=...\n` (S command response)
- Diagnostic lines (`# ...`) do NOT interfere with response parsing

**Implementation**: Complete `serializeResponse()` in T015 (master/src/protocol/SerialProtocol.cpp) to handle all response types per the contract format.

#### T011: Enumeration Model Test
**File**: `test/native/test_enumeration.cpp`

Requirements from `contracts/i2c-frames.md` §"Required contract tests" #3:
- Simulate EN-chain with N=1, 3, 16 slaves
- Enumerator assigns addresses `0x50..0x50+N-1` and IDs `1..N` in chain order
- Test the FSM transitions per `contracts/i2c-frames.md` §"Enumeration sequence" steps 1–7

**Implementation**: Implement T029 (master/src/domain/Enumerator.h + .cpp) with:
- `run()` method that drives the full enumeration FSM
- `detectTopologyChange()` for hot-plug detection
- Inject-able clock for testing (no dependency on real `millis()`)

---

### 2. Transport Layer (T017–T019)

These provide the electrical interface (USB-serial, I2C bus).

#### T017: Serial Transport
**File**: `master/src/transport/SerialTransport.h` + `.cpp`

Wraps Arduino `Serial` at 115200 8N1:
- `pollLine(char* buf, size_t cap, size_t& outLen)` — non-blocking read, returns true if line ready
- `writeLine(const char* s)` — write line without echo
- Bounded by `SERIAL_LINE_MAX = 63` bytes

#### T018: I2C Bus (Master)
**File**: `master/src/transport/I2CBus.h` + `.cpp`

Wraps Arduino `Wire` as bus-master at 100 kHz:
- `write(addr, const uint8_t buf[4])` — sends 4-byte command
- `request(addr, uint8_t out[4])` — reads 4-byte status response
- Returns result code: `OK`, `NACK`, `TIMEOUT`, `SHORT_READ`
- Enforces ≤1 ms clock-stretch tolerance (FR-017)

**Includes**: T063 bus-retry logic (3× retries with 10/20/40 ms backoff)

#### T019: I2C Slave
**File**: `slave/src/transport/I2CSlave.h` + `.cpp`

Wraps Arduino `Wire` in slave-mode:
- Listens on default address `0x60` during enumeration
- Re-binds to assigned address after `SET_ID`
- Callbacks: `onReceive(const uint8_t buf[4], size_t len)` and `onRequest()` (returns cached 4-byte status frame)

---

### 3. HAL Layer (T020–T026)

Hardware abstraction for peripherals. Can be stubbed/mocked for testing.

#### T020: Stepper Control (Master)
**File**: `master/src/hal/Stepper.h` + `.cpp`

TMC2209 wrapper via `TMCStepper` library:
- One-time config in `setup()`
- `start(direction)`, `stop()`, `isRunning()`
- Non-blocking pulse generation at `STEPPER_FEED_RATE_HZ` (2000 Hz) on `millis()` cadence
- No stall detection (FR-002 out-of-scope)

#### T021: Enable Chain (Master)
**File**: `master/src/hal/EnableChain.h` + `.cpp`

Drives master `EN_OUT` GPIO:
- `deassertAll()` — pull low (disable downstream slaves)
- `assert()` — release high (enable downstream slave)
- Used during enumeration

#### T022: Diagnostic Log (Master)
**File**: `master/src/hal/DiagnosticLog.h` + `.cpp`

Per FR-020: emits `# <ts_ms> <channel> <opcode> <error> <retry>\n` via `SerialTransport::writeLine`
- In-RAM ringbuffer `DiagLogEvent[16]` (per data-model.md §2.6)
- Strings via `F()`/`PROGMEM` for flash storage (FR-011 / research.md R-011)

#### T023–T026: Slave HAL
- **T023**: `FilamentSensor` — debounced microswitch read (5 ms per R-009)
- **T024**: `Gripper` — `ESP32Servo` wrapper with non-blocking travel
- **T025**: `LedIndicator` — `Adafruit_NeoPixel` wrapper, 20 Hz refresh
- **T026**: `EnablePin` — reads `EN_IN`, drives `EN_OUT`

---

### 4. Domain & Main Skeleton (T029–T033)

FSM skeleton and boot logic.

#### T029: Enumerator (Master)
**File**: `master/src/domain/Enumerator.h` + `.cpp`

Per `contracts/i2c-frames.md` §"Enumeration sequence":
- Probe EN_IN initially low
- Send `SET_ID` to default address `0x60`
- Assign addresses `0x50..0x5F` and IDs `1..N`
- Asserts `EN_OUT` to wake next slave

Inject-able clock for testing (run-to-completion ticks).

#### T030: Enumeration Responder (Slave)
**File**: `slave/src/domain/EnumerationResponder.h` + `.cpp`

Slave-side handler per enumeration sequence steps 4–7:
- On EN_IN rising edge, register as default-address listener `0x60`
- On receiving `SET_ID`, re-bind to assigned address
- Assert `EN_OUT` to downstream

#### T031: Slave Bus (Master)
**File**: `master/src/domain/SlaveBus.h` + `.cpp`

Periodic polling per-slave:
- PING loop at `POLL_SLAVE_INTERVAL_MS` (50 ms)
- Updates `Slave::mode`, `sensorB`, `lastError`, `lastPingMs`
- `sendCommand()` with transport-level result & retry wrapping

#### T032, T033: Main Skeleton
**Files**: `master/src/main.cpp`, `slave/src/main.cpp`

Master:
```cpp
void setup() {
    Serial.begin(115200);
    Wire.begin();
    Stepper::init();
    EnableChain::init();
    DiagnosticLog::init();
    // Run enumeration until complete
}

void loop() {
    serviceTransport();  // Read USB-serial
    serviceProtocol();   // Parse commands
    tickFsm();          // FSM tick (IDLE)
    driveHardware();    // Stepper/servo updates
    tickPolling();      // PING slaves
}
```

Slave:
```cpp
void setup() {
    Wire.begin(I2C_DEFAULT_ADDR);
    Servo::init();
    FilamentSensor::init();
    LedIndicator::init(IDLE_AWAITING_LOAD);
    // Boot classification: sensor → READY or EMPTY per FR-010a
}

void loop() {
    serviceI2c();   // Receive commands
    tickSensor();   // Debounce
    tickServo();    // Non-blocking travel
    tickFsm();      // State machine
    tickLed();      // Update LED
}
```

---

## Phase 2 Dependencies & Parallelization

**Dependency Chain** (must run sequentially):
1. T008–T011 contract tests (write first, will fail)
2. T012–T016 protocol codec (makes tests pass)
3. T017–T019 transport layer
4. T020–T026 HAL
5. T029–T033 domain & main

**Parallelizable within each group**:
- All HAL tasks (T020–T026) are independent ✓ → all can run in parallel
- All transport tasks (T017–T019) can start once protocol (T012–T016) is done ✓
- Domain tasks (T029–T031) depend on transport & HAL, but can parallelize with each other

**Estimated Effort**:
- Contract tests: 2–3 hours (write + debug)
- Protocol: 1–2 hours
- Transport: 2–3 hours
- HAL: 3–5 hours (most calls are library wrappers)
- Domain & main: 2–3 hours
- **Total Phase 2**: ~12–16 hours

---

## Testing Strategy

### During Development

After each major section completes, run:

```bash
# Build firmware (compile-check, no hardware needed)
pio build -e master
pio build -e slave

# Run host-side tests
pio test -e native
```

### Incremental Test Execution

Example flow:
1. Write T008 (I2C frame test) → FAILS (no encoder)
2. Implement T012 (encoder) → `pio test -e native` → T008 PASSES ✓
3. Write T009 (parser test) → FAILS
4. Implement T015 (parser) → `pio test -e native` → T009 PASSES ✓
5. Continue iterating

### Phase 2 Checkpoint

Once T008–T011 all PASS + `pio build` succeeds:
- ✓ Protocol format is validated end-to-end
- ✓ USB-serial parsing is robust
- ✓ I2C frame encoding is correct
- ✓ Enumeration FSM is testable
- **Ready to proceed to Phase 3 (User Story 1)**

---

## Reference Documents

Key specs to review:
- `specs/001-multi-material-upgrade/data-model.md` — struct layouts
- `specs/001-multi-material-upgrade/contracts/usb-serial.md` — USB protocol format
- `specs/001-multi-material-upgrade/contracts/i2c-frames.md` — I2C protocol format
- `specs/001-multi-material-upgrade/SLAVE_STATE_TRANSITIONS.md` — slave FSM
- `specs/001-multi-material-upgrade/STATE_TRANSITIONS.md` — master FSM
- `specs/001-multi-material-upgrade/TECHNICAL_DESIGN.md` — process flow diagrams

---

## Code Style & Quality

- Use `#include "../../shared/Protocol.h"` for shared types
- All constants in `Config.h` (no magic numbers)
- Suffix tests: `_valid`, `_rejected`, `_roundtrip`, `_boundary`
- Use Unity asserts: `TEST_ASSERT_EQUAL_INT`, `TEST_ASSERT_EQUAL_HEX8`
- Run clang-format before commit: `clang-format -i <file>`

---

## Debugging Tips

**Test Failure Debugging**:
```bash
pio test -e native -v  # Verbose output
```

**Build Failures**:
```bash
pio build -e native -v  # See compiler errors
```

**Check Include Paths**:
- Master: `master/include/`, `shared/`
- Slave: `slave/include/`, `shared/`
- Tests: `shared/` only (no Arduino headers)

**Mock Arduino in Native Tests**:
For I2C/Serial operations in tests, create stubs or mock Wire/Serial functions.

---

## Commit Frequency

- After each task completes: commit with message `"[T###] <task name>"`
- Example: `git commit -m "[T012] I2CFrame encoder/decoder implementation"`
- Repository has auto-commit hooks configured (optional pre-implement commit)

---

## What to Avoid

❌ Dynamic allocation (`new`, `malloc`) on hot path  
❌ `String` concatenation in steady state  
❌ `delay()` — always use `millis()` deadlines  
❌ Unbounded buffers (always use fixed-size arrays)  
❌ Multi-file edits in one task (split into separate commits)  
❌ Implementating tests after code (test-first always!)

---

## Success Criteria for Phase 2

✓ All 4 contract tests (T008–T011) PASS  
✓ `pio build -e master` succeeds  
✓ `pio build -e slave` succeeds  
✓ No compile warnings (address or suppress)  
✓ All code formatted with clang-format  
✓ README.md updated with Phase 2 completion status  

---

## Next Phase (Phase 3) Overview

Once Phase 2 is solid:
- **T034–T048**: User Story 1 — Material change (tool change FSM end-to-end)
- Estimated: 20–30 hours
- Can run in parallel with Klipper macro development (T046–T048)
- Bench test per `quickstart.md` §"User Story 1" scenario

---

**Last Updated**: 2026-05-26  
**By**: GitHub Copilot (speckit.implement mode)  
**Status**: Ready for Phase 2 continuation
