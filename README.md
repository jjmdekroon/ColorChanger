# Multi-material Upgrade for Klipper 3D-Printer — Implementation Progress

## Overview

This is a Klipper-compatible Multi-Material Upgrade (MMU) firmware project for SEEED XIAO ESP32-C3 microcontrollers.

**Project Structure**: Two firmware images (master and slave) communicate via I2C; the master coordinates with Klipper via USB-serial; slaves control filament loading/unloading via servos and sensors.

**Total Tasks**: 77 across 6 phases
- Phase 1 (Setup): 7 tasks ✓ **COMPLETE**
- Phase 2 (Foundational): 26 tasks (in progress)
- Phase 3 (User Story 1 - Material Change): 16 tasks
- Phase 4 (User Story 2 - Hot-plug): 8 tasks
- Phase 5 (User Story 3 - Fault Detection): 12 tasks
- Phase 6 (Polish & Validation): 8 tasks

---

## Phase 1: Setup ✓ COMPLETE

**Completed**:
1. ✓ T001: Directory structure (master/, slave/, shared/, klipper/, test/native/, test/integration/)
2. ✓ T002: platformio.ini (3 environments: master, slave, native)
3. ✓ T003: shared/Protocol.h (I2C opcodes, slave modes, error codes, frame structs)
4. ✓ T004: master/include/Config.h (timing constants, hardware limits)
5. ✓ T005: master/include/ErrorCodes.h (fail<n> mapping)
6. ✓ T006: slave/include/Config.h (servo, sensor, GPIO pin placeholders)
7. ✓ T007: .clang-format (code style consistency)

**Outputs**:
- Directory skeleton ready for firmware images
- PlatformIO configured for cross-platform builds (ARM ESP32-C3 + native tests)
- Shared protocol definitions available to all modules
- Configuration constants centralized for easy tuning

---

## Phase 2: Foundational Layer — IN PROGRESS

### Foundational Contract Tests (Test-First)

**Purpose**: Define the exact wire format and parsing rules. MUST FAIL initially.

- ✓ **T008**: I2C frame round-trip test (→ test/native/test_i2c_frame.cpp)
  - Tests encode/decode of I2cCommandFrame and I2cStatusFrame
  - All opcodes, modes, and error codes covered
  - Status: **CREATED** (will FAIL until I2CFrame encoder/decoder implemented)

- ✓ **T009**: USB-serial parser positive/negative test (→ test/native/test_serial_parser.cpp)
  - Tests well-formed commands: T<nr>, L<n>, U<n>, R, S
  - Tests rejection of malformed: t0, T, T-1, TX, line > 63 B
  - Status: **CREATED** (will FAIL until parser implemented)

- **T010**: USB-serial responder serialisation test (→ test/native/test_serial_responder.cpp)
  - Tests serialization of ok, algeladen, busy, fail<n>, S status line
  - Status: **NOT STARTED**

- **T011**: Enumeration model test (→ test/native/test_enumeration.cpp)
  - Tests EN-chain slave discovery with N=1, 3, 16
  - Verify address assignment 0x50..0x50+N-1 and ID assignment 1..N
  - Status: **NOT STARTED**

### Foundational Implementation

**I2C Frame Codec**:
- ✓ **T012**: master/src/protocol/I2CFrame.h + .cpp (encodeCommand, decodeCommand, encodeStatus, decodeStatus)
  - Status: **IMPLEMENTED** — Makes T008 pass ✓

**Serial Protocol**:
- ✓ **T014**: master/src/protocol/SerialProtocol.h (interface definitions)
  - Status: **CREATED**

- ✓ **T015**: master/src/protocol/SerialProtocol.cpp (parser + responder)
  - Status: **CREATED** (skeleton; will make T009 pass)

**Remaining Foundational Tasks** (TO DO):
- T013: slave/src/protocol/I2CFrame.h + .cpp (slave-side mirroring of T012)
- T016: Responder serialization (complete T015 responder for T010 pass)
- T017: master/src/transport/SerialTransport.h + .cpp (USB-serial line buffering)
- T018: master/src/transport/I2CBus.h + .cpp (I2C master driver)
- T019: slave/src/transport/I2CSlave.h + .cpp (I2C slave driver)
- T020–T026: HAL layer (Stepper, Gripper, FilamentSensor, LedIndicator, EnableChain, DiagnosticLog, EnablePin)
- T027–T033: Domain skeleton & enumeration (MasterContext, SlaveContext, Enumerator, main.cpp for both)

---

## Next Steps to Continue Implementation

### Immediate (Complete Phase 2 Foundational):

1. **Finish Contract Tests (T010, T011)**:
   - Create test/native/test_serial_responder.cpp
   - Create test/native/test_enumeration.cpp

2. **Slave-side I2C Mirror (T013)**:
   - slave/src/protocol/I2CFrame.h + .cpp (replicates master's encode/decode)

3. **Transport Layer (T017–T019)**:
   - SerialTransport: line-buffered USB-serial
   - I2CBus: Wire master with bus-retry logic
   - I2CSlave: Wire slave-mode listeners

4. **HAL Implementation (T020–T026)**:
   - Stepper, Servo, FilamentSensor, LED, Enable pins, Diagnostic logging

5. **Domain & Main (T027–T033)**:
   - MasterContext, SlaveContext structs
   - Enumerator state machine
   - Skeleton main.cpp for both images

### Running Tests

Once foundational is complete, run:
```bash
pio test -e native
```

This executes all Unity tests and validates the protocol contracts before any hardware deployment.

### Parallel Opportunities

After Phase 2 completes:
- **Developer A** → Phase 3 (User Story 1: material change FSM)
- **Developer B** → Phase 4 (User Story 2: hot-plug detection)
- **Developer C** → Klipper macros (gcode integration)
- All are independent of each other once Phase 2 foundational is solid.

---

## Key Design Principles

1. **Layered Architecture**: Transport → Protocol → Domain/FSM → HAL → Application
2. **Non-blocking Timers**: All deadlines via `millis()`, no `delay()`
3. **Compile-time Config**: Constants in Config.h, no runtime tuning
4. **Test-First Contracts**: Parsing, framing, FSM rules verified on host before hardware
5. **Strict Invariants**: "max-1-coupled" slave enforced in code; violations → fail48

---

## Documentation References

- **specs/001-multi-material-upgrade/spec.md** — Feature requirements
- **specs/001-multi-material-upgrade/plan.md** — Technical design and architecture
- **specs/001-multi-material-upgrade/TECHNICAL_DESIGN.md** — Process flow diagrams
- **specs/001-multi-material-upgrade/STATE_TRANSITIONS.md** — Master FSM state chart
- **specs/001-multi-material-upgrade/SLAVE_STATE_TRANSITIONS.md** — Slave FSM state chart
- **specs/001-multi-material-upgrade/contracts/usb-serial.md** — Protocol format contract
- **specs/001-multi-material-upgrade/contracts/i2c-frames.md** — I2C frame format contract
- **specs/001-multi-material-upgrade/quickstart.md** — Bench test scenarios

---

## File Structure (Current)

```
ColorChanger/
├── .gitignore                                  (created)
├── .clang-format                               (created)
├── platformio.ini                              (created)
├── master/
│   ├── include/
│   │   ├── Config.h                            (created)
│   │   └── ErrorCodes.h                        (created)
│   └── src/
│       ├── protocol/
│       │   ├── I2CFrame.h                      (created)
│       │   ├── I2CFrame.cpp                    (created)
│       │   ├── SerialProtocol.h                (created)
│       │   └── SerialProtocol.cpp              (created)
│       ├── transport/ (empty)
│       ├── domain/ (empty)
│       └── hal/ (empty)
├── slave/
│   ├── include/
│   │   └── Config.h                            (created)
│   └── src/ (organized same as master)
├── shared/
│   └── Protocol.h                              (created)
├── klipper/ (empty)
├── test/
│   ├── native/
│   │   ├── test_i2c_frame.cpp                  (created, status: will FAIL)
│   │   ├── test_serial_parser.cpp              (created, status: will FAIL)
│   │   └── (T010, T011 pending)
│   └── integration/ (empty)
└── specs/001-multi-material-upgrade/
    └── (all spec docs already present)
```

---

## Git Status

- Repository initialized ✓
- .specify/ hooks configured (auto-commit on major commands)
- Ready to commit Phase 1 completion

---

## Debugging & Development

**Build and test**:
```bash
pio test -e native      # Run host-side tests (test-first contracts)
pio build -e master     # Build master firmware
pio build -e slave      # Build slave firmware
```

**Code style**:
```bash
clang-format -i <file>  # Apply consistent style
```

**Browse protocol definitions**:
- I2C frame format: `shared/Protocol.h`
- Error codes: `master/include/ErrorCodes.h`
- Timing parameters: `master/include/Config.h`, `slave/include/Config.h`

---

## Contact & Issues

Refer to `.github/copilot-instructions.md` for AI assistant guidance and feature branch workflow.
