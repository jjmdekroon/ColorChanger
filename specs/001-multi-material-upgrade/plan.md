# Implementation Plan: Multi-material Upgrade voor Klipper 3D-printer

**Branch**: `001-create-feature-branch` | **Date**: 2026-05-26 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from [specs/001-multi-material-upgrade/spec.md](spec.md)

## Summary

Een Klipper-compatibele Multi-Material Upgrade (MMU) voor een bestaande 3D-printer.
Eén **master**-controller op SEEED XIAO ESP32-C3 communiceert via USB-serial met
Klipper (op de Raspberry Pi) en stuurt een gedeelde aandrijfas aan via een
**TMC2209**-stepperdriver. Eén of meer **slave**-modules — eveneens XIAO ESP32-C3 —
worden mechanisch met die aandrijfas gekoppeld via een eigen klem (servo-aangedreven)
en zijn elektrisch verbonden via een vijf-pins pogo-verbinding (`5V`, `GND`, `SCL`,
`SDA`, `EN`). Elke slave heeft een **microswitch** als filamentsensor net na het klem-
mechanisme. Slaves zijn hot-pluggable in idle: de master enumereert ze via een
daisy-chained EN-handshake en wijst dynamisch I2C-adressen toe op basis van fysieke
positie. Aansturing vanuit G-code gebeurt via standaard tool-change macro's
(`T0`, `T1`, …) plus aanvullende `MMU_LOAD`/`MMU_EJECT`/`MMU_RESET`/`MMU_STATUS` macro's
die intern het regelgebaseerde USB-serial protocol naar de master verzenden en blokkeren
tot een `ok`/`fail<n>`/`algeladen`-antwoord ontvangen is.

Technische aanpak: twee firmware-images (master, slave) gebouwd met PlatformIO op het
Arduino-framework voor ESP32-C3, met strikt gelaagde architectuur
(transport → protocol → domein/FSM → HAL → orchestratie), non-blocking FSM's op
`millis()`-deadlines, en compile-time configuratie. Klipper-zijde levert een set
`[gcode_macro]`-definities. Een host-side simulatie/testharness valideert protocol-
contracten (USB-serial en I2C-frames) en FSM-transities zonder bench-hardware.

## Technical Context

**Language/Version**: C++17 (Arduino-framework op ESP32-C3 via PlatformIO; toolchain
`riscv32-esp-elf` GCC zoals geleverd door `platformio/espressif32`). Klipper-macro's in
standaard Klipper-config-syntax (Python/Jinja2 expressies in `[gcode_macro]`).

**Primary Dependencies**:
- PlatformIO core + platform `espressif32` (pinned major version, in `platformio.ini`)
- Arduino-ESP32 core (gebundeld bij de pinned `espressif32`-platformversie)
- Stepper-driver: `TMCStepper` library (TMC2209 UART/STEP/DIR control), pinned
- Servo: `ESP32Servo` library, pinned
- WS2812 LED: `Adafruit_NeoPixel` (keuze toegelicht in `research.md`), pinned
- I2C: Arduino `Wire` (bus-master) en `Wire` slave-mode op ESP32-C3
- Host-side tests: Unity (PlatformIO `test`-framework) voor logica die op de host kan draaien (FSM, parsers, retry-rekenwerk)
- Klipper-zijde: alleen standaard `[gcode_macro]`/`[respond]`/`[pause_resume]`-functionaliteit (Klipper ≥ 0.11)

**Storage**: Geen persistente opslag op slaves (constitutioneel verboden). Master gebruikt
evenmin NVS/flash-config; alle tunables zijn compile-time constants in `Config.h`. Een
in-memory ringbuffer voor diagnose-logs is vluchtig.

**Testing**:
- Unit/FSM-tests host-side via PlatformIO `pio test -e native` (Unity)
- Contracttests voor USB-serial-protocol (regelgebaseerd tekst) en I2C-frameformaat als host-side fixtures die master/slave-parsers aanroepen
- Integration-tests op bench: `quickstart.md`-scenario's voor User Stories US1–US3
- Klipper-macro's getest met een minimale stub-host die `T<nr>`/`L<n>`/`U<n>`/`R`/`S` regels stuurt en antwoorden valideert

**Target Platform**:
- Firmware: SEEED XIAO ESP32-C3 (zowel master als alle slaves)
- Host (test/dev): Windows/Linux ontwikkel-PC met PlatformIO
- Klipper-host: Raspberry Pi met Klipper ≥ 0.11

**Project Type**: Embedded firmware (twee firmware-images: `master/`, `slave/`) +
gedeelde header + Klipper-configfragmenten. Geen webfrontend, geen database. Aanvullend
host-test-environment.

**Performance Goals**:
- I2C-pollcyclus per slave ≤ 50 ms (`POLL_SLAVE_INTERVAL_MS` uit `TECHNICAL_DESIGN.md`)
- USB-serial: regelgebaseerde request/response, latency van `T<nr>`→`ok` gedomineerd door fysiek laadtraject; geen harde latency-eis
- Materiaalwissel-totaaltijd ≤ 30 s in nominale gevallen (impliciet uit `BUSY_RETRY_MAX × BUSY_RETRY_HINT_MS = 60 × 500 ms`)
- FSM-tick: elke `loop()`-iteratie run-to-completion, geen blocking call > 1 ms in steady state
- Feed-validatie binnen FR-007: standaard 5 s safe-feed time bound

**Constraints**:
- Geen blocking `delay()` op hot path (Principle II)
- Geen dynamische allocatie of `String`-concatenatie in steady state (Principle III)
- Inter-board interface STRIKT 5 pogo-pin contacten: `5V`, `GND`, `SCL`, `SDA`, `EN` — geen uitbreiding (Principle IV / Hardware Constraints)
- I2C-bus 100 kHz Standard Mode, master is enige bus-master
- Slaves hebben GEEN persistent geheugen voor adres of state (Principle III, FR-015)
- Veiligheid: maximaal één slave gekoppeld op enig moment ("max-1-coupled"-invariant); softwarematig afgedwongen, geen stall-detectie
- Klipper-versie ≥ 0.11; geen versiespecifieke gating in firmware
- Compile-time-only configuratie; geen runtime/NVS config

**Scale/Scope**:
- Aantal slaves: geen harde bovengrens in spec; firmware-cap `MAX_SLAVES = 16` in `Config.h` (gemotiveerd door I2C-adresruimte 0x50…0x5F en RAM-budget)
- Codebase: ~2k–4k regels C++ totaal (master + slave + shared) verwacht; ruim binnen ESP32-C3 flash (4 MB)
- Klipper-macro's: ~5 macro's, < 200 regels Klipper-config
- Test scope: alle 19 functionele requirements (FR-001…FR-020) + 5 success criteria (SC-001…SC-005)

## Constitution Check

> **Procedure**: voor elk principe (I–V) wordt expliciet de status en rationale benoemd.
> Een gate faalt alleen als een violation niet in Complexity Tracking wordt verantwoord.

### Principle I — Separation of Concerns (Layered Architecture)

- **Status**: PASS.
- **Rationale**: De geplande source-tree (zie *Project Structure*) scheidt expliciet:
  `Transport` (`SerialTransport`, `I2CBus`, `I2CSlave`) → `Protocol` (`SerialProtocol`,
  `I2CFrame`) → `Domain/FSM` (`MasterStateMachine`, `SlaveStateMachine`, `Slave`,
  `Enumerator`, `RetryPolicy`) → `HAL` (`Stepper`, `Gripper`, `FilamentSensor`,
  `LedIndicator`, `EnablePin`, `EnableChain`) → `Application` (`main.cpp` met korte
  `loop()`). Domeinvocabulaire uit de spec (`coupledSlave`, `tCommand`,
  `readyConfirmation`, `clampServo`, `filamentSensor`) wordt overgenomen. `loop()` op
  beide images blijft ≤ ~30 regels en bestaat uit benoemde helper-calls
  (`serviceTransport(); serviceProtocol(); tickFsm(); driveHardware(); tickLed();`).
  Gedeelde state in `MasterContext` / `SlaveContext` structs, geen scattered globals.

### Principle II — Real-Time Determinism and Concurrency

- **Status**: PASS.
- **Rationale**: Alle timers (servo-settling 300 ms, feed-timeout 5 s, retry-backoffs
  1/2/4 s, bus-retry 10/20/40 ms, 50 ms poll-cadens, 1 s heartbeat) draaien op
  `millis()`-deadlines in state-context structs; geen `delay()` op hot path. Beide
  firmware-images zijn run-to-completion FSM's met benoemde states (zie
  `STATE_TRANSITIONS.md`, `SLAVE_STATE_TRANSITIONS.md`). T-commando's zijn strikt
  geserialiseerd (FR-019); materiaalwissel is sequentieel met sensorverificatie
  (FR-004a). De "max-1-coupled"-invariant wordt in
  `MasterStateMachine::handleCoupleRequest()` gecheckt vóór enige I2C-`GRIP`-uitgifte.
  Autonome load-trigger (FR-005a) wordt strikt gegate op `MasterState == IDLE`: bij
  het verlaten van `IDLE` zet de master alle lege slaves naar `MODE_READY` (passief,
  insertion-disabled) en bij terugkeer naar `IDLE` weer naar `MODE_AWAITING_LOAD`;
  de transitie van een slave naar `READY` door operator-insteek wordt door reguliere
  polling waargenomen en levert geen extra USB-serial respons op naar Klipper.
  Bring-up exceptions: peripheral-reset-pulsen in `setup()` en de bit-bang van WS2812
  binnen vetted library; beide gedocumenteerd.

### Principle III — Strict Resource Management

- **Status**: PASS.
- **Rationale**: Tunables zitten in `master/include/Config.h` en `slave/include/Config.h`
  als `constexpr`. Geen NVS, geen flash-config, geen runtime-config (FR-007 expliciet).
  Slaves persisteren geen adres (FR-015). Geen `String`, geen `new`/`malloc` op hot
  path; buffers (`I2C_FRAME_BUF[4]`, `SERIAL_LINE_BUF[64]`) zijn statisch en
  bounds-gechecked. Integertypes precies-passend (`uint8_t` voor opcodes/slave-ID,
  `uint32_t` voor `millis()`-deadlines). Diagnose-strings via `F()`-macro in flash. Geen
  buffer > 256 B zonder expliciete justificatie. `MAX_SLAVES = 16` cap voorkomt
  onbeperkt RAM-verbruik.

### Principle IV — Modularity and Reusability

- **Status**: PASS.
- **Rationale**: Fysieke modulariteit is de kern-feature; de 5-pin pogo-interface
  (`5V`, `GND`, `SCL`, `SDA`, `EN`) is exact zoals constitutie & user-input voorschrijven
  en wordt NIET uitgebreid. Slave-identiteit is positioneel via EN-chain (FR-014/015).
  Software-modulariteit: helpers met single responsibility en sprekende namen;
  libraries (`TMCStepper`, `ESP32Servo`, `Adafruit_NeoPixel`, `Wire`) worden gepind in
  `platformio.ini` `lib_deps` met expliciete versie of git-tag. Hand-rolled vervangers
  worden niet geïntroduceerd zonder Complexity Tracking-entry.

### Principle V — Fault Tolerance and Reliability

- **Status**: PASS.
- **Rationale**:
  - *Spec-first*: `spec.md` heeft geen open `[NEEDS CLARIFICATION]`-markers meer (vijf
    clarify-sessies afgerond).
  - *Test-first op contracten*: dit plan levert `contracts/usb-serial.md` en
    `contracts/i2c-frames.md`; de bijbehorende host-side contracttests worden in
    Phase 2 (`/speckit.tasks`) vóór de productiecode geschreven.
  - *Runtime-fault-handling*: feed-retries 3× met exponentiële backoff 1/2/4 s
    (FR-011/011a), bus-retries 3× met 10/20/40 ms (FR-017); `FAULT`-state is sticky tot
    operator-interventie / master-reset (FR-016).
  - *Observability*: LEDs zijn primaire operatorfeedback (FR-010); diagnose-regels op
    de bestaande USB-serial worden geëmitteerd zoals FR-020 voorschrijft.
  - *De-risking*: FSM-, parser- en retry-rekenwerk getest op host via PlatformIO
    `native`-env; bench-hardware is niet het eerste platform. Datatype-sizes en
    ESP32-C3 register-semantiek worden geverifieerd tegen de Espressif ESP32-C3 TRM
    en de Arduino-Reference; afwijkingen worden in commentaar gerefereerd.

**Resultaat**: alle vijf gates passeren zonder violations. Complexity Tracking-tabel
blijft daarom leeg.

## Project Structure

### Documentation (this feature)

```text
specs/001-multi-material-upgrade/
├── spec.md                       # Feature specification (existing)
├── plan.md                       # This file
├── research.md                   # Phase 0 output
├── data-model.md                 # Phase 1 output
├── quickstart.md                 # Phase 1 output
├── contracts/                    # Phase 1 output
│   ├── usb-serial.md             # Klipper ↔ Master protocol contract
│   └── i2c-frames.md             # Master ↔ Slave I2C frame contract
├── ARCHITECTURE.md               # Existing
├── TECHNICAL_DESIGN.md           # Existing
├── STATE_TRANSITIONS.md          # Existing
├── SLAVE_STATE_TRANSITIONS.md    # Existing
├── checklists/requirements.md    # Existing
└── tasks.md                      # Phase 2 (created by /speckit.tasks)
```

### Source Code (repository root)

Greenfield embedded firmware. Layout volgt `TECHNICAL_DESIGN.md` § 12 (PlatformIO
multi-env) met aanvullende `test/`-opzet voor host-side contract- en unit-tests, en
expliciete onderverdeling in `transport/` / `protocol/` / `domain/` / `hal/` om
Principle I (layered architecture) mechanisch zichtbaar te maken.

```text
platformio.ini                       # Envs: master, slave, native (host tests)

shared/
└── Protocol.h                       # I2C opcodes, status-frame struct, shared enums, ErrorCodes

master/
├── src/
│   ├── main.cpp                     # setup() + lean loop()
│   ├── transport/
│   │   ├── SerialTransport.cpp/.h   # Raw USB-serial byte I/O
│   │   └── I2CBus.cpp/.h            # I2C bus-master wrapper (Wire)
│   ├── protocol/
│   │   ├── SerialProtocol.cpp/.h    # T<nr>/L<n>/U<n>/R/S parser + responses
│   │   └── I2CFrame.cpp/.h          # Encode/decode 4-byte status frames
│   ├── domain/
│   │   ├── MasterStateMachine.cpp/.h
│   │   ├── Slave.cpp/.h             # Per-slave state object
│   │   ├── SlaveBus.cpp/.h          # Slave-collection + 50 ms poll loop
│   │   ├── Enumerator.cpp/.h        # EN-chain enumeration FSM
│   │   ├── RetryPolicy.cpp/.h       # Feed-retry + bus-retry counters/backoff
│   │   └── MasterContext.h          # Aggregated state struct
│   └── hal/
│       ├── Stepper.cpp/.h           # TMC2209 wrapper (TMCStepper)
│       ├── EnableChain.cpp/.h       # EN GPIO drive
│       └── DiagnosticLog.cpp/.h     # FR-020 text diagnostics on USB-serial
└── include/
    ├── Config.h                     # Master compile-time constants
    └── ErrorCodes.h                 # fail<n> code table

slave/
├── src/
│   ├── main.cpp                     # setup() + lean loop()
│   ├── transport/
│   │   └── I2CSlave.cpp/.h          # Wire slave-mode wrapper
│   ├── protocol/
│   │   └── I2CFrame.cpp/.h          # Mirror of shared/Protocol.h decoding
│   ├── domain/
│   │   ├── SlaveStateMachine.cpp/.h
│   │   ├── EnumerationResponder.cpp/.h
│   │   └── SlaveContext.h
│   └── hal/
│       ├── FilamentSensor.cpp/.h    # Microswitch debounce + read
│       ├── Gripper.cpp/.h           # ESP32Servo wrapper, non-blocking
│       ├── LedIndicator.cpp/.h      # WS2812 state-coded LED
│       └── EnablePin.cpp/.h         # EN_IN read / EN_OUT drive
└── include/
    └── Config.h                     # Slave compile-time constants

klipper/
├── mmu_macros.cfg                   # [gcode_macro] T0/T1/MMU_LOAD/MMU_EJECT/MMU_RESET/MMU_STATUS
└── README.md                        # How to include mmu_macros.cfg from printer.cfg

test/
├── native/                          # PlatformIO native env (host)
│   ├── test_serial_protocol/        # USB-serial parser/responder contract
│   ├── test_i2c_frame/              # I2C frame encode/decode contract
│   ├── test_master_fsm/             # Master FSM transitions
│   ├── test_slave_fsm/              # Slave FSM transitions
│   ├── test_retry_policy/           # FR-011/011a/017 backoff arithmetic
│   └── test_enumeration/            # EN-chain enumeration model
└── integration/                     # Bench-only quickstart scenarios (manual)
    └── README.md
```

**Structure Decision**: Twee PlatformIO-environments (`master`, `slave`) met gedeelde
headers via `shared/`, en een derde `native`-environment voor host-side contract- en
unit-tests (Principle V "de-risking changes"). Klipper-zijde wordt geleverd als losse
`.cfg`-fragmenten in `klipper/` die de operator vanuit `printer.cfg` includet.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

Geen violations; tabel leeg.

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| —         | —          | —                                   |
