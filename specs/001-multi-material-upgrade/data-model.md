# Phase 1 Data Model — Multi-material Upgrade

This document maps the Key Entities from `spec.md` to concrete in-firmware data
structures. There is no persistent storage; "data model" here means the
in-memory representation that the master and slave firmware maintain at
runtime, plus the wire-format structures shared between them.

All types target C++17 on ESP32-C3 (RISC-V, little-endian) under the Arduino
framework. Integer types are minimum-fit per Principle III.

---

## 1. Shared types (`shared/Protocol.h`)

### 1.1 `I2cOpcode` (enum class : uint8_t)

| Name                | Value | Direction       | Description                          |
|---------------------|-------|-----------------|--------------------------------------|
| `PING`              | 0x01  | Master → Slave  | Request slave status frame            |
| `SET_ID`            | 0x02  | Master → Slave  | Assign final I2C address + slave ID  |
| `GRIP`              | 0x10  | Master → Slave  | Close clamp servo                    |
| `RELEASE`           | 0x11  | Master → Slave  | Open clamp servo                     |
| `MODE_AWAITING_LOAD`| 0x20  | Master → Slave  | Enter `IDLE_AWAITING_LOAD`           |
| `MODE_EJECTING`     | 0x21  | Master → Slave  | Enter `EJECTING`                     |
| `MODE_IN_PRINTER`   | 0x22  | Master → Slave  | Enter `IN_PRINTER`                   |
| `MODE_READY`        | 0x23  | Master → Slave  | Enter `READY`                        |
| `SET_LED`           | 0x30  | Master → Slave  | Optional explicit LED override       |

Validation rules:
- Any opcode not listed → slave MUST respond with an error-coded status frame
  (`error_code = ERR_BAD_OPCODE`) and remain in its current state.
- `SET_ID` payload: `[final_address, slave_id]`. Only legal while slave is
  `UNADDRESSED`; ignored otherwise.

### 1.2 `SlaveMode` (enum class : uint8_t)

Mirrors `SLAVE_STATE_TRANSITIONS.md` states; the byte is reported back inside
the I2C status frame.

| Name                  | Value |
|-----------------------|-------|
| `UNADDRESSED`         | 0x00  |
| `IDLE_AWAITING_LOAD`  | 0x01  |
| `LOADING`             | 0x02  |
| `READY`               | 0x03  |
| `COUPLED`             | 0x04  |
| `IN_TRANSIT`          | 0x05  |
| `IN_PRINTER`          | 0x06  |
| `CATCHING`            | 0x07  |
| `EJECTING`            | 0x08  |
| `FAULT`               | 0xFF  |

### 1.3 `I2cStatusFrame` (4 bytes, packed)

```cpp
struct I2cStatusFrame {
    uint8_t mode;          // SlaveMode
    uint8_t sensor_b;      // 0 = empty, 1 = filament present
    uint8_t last_event;    // LastEvent enum (e.g. last command opcode seen)
    uint8_t error_code;    // ErrorCode enum; 0 = OK
};
static_assert(sizeof(I2cStatusFrame) == 4);
```

### 1.4 `I2cCommandFrame` (4 bytes, packed)

```cpp
struct I2cCommandFrame {
    uint8_t opcode;        // I2cOpcode
    uint8_t payload0;
    uint8_t payload1;
    uint8_t payload2;
};
static_assert(sizeof(I2cCommandFrame) == 4);
```

### 1.5 `ErrorCode` (enum class : uint8_t)

| Name                  | Value | Source                       |
|-----------------------|-------|------------------------------|
| `OK`                  | 0x00  | Default                      |
| `ERR_BAD_OPCODE`      | 0x01  | Unknown I2C opcode           |
| `ERR_ILLEGAL_STATE`   | 0x02  | Opcode invalid in current FSM state |
| `ERR_FEED_TIMEOUT`    | 0x10  | Sensor B did not toggle within FR-007 window |
| `ERR_SENSOR_STUCK`    | 0x11  | Sensor reports active while clamp open (edge case) |
| `ERR_I2C_BUS`         | 0x20  | Reserved for master-side annotation (not sent by slave) |
| `ERR_INTERNAL`        | 0xFE  | Catch-all                    |

The master maps these to the `fail<n>` line responses via the table in
`master/include/ErrorCodes.h`.

---

## 2. Master domain types

### 2.1 `MasterState` (enum class : uint8_t)

Mirrors `STATE_TRANSITIONS.md`:

```
BOOT, ENUMERATE_SLAVES, SYNC_STATE, IDLE, VALIDATE,
LOAD_SLAVE, EJECT_SLAVE, LOAD_PRINTER, UNLOAD_PRINTER,
RESET, FAULT
```

### 2.2 `Slave` (per-slave record on master)

```cpp
struct Slave {
    uint8_t  id;                 // 1..MAX_SLAVES (positional)
    uint8_t  i2cAddress;         // 0x50..(0x50+MAX_SLAVES-1)
    SlaveMode mode;              // last known mode (from PING reply)
    bool     sensorB;            // last known filament presence
    bool     online;             // false → unreachable / removed
    uint32_t lastPingMs;         // millis() of last successful PING
    uint8_t  busRetryCount;      // current in-flight I2C retry count (FR-017)
    uint8_t  feedRetryCount;     // current feed retry count (FR-011)
    uint32_t feedDeadlineMs;     // millis() deadline for current feed (FR-007)
    ErrorCode lastError;         // last reported slave error_code
};
```

Validation rules:
- `i2cAddress` MUST be unique across the `Slave[]` array.
- `id` MUST equal the array index + 1 (positional invariant, FR-014).
- `feedRetryCount ≤ 3` at all times; transition to operator-choice or `FAULT`
  beyond that (FR-011).
- `busRetryCount ≤ 3` per in-flight command; reset on each new command (FR-017).

### 2.3 `MasterContext` (aggregated state)

```cpp
struct MasterContext {
    MasterState   state;
    Slave         slaves[MAX_SLAVES];      // sized at compile time
    uint8_t       slaveCount;              // populated by Enumerator
    int8_t        coupledSlaveIdx;         // -1 when none coupled (invariant: ≤ 1 coupled)
    uint8_t       currentToolIdx;          // -1 if no T<nr> active
    SerialCommand inFlightCommand;         // current command being serviced
    uint32_t      busyDeadlineMs;          // when to respond `busy` on inbound new T<nr>
    uint32_t      lastBroadcastMs;         // broadcast cadence (FR-013/013a)
    bool          broadcastSuspended;      // true during prints/material changes
};
```

State transitions (master) — see `STATE_TRANSITIONS.md` for the diagram. Key
invariant assertions enforced on every transition:
- *Max-1-coupled*: at most one `slaves[i].mode == COUPLED || IN_TRANSIT`.
- *Tool-change serialisation*: `state` never re-enters `LOAD_PRINTER` while
  already in `LOAD_PRINTER`.
- *Idle-only broadcast*: `lastBroadcastMs` is only updated while
  `state == IDLE && !broadcastSuspended`.

### 2.4 `SerialCommand` (parsed line)

```cpp
struct SerialCommand {
    enum class Kind : uint8_t { NONE, T, L, U, R, S } kind;
    uint8_t arg;          // ignored for R/S
};
```

Validation rules (parser):
- `T<nr>` / `L<n>` / `U<n>`: `0 ≤ arg < slaveCount` else `fail<bad_index>`.
- Lines starting with `# ` are ignored (diagnostic echoes).
- Lines longer than `SERIAL_LINE_MAX = 63` bytes are rejected with `fail<line_overflow>`.

### 2.5 `RetryPolicy` (helper)

```cpp
struct RetryPolicy {
    static constexpr uint32_t BUS_BACKOFF_MS[3]  = {10, 20, 40};
    static constexpr uint32_t FEED_BACKOFF_MS[3] = {1000, 2000, 4000};
    static constexpr uint8_t  MAX_BUS_RETRIES  = 3;
    static constexpr uint8_t  MAX_FEED_RETRIES = 3;
    // Pure functions: nextBackoff(uint8_t attempt) etc.
};
```

### 2.6 `DiagLogEvent` (ringbuffer entry, FR-020)

```cpp
struct DiagLogEvent {
    uint32_t ts_ms;
    uint8_t  channel;      // slave id; 0xFF = master-global
    uint8_t  opcode;       // I2cOpcode or SerialCommand::Kind+0x80
    uint8_t  errorCode;
    uint8_t  retryAttempt;
};
// Ringbuffer: DiagLogEvent[16] (≈80 B, well under the 256 B threshold).
```

---

## 3. Slave domain types

### 3.1 `SlaveStateLocal` (enum class : uint8_t)

Equal to `SlaveMode` enum; the slave updates this field as the authoritative
state and reports it on `PING`. The slave also tracks:

```cpp
struct SlaveContext {
    SlaveMode   mode;
    uint8_t     id;                  // assigned by SET_ID; 0 = unassigned
    uint8_t     i2cAddress;          // assigned by SET_ID
    bool        sensorBStable;       // post-debounce filament presence
    uint32_t    sensorEdgeMs;        // millis() of last debounced edge
    uint32_t    servoTargetReachedMs;// deadline for servo travel (300 ms)
    bool        servoClosedTarget;   // intended servo state
    uint32_t    feedDeadlineMs;      // local view of FR-007 deadline
    ErrorCode   lastError;
    LastEvent   lastEvent;           // last I2C opcode received
    bool        enInActive;          // EN_IN GPIO state
    bool        enOutDriven;         // current EN_OUT drive state
};
```

State transitions (slave) — see `SLAVE_STATE_TRANSITIONS.md`. Key auto-actions:
- `IDLE_AWAITING_LOAD`: sensor edge `0 → 1` triggers servo close, transition to
  `LOADING`, then on settling `LOADING → READY`.
- `IN_PRINTER`: sensor edge `1 → 0` triggers servo close (autonomous catch),
  transition to `CATCHING`.
- `EJECTING`: sensor edge `1 → 0` confirms eject, transition to
  `IDLE_AWAITING_LOAD` and report `OK`.
- Any state: `feedDeadlineMs` reached without expected sensor edge →
  `FAULT` with `ERR_FEED_TIMEOUT`.

### 3.2 `EnumerationState`

```cpp
enum class EnumState : uint8_t {
    LISTENING_ON_DEFAULT,   // I2C address = 0x60
    AWAITING_ACK,           // SET_ID received, awaiting confirm
    OPERATIONAL,            // address assigned
};
```

---

## 4. Klipper-side data (config fragments only)

There is no Klipper-side data model beyond what `mmu_macros.cfg` expresses:

- One `[gcode_macro T<n>]` per supported tool index.
- One `[gcode_macro MMU_LOAD]`, `MMU_EJECT`, `MMU_RESET`, `MMU_STATUS`.
- A configured USB-serial device path (operator-supplied in `printer.cfg`).

No Klipper SAVE_VARIABLES are used; the master holds all live state.

---

## 5. Entity → spec mapping

| Spec entity (from `spec.md`) | Implementation home                              |
|------------------------------|--------------------------------------------------|
| Master Controller            | `master/` image + `MasterContext`                |
| Slave Module                 | `slave/` image + `SlaveContext`                  |
| Filament Channel             | `MasterContext::slaves[i]` (positional)          |
| Drive Coupling State         | `MasterContext::coupledSlaveIdx` + `Slave::mode` |
| Feed Validation Event        | Per-`Slave::feedDeadlineMs` + `DiagLogEvent`     |
| Recovery Action              | `RetryPolicy` outcome → operator prompt over USB-serial diagnostics + Klipper `PAUSE` flow |
| I2C Command Frame            | `I2cCommandFrame`                                |
| I2C Status Frame             | `I2cStatusFrame`                                 |

All entities accounted for; no spec entity lacks an implementation home.
