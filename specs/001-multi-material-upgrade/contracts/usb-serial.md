# Contract: USB-serial protocol (Klipper ↔ Master)

This contract defines the *wire-level* behaviour the master MUST implement and
that Klipper macros MUST conform to. It is the authoritative source for the
host-side contract tests under `test/native/test_serial_protocol/`.

## Transport

- Physical: USB CDC serial, exposed by the master ESP32-C3 as a virtual COM port.
- Baud rate: 115200 8N1 (master ignores host baud setting; USB CDC is rate-less).
- Framing: ASCII, `\n` line terminator. Lines longer than 63 bytes (excluding
  `\n`) are rejected with `fail<line_overflow>`.
- Echo: master MUST NOT echo received lines.
- Strict request/response: Klipper sends exactly one command line, then waits
  for either a terminal response (`ok` / `algeladen` / `fail<n>`) or a `busy`
  hint that triggers a Klipper-side retry. The master MUST NOT accept a new
  command line while a `T<nr>`/`L<n>`/`U<n>`/`R` is in progress; instead it MUST
  respond `busy` to that new command and continue servicing the in-flight one
  (per FR-019).

## Commands (Klipper → Master)

| Line       | Meaning                                              | Pre-conditions                                      |
|------------|------------------------------------------------------|-----------------------------------------------------|
| `T<nr>\n`  | Tool change: load tool `<nr>` (0-indexed; T0 = slave id 1) | `0 ≤ nr < slaveCount`; slave online; if a tool is loaded in printer, it is unloaded first as part of this command's execution. |
| `L<n>\n`   | Local load: enter `IDLE_AWAITING_LOAD` on slave `n`  | `0 ≤ n < slaveCount`; slave online; slave currently `EMPTY` (`IDLE_AWAITING_LOAD` is idempotent). |
| `U<n>\n`   | Eject filament from slave `n`                        | `0 ≤ n < slaveCount`; slave online; slave NOT `IN_PRINTER`. |
| `R\n`      | Full reset procedure                                 | Always accepted; aborts any in-flight op.           |
| `S\n`      | Status query                                         | Always accepted; non-blocking.                      |

Whitespace: leading/trailing spaces in a line are ignored. Lower-case command
letters are NOT accepted (`t0` is `fail<bad_command>`).

## Responses (Master → Klipper)

| Line             | Meaning                                                                                 |
|------------------|-----------------------------------------------------------------------------------------|
| `ok\n`           | Command completed successfully.                                                         |
| `algeladen\n`    | `T<nr>` requested a tool that is already loaded in the printer; no action performed.    |
| `busy\n`         | Master is currently servicing another command. Klipper SHOULD retry after `BUSY_RETRY_HINT_MS` (default 500 ms), up to `BUSY_RETRY_MAX` (default 60) attempts. |
| `fail<n>\n`      | Command failed with internal error code `<n>` (decimal, 1–255). See `ErrorCodes.h` for the table. Klipper MUST invoke `PAUSE` per FR-003c. |

Multiple-line responses are not allowed; exactly one terminal response per
command. Diagnostic lines (see below) MAY appear before the terminal response
but are NOT terminal.

### Response to `S`

`S` returns a single line of the form:

```
state=<MasterState> coupled=<int|none> slaves=<n> tool=<int|none>\n
```

where:
- `<MasterState>` ∈ {`BOOT`, `ENUMERATE_SLAVES`, `SYNC_STATE`, `IDLE`, `VALIDATE`, `LOAD_SLAVE`, `EJECT_SLAVE`, `LOAD_PRINTER`, `UNLOAD_PRINTER`, `RESET`, `FAULT`}
- `coupled` is the 1-indexed slave id of the currently-coupled slave or `none`.
- `slaves` is the integer count of online slaves.
- `tool` is the currently-loaded tool index (0-indexed) or `none`.

This line replaces the usual `ok` (the `S` response IS terminal).

### Diagnostic lines (FR-020)

The master MAY emit zero or more lines prefixed with `# ` at any time. Format:

```
# <ts_ms> <channel> <opcode> <error> <retry>\n
```

- `<ts_ms>`: decimal `millis()` timestamp at emission.
- `<channel>`: slave id (1-indexed) or `M` for master-global.
- `<opcode>`: textual mnemonic (`T`, `L`, `U`, `R`, `S`, `PING`, `GRIP`, …).
- `<error>`: textual ErrorCode mnemonic (`OK`, `ERR_FEED_TIMEOUT`, …).
- `<retry>`: decimal retry attempt (0 if not a retry).

Klipper-side parsers MUST ignore any line starting with `# `.

## Error code table (subset; full table in `master/include/ErrorCodes.h`)

| `fail<n>` | Mnemonic                | Cause                                               |
|-----------|-------------------------|-----------------------------------------------------|
| `fail1`   | `ERR_BAD_OPCODE`        | Unknown command line.                               |
| `fail2`   | `ERR_BAD_INDEX`         | `<nr>` / `<n>` out of range.                        |
| `fail3`   | `ERR_SLAVE_OFFLINE`     | Targeted slave not currently online.                |
| `fail4`   | `ERR_ILLEGAL_STATE`     | Command not allowed in current FSM state.           |
| `fail16`  | `ERR_FEED_TIMEOUT`      | Feed validation failed after retries.               |
| `fail17`  | `ERR_SENSOR_STUCK`      | Sensor reports active while clamp open.             |
| `fail32`  | `ERR_BUS_TIMEOUT`       | I2C bus retries exhausted on a slave.               |
| `fail48`  | `ERR_INVARIANT`         | Max-1-coupled invariant would be violated.          |
| `fail255` | `ERR_INTERNAL`          | Catch-all.                                          |

The numeric assignment is fixed for the lifetime of this contract; new codes
MUST be appended with a new number, never re-using a retired number.

## Required contract tests (host-side)

The following MUST pass before any production firmware code is written:

1. **Parser positive**: each well-formed command line parses to the expected
   `SerialCommand` struct.
2. **Parser negative**: malformed lines (`t0`, `T`, `T-1`, `TX`, lines > 63 B)
   yield `fail<bad_command>` / `fail<bad_index>` / `fail<line_overflow>` as
   documented.
3. **Responder serialisation**: `MasterContext` → response line is exactly as
   documented for `ok`, `algeladen`, `busy`, `fail<n>`, and the `S`-state line.
4. **Strict request/response**: after a `T<nr>` is accepted, any further command
   line received before the terminal response gets `busy` (FR-019).
5. **Diagnostic line non-interference**: a stream of diagnostic `# `-lines
   interleaved with the terminal response still results in the terminal
   response being the last non-`# ` line, and a Klipper-side line-matcher that
   ignores `# `-prefixed lines reaches the terminal response.
