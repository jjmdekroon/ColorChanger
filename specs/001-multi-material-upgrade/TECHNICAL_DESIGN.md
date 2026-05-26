# MMU Technisch Ontwerp (Volledige Specificatie)

Multi-material upgrade voor een 3D-printer (Klipper-gebaseerd). Een master coördineert één of meer slaves; elke slave verzorgt de toevoer van één filament. De master communiceert met Klipper via USB-serial en stuurt via de master een aandrijfas. De slaves koppelen aan deze aandrijfas. Elke slaves gebruikt een servo, deze klemt het het filament vast tegen een aangedreven wiel die vast zit op de aandrijfas. Een filament sensor zit na het klem mechanisme. Het wordt een modulair systeem waarmee slaves 'hot-pluggable' moeten zijn zowel electrisch als mechanisch.

---

## 1. Hardware-overzicht

```
┌────────────────────┐  USB-Serial   ┌─────────────────────┐
│  Raspberry Pi      │ ─────────────►│       MASTER        │
│  (Klipper)         │ ◄─────────────│      (ESP32)        │
└────────────────────┘   T<nr>, L<n>,│                     │
                         U<n>, R, S  │  - Stepper-driver   │
                         ok/fail/    │    (gedeelde as)    │
                         busy/       │  - I2C bus master   │
                         algeladen   │  - EN-chain output  │
                                     └──────────┬──────────┘
                                                │ I2C + EN-chain
                       ┌────────────────────────┼────────────────────────┐
                       ▼                        ▼                        ▼
                ┌────────────┐           ┌────────────┐           ┌────────────┐
                │  SLAVE 1   │  EN ─►    │  SLAVE 2   │  EN ─►    │  SLAVE N   │
                │ (ESP32)    │           │ (ESP32)    │           │ (ESP32)    │
                │ - Servo    │           │ - Servo    │           │ - Servo    │
                │ - Sensor B │           │ - Sensor B │           │ - Sensor B │
                │ - WS2812   │           │ - WS2812   │           │ - WS2812   │
                └─────┬──────┘           └─────┬──────┘           └─────┬──────┘
                      │ filament-pad           │                        │
                      └──────────────┬─────────┴────────────────────────┘
                                     ▼
                                 Y-HUB
                                     │
                                     ▼
                               Gedeelde Bowden
                                     │
                                     ▼
                              Extruder + Hotend
```

### Master (ESP32)
- Stepper-driver voor de **gedeelde aandrijfas** (enige bron van bulk-filamentdoorvoer).
- I2C bus master (verbonden met alle slaves).
- USB-serial naar Klipper op de Pi.
- Enable-keten output naar slave 1 (start van de daisy chain).
- Houdt globale state bij; bewaakt de "max 1 slave gekoppeld" invariant.

### Slave (ESP32)
- **Knijp/koppel-servo**: drukt het filament op de gedeelde as, of laat het los.
- **Sensor B**: detecteert filament voorbij het koppelpunt richting de Y-hub.
- **WS2812 RGB-LED** op de PCB voor state-indicatie.
- **EN_IN** (input van vorige slave of master) en **EN_OUT** (output naar volgende slave)
  voor adres-toewijzing tijdens enumeratie.
- Geen mini-aandrijving, geen tweede sensor.

---

## 2. Communicatielagen

| Link              | Tech       | Richting                                  |
|-------------------|------------|-------------------------------------------|
| Klipper ↔ Master  | USB-Serial | Bidirectioneel, commando + antwoord       |
| Master ↔ Slaves   | I2C        | Master polt elke 50 ms; slaves reageren   |
| Master → Slaves   | EN-chain   | Daisy chain voor enumeratie + adres       |

---

## 3. Adres-toewijzing en enumeratie

Slaves hebben **geen** vaste ID. De volgorde in de daisy chain bepaalt alles:
de slave dichtst bij de master = **ID 1**, daarna 2, 3, ... t/m N.

Klipper telt vanaf 0:
- `T0` → slave ID 1
- `T1` → slave ID 2
- `T<k>` → slave ID `k+1`

### Boot-enumeratie

1. Master trekt alle EN-lijnen laag (alle slaves slapen / unaddressed).
2. Master maakt zijn EN-uitgang hoog → slave 1 wordt wakker met *default I2C-adres*.
3. Master stuurt over I2C: "Neem definitief adres aan + jij bent ID 1".
4. Slave 1 zet zijn definitieve adres en maakt zijn EN_OUT hoog.
5. Slave 2 wordt wakker met default-adres, krijgt ID 2 toegewezen, enzovoort.
6. Master ontdekt het einde van de keten als er geen nieuwe slave op het default-adres
   antwoordt binnen een time-out.

### Slave verlies en herstel (hot-plug)

- Wanneer EN_IN van een slave wegvalt **na** adres-toewijzing, **gaat de slave offline**
  en vergeet zijn adres. Bij re-enumeratie krijgt hij een nieuw adres op basis van
  positie in de keten.
- Master pingt elke X seconden alle bekende slaves. Bij verloren ping → markeren als
  offline.
- Master kan periodiek / op verzoek een **re-enumeratie** starten om nieuwe slaves
  achter de laatste positie te ontdekken.

> Consequentie: een slave-ID is **niet stabiel** over de tijd. State wordt elke keer
> opnieuw afgeleid uit sensor-data en commando-historie.

---

## 4. Sensorlogica en slave-state

### De enkele sensor B

**Zie [SLAVE_STATE_TRANSITIONS.md](SLAVE_STATE_TRANSITIONS.md) voor gedetailleerde slave state machine, automaton-logica, en error-codes.**

Sensor B zit **voorbij het koppelpunt**, richting de Y-hub. Hij detecteert dus alleen
of filament dáár voorbij komt — niet of er filament vóór de servo in de slave zit.

### State afleiden uit servo × sensor + commando-historie

| Servo  | Sensor B | Interpretatie                                          |
|--------|----------|--------------------------------------------------------|
| Open   | Leeg     | EMPTY (geen filament voorbij sensor)                   |
| Open   | Gevuld   | Filament wordt ingestoken (proces 1 in gang)           |
| Dicht  | Leeg     | Geknepen, filament zit ofwel vóór sensor B ofwel weg   |
| Dicht  | Gevuld   | Geknepen en filament voorbij sensor (LOADED/IN_PRINTER)|

De ambiguïteit van "Dicht + Leeg" wordt opgelost via **historie**: welke commando-flow
liep precies vóór deze sensor-overgang? Dat onderscheidt:
- `EJECTING` (filament wordt uitgeworpen) → na deactivering: EMPTY
- `UNLOADING_FROM_PRINTER` (proces 4) → na deactivering: READY

### Slave-modes (gedrag bij sensor-events)

| Mode                    | Sensor B activeert                  | Sensor B deactiveert                       |
|-------------------------|-------------------------------------|--------------------------------------------|
| `IDLE_AWAITING_LOAD`    | Servo knijpt → state READY          | — (was leeg)                               |
| `READY`                 | — (al gekoppeld of in printer)      | — (geen wijziging zonder commando)         |
| `IN_PRINTER`            | — (filament al voorbij)             | Servo knijpt autonoom → mode `CATCHING`    |
| `CATCHING` (≈ proces 4) | —                                   | Bevestigt: filament voorbij sensor weg     |
| `EJECTING` (proces 2)   | —                                   | Bevestigt: leeg                            |

De slave bewaakt sensor B autonoom; de master polt periodiek voor sync.

---

## 5. Globale invariant

> **Maximaal één slave is op enig moment gekoppeld op de gedeelde aandrijfas.**

Bewaking: master houdt expliciet `coupled_slave` bij (None of een ID). Elk koppel-
commando wordt afgewezen als er al iets gekoppeld is. (Hardware stepper-stall detectie
is bewust **niet** geïmplementeerd — software-lock is voldoende.)

---

## 6. De vier processen

### Proces 1 — Laden van filament in slave (lokaal)

1. Master commando aan slave: ga naar mode `IDLE_AWAITING_LOAD` (servo open, LED uit).
2. Gebruiker steekt filament met de hand door de slave totdat sensor B activeert.
3. Slave knijpt autonoom de servo zodra sensor B activeert.
4. Slave gaat naar state `READY`, rapporteert via I2C.
5. LED → groen.

### Proces 2 — Ontladen / eject uit slave (lokaal)

1. Master commando aan slave: ga naar mode `EJECTING`.
2. Slave knijpt (als nog niet) → master draait gedeelde as reverse.
3. Sensor B deactiveert → master stopt motor.
4. Slave laat servo los → gebruiker trekt filament er handmatig uit.
5. State: `EMPTY`. LED → uit.

### Proces 3 — Laden in printer (`T<nr>`)

Voorwaarde: er is geen ander filament geladen in de printer. Anders eerst proces 4.

1. Master controleert dat `coupled_slave == None`.
2. Slave knijpt servo (koppelt op gedeelde as).
3. Master draait stepper forward → filament naar Y-hub en gedeelde Bowden, tot net
   in/voorbij de hotend (afstand uit constants).
4. Master geeft slave commando: `release` → servo los.
5. Master beantwoordt Klipper met `ok`.
6. Klipper-macro doet vervolgens zijn eigen prime / extrude met de printer-extruder.
7. State: `IN_PRINTER` voor deze slave. LED → blauw.

### Proces 4 — Ontladen uit printer

Wordt geïnitieerd als Klipper een `T<nr>` met andere kleur stuurt, of via `U<nr>`-flow.

1. Klipper-macro retract via extruder (eventueel met tip-shaping).
2. Slave is in mode `IN_PRINTER` en bewaakt sensor B autonoom.
3. Wanneer filament terugkomt en sensor B deactiveert: slave gaat naar `CATCHING`
   en knijpt zelf.
4. Master draait stepper reverse — trekt filament netjes terug tot net achter sensor.
5. Master release commando → servo los.
6. State: `READY`. LED → groen.

### Reset

Volledig sequentiële procedure om bekende state te krijgen na master-reset of bij
twijfel:

1. Voorbereiding: alle slaves servo los.
2. Voor elke slave 1 → N:
   a. Slave knijpt.
   b. Master draait gedeelde as reverse (probeer filament terug te trekken).
   c. Was sensor B actief en gaat naar inactief? → deze slave was de actieve →
      state `READY`.
   d. Bleef sensor B inactief na time-out? → geen filament in deze slave →
      state `EMPTY`.
   e. Slave laat servo los (behalve bij `READY`, daar blijft de filament-positie
      bekend; servo open is OK omdat filament al voorbij koppelpunt naar buiten zit).
3. Na de loop: globale state is bekend, master gaat IDLE.

> "Geknepen maar sensor blijft leeg" wordt geaccepteerd als `EMPTY`; gebruiker moet
> dan eventueel handmatig schoonmaken.

---

## 7. Master state machine (globaal)

**Zie [STATE_TRANSITIONS.md](STATE_TRANSITIONS.md) voor gedetailleerde state transition diagram, timing, en LED-feedback.**

```
                  ┌─────────┐
                  │ BOOT    │
                  └────┬────┘
                       ▼
                  ┌─────────────────────┐
                  │ ENUMERATE_SLAVES    │
                  └────┬────────────────┘
                       ▼
                  ┌─────────────────────┐
                  │ SYNC_STATE          │
                  │ (vraag alle slaves) │
                  └────┬────────────────┘
                       ▼
                  ┌─────────┐  T<nr>            ┌─────────────────┐
                  │  IDLE   │ ─────────────────►│  VALIDATE       │
                  │         │  L<n>/U<n>/R/S    │  (regels check) │
                  └────▲────┘                   └────┬────────────┘
                       │                             │
                       │ klaar                       ▼
                       │              ┌──────────────────────────┐
                       │              │ DISPATCH:                │
                       │              │  - LOAD_SLAVE  (proces 1)│
                       │              │  - EJECT_SLAVE (proces 2)│
                       │              │  - LOAD_PRINTER(proces 3)│
                       │              │  - UNLOAD_PRNT (proces 4)│
                       │              │  - RESET                 │
                       │              └────┬─────────────────────┘
                       │                   │
                       │                   │ fail
                       │                   ▼
                       │              ┌─────────┐
                       └──────────────┤  FAULT  │ (alleen master-reset uit)
                                      └─────────┘
```

Tijdens elk proces komt nieuw inkomend commando uit Klipper terug als **`busy`**.
Klipper-macro retries.

`FAULT`-state: master accepteert alleen `S` (status); alle andere commando's krijgen
`fail`. Uit FAULT komt men alleen via een fysieke reset van de master.

---

## 8. Protocollen

### 8.1 Klipper ↔ Master (USB-Serial, tekst, regelgebaseerd)

| Inkomend     | Betekenis                                          |
|--------------|----------------------------------------------------|
| `T<nr>`      | Schakel naar kleur `<nr>` (0-indexed, T0=slave ID 1)|
| `L<n>`       | Start proces 1 (load filament in slave n, 0-indexed)|
| `U<n>`       | Start proces 2 (eject filament uit slave n)        |
| `R`          | Volledige reset-procedure                          |
| `S`          | Status opvragen                                    |

| Uitgaand        | Betekenis                                                |
|-----------------|----------------------------------------------------------|
| `ok`            | Commando uitgevoerd                                      |
| `algeladen`     | Gevraagde kleur was al geladen, niets gedaan             |
| `busy`          | Master is bezig met een ander proces, retry later        |
| `fail<n>`       | Fout, met code `<n>` (zie fail-code register hieronder)  |

### 8.2 Master ↔ Slave (I2C, binair)

Commando-bytes van master → slave:

| Byte | Naam                 | Effect                                            |
|------|----------------------|---------------------------------------------------|
| 0x01 | `PING`               | Status uitlezen                                   |
| 0x02 | `SET_ID`             | Bevestig definitief I2C-adres en ID               |
| 0x10 | `GRIP`               | Servo knijpen                                     |
| 0x11 | `RELEASE`            | Servo loslaten                                    |
| 0x20 | `MODE_AWAITING_LOAD` | Slave gaat naar `IDLE_AWAITING_LOAD`              |
| 0x21 | `MODE_EJECTING`      | Slave gaat naar `EJECTING`                        |
| 0x22 | `MODE_IN_PRINTER`    | Slave gaat naar `IN_PRINTER` (bewaakt sensor)     |
| 0x23 | `MODE_READY`         | Slave gaat naar `READY`                           |
| 0x30 | `SET_LED`            | LED-state expliciet zetten (optioneel)            |

Slave-reply op `PING` (read request): `[mode, sensor_b, last_event, error_code]`

### 8.3 Fail-codes

`fail<n>` waar `n` een **lopend nummer** is van een interne tabel. De master logt
de details (welke slave, welk proces, timestamp) intern (Serial.println debug-output
of in-memory ringbuffer). De gebruiker kan via `S` uitgebreidere details opvragen.

---

## 9. Visuele feedback

WS2812-LED per slave:

| State                  | Kleur / patroon          |
|--------------------------|--------------------------|
| EMPTY                  | Uit                      |
| IDLE_AWAITING_LOAD     | Wit langzaam pulserend   |
| LOADING (transient)    | Wit knipperend snel      |
| READY                  | Groen                    |
| IN_PRINTER             | Blauw                    |
| ACTIEF PROCES (3 of 4) | Geel knipperend          |
| FAULT                  | Rood                     |

---

## 10. Klipper-zijde (macro's, hoog niveau)

Macro's worden via `[gcode_macro ...]` gedefinieerd. De macro stuurt onder water de
juiste serial-string naar de master en bewaakt het antwoord met een retry-loop op
`busy`.

| Klipper macro            | Onderliggend serial commando |
|--------------------------|------------------------------|
| `T0`, `T1`, ...          | `T0`, `T1`, ...              |
| `MMU_LOAD INDEX=<n>`     | `L<n>`                       |
| `MMU_EJECT INDEX=<n>`    | `U<n>`                       |
| `MMU_RESET`              | `R`                          |
| `MMU_STATUS`             | `S`                          |

Bij Klipper boot stuurt de host eenmalig `S` zodat Klipper de master-state kent.

---

## 11. Timeouts en parameters (`Config.h`, constants)

Alle relevante getallen leven in één `Config.h` per project (master, slave) zodat
ze makkelijk aan te passen zijn zonder code te wijzigen.

```cpp
// Master config
constexpr uint32_t TIMEOUT_LOAD_TO_HOTEND_MS   = 15000;
constexpr uint32_t TIMEOUT_RETRACT_MS          = 10000;
constexpr uint32_t TIMEOUT_RESET_PER_SLAVE_MS  = 8000;
constexpr uint32_t POLL_SLAVE_INTERVAL_MS      = 50;
constexpr uint32_t BUSY_RETRY_HINT_MS          = 500;
constexpr uint32_t BUSY_RETRY_MAX              = 60;   // 60 × 500ms = 30s
constexpr uint32_t HEARTBEAT_INTERVAL_MS       = 1000;

// Gedeelde as / stepper
constexpr uint32_t STEPPER_LOAD_STEPS          = 5000; // tune-baar
constexpr uint32_t STEPPER_FEED_RATE_HZ        = 2000;

// Slave config
constexpr uint16_t SERVO_OPEN_US               = 1000;
constexpr uint16_t SERVO_GRIP_US               = 2000;
constexpr uint32_t SERVO_TRAVEL_MS             = 300;
```

---

## 12. Code-organisatie (PlatformIO, .h/.cpp scheiding)

```
mmu-project/
├── platformio.ini                  ← env's voor master en slave
├── shared/
│   └── Protocol.h                  ← I2C cmd-bytes, enums, gedeelde structs
├── master/
│   ├── src/
│   │   ├── main.cpp
│   │   ├── SerialProtocol.cpp
│   │   ├── SerialProtocol.h
│   │   ├── SlaveBus.cpp            ← I2C wrapper + poll-loop
│   │   ├── SlaveBus.h
│   │   ├── Slave.cpp               ← één slave als object (state + I/O)
│   │   ├── Slave.h
│   │   ├── Stepper.cpp             ← gedeelde as
│   │   ├── Stepper.h
│   │   ├── Enumerator.cpp          ← enable-chain enumeratie
│   │   ├── Enumerator.h
│   │   └── MasterStateMachine.cpp/.h
│   └── include/
│       ├── Config.h
│       └── ErrorCodes.h
└── slave/
    ├── src/
    │   ├── main.cpp
    │   ├── FilamentSensor.cpp/.h
    │   ├── Gripper.cpp/.h          ← servo wrapper (non-blocking)
    │   ├── LedIndicator.cpp/.h     ← WS2812
    │   ├── I2CHandler.cpp/.h
    │   └── SlaveStateMachine.cpp/.h
    └── include/
        ├── Config.h
        └── ErrorCodes.h
```

---

## 13. Openstaande discussiepunten

Voor later, niet blocking voor implementatie:

- Welke stepper-driver chip op de master (TMC2209, DRV8825, ...)?
- Welke filament-sensor-type voor sensor B (optische barrier vs. mechanische micro-switch)?
- Welk koppel-mechanisme fysiek (rubberen wieltje, V-groove, ...)?
- Hoeveel slaves max ondersteunen we (waar in firmware het maximum staat)?
- Logging-target: alleen Serial debug of óók een ringbuffer in master-RAM die via `S` is uit te lezen?
