# Contract: I2C frame protocol (Master ↔ Slave)

Authoritative wire-level contract for the I2C bus between the master and each
slave. Host-side contract tests under `test/native/test_i2c_frame/` MUST
exercise every command and response shape described here.

## Transport

- Bus: I2C, 100 kHz Standard Mode.
- Wires: `SCL`, `SDA`, plus shared `GND` and `5V` and a daisy-chained `EN` —
  exactly the 5 pogo-pin contacts mandated by the constitution. No additional
  signals.
- Master is the sole bus master; slaves never initiate transfers.
- Addressing: 7-bit. Unaddressed slaves listen on the default address `0x60`.
  Final addresses are assigned sequentially `0x50, 0x51, …` up to
  `0x50 + MAX_SLAVES - 1`. Addresses are NEVER persisted in slave memory.

## Frame format

### Master → Slave (command frame, fixed 4 bytes)

```
[byte0: opcode] [byte1: payload0] [byte2: payload1] [byte3: payload2]
```

Unused payload bytes MUST be zero. A slave MUST accept exactly 4 bytes per
write transaction; transactions of any other length are an error and the
slave responds (on the next `PING`) with `ERR_BAD_OPCODE`.

### Slave → Master (status frame, fixed 4 bytes)

Returned on a Wire `requestFrom(addr, 4)` after the master has previously
written a `PING` opcode:

```
[byte0: mode] [byte1: sensor_b] [byte2: last_event] [byte3: error_code]
```

- `mode`: `SlaveMode` enum (see `data-model.md`).
- `sensor_b`: `0` = empty, `1` = filament present (post-debounce).
- `last_event`: opcode of the last command received (or 0 if none since boot).
- `error_code`: `ErrorCode` enum; `0x00 = OK`.

The status frame is updated by the slave at the moment of `PING` reception
and held in a 4-byte response buffer until the master issues the
`requestFrom`. Slaves MUST NOT stretch the clock for more than 1 ms.

## Opcodes

| Opcode | Mnemonic               | Payload                          | Effect                                                                    |
|--------|------------------------|----------------------------------|---------------------------------------------------------------------------|
| 0x01   | `PING`                 | (none, zero bytes)               | Refresh status frame; master will issue a follow-up `requestFrom(4)`.     |
| 0x02   | `SET_ID`               | `[final_addr, slave_id, 0]`      | Assign final 7-bit address and slave id; only honoured in `UNADDRESSED`.  |
| 0x10   | `GRIP`                 | (zero)                           | Close clamp servo. Transitions: `READY → COUPLED`. Idempotent in `COUPLED`.|
| 0x11   | `RELEASE`              | (zero)                           | Open clamp servo. Transitions: `COUPLED → READY`, `IN_TRANSIT → READY` (after stepper stop). |
| 0x20   | `MODE_AWAITING_LOAD`   | (zero)                           | Enter `IDLE_AWAITING_LOAD` (servo open, sensor monitored).                |
| 0x21   | `MODE_EJECTING`        | (zero)                           | Enter `EJECTING` (servo closed, monitor sensor `1→0`).                    |
| 0x22   | `MODE_IN_PRINTER`      | (zero)                           | Enter `IN_PRINTER` (servo open, monitor sensor `1→0` for autonomous catch).|
| 0x23   | `MODE_READY`           | (zero)                           | Enter `READY` (servo open, sensor active expected).                       |
| 0x30   | `SET_LED`              | `[r, g, b]`                      | Override LED colour (optional; state-coded scheme normally drives LED).   |

Any other opcode → `error_code = ERR_BAD_OPCODE`, state unchanged.

## State-machine integration

Each opcode's legality depends on the current `SlaveMode` (see
`SLAVE_STATE_TRANSITIONS.md`). Illegal combinations MUST set
`error_code = ERR_ILLEGAL_STATE` and leave the slave's state unchanged. The
authoritative legality matrix is encoded as a table in `slave/src/domain/
SlaveStateMachine.cpp` and MUST be exercised by `test/native/test_slave_fsm/`.

## Enumeration sequence (EN-chain)

This sequence is the boot-time and re-enumeration handshake.

1. Master pulls all known slave I2C addresses out of its table (forgets them).
2. Master deasserts its EN output (low) — all downstream slaves see `EN_IN = 0`
   and remain `UNADDRESSED` listening on `0x60`.
3. Master asserts EN output (high).
4. The slave whose `EN_IN` is now high transitions to "default address listener"
   and waits for `SET_ID` on `0x60`.
5. Master writes `SET_ID` with payload `[0x50, 1, 0]` to `0x60`.
6. Master issues `PING` to `0x50` then `requestFrom(0x50, 4)` to confirm the
   slave is alive on its new address. On the first successful `PING` reply,
   the master instructs the slave (via a follow-up `SET_ID` semantics check)
   that it should now assert its `EN_OUT` — in this design that side effect
   happens automatically inside the slave upon successful `SET_ID`.
7. The next-downstream slave now sees `EN_IN = 1`, becomes the default-address
   listener, and the master assigns `0x51, slave_id = 2`, …
8. After `MAX_SLAVES` iterations OR a `PING` on `0x60` that yields no reply
   (timeout `ENUM_DEFAULT_PING_TIMEOUT_MS = 50` ms), enumeration is complete.

Failure modes:
- A `SET_ID` write returns NACK → master applies `RetryPolicy::busBackoff` and
  retries up to 3× before declaring enumeration failure and entering `FAULT`.
- A `PING` on a freshly-assigned address times out → same retry logic; on
  exhaustion, that slave is marked `online = false` and enumeration continues
  with the next position (the offline slave's address slot is left empty).

## Polling cadence

Master polls each online slave's status frame every
`POLL_SLAVE_INTERVAL_MS = 50` ms:

1. Write `PING` (4 bytes) to slave address.
2. `requestFrom(addr, 4)` for the status frame.
3. Update `Slave::mode`, `Slave::sensorB`, `Slave::lastError`, `Slave::lastPingMs`.

Polling is suspended during enumeration and during the bursty parts of a
material change (when the master is driving step pulses). The pause is
bounded to one FSM tick and never exceeds 200 ms.

## Required contract tests (host-side)

1. **Frame round-trip**: encode any `I2cCommandFrame` → 4 bytes → decode →
   equal struct (and same for `I2cStatusFrame`).
2. **Opcode legality matrix**: for every `(SlaveMode × I2cOpcode)` pair, the
   slave FSM (host-simulated) produces the documented outcome (transition,
   `ERR_ILLEGAL_STATE`, or `ERR_BAD_OPCODE`).
3. **Enumeration model**: with a simulated EN-chain of N slaves, the
   enumeration FSM assigns addresses `0x50..0x50+N-1`, ids `1..N`, in the
   order matching the chain.
4. **Bus-retry**: when the simulated bus injects 1, 2, or 3 NACKs followed by
   ACK, the master's command path retries with backoffs `10/20/40 ms` (FR-017)
   and succeeds; 4 consecutive NACKs trigger `FAULT` for that channel.
5. **Polling suspension**: under simulated material-change activity, no
   `PING` is emitted; once the activity drops, polling resumes within one
   `POLL_SLAVE_INTERVAL_MS` window.
