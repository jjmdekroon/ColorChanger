<!--
Sync Impact Report
==================
Version change: 2.0.0 → 2.1.0
Rationale: MINOR — added a new mandatory build-validation gate in Principle V
and Development Workflow so implementation completion requires successful
compilation evidence for affected targets.

Historical context (2.0.0 restructure): MAJOR — the principle set is restructured. The previous ten
principles (I–X) are collapsed into the five named principles requested by
the maintainer. No operational rule is dropped: every concrete obligation
from v1.5.0 is folded into one of the new principles. Because prior plans
reference principles by their previous numbers (e.g. "Principle III safety
invariants"), this is a backward-incompatible restructure and a MAJOR bump.

Modified principles:
  - Old I  (Spec-First, Clarify-Before-Build)             → New V  (Fault Tolerance & Reliability — process gates)
  - Old II (Simplicity & Compile-Time Configuration)      → New III (Strict Resource Management)
  - Old III (Determinism & Safety Invariants)             → New II  (Real-Time Determinism & Concurrency)
  - Old IV (Test-First on Protocol Contracts)             → New V  (Fault Tolerance & Reliability)
  - Old V  (Observability via Text Streams; LEDs)         → New V  (Fault Tolerance & Reliability — observability)
  - Old VI (Non-Blocking Execution via FSM)               → New II  (Real-Time Determinism & Concurrency)
  - Old VII (Lean, Modular `loop()`)                      → New I   (Separation of Concerns) + New IV (Modularity & Reusability)
  - Old VIII (Memory Discipline)                          → New III (Strict Resource Management)
  - Old IX (Naming & Code Documentation)                  → New I   (Separation of Concerns — domain vocabulary) + New IV (Modularity)
  - Old X  (Tooling, Reuse & Simulation)                  → New IV  (Modularity & Reusability) + New V (Reliability — simulation/host tests)

Added sections:
  - Core Principles restructured to exactly five named principles:
      I.   Separation of Concerns (Layered Architecture)
      II.  Real-Time Determinism and Concurrency
      III. Strict Resource Management
      IV.  Modularity and Reusability
      V.   Fault Tolerance and Reliability
Removed sections: none at the top-level structure (Hardware & Platform
Constraints, Development Workflow & Quality Gates, and Governance are retained
with workflow-gate references updated I–X → I–V).

Templates requiring updates:
  - .specify/templates/plan-template.md          ⚠ pending (Constitution Check
    section is still generic; can now be tightened to enumerate Principles
    I–V as explicit gates. Not blocking.)
  - .specify/templates/spec-template.md          ✅ no change required
  - .specify/templates/tasks-template.md         ✅ no change required
  - .specify/templates/checklist-template.md     ✅ no change required
  - .github/prompts/*.prompt.md                  ✅ no change required

Runtime guidance docs reviewed:
  - specs/001-multi-material-upgrade/spec.md          ✅ consistent
  - specs/001-multi-material-upgrade/ARCHITECTURE.md  ✅ consistent
  - specs/001-multi-material-upgrade/TECHNICAL_DESIGN.md ✅ consistent
  - specs/001-multi-material-upgrade/STATE_TRANSITIONS.md ✅ consistent
  - specs/001-multi-material-upgrade/SLAVE_STATE_TRANSITIONS.md ✅ consistent

Previous sync impacts (kept for history):
  - 1.5.0 — Added Principle X (Tooling, Reuse & Simulation Discipline).
  - 1.4.0 — Added Principle IX (Naming & Code Documentation).
  - 1.3.0 — Added Principle VIII (Memory Discipline).
  - 1.2.0 — Added Principle VII (Lean, Modular `loop()`).
  - 1.1.0 — Added Principle VI (Non-Blocking Execution via FSM).
  - 1.0.0 — Initial ratification, five principles (then-I–V), Hardware &
    Platform Constraints, Development Workflow & Quality Gates, Governance.

Follow-up TODOs:
  - TODO(plan-template-gates): Tighten the "Constitution Check" section in
    .specify/templates/plan-template.md to enumerate explicit pass/fail
    gates for each of the five new principles. Deferred to next plan iteration.
  - TODO(plan-back-references): Existing in-repo plans that cite principles
    by their old numbers (if any get written before this date) MUST be
    re-mapped to the new numbering on next amendment of those plans.
-->

# ColorChanger Constitution

## Core Principles

### I. Separation of Concerns (Layered Architecture)

The firmware on both master and slave roles MUST be organised in clearly
distinct layers, each with a single responsibility and a one-way dependency
direction:

1. **Transport layer** — raw byte handling on USB-serial, I2C, and the EN-chain.
   No domain logic; just framing, parity, ACK/NACK detection.
2. **Protocol layer** — interprets transport bytes as the documented
   USB-serial commands (`T<nr>`, `L<n>`, `U<n>`, `R`, `S`) and the I2C frame
   opcodes (`PING`, `SET_ID`, `GRIP`, `RELEASE`, mode commands).
3. **Domain/state layer** — the finite-state machines specified in
   `STATE_TRANSITIONS.md` and `SLAVE_STATE_TRANSITIONS.md`, including the
   safety invariants and the four canonical processes (load slave, eject
   slave, load printer, unload printer).
4. **Hardware Abstraction Layer (HAL)** — driver-level access to the
   stepper/TMC2209, servo, microswitch, WS2812 LED, and EN-pin signalling.
5. **Application/orchestration** — `setup()` and `loop()` only; `loop()` MUST
   be a short, readable list of descriptive helper calls (e.g.
   `serviceTransport(); serviceProtocol(); tickFsm(); driveHardware();
   tickLed();`) and MUST NOT exceed roughly 30 lines or contain more than
   one level of conditional nesting.

Cross-layer rules:

- Higher layers MAY call lower layers; lower layers MUST NOT call upward.
- Identifiers MUST use the domain vocabulary of the spec
  (`slave`, `clamp`, `filamentSensor`, `readyConfirmation`, `tCommand`,
  `coupledSlave`). Synonyms invented inside firmware are forbidden when the
  spec already defines the term.
- Shared state between helpers MUST live in explicitly named context
  structs (`MasterContext`, `SlaveContext`), never as scattered globals.

Rationale: The system spans two distinct firmware images that communicate
over a tightly specified wire protocol. A strict layering makes it
mechanically obvious whether a change touches the wire format, the state
machine, or just a driver, which keeps Principle V's contract tests focused
and prevents the layers from accumulating accidental coupling.

### II. Real-Time Determinism and Concurrency

The system MUST behave deterministically with respect to time and
concurrency. Concretely:

- **No blocking waits.** `delay()`, `delayMicroseconds()` for non-trivial
  durations, busy-wait loops on I/O, and any other call that halts the main
  loop while a timer elapses MUST NOT be used in production code paths. All
  time-based behaviour (debouncing, servo settling, feed timeouts, retry
  backoffs, broadcast cadence) MUST be implemented using `millis()` (or
  `micros()` where finer granularity is justified) compared against a
  per-state deadline stored in state context.
- **FSM execution.** Firmware on both roles MUST be structured as a
  continuously looping finite-state machine. Each `loop()` iteration is
  run-to-completion: it evaluates the current state, performs at most one
  bounded action, and returns. States MUST be named explicitly and
  transitions MUST be the only way state changes.
- **Strict serialisation of tool-changes.** `T<nr>` commands from Klipper
  are processed strictly one at a time; each MUST be terminated with either
  a ready-confirmation or an error before the next is accepted. The master
  MUST NOT implement queueing or cancellation.
- **Sequential material-change.** Material-change sequences are sequential
  with sensor verification (deactivate → verify empty → activate → verify
  present). Parallel activation/deactivation is forbidden.
- **Safety invariant.** At most ONE slave is mechanically coupled to the
  shared drive shaft at any instant. Any command path that could violate
  this MUST be rejected at the master before any I2C command is issued.
- **State derivation, not speculation.** Slave state is derived from the
  local microswitch combined with master command history. On any sensor
  anomaly within the safe-feed time bound, the slave MUST transition to
  `FAULT`.

Limited bring-up exceptions (each MUST be justified in code review):
one-shot peripheral reset pulses in `setup()` before the main FSM starts;
cycle-accurate signal generation already implemented by a vetted library
(WS2812 bit-bang, hardware I2C clock stretching).

Rationale: The master concurrently services USB-serial, an I2C bus polled
at 50 ms cadence, a stepper driver, and the EN-chain handshake; slaves
concurrently service I2C, a servo, a microswitch, and a WS2812 LED. A
single `delay()` of tens of milliseconds risks missed interrupts and late
sensor edges, which directly break the safety invariant. The hardware
design intentionally omits stall detection, so the software lock is the
sole safety mechanism — it is non-negotiable.

### III. Strict Resource Management

The system MUST treat RAM, flash, configuration surface, and persisted
state as scarce resources, even on parts with comfortable budgets.

- **Compile-time configuration only.** Tunables (timeouts, retry counts,
  backoff intervals, kinematic constants) MUST be exposed as compile-time
  constants (`#define` / `constexpr`). Runtime configuration channels,
  NVS/flash-stored config, and per-command parameter overrides are
  forbidden unless a spec-level clarification explicitly mandates one.
- **No persistent slave state.** Slaves MUST NOT persist any state
  (including I2C address) across power loss or disconnect; this is a hard
  requirement of the EN-chain re-enumeration model.
- **Minimise globals.** Variables MUST be declared in the narrowest scope
  satisfying their lifetime; promote to file-scope `static` only when state
  must survive across function calls; group true globals into named
  context structs (Principle I).
- **Right-size data types.** Pick the smallest integer type that provably
  fits the value domain (`uint8_t`, `uint16_t`, `uint32_t` for `millis()`
  deadlines). Avoid bare `int`/`unsigned`. Avoid `float`/`double` unless
  the algorithm genuinely requires fractional arithmetic.
- **Flash-resident strings.** Constant literal strings used in logging or
  diagnostics MUST live in flash, not RAM (`F("…")` on Arduino-family
  toolchains; equivalent `PROGMEM`/`const char[]` placement on bare
  ESP-IDF).
- **No dynamic allocation in steady state.** `malloc`/`new`/`String`
  concatenation MUST NOT occur on the main loop's hot path or inside ISRs.
  Buffers MUST be statically sized at compile time with bounds documented
  next to the declaration.
- **No surprise large buffers.** Any single buffer larger than 256 bytes
  MUST be declared at file or module scope with a comment justifying the
  size; ad-hoc large local arrays inside functions are forbidden.
- **YAGNI.** Features, abstractions, and helpers MUST NOT be added "for
  future use".

Rationale: Wire-format buffers (I2C frames, USB-serial lines, WS2812
frame) are small and fixed; RAM bloat elsewhere erodes the safety margin.
A stateless slave is a hard prerequisite for dynamic re-enumeration after
hot-plug. Avoiding runtime configuration removes a whole class of bugs and
keeps the bill of operator-visible surface area at exactly what the spec
defines.

### IV. Modularity and Reusability

The system is modular both physically and in software, and MUST reuse
proven components where they exist.

Physical modularity:

- Slaves are hot-pluggable mechanically and electrically. The inter-board
  electrical interface is exactly five (5) pogo-pin contacts —
  `5V`, `GND`, `SCL`, `SDA`, `EN` — and MUST NOT be extended.
- A slave's identity is positional, assigned at enumeration via the
  EN-chain handshake; identity MUST NOT be persisted in the slave.

Software modularity:

- Each helper function inside the layers of Principle I MUST have a single
  responsibility, a name that states what it does, and SHOULD be testable
  in isolation (host-side where practical).
- Identifiers MUST describe purpose, not type or position
  (`clampServoPin`, not `pin1`; `feedTimeoutMs`, not `t`). Single-letter
  names are permitted only for short-lived loop counters. Casing scheme:
  `lowerCamelCase` for variables/functions, `UpperCamelCase` for
  types/structs/enums, `UPPER_SNAKE_CASE` for compile-time constants.
  Boolean variables and predicate functions MUST read as statements
  (`isFilamentPresent`, `hasReadyConfirmation`).
- Every non-trivial function MUST carry a short header comment stating its
  single responsibility, its preconditions, and any side effects. Magic
  numbers MUST be named constants with units in the name or comment.

Reuse discipline:

- For well-solved building blocks (I2C driver, servo PWM, WS2812 colour
  driver, TMC2209 control), the project MUST prefer an existing
  maintained library over a hand-rolled implementation.
- Libraries MUST be pinned to a specific version (Arduino library-manager
  version, PlatformIO `lib_deps` entry, or git tag/SHA) and MUST NOT float
  on "latest". Upgrades are deliberate changes recorded on a feature
  branch.
- A hand-rolled alternative MUST NOT be introduced unless: (a) no
  maintained library exists, (b) the maintained library demonstrably
  violates another principle (e.g. blocks `loop()` in violation of
  Principle II), or (c) the in-tree implementation is materially smaller
  than the alternative; the reason MUST be recorded in the plan's
  Complexity Tracking table.

Rationale: Physical modularity is the user-visible product feature
(hot-plug additional filaments). Software modularity makes that physical
modularity tractable on the firmware side and supports Principle V's
contract tests. Pinned reuse keeps builds reproducible and avoids
re-implementing what mature libraries already get right.

### V. Fault Tolerance and Reliability

The system MUST be reliable in the face of expected and unexpected faults,
and MUST give operators and developers enough signal to recover and
diagnose.

Process gates (avoid building the wrong thing):

- **Spec-First, Clarify-Before-Build.** Every feature MUST begin life as a
  written specification in `specs/<NNN>-<slug>/spec.md`. Implementation
  MUST NOT start while any `[NEEDS CLARIFICATION]` markers, unresolved
  ambiguities, or open clarification questions remain.
- **Test-First on Protocol Contracts.** The USB-serial protocol
  (Klipper ↔ master) and the I2C frame protocol (master ↔ slave) MUST
  have executable contract tests written and reviewed BEFORE the
  corresponding firmware code is written. Contract tests MUST cover every
  command opcode, every documented response, every documented error code,
  and every state transition specified in the design documents.
  Material-change happy paths and the feed-retry and bus-retry/backoff
  paths MUST be covered by integration tests (host-side simulation is
  acceptable where bench hardware is not).
- **Build-First Completion Gate.** No implementation task is considered
  complete until the affected build targets compile successfully. At
  minimum, run `pio run -e master` for master changes and `pio run -e slave`
  for slave changes; when shared protocol or cross-role behavior changes,
  both firmware targets MUST build and host tests MUST run via
  `pio test -e native`.

Runtime fault handling (specified in the feature spec; the constitution
fixes the *discipline*, the spec fixes the numbers):

- Filament-feed failures use bounded automatic retry with exponential
  backoff before escalating to the operator with a choice of retry-same
  or switch-channel.
- I2C bus-level errors (NACK, bus timeout, corrupt frame) use bounded
  bus-level retries with short backoff before declaring the channel
  `FAULT`. Bus retries are accounted separately from feed retries.
- `FAULT` is a sticky state on the affected channel until operator
  intervention; safety invariants (Principle II) MUST hold throughout.

Observability:

- **Operator feedback** is delivered exclusively via the per-slave WS2812
  LEDs using the state-coded scheme defined in the spec. No display, no
  Klipper status variable, no out-of-band UI is used as the primary
  operator feedback channel.
- **Developer/support diagnostics** are emitted as human-readable lines on
  the existing USB-serial channel (timestamp, channel ID, command, error
  code, retry attempt) in a form that Klipper safely ignores. No separate
  UART, no JTAG-only logging, no compile-time logging-stripped production
  builds.

De-risking changes:

- Logic that does not depend on real hardware peripherals (FSM
  transitions, protocol parsing, retry counters, timeout arithmetic) MUST
  be testable on the developer host. Bench hardware MUST NOT be the first
  place a new code path executes.
- Where a circuit-level simulator covers the target part (Wokwi, etc.),
  simulation SHOULD validate wiring and basic peripheral behaviour before
  flashing physical boards. When the target part is not modelled (e.g.
  Tinkercad Circuits does not currently model ESP32-C3), the simulation
  requirement is met by host-side unit/integration tests of the same
  logic.
- Datatype sizes, peripheral register semantics, and platform-specific
  behaviour MUST be confirmed against authoritative references (the
  Arduino Reference, the Espressif ESP32-C3 Technical Reference Manual,
  or the relevant chip datasheet) and the reference cited in comments
  where the detail is non-obvious. AI-generated or memorised values are
  not authoritative.

Rationale: Reliability for a multi-material upgrade is the difference
between a successful 12-hour print and a 12-hour-wasted reel of filament.
The combination of spec-first discipline, contract tests, bounded
retries, state-coded LEDs for operators, and text logs for developers
gives a layered defence: fewer wrong things get built, fewer bugs reach
the bench, and the bugs that do reach the field can be diagnosed
remotely from a text log and a colour photograph of the LEDs.

## Hardware & Platform Constraints

- **MCU**: SEEED XIAO ESP32-C3 on every board (master and all slaves).
  Selecting a different MCU for any role is a constitutional change
  (MAJOR version bump).
- **Inter-board electrical interface**: exactly five (5) pogo-pin contacts
  per slave — `5V`, `GND`, `SCL`, `SDA`, `EN`. No additional signals may be
  added to the inter-board interface without amending this constitution.
- **I2C bus**: 100 kHz Standard Mode. Master is sole bus master. Slave
  addresses are master-assigned at enumeration and never persisted.
- **EN chain**: Daisy-chained Enable handshake is the ONLY mechanism for
  slave enumeration and ordering. Static addressing, DIP-switch IDs, or
  electronically-fused identities are forbidden.
- **Actuators & sensing**: Each slave has exactly one servo (clamp) and one
  binary microswitch (filament-presence). The master drives one stepper
  motor through a TMC2209 driver on the shared drive shaft. Additional
  sensing (encoders, current sensing, hotend sensors) is out of scope.
- **Power**: A single supply is provided to the master; slaves draw 5V/GND
  through the pogo-pin chain. Independent slave power is not supported.
- **Klipper compatibility**: Klipper ≥ 0.11. No version-specific gating
  logic is permitted in firmware.

## Development Workflow & Quality Gates

- **Branching**: Feature work happens on `NNN-<feature-slug>` branches
  created by `/speckit.git.feature`. Direct commits to the default branch
  are forbidden.
- **Spec lifecycle**: `/speckit.specify` → `/speckit.clarify` (until zero
  open questions) → `/speckit.plan` → `/speckit.tasks` → `/speckit.implement`.
  Skipping steps is forbidden.
- **Constitution check**: Every plan MUST include a Constitution Check
  section that explicitly states, for each principle (I–V), whether the
  plan complies. Violations MUST be recorded in the plan's Complexity
  Tracking table with a written justification; unjustified violations
  block the plan.
- **Review**: Every PR MUST verify (a) spec/plan/tasks consistency,
  (b) contract tests exist and pass for any protocol-touching change,
  (c) the safety invariant in Principle II is upheld, and (d) no
  runtime-config or persistent-state shortcuts have been introduced
  (Principle III).
- **Build validation**: Every PR and every `/speckit.implement` completion
  report MUST include the exact build/test commands executed and their
  result (`SUCCESS`/`FAILED`) for all affected targets. Missing build evidence
  blocks merge.
- **Hot-plug rule**: Code paths that mutate slave topology MUST be
  reachable only when the master is in `IDLE` (no active print, no
  in-flight material change). Tests MUST assert that broadcast queries are
  suspended outside `IDLE`.
- **No destructive shortcuts**: `git push --force` to shared branches,
  history rewrites of merged commits, and `--no-verify` commits are
  forbidden.

## Governance

This constitution supersedes ad-hoc conventions, informal agreements, and
prior practice. Where this document conflicts with code, comments, or other
docs, this document wins and the conflicting artifact MUST be updated.

Amendment procedure:

1. Open a feature branch and edit `.specify/memory/constitution.md`.
2. Update the version line per the policy below and write a Sync Impact
   Report at the top of the file as an HTML comment.
3. Propagate changes through dependent templates and runtime guidance docs
   (`.specify/templates/*.md`, agent prompt files, README, in-repo design
   docs).
4. Land via PR with at least one reviewer's approval. Self-merge is
   permitted only for solo-maintainer mode and only when the version bump
   is PATCH.

Versioning policy (semantic):

- **MAJOR**: Backward-incompatible governance changes, principle removals,
  principle redefinitions that invalidate prior plans, or hardware-platform
  changes (e.g. swapping MCU family).
- **MINOR**: Addition of a new principle or section; material expansion of
  guidance; new mandatory gate.
- **PATCH**: Clarifications, wording fixes, typo fixes, non-semantic
  refinements.

Compliance review: every `/speckit.plan` invocation re-evaluates the
Constitution Check both before Phase 0 research and after Phase 1 design.
Any new violation discovered during implementation MUST be either resolved
or escalated to a constitution amendment before merge.

**Version**: 2.1.0 | **Ratified**: 2026-05-26 | **Last Amended**: 2026-05-26
