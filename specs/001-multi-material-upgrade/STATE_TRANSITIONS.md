# Master State Transition Diagram

## Gedetailleerde State Machine

### States

| State | Omschrijving |
|-------|-------------|
| **BOOT** | Opstartfase, hardware-initialisatie |
| **ENUMERATE_SLAVES** | Slaves detecteren via EN-chain en adressen toewijzen |
| **SYNC_STATE** | Vraag status van alle slaves op |
| **IDLE** | Wacht op commands van Klipper |
| **VALIDATE** | Controleert of commando legaal is (invariants, precondities) |
| **LOAD_SLAVE** (Proces 1) | Filament handmatig in slave laden |
| **EJECT_SLAVE** (Proces 2) | Filament uit slave ejecten |
| **LOAD_PRINTER** (Proces 3) | Filament van slave naar hotend voeren |
| **UNLOAD_PRINTER** (Proces 4) | Filament uit hotend terugtrekken |
| **RESET** | Reset procedure: alle slaves resetten naar EMPTY |
| **FAULT** | Foutstatus, accepteert alleen `S` en `R` |

---

## Volledige Transition Diagram

```
                           ┌──────────────┐
                           │    BOOT      │
                           └──────┬───────┘
                                  │ hardware OK
                                  ▼
                    ┌─────────────────────────────┐
                    │   ENUMERATE_SLAVES          │
                    │ (enable-chain sequencing)   │
                    └──────────┬──────────────────┘
                               │ all slaves addressed
                               ▼
                    ┌─────────────────────────────┐
                    │   SYNC_STATE                │
                    │ (poll all for current mode) │
                    └──────────┬──────────────────┘
                               │ sync complete
                               ▼
                    ┌──────────────────────────────────────────────┐
                    │           IDLE                               │
                    │  Wacht op serial commands van Klipper        │
                    │  Houdt globale state bij                     │
                    │  Polt periodiek slaves (elke 50ms)           │
                    └──────────┬──────────────────────────────────┘
                               │
           ┌───────────────────┼───────────────────┬────────────┬──────────┐
           │                   │                   │            │          │
        T<nr>              L<n>                U<n>          R         S   timeout
           │                   │                   │            │          │
           ▼                   ▼                   ▼            ▼          ▼
     ┌──────────┐        ┌──────────┐       ┌──────────┐  ┌─────────┐ ┌────────┐
     │VALIDATE  │        │VALIDATE  │       │VALIDATE  │  │  RESET  │ │ FAULT  │
     │(Proces 3)│        │(Proces 1)│       │(Proces 2)│  │procedure│ │        │
     └────┬─────┘        └────┬─────┘       └────┬─────┘  └────┬────┘ └────────┘
          │                   │                   │             │
          │ OK                │ OK                │ OK          │ OK (any state)
          ▼                   ▼                   ▼             ▼
    ┌───────────────┐  ┌─────────────┐    ┌──────────────┐ ┌───────────┐
    │LOAD_PRINTER   │  │LOAD_SLAVE   │    │EJECT_SLAVE   │ │ RESET     │
    │(Proces 3)     │  │(Proces 1)   │    │(Proces 2)    │ │procedure  │
    └────┬──────────┘  └─────┬───────┘    └──────┬───────┘ └─────┬─────┘
         │                   │                   │              │
         │ stepper OK        │ user action       │ sensor OK    │ all slaves
         │ servo release     │ sensor B          │ servo open   │ reset
         ▼                   ▼                   ▼              ▼
    ┌──────────────────────────────────────────────────────────────┐
    │              IDLE (state updated)                            │
    └──────────────────────────────────────────────────────────────┘
         ▲                                              │
         │                                             │ commando error
         │                                             │ timeout
         │                                             │ hardware fail
         │                                             ▼
         │                                        ┌──────────┐
         │                                        │  FAULT   │
         └────────────────────────────────────────┤ (only S  │
                                                   │  + R)    │
                                                   └──────────┘

```

---

## Gedetailleerde Transities

### BOOT → ENUMERATE_SLAVES
**Trigger**: Power-on, hardware OK  
**Acties**:
- Initialiseer stepper-driver
- Initialiseer I2C-bus
- Zet alle EN-lijnen laag

---

### ENUMERATE_SLAVES → SYNC_STATE
**Trigger**: Alle slaves gëenumereerd (timeout op default-adres)  
**Acties**:
- Noteer aantal slaves (N)
- Weet nu de I2C-adressen van slave 1 → N

---

### SYNC_STATE → IDLE
**Trigger**: Alle slaves gëpolled, mode opgehaald  
**Acties**:
- Globale state bekend (coupled_slave, per-slave state)
- Klaar voor commands

---

### IDLE → VALIDATE (on `T<nr>`)
**Trigger**: Klipper stuurt `T<nr>`  
**Precondities**:
- `<nr>` is geldig (0 ≤ nr < N)
- Slave nr is niet OFFLINE
- (Optioneel) Controleer dat max 1 slave gekoppeld is

**If OK**: VALIDATE → LOAD_PRINTER  
**If FAIL**: IDLE, reply `fail<n>`

---

### IDLE → VALIDATE (on `L<n>`)
**Trigger**: Klipper/operator stuurt `L<n>` (load slave n)  
**Precondities**:
- `<n>` is geldig
- Slave n is niet OFFLINE
- Slave n is EMPTY (geen filament)

**If OK**: VALIDATE → LOAD_SLAVE  
**If FAIL**: IDLE, reply `fail<n>`

---

### IDLE → VALIDATE (on `U<n>`)
**Trigger**: Klipper stuurt `U<n>` (eject slave n)  
**Precondities**:
- `<n>` is geldig
- Slave n is niet OFFLINE
- Slave n is niet IN_PRINTER

**If OK**: VALIDATE → EJECT_SLAVE  
**If FAIL**: IDLE, reply `fail<n>`

---

### IDLE → RESET
**Trigger**: Klipper stuurt `R` (reset)  
**Acties**:
- Onderbreek enig actief proces
- Ga naar RESET-procedure

---

### IDLE → FAULT
**Trigger**: Heartbeat-fout, hardware-fout detected  
**Acties**:
- Log error
- Zet master in FAULT-state
- Wacht op reset

---

### LOAD_PRINTER (Proces 3) → IDLE
**Trigger**: Stepper voltooid, servo released, slave in IN_PRINTER  
**Acties**:
- Reply `ok` naar Klipper
- coupled_slave = None (release lock)
- Update globale state

**Foutpaden**:
- Timeout stepper → FAULT
- Servo-fout → FAULT

---

### LOAD_SLAVE (Proces 1) → IDLE
**Trigger**: User action voltooid (sensor B actief, slave knijpt)  
**Acties**:
- Reply `ok`
- State slave = READY (groen LED)

**Foutpaden**:
- Sensor B blijft passief > timeout → FAULT
- I2C-communicatie fail → FAULT

---

### EJECT_SLAVE (Proces 2) → IDLE
**Trigger**: Sensor B deactiveert (filament weg)  
**Acties**:
- Reply `ok`
- State slave = EMPTY (LED uit)

**Foutpaden**:
- Sensor B blijft actief > timeout → FAULT

---

### RESET → IDLE
**Trigger**: Reset-procedure compleet  
**Acties**:
- Zet alle slaves naar state EMPTY
- coupled_slave = None
- Globale state = known
- Reply `ok`

**Foutpaden**:
- Slave(s) niet responsief > timeout → FAULT

---

### FAULT → RESET
**Trigger**: Klipper stuurt `R` vanuit FAULT  
**Acties**:
- Voer full reset uit
- Clear error flag

---

### FAULT → (stay FAULT)
**Trigger**: Elk ander commando  
**Response**: `fail<n>` (fout-code indicates "in FAULT")

---

### FAULT → IDLE
**Trigger**: (Optioneel) Operationeel reset na diagnose  
**Note**: Kan ook via hardware reset worden bereikt

---

## Globale Invarianten (allijd bewaakt)

1. **Max 1 gekoppeld**: `coupled_slave` is None of één geldige ID
2. **State-consistency**: Per-slave state matches servo + sensor + historie
3. **Heartbeat alive**: Alle slaves moeten elke X seconden reageren
4. **Klipper sync**: USB-serial blijft responsive

---

## Timing

| Transitie | Typisch | Max |
|-----------|---------|-----|
| BOOT → ENUMERATE | 1s | 2s |
| ENUMERATE (per slave) | 100ms | 200ms |
| SYNC_STATE (N slaves) | 50ms × N | 100ms × N |
| IDLE → Proces 1,2,3,4 | 5-15s | 30s |
| Elke IDLE poll-cyclus | 50ms | 100ms |

---

## LED-feedback (per slave) gekoppeld aan state

| State | LED | Betekenis |
|-------|-----|-----------|
| OFFLINE | Rood knipperend snel | Niet reactief |
| EMPTY | Uit | Geen filament |
| IDLE_AWAITING_LOAD | Wit pulserend langzaam | Klaar om filament in te steken |
| LOADING (transient) | Wit knipperend snel | Bezig met laden |
| READY | Groen vast | Filament aanwezig, niet gekoppeld |
| COUPLED (proces 3) | Geel knipperend | Motor draait |
| IN_PRINTER | Blauw vast | Filament in hotend |
| UNLOADING (proces 4) | Geel knipperend | Motor reverse |
| FAULT | Rood vast | Fout in deze slave |

