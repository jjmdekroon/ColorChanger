# System Architecture: Multi-material Upgrade

> **For complete technical specifications, state machines, protocols, and code organization details, see [TECHNICAL_DESIGN.md](TECHNICAL_DESIGN.md).**

## Architecture Summary (Core Concepts)

### Master (ESP32 XIAO)
- **Stepper-driver** voor de **gedeelde aandrijfas** (enige bron van bulk-filamentdoorvoer)
- I2C bus master (verbonden met alle slaves)
- USB-serial naar Klipper op de Pi
- Enable-keten output naar slave 1 (start van de daisy chain)
- Houdt globale state bij; bewaakt de "max 1 slave gekoppeld" invariant

### Slave (ESP32 XIAO)
- **Knijp/koppel-servo**: drukt het filament op de gedeelde as, of laat het los
- **Sensor B**: detecteert filament voorbij het koppelpunt richting de Y-hub
- **WS2812 RGB-LED** op de PCB voor state-indicatie
- **EN_IN** (input van vorige slave of master) en **EN_OUT** (output naar volgende slave)  
  voor adres-toewijzing tijdens enumeratie
- Geen mini-aandrijving, geen tweede sensor

---

## Overview Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  Raspberry Pi (Klipper)                                          │
│  └─── USB-Serial ───────────────────────────────────────────┐   │
│                                                             │   │
│  Commands: \<cmd\>, \<cn\>,                                 │   │
│  \<ucn\>, R, S                                              │   │
│  ok/fail/busy/algeladen                                     │   │
│                                                             │   │
└─────────────────────────────────────────────────────────────┼───┘
                                                              │
                         ┌────────────────────────────────────┘
                         │
                    ┌─────────────────────────────────┐
                    │    MASTER (ESP32 XIAO)          │
                    │  - Stepper-driver (gdeelde as) │
                    │  - I2C bus master               │
                    │  - EN-chain output              │
                    └──────────┬──────────────────────┘
                               │
                    ┌──────────┴──────────┐
                    │  I2C + EN-chain      │
                    │                      │
        ┌───────────┴─────────┬────────────┴──────────┬──────────┐
        │                     │                       │          │
    ┌─────────────────┐  ┌─────────────────┐  ┌──────────────────┐
    │  SLAVE 1 (ESP32)│  │  SLAVE 2 (ESP32)│  │  SLAVE N (ESP32) │
    ├─────────────────┤  ├─────────────────┤  ├──────────────────┤
    │ - Servo         │  │ - Servo         │  │ - Servo          │
    │ - Sensor B      │  │ - Sensor B      │  │ - Sensor B       │
    │ - WS2812 LED    │  │ - WS2812 LED    │  │ - WS2812 LED     │
    └────────┬────────┘  └────────┬────────┘  └────────┬─────────┘
             │           EN →     │                    │
             └───────────────────┬┴────────────────────┘
                                 │
                          ┌──────▼──────┐
                          │ Filament-pad│
                          │  (gemeensch.)│
                          └──────┬──────┘
                                 │
                          ┌──────▼──────┐
                          │  Y-HUB      │
                          │ (Bowden)    │
                          └──────┬──────┘
                                 │
                          ┌──────▼───────────┐
                          │ Extruder + Hotend│
                          └──────────────────┘
```

## Component Descriptions

### Master Controller (ESP32 XIAO)

**Role**: Coördineert slavekommunicatie, materiaalwissels en kanalstatus.

**Responsibilities**:
- USB-Serial communicatie met Klipper (RPi)
- I2C-bus masterrol voor all slaves
- Stepper-driver aansturing (gedeelde aandrijfas)
- EN-chain output sequencing voor slave-addressering
- Periodieke broadcast-detectie van topologieveranderingen
- Re-enumeratie bij slave add/remove/fail

**Key I/O**:
- USB-serieel ↔ Klipper
- I2C SCL/SDA ↔ Slaves
- EN (Enable) chain ↔ Slaves (sequentiële handshake)
- PWM ↔ Stepper-driver (gedeelde as)

---

### Slave Modules (ESP32 XIAO per slave)

**Role**: Onafhankelijk filamentkanaal per fysieke module.

**Responsibilities**:
- I2C-slave met dynamisch toegewezen adres
- EN-chain relay (pass-through naar volgende slave of bus)
- Servo-control (klemactivatie/deactivatie)
- Sensor-B uitlezing (binaire filamentdetectie)
- Status-reporting naar master
- Geen adres-persistentie (reset bij disconnect)

**Key I/O per Slave**:
- I2C SCL/SDA (bus slave)
- EN in/out (handshake chain)
- PWM servo control
- Digital/analog sensor input
- WS2812 LED output (statusfeedback)

---

### Drive Coupling

**Type**: Gemeenschappelijke aandrijfas aangestuurd door master-stepper.

**Slaves**: Elk slave-kanaal heeft een servo-geactueerde klem die filament tegen het aangedreven wiel drukt.

**State**:
- Only one slave can clamp at a time (active channel)
- Unclamped slaves allow free filament movement on shared shaft

---

### Filament Path

**Route**: Gedeelde Y-hub Bowden-connector na klem, naar extruder+hotend.

**Sensors**: Filament-pad sensor (Sensor B) geplaatst na klem-mechanisme per slave (detecteert actieve toevoer).

---

## Communication Protocols

### USB-Serial (Klipper ↔ Master)

**Interface**: Standard serial port 115200 baud (or configurable per Klipper setup)

**Commands from Klipper**:
- `<cmd>`: Material select command
- `<cn>`: Cancel/next
- `<ucn>`: User cancel
- `R`: Status request
- `S`: Start/resume

**Responses**:
- `ok`: Command accepted
- `fail`: Command rejected
- `busy`: System busy
- `algeladen`: Ready/idle

---

### I2C (Master ↔ Slaves)

**Bus Speed**: 100 kHz Standard Mode

**Addressing**:
- Slaves assigned sequential addresses at startup (0x50, 0x51, ... based on EN-chain order)
- No persistent storage of address in slave
- Re-enumeration on topology change (add/remove/fail)

**Protocol**:
- Master: Periodic broadcast query "who wants address?" (discovery)
- Slave: EN-enabled slave responds, receives address, activates EN for next
- Master: Polls each addressed slave for status (servo, sensor, health)

---

### EN-Chain (Sequencing)

**Purpose**: Deterministic slave discovery and address assignment without fixed topology.

**Mechanism**:
1. Master sends broadcast: "slave with EN=1, respond"
2. Only slave with EN active responds
3. Master assigns address (0x50 + index)
4. Slave stores address in RAM (temporary), activates EN output
5. Next slave receives broadcast, repeats
6. When no response after timeout → enumeration complete

**Topological Change Detection**:
- Master periodically broadcasts discovery query
- Detects missing slaves (no response on last known address)
- Detects new slaves (response on broadcast address)
- Triggers full re-enumeration to restore correct address mapping

---

## Initialization Sequence

1. **Startup** (Master powered, all slaves powered):
   - Master enters "enumerate" state
   - Sends broadcast "who wants address?" on EN-chain
   - Slave 1 (physically first) responds with EN=1
   - Master assigns address 0x50, instructs slave to activate EN output
   - Slave 1 EN output connects to next slave EN input
   - Process repeats until no response after timeout
   - Master: Enumeration complete, all slaves mapped to contiguous addresses

2. **Operational** (Normal print):
   - Master periodically polls all slaves for status
   - Reacts to Klipper commands (material select)
   - Activates requested slave kanaal (servo clamp + stepper engaged)
   - Monitors filament sensor feedback

3. **Topology Change** (Slave add/remove/fail):
   - Master detects missing/new slave on periodic broadcast
   - Halts current print (if active) or logs warning (if idle)
   - Executes full re-enumeration
   - Updates channel map
   - Resumes print or notifies operator

---

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| No address persistence | Ensures slaves always map to physical position; prevents stale address conflicts |
| Sequential EN-chain | Deterministic enumeration without configuration; works with arbitrary number of slaves |
| Periodic broadcast | Enables detection of failures and plug-events without interrupting operation |
| Shared stepper + per-slave servo | Reduces hardware complexity; one motor for material advance, clamping done per-channel |
| 100 kHz I2C | Reliable over printer cable distances; sufficient bandwidth for status polling |
| ESP32 XIAO hardware | Small form factor, built-in I2C, PWM, sufficient GPIO for all slave components |

