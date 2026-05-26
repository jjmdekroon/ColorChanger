# Slave State Transition Diagram

## Slave States

| State | Omschrijving |
|-------|-------------|
| **UNADDRESSED** | Startup-staat: geen I2C-adres, wacht op SET_ID van master |
| **IDLE_AWAITING_LOAD** | Wacht op handmatige filament-insteek; servo open, LED wit pulserend |
| **LOADING** (transient) | Filament wordt ingestoken; servo knijpt autonoom bij sensor-activatie |
| **READY** | Filament aanwezig, niet gekoppeld; servo open, LED groen |
| **COUPLED** | Gekoppeld op master-stepper; servo knijpt filament op as, LED geel |
| **IN_TRANSIT** | Filament onderweg naar hotend via master-stepper; LED geel knipperend |
| **IN_PRINTER** | Filament in hotend; servo los, LED blauw, bewaakt sensor voor retract |
| **CATCHING** | Retract in gang: sensor B werd actief→inactief, slave knijpt autonoom |
| **EJECTING** | Filament wordt uitgeworpen; servo knijpt, master draait as reverse |
| **FAULT** | Error-staat; LED rood, accepteert alleen PING |

---

## State Transition Diagram (Globaal)

```
                    ┌────────────────┐
                    │  UNADDRESSED   │
                    │ (power-on)     │
                    └────────┬───────┘
                             │
                             │ SET_ID cmd
                             │ (adres + ID)
                             ▼
                    ┌────────────────────────────┐
                    │ IDLE_AWAITING_LOAD         │
                    │ servo=open, sensor=empty   │
                    │ LED=wit pulserend          │
                    └──────────────┬─────────────┘
                                   │
                    ┌──────────────┬┴────────────────┐
                    │              │                │
        MODE cmd    │   user       │   EJECT cmd    │
        (READY)     │   action     │                │
                    │              │                ▼
                    │              │          ┌─────────────┐
                    │              ▼          │  EJECTING   │
                    │        ┌──────────────┐ │ servo=closed│
                    │        │   LOADING    │ │ LED=geel    │
                    │        │ (transient)  │ └──────┬──────┘
                    │        │              │        │
                    │        │ sensor B     │        │ sensor B
                    │        │ activates    │        │ deactivates
                    │        │ →servo grip  │        │ (timeout OK)
                    │        └──────┬───────┘        │
                    │               │                │
                    └───────┬───────┴────────────────┘
                            │
                            │ transition done
                            ▼
                    ┌──────────────────────┐
                    │      READY           │
                    │ servo=open           │
                    │ sensor=filled        │
                    │ LED=groen            │
                    └──────────┬───────────┘
                               │
                    ┌──────────┴──────────┐
                    │                     │
              GRIP cmd              MODE_IN_PRINTER
              (master)              (master)
                    │                     │
                    ▼                     ▼
              ┌──────────┐         ┌──────────────────┐
              │ COUPLED  │         │ IN_PRINTER       │
              │ servo    │         │ servo=open       │
              │ closed   │         │ LED=blauw        │
              │ LED=geel │         │ BEWAAKT SENSOR   │
              └────┬─────┘         └────────┬─────────┘
                   │                        │
                   │ master draait          │ user/Klipper
                   │ stepper forward        │ retract inleiding
                   │                        │
                   ▼                        ▼
              ┌──────────┐         ┌──────────────────┐
              │IN_TRANSIT│         │ CATCHING         │
              │LED=geel  │         │ sensor B deactiv. │
              │knipperend│         │ servo knijpt auto│
              └────┬─────┘         │ LED=geel         │
                   │               │ knipperend       │
                   │               └────────┬─────────┘
                   │                        │
                   │ stepper stop           │ master draait
                   │ (afstand OK)           │ stepper reverse
                   │                        │
                   └──────────┬─────────────┘
                              │
                              ▼
                    ┌──────────────────────┐
                    │     READY (again)    │
                    │ servo=open           │
                    │ sensor=empty         │
                    │ LED=groen            │
                    └──────────────────────┘


            ┌────────────────────────────────┐
            │    FAULT (any state)           │
            │ LED=rood, alleen PING reagent  │
            │ (reset of master-reset nodig)  │
            └────────────────────────────────┘
```

---

## Gedetailleerde State Beschrijvingen

### UNADDRESSED
**Ingang**: Power-on  
**Servo**: Los  
**Sensor B**: —  
**LED**: Uit  
**Commando's**:
- `SET_ID`: Ontvang adres + ID, ga naar IDLE_AWAITING_LOAD

**Beschrijving**: Slave wacht op het distributie-commando van master via enable-chain.

---

### IDLE_AWAITING_LOAD
**Ingang**: `SET_ID` voltooid OF `MODE_AWAITING_LOAD` commando  
**Servo**: Open  
**Sensor B**: Leeg (normaal)  
**LED**: Wit langzaam pulserend  
**Auto-acties**:
- Monitor sensor B: als activeert → servo knijpt, naar LOADING

**Commando's**:
- `MODE_AWAITING_LOAD`: Zorg servo open, reset state (idempotent)
- `GRIP`: Servo knijpen (user-trigger alternative)
- `PING`: Report mode

**Beschrijving**: Gebruiker steekt filament handmatig in; slave detecteert dit via sensor B.

---

### LOADING (transient)
**Ingang**: Sensor B activeert in IDLE_AWAITING_LOAD  
**Servo**: Knijpt (auto-commando geïnternaliseerd)  
**Sensor B**: Actief (filament voorbij sensor)  
**LED**: Wit snel knipperend  
**Auto-acties**:
- Wacht tot servo fully gesloten (servo_position >= GRIP_US)
- Dan → READY

**Commando's**:
- `RELEASE`: Abort load, servo open, terug naar IDLE_AWAITING_LOAD
- `PING`: Report LOADING

**Beschrijving**: Korte transient-fase tussen user-action en stabiele state.

---

### READY
**Ingang**: LOADING completed OF `MODE_READY` commando  
**Servo**: Open  
**Sensor B**: Gevuld (filament voorbij sensor, maar niet gekoppeld)  
**LED**: Groen vast  
**Auto-acties**:
- Sensor B monitoren: als deactiveert → FAULT (onverwachte filament-verlies)

**Commando's**:
- `GRIP`: Knijp servo, ga naar COUPLED
- `MODE_EJECTING`: Ga naar EJECTING-modus
- `MODE_IN_PRINTER`: Ga naar IN_PRINTER (master-driven load)
- `RELEASE`: Servo los (idempotent; al los)
- `PING`: Report READY

**Beschrijving**: Filament aanwezig in slave, niet actief op stepper. Stabiele wacht-staat.

---

### COUPLED
**Ingang**: `GRIP` commando in READY  
**Servo**: Dicht (knijpt filament op as)  
**Sensor B**: —  
**LED**: Geel vast  
**Auto-acties**:
- Zorg servo blijft dicht

**Commando's**:
- `RELEASE`: Servo los, terug naar READY
- `PING`: Report COUPLED

**Beschrijving**: Servo knijpt filament tegen gedeelde as; master gaat nu stepper draaien.

---

### IN_TRANSIT
**Ingang**: Master start stepper forward terwijl slave COUPLED is  
**Servo**: Dicht (blijft dicht)  
**Sensor B**: —  
**LED**: Geel knipperend  
**Auto-acties**: —

**Commando's**:
- `RELEASE`: Servo los, terug naar READY
- `PING`: Report IN_TRANSIT

**Beschrijving**: Filament wordt fysiek voortgeduwt naar hotend. Slave volgt commando's maar staat onder controle van master-stepper-timing.

---

### IN_PRINTER
**Ingang**: Master release na voltooid IN_TRANSIT, of `MODE_IN_PRINTER` direct  
**Servo**: Los  
**Sensor B**: Bewaken (kan deactiveren als Klipper retract start)  
**LED**: Blauw vast  
**Auto-acties**:
- Monitor sensor B: als deactiveert → CATCHING
- Timeout: als sensor B blijft actief > TIMEOUT_IN_PRINTER_MS → FAULT

**Commando's**:
- `MODE_CATCHING`: Pre-emptive signaal dat retract begint
- `PING`: Report IN_PRINTER

**Beschrijving**: Filament in hotend. Slave bewaakt sensor B op afstand in printer-Bowden voor teken van retract.

---

### CATCHING
**Ingang**: Sensor B deactiveert terwijl IN_PRINTER, automatisch of via `MODE_CATCHING`  
**Servo**: Knijpt (auto-actie)  
**Sensor B**: Leeg (filament trok terug voorbij sensor)  
**LED**: Geel knipperend  
**Auto-acties**:
- Zorg servo knijpt
- Wacht master-signaal om naar READY terug te gaan

**Commando's**:
- `RELEASE`: Servo los, naar READY
- `MODE_READY`: Servo los, naar READY (sync-commando)
- `PING`: Report CATCHING

**Beschrijving**: Filament-retract gedetecteerd; slave bereidt zich voor op reverse-transport.

---

### EJECTING
**Ingang**: `MODE_EJECTING` commando in READY  
**Servo**: Knijpt (auto)  
**Sensor B**: Bewaken (zal naar leeg gaan als alles goed gaat)  
**LED**: Geel knipperend  
**Auto-acties**:
- Zorg servo knijpt
- Monitor sensor B: als deactiveert → log "eject OK", await RELEASE

**Commando's**:
- `RELEASE`: Servo los, naar IDLE_AWAITING_LOAD (eject voltooid)
- `PING`: Report EJECTING
- Timeout: als sensor B niet deactiveert > TIMEOUT_EJECT_MS → FAULT

**Beschrijving**: Gebruiker trekt filament er handmatig uit terwijl servo knijpt (weerstand) en master stepper reverse draait.

---

### FAULT
**Ingang**: Uit enige state bij error-conditie (timeout, hardware-fout, etc.)  
**Servo**: Onbepaald (veilig los)  
**Sensor B**: —  
**LED**: Rood vast  
**Auto-acties**: —

**Commando's**:
- `PING`: Geef error-code terug, stay in FAULT
- Alle andere commando's: Ignored of rejected

**Beschrijving**: Error-staat. Alleen `PING` antwoordt; master en/of operator moeten handelen (slave reset, check hardware).

---

## State-Event Matrix (I2C Commands)

| Commando | UNADD | AWAITING | LOADING | READY | COUPLED | TRANSIT | IN_PRINTER | CATCHING | EJECTING | FAULT |
|----------|-------|----------|---------|-------|---------|---------|------------|----------|----------|-------|
| `PING` | Nee | Ja | Ja | Ja | Ja | Ja | Ja | Ja | Ja | Ja (err) |
| `SET_ID` | Ja | Nee | Nee | Nee | Nee | Nee | Nee | Nee | Nee | Nee |
| `GRIP` | Nee | Ja (→LOAD) | Nee | Ja (→COUP) | Nee | Nee | Nee | Nee | Nee | Nee |
| `RELEASE` | Nee | Nee | Ja | Ja | Ja (→READY) | Ja | Nee | Ja (→READY) | Ja (→AWAIT) | Nee |
| `MODE_AWAITING` | Nee | Ja | Ja | Nee | Nee | Nee | Nee | Nee | Nee | Nee |
| `MODE_EJECTING` | Nee | Nee | Nee | Ja | Nee | Nee | Nee | Nee | Nee | Nee |
| `MODE_IN_PRINTER` | Nee | Nee | Nee | Ja | Nee | Nee | Nee | Nee | Nee | Nee |
| `MODE_READY` | Nee | Nee | Nee | Nee | Nee | Nee | Nee | Ja (→READY) | Nee | Nee |
| `MODE_CATCHING` | Nee | Nee | Nee | Nee | Nee | Nee | Ja | Nee | Nee | Nee |
| `SET_LED` | Optioneel | Optioneel | Optioneel | Optioneel | Optioneel | Optioneel | Optioneel | Optioneel | Optioneel | Optioneel |

(Ja = geldig; Nee = genegeerd/fout; Pijl geeft resulting state aan)

---

## Sensor B Automaton (Asynchrone Bewaking)

De slave bewaakt sensor B **onafhankelijk** van master-polling; aanpassingen gebeuren lokaal.

```
Current State: READY
├─ sensor B actief + dicht? → stay READY (filament zit, servo los)
├─ sensor B inactief + dicht? → FAULT (filament verloren terwijl geknepen?)
└─ sensor B inactief + los? → stay READY (normaal)

Current State: IN_PRINTER
├─ sensor B actief (normaal)? → stay IN_PRINTER
├─ sensor B inactief (retract)? → CATCHING + servo knijpt auto
└─ timeout no state-change? → FAULT

Current State: CATCHING
├─ sensor B inactief (normaal)? → stay CATCHING (wacht RELEASE)
├─ sensor B activeert (fout)? → FAULT (filament teruggetrokken?)
└─ timeout (> TIMEOUT_CATCHING_MS)? → FAULT

Current State: EJECTING
├─ sensor B inactief (filament weg, OK)? → stay EJECTING (wacht RELEASE)
├─ sensor B actief (fout)? → FAULT (filament zat niet los?)
└─ timeout (> TIMEOUT_EJECT_MS)? → FAULT
```

---

## Timing & Timeouts (in `slave/include/Config.h`)

```cpp
// Servo timing
constexpr uint16_t SERVO_OPEN_US               = 1000;  // µs
constexpr uint16_t SERVO_GRIP_US               = 2000;
constexpr uint32_t SERVO_TRAVEL_TIME_MS        = 300;

// State timeouts
constexpr uint32_t TIMEOUT_SENSOR_RESPONSE_MS  = 1000;  // sensor moet reageren
constexpr uint32_t TIMEOUT_CATCHING_MS         = 5000;
constexpr uint32_t TIMEOUT_EJECTING_MS         = 8000;
constexpr uint32_t TIMEOUT_IN_PRINTER_MS       = 120000;  // 2 min = long print
constexpr uint32_t TIMEOUT_I2C_RESPONSE_MS     = 500;

// Mode-transition delays
constexpr uint32_t DELAY_SERVO_SETTLE_MS       = 50;
```

---

## Error-Codes (slave → master via PING-reply)

```cpp
enum SlaveErrorCode : uint8_t {
    ERROR_NONE                   = 0x00,
    ERROR_SENSOR_STUCK_ACTIVE    = 0x01,
    ERROR_SENSOR_STUCK_INACTIVE  = 0x02,
    ERROR_SERVO_STUCK_OPEN       = 0x03,
    ERROR_SERVO_STUCK_CLOSED     = 0x04,
    ERROR_TIMEOUT_EJECT          = 0x05,
    ERROR_TIMEOUT_CATCHING       = 0x06,
    ERROR_UNEXPECTED_STATE       = 0x07,
    ERROR_I2C_CORRUPTION         = 0x08,
};
```

---

## Code Strukture (slave/src/)

```
slave/
├── src/
│   ├── main.cpp
│   ├── SlaveStateMachine.cpp/.h
│   │   ├── State current_state
│   │   ├── void handle_command(uint8_t cmd)
│   │   ├── void update_sensor()  [asynchronous ISR-safe]
│   │   ├── void poll_transitions()  [every loop() cycle]
│   │   └── void transition_to(State new_state)
│   │
│   ├── FilamentSensor.cpp/.h
│   │   ├── bool is_filament_detected()  [ISR-attached]
│   │   └── void on_sensor_change(bool active)
│   │
│   ├── Gripper.cpp/.h (servo wrapper)
│   │   ├── void grip()
│   │   ├── void release()
│   │   ├── bool is_gripped()  [non-blocking servo feedback]
│   │   └── void update()  [call every loop iteration]
│   │
│   ├── LedIndicator.cpp/.h
│   │   ├── void set_color(State state)
│   │   └── void update_animation()
│   │
│   └── I2CHandler.cpp/.h
│       ├── void on_i2c_command(uint8_t cmd)
│       ├── uint8_t[] prepare_status_reply()
│       └── void init_i2c_slave(uint8_t address)
│
└── include/
    ├── Config.h
    └── ErrorCodes.h
```

---

## Sequence Diagrams (Samples)

### Load-in-printer (Proces 3)

```
Master          Slave
  │               │
  ├─GRIP cmd──────►
  │               │ servo knijpt (COUPLED)
  │               │
  ├─stepper fwd───►  (slave not aware; just gripped)
  │               │
  │ (filament flows to hotend)
  │               │
  ├─RELEASE cmd───►
  │               │ servo los → IN_PRINTER state
  │               │ start monitoring sensor B
```

### Retract (Proces 4)

```
Master              Slave (IN_PRINTER)
  │                   │
  │ (Klipper retract)  │
  │                   │
  │                   │ sensor B: aktief → inaktief
  │                   │ → auto-transition to CATCHING
  │                   │ → servo knijpt auto
  │
  ├─ stepper reverse──►
  │                   │
  │ (filament pulls back)
  │                   │
  │ (timeout or signal)
  │                   │
  ├─RELEASE cmd───────►
  │                   │ servo los → READY
```

