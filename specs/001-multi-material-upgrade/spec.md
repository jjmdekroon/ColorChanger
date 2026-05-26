# Feature Specification: Multi-material upgrade voor Klipper 3D-printer

**Feature Branch**: `001-multi-material-upgrade`

**Created**: 2026-05-26

**Status**: Draft

**Input**: User description: "Multi-material upgrade voor een 3D-printer (Klipper-gebaseerd). Een master coördineert één of meer slaves; elke slave verzorgt de toevoer van één filament. De master communiceert met Klipper via USB-serial en stuurt via de master een aandrijfas. De slaves koppelen aan deze aandrijfas. Elke slaves gebruikt een servo, deze klemt het het filament vast tegen een aangedreven wiel die vast zit op de aandrijfas. Een filament sensor zit na het klem mechanisme. Het wordt een modulair systeem waarmee slaves 'hot-pluggable' moeten zijn zowel electrisch als mechanisch."

## User Scenarios & Testing *(mandatory)*

> **Design References**:  
> - [ARCHITECTURE.md](ARCHITECTURE.md) — Hoog-level component- en protocol-overzicht
> - [TECHNICAL_DESIGN.md](TECHNICAL_DESIGN.md) — Volledige technische specificatie (state machines, processen, code-organisatie)
>

### User Story 1 - Materiaal kiezen tijdens print (Priority: P1)

Als printeroperator wil ik tijdens een print een specifiek filamentkanaal kunnen kiezen, zodat de master het juiste slave-kanaal activeert en de print met het gekozen materiaal doorgaat.

**Why this priority**: Dit is de kernwaarde van de upgrade: multi-materiaalprints uitvoeren zonder handmatige filamentwissel per kleur of materiaal.

**Independent Test**: Kan volledig getest worden door een testprint met meerdere materiaalwissels uit te voeren en te verifiëren dat elke wissel via het juiste slave-kanaal verloopt.

**Acceptance Scenarios**:

1. **Given** een werkende master met minimaal twee aangesloten slaves, **When** de operator een materiaalwissel naar kanaal B start, **Then** activeert het systeem alleen slave B en wordt filament B correct aangevoerd.
2. **Given** een actieve print met geplande materiaalwissel, **When** de master de wissel uitvoert, **Then** hervat de print zonder verlies van de volgende printstap.

---

### User Story 2 - Hot-plug van modules (Priority: P2)

Als operator wil ik een slave-module mechanisch en elektrisch kunnen toevoegen of verwijderen zonder het hele systeem uit te schakelen, zodat onderhoud en uitbreiding snel kunnen gebeuren.

**Why this priority**: Modulaire uitbreiding en vervanging is essentieel voor gebruiksgemak en schaalbaarheid in de praktijk.

**Independent Test**: Kan onafhankelijk getest worden door een slave-module tijdens idle-toestand toe te voegen en te verwijderen en te verifiëren dat detectie en beschikbaarheid automatisch worden bijgewerkt.

**Acceptance Scenarios**:

1. **Given** een draaiende master in idle, **When** een nieuwe slave wordt aangesloten, **Then** verschijnt deze automatisch als beschikbaar filamentkanaal.
2. **Given** een verbonden slave die niet actief filament levert, **When** de slave wordt losgekoppeld, **Then** markeert het systeem dit kanaal als niet beschikbaar zonder storing in overige kanalen.

---

### User Story 3 - Foutdetectie op filamentpad (Priority: P3)

Als operator wil ik direct een duidelijke melding krijgen wanneer filament niet correct wordt doorgevoerd, zodat ik snel kan ingrijpen en printverlies minimaliseer.

**Why this priority**: Betrouwbare foutdetectie beperkt materiaalverlies en vermindert mislukte prints.

**Independent Test**: Kan zelfstandig getest worden door een situatie te simuleren waarin een actieve slave geen filamentdetectie na de klemfase geeft.

**Acceptance Scenarios**:

1. **Given** een actieve slave tijdens toevoer, **When** de filament sensor geen filament detecteert binnen de verwachte toevoertijd, **Then** meldt het systeem een toevoerfout met kanaalidentificatie en aanbevolen herstelactie.
2. **Given** een gedetecteerde toevoerfout, **When** de operator herstel bevestigt, **Then** biedt het systeem een veilige herstart van de materiaaltoevoer voor hetzelfde of een alternatief kanaal.

---

### Edge Cases

- Een slave wordt aangesloten maar levert geen geldige status; het kanaal blijft geblokkeerd totdat geldige status beschikbaar is.
- Een slave wordt losgekoppeld terwijl die als volgende voor een materiaalwissel gepland staat; het systeem moet overschakelen naar foutstatus met herstelopties.
- **Out-of-scope**: Loskoppelen van een slave tijdens een actieve print (inclusief tijdens een materiaalwissel) valt buiten de ondersteunde gebruiksscenario's; gedrag in dit geval is ongedefinieerd.
- **Out-of-scope**: Mechanische jam-/motor-stall-detectie tijdens continue extrusie valt buiten scope; filamentsensor-gebaseerde validatie wordt alleen tijdens load/unload/materiaalwissel toegepast.
- De filament sensor detecteert permanent filament terwijl de klem open staat; het systeem moet dit als sensordefect behandelen.
- Mechanische koppeling is aanwezig maar elektrische koppeling ontbreekt; het kanaal mag niet als inzetbaar worden gemarkeerd.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Het systeem MUST een master ondersteunen die één of meer slave-modules coördineert als afzonderlijke filamentkanalen.
- **FR-002**: Het systeem MUST per slave exact één filamentkanaal beheren met een unieke kanaalidentiteit.
- **FR-003**: Gebruikers MUST een doel-filamentkanaal kunnen selecteren voor een materiaalwissel tijdens een printtaak via standaard G-code tool-change macros (T0, T1, T2…) in de Klipper-configuratie.
- **FR-003-MAP**: De master MUST `T<nr>` opdrachten direct mappen op de slave op fysieke ketenpositie `n` (T0 = eerste slave na master, T1 = tweede slave, …). Aangezien adressen na hot-plug opnieuw worden toegewezen op basis van fysieke positie (FR-014/FR-015), kan re-enumeratie de fysieke slave waarnaar een bepaalde `T<nr>` verwijst wijzigen; er is geen stabiele identiteit per slave en geen operator-configuratie van de mapping vereist.
- **FR-003a**: De master MUST na voltooiing van een materiaalwissel een gereedheidsbevestiging (`ok`) terugsturen naar Klipper via USB-serieel, zodat de printuitvoering pas hervat nadat het nieuwe filament beschikbaar is bij de extruder. **Already-loaded fast path**: indien een `T<nr>`-commando het reeds gekoppelde (`coupled`) en `LOADED`-slave-kanaal adresseert, MUST de master onmiddellijk de bevestiging `algeladen` retourneren ZONDER de FR-004a-sequentie uit te voeren (geen RELEASE, geen GRIP, geen stepper-beweging, geen sensorverificatie); de master-state blijft `IDLE` en `coupledSlaveIdx` / `currentToolIdx` blijven onveranderd. Klipper behandelt `algeladen` semantisch identiek aan `ok`.
- **FR-003b**: De master MUST een foutmelding terugsturen naar Klipper wanneer een T<nr> opdracht een kanaal adresseert dat niet als actieve slave geregistreerd is.
- **FR-003c**: Bij ontvangst van een foutmelding van de master MUST de Klipper-macro de print automatisch pauzeren (via `PAUSE`) en de foutmelding tonen; de operator hervat de print handmatig na herstel.
- **FR-004**: Het systeem MUST bij materiaalwissel uitsluitend het geselecteerde slave-kanaal activeren en andere kanalen gedeactiveerd houden.
- **FR-004a**: Het systeem MUST een materiaalwissel uitvoeren als sequentieel proces met sensorverificatie: (1) deactiveer huidig actief kanaal (klem loslaten), (2) verifieer via filamentsensor dat filament verwijderd is, (3) activeer doelkanaal (klem aandrukken), (4) verifieer via filamentsensor dat nieuw filament aanwezig is. Indien stap 2 of 4 niet binnen de veilige tijdsgrens (FR-007) bevestigd wordt, MUST de wissel falen en een toevoerfout volgens FR-011 worden gestart.
- **FR-005**: Het systeem MUST per slave de klemstatus beheersen zodat filament alleen wordt aangedrukt wanneer dat kanaal actief is.
- **FR-005a**: Een slave die in `IDLE_AWAITING_LOAD` (EMPTY, servo open) staat MUST een filament-insteek door de operator autonoom detecteren via activatie van zijn filamentsensor en deze gebeurtenis behandelen als een impliciet `L<n>`-commando voor dat kanaal: de slave knijpt de servo, voert de LOADING-transient uit en eindigt in `READY`. De master MUST deze autonome trigger uitsluitend toestaan wanneer hij in `IDLE` is (geen in-flight `T<nr>`/`L<n>`/`U<n>`/`R`-procedure). Wanneer de master niet idle is, MUST hij actief verhinderen dat een lege slave naar `IDLE_AWAITING_LOAD` overgaat door die slave in een passieve mode (`MODE_READY`-equivalent of expliciete "insertion-disabled"-mode) te houden tot de master terugkeert naar `IDLE`. De master MUST de impliciete `L<n>` via reguliere polling waarnemen, zijn interne bookkeeping bijwerken en een diagnose-regel (FR-020) emitten; er wordt GEEN extra USB-serial respons naar Klipper gestuurd (de impliciete trigger is operator-initiated, niet host-initiated).
- **FR-006**: Het systeem MUST de filament sensorstatus na het klemmechanisme gebruiken om te bevestigen dat filament daadwerkelijk wordt doorgevoerd.
- **FR-007**: Het systeem MUST een toevoerfout detecteren en melden wanneer filamentdetectie uitblijft binnen een veilige tijdsgrens (standaard: 5 seconden na het toevoercommando). Deze tijdsgrens, evenals retry-aantal (FR-011) en backoff-tijden (FR-011/FR-017), zijn compile-time constanten in de firmware; aanpassen vereist een nieuwe firmware-build en flash. Er is geen runtime-configuratiekanaal.
- **FR-008**: Het systeem MUST hot-plug van slaves ondersteunen in idle-toestand, inclusief automatische registratie bij aansluiten en deregistratie bij loskoppelen.
- **FR-009**: Het systeem MUST voorkomen dat een losgekoppeld of defect kanaal geselecteerd wordt voor nieuwe materiaalwissels.
- **FR-010**: Het systeem MUST duidelijke operatorfeedback tonen over kanaalstatus (beschikbaar, actief, fout, losgekoppeld) uitsluitend via LED-kleuren op de slave-modules (geen master-display, geen Klipper-statusvariabelen); de LED-codering is puur state-afhankelijk: uit = geen filament, wit knipperend = filament wordt ingestoken, groen = filament in slave (ready), blauw = filament in hotend, geel knipperend = actief proces, rood = fault.
- **FR-010a**: Elke slave MUST zijn eigen filament-locatietoestand bijhouden (`EMPTY`, `READY` = filament in slave, `LOADED` = filament in hotend) op basis van zijn lokale filamentsensor in combinatie met door de master ontvangen commando's; er is geen aparte hotend-sensor vereist. Omdat slaves GEEN eigen stepper-driver hebben (de gedeelde aandrijfas wordt uitsluitend door de master via TMC2209 aangedreven), MUST de slave alle filament-beweging via een gekoppelde master-driven sequence uitvoeren; de slave kan op eigen kracht geen filament transporteren. Toestandstransities:
    - **Reset / power-up (sensor-only classificatie)**: bij boot voert de slave GEEN beweging uit. De slave classificeert uitsluitend op basis van de filamentsensor-status: sensor actief → `READY` (LED groen); sensor inactief → `EMPTY` (LED uit). Indien de master een genormaliseerde startpositie wenst, MUST de master na enumeratie via reguliere GRIP + stepper-aandrijving een retract/feed-cyclus uitvoeren onder zijn eigen FSM-controle; dit valt buiten de slave-bootsequence.
    - **LOAD-commando (filament naar hotend)**: master drijft de gedeelde aandrijfas; slave is gekoppeld via GRIP. Gedurende de aanvoer en zolang de filamentsensor actief blijft, beschouwt de slave het filament als `LOADED` (LED blauw). Sensor-edge `1→0` op een `LOADED` slave triggert de autonome catch-transitie naar `CATCHING` (zie SLAVE_STATE_TRANSITIONS.md).
    - **UNLOAD-commando (filament terug uit hotend)**: master drijft de gedeelde aandrijfas in retract-richting; de slave wacht tot de filamentsensor deactiveert en vervolgens tot de sensor weer activeert; eindtoestand `READY` (LED groen).
    - **Sensor-uitval tijdens een operatie** (sensor wijzigt niet binnen FR-007 tijdsgrens): slave gaat naar `FAULT` (LED rood) en meldt de fout via I2C status volgens FR-011.
    - **Slave-mode-uitbreiding**: de drie spec-toestanden `EMPTY` / `READY` / `LOADED` corresponderen één-op-één met de design-modes uit [SLAVE_STATE_TRANSITIONS.md](SLAVE_STATE_TRANSITIONS.md): `EMPTY` ↔ `IDLE_AWAITING_LOAD` (of de passieve `MODE_READY` wanneer master niet idle, zie FR-005a); `READY` ↔ `READY` (na operator-insteek of unload), met de transient sub-modes `LOADING`/`COUPLED`/`IN_TRANSIT`/`EJECTING`/`CATCHING` als implementatie-detail; `LOADED` ↔ `IN_PRINTER`. `FAULT` is een aparte sticky toestand orthogonaal aan deze drie.
- **FR-011**: Het systeem MUST een veilig herstelpad bieden na een kanaalfout met automatische retry-logica: bij filamenttoevoerfout MUST het systeem het kanaal tot 3× opnieuw proberen met exponentiële backoff (1s, 2s, 4s tussen pogingen); na 3 mislukte pogingen MUST het systeem de operator een keuze aanbieden tussen opnieuw proberen op hetzelfde kanaal of overschakelen naar een alternatief beschikbaar kanaal.
- **FR-011a**: Een automatische retry uit FR-011 MUST uitsluitend de falende sub-stap van de FR-004a-sequentie herhalen op dezelfde slave (re-engage klem / re-feed en opnieuw filamentsensor controleren binnen FR-007 tijdsgrens). De eerder gedeactiveerde slave wordt NIET opnieuw aangeraakt; stap (1) en (2) van FR-004a worden niet opnieuw uitgevoerd tijdens retries. Pas wanneer alle 3 retries falen en de operator een alternatief kanaal kiest (FR-011), wordt een volledige nieuwe FR-004a-sequentie naar dat alternatief gestart.
- **FR-012**: Het systeem MUST de voortgang van de printtaak behouden bij succesvolle materiaalwissels zonder handmatige herinitialisatie van de printjob.
- **FR-013**: Het systeem MUST periodiek een broadcast-query uitvoeren om nieuwe aangesloten slaves te detecteren en defecte/verwijderde slaves op te sporen.
- **FR-013a**: De periodieke broadcast-query uit FR-013 MUST uitsluitend worden uitgevoerd wanneer de master idle is (geen actieve printtaak en geen in-flight materiaalwissel). Tijdens een actieve print of materiaalwissel MUST de broadcast worden opgeschort en pas worden hervat zodra de master terugkeert naar idle. Dit waarborgt de out-of-scope-garantie dat slaves nooit tijdens een actieve print worden gere-enumereerd en voorkomt I2C-buscontentie tijdens feed-/klemoperaties.
- **FR-014**: Het systeem MUST bij detectie van topologieverandering (slave toevoeg/verwijder) automatisch een volledige re-enumeratie uitvoeren, adressen opnieuw toewijzen gebaseerd op fysieke positie in de keten, en defecte slaves invalideren.
- **FR-015**: Het systeem MUST adressen NIET persisteren in slave-geheugen over disconnects; bij reconnect ontvangt een slave altijd een nieuw adres gebaseerd op zijn positie.
- **FR-016**: Bij master-reboot MUST het systeem alle slaves opnieuw opsommen via I2C-enumeratie, alle kanalen als beschikbaar markeren, en eventuele in-progress materiaalwisselstatus verwijderen. De operator MUST expliciet een nieuwe materiaalwissel initiëren na herstart.
- **FR-017**: Het systeem MUST I2C-buscommunicatiefouten (NACK, bus-timeout, corrupt of onvolledig statusframe) onderscheiden van filament-feed-fouten (FR-007). Bij een I2C-fout op een commando naar een slave MUST de master het exact zelfde commando tot 3× opnieuw versturen met korte backoff (10 ms, 20 ms, 40 ms). Indien de slave na 3 bus-retries nog steeds niet correct antwoordt, MUST de master het betreffende kanaal naar `FAULT` zetten (LED rood) en een foutmelding naar Klipper sturen conform FR-003b/FR-003c. Bus-retries tellen NIET mee voor de FR-011 feed-retry-teller.
- **FR-018**: Het systeem detecteert GEEN mechanische jam of motor-stall tijdens actief printen. Filamentsensor-gebaseerde feed-validatie (FR-006/FR-007/FR-010a) wordt uitsluitend toegepast tijdens load-/unload-/materiaalwisselsequenties. Detectie van print-problemen tijdens continue extrusie (slip, vastlopen, onderextrusie) valt buiten scope en wordt overgelaten aan Klipper / de extruder-zijde.
- **FR-019**: Het USB-serial protocol tussen Klipper en master is strikt request/response per commando: elk `T<nr>`/`L<n>`/`U<n>`/`R` MUST worden afgesloten met één van de terminale antwoorden `ok` (FR-003a), `algeladen` (FR-003a, already-loaded fast path), of `fail<n>` (FR-003b) voordat Klipper een volgend commando verstuurt. Het transient antwoord `busy` (FR-025) is NIET terminaal — het signaleert alleen dat de master nog bezig is en het commando opnieuw moet worden ingediend. `S` (FR-024) is uitgezonderd van de serialisatieregel en kan op elk moment worden verstuurd. De master gaat ervan uit dat terminale commando's nooit overlappen en hoeft geen queue of cancel-logica te implementeren.
- **FR-020**: De master MUST mens-leesbare diagnose-/event-regels uitvoeren op het bestaande USB-serial-kanaal (timestamp, kanaal-ID, commando, foutcode, retry-poging) als aanvullende regels die Klipper kan negeren. Dit kanaal is bedoeld voor ontwikkeling en bug-rapportage; er worden GEEN eisen gesteld aan Klipper-zijde om deze regels te parsen of te tonen, en het vervangt of dupliceert de LED-feedback (FR-010) niet als primaire operatorfeedback.
- **FR-021**: De master MUST een expliciet host-geïnitieerd laad-commando `L<n>` accepteren via USB-serial, dat de slave op fysieke ketenpositie `n` aanstuurt om filament van `EMPTY` (`IDLE_AWAITING_LOAD`) naar `READY` te brengen (GRIP + master-driven feed tot filamentsensor activeert). Het commando volgt dezelfde request/response-discipline als FR-019 (`ok` / `fail<n>`) en dezelfde retry-/fault-semantiek als FR-011/FR-011a/FR-017. Een `L<n>` op een slave die reeds `READY` of `LOADED` is, MUST `ok` retourneren zonder mechanische actie; een `L<n>` naar een niet-geregistreerde of `FAULT`-slave MUST een foutmelding retourneren conform FR-003b. De Klipper-zijde exposeert dit als de macro `MMU_LOAD INDEX=<n>`.
- **FR-022**: De master MUST een expliciet host-geïnitieerd unload-/eject-commando `U<n>` accepteren via USB-serial, dat de slave op positie `n` aanstuurt om filament terug naar `EMPTY` (`IDLE_AWAITING_LOAD`) te brengen (master-driven retract tot filamentsensor deactiveert, dan RELEASE). Request/response-discipline en retry-/fault-semantiek identiek aan FR-021. Een `U<n>` op een slave die reeds `EMPTY` is, MUST `ok` retourneren zonder mechanische actie. De Klipper-zijde exposeert dit als de macro `MMU_EJECT INDEX=<n>`.
- **FR-023**: De master MUST een soft-reset-commando `R` accepteren via USB-serial dat alle kanaal-state opschoont zonder hardware-reboot: voor elke slave wordt `RELEASE` + passieve mode uitgegeven, alle feed-/bus-retry-tellers en `lastError`-velden worden gewist, `coupledSlaveIdx` en `currentToolIdx` worden naar 'geen' gezet, en de master keert terug naar `IDLE`. Re-enumeratie wordt NIET uitgevoerd (de bestaande adres-toewijzing blijft geldig). `R` is het primaire operator-pad om uit een sticky `FAULT`-toestand te herstellen zonder fysieke reboot. Antwoord: `ok` na voltooiing. De Klipper-zijde exposeert dit als de macro `MMU_RESET`.
- **FR-024**: De master MUST een status-query-commando `S` accepteren via USB-serial dat een mens-leesbare statusregel retourneert in het formaat `state=<MasterState> coupled=<int|none> slaves=<n> tool=<int|none>`. Dit commando MUST altijd onmiddellijk antwoorden, ook tijdens een in-flight `T<nr>`/`L<n>`/`U<n>`/`R`-procedure, en MUST GEEN mechanische actie uitvoeren of state wijzigen. De Klipper-zijde exposeert dit als de macro `MMU_STATUS`.
- **FR-025**: Indien de master een nieuw `T<nr>`/`L<n>`/`U<n>`/`R`-commando ontvangt terwijl een eerder commando nog niet met `ok`/`algeladen`/`fail<n>` is afgesloten, MUST de master onmiddellijk het ASCII-antwoord `busy\n` retourneren zonder het nieuwe commando te queueen, zonder het in-flight commando te annuleren, en zonder zijn state te wijzigen. De Klipper-zijde MUST een `busy`-antwoord behandelen als een transient hint, het commando na `BUSY_RETRY_HINT_MS` opnieuw versturen, en na `BUSY_RETRY_MAX` opeenvolgende `busy`-antwoorden de print pauzeren via `PAUSE` met een foutmelding. `S` is uitgezonderd van deze regel (zie FR-024). Het `busy`-mechanisme is de Klipper-zijdige tegenhanger van FR-019's strikte request/response-contract en MUST geen impact hebben op feed-retry- of bus-retry-tellers van de master.

### Key Entities *(include if feature involves data)*

- **Master Controller**: Centrale eenheid die kanaalselectie, activatievolgorde, foutafhandeling en statusdistributie beheert.
- **Slave Module**: Modulair kanaal dat één filament toevoert en status levert over koppeling, klemactivering en sensorresultaat.
- **Filament Channel**: Logische representatie van een selecteerbaar materiaalpad met unieke identiteit, beschikbaarheid en actuele toestand.
- **Drive Coupling State**: Toestandsinformatie over mechanische en elektrische koppeling tussen master en slave, gebruikt voor inzetbaarheid.
- **Feed Validation Event**: Gebeurtenis die vastlegt of filament na activatie succesvol gedetecteerd werd binnen de veilige tijdsgrens.
- **Recovery Action**: Door operator gekozen herstelstap na fout (opnieuw proberen, ander kanaal kiezen, taak pauzeren).
- **I2C Command Frame**: Vaste 4-byte frame van master naar slave: 1 opcode byte (`PING`, `SET_ID`, `GRIP`, `RELEASE`, `MODE_READY`, `MODE_AWAITING_LOAD`, `MODE_IN_PRINTER`, `MODE_EJECTING`) + 3 payload bytes. Zie [contracts/i2c-frames.md](contracts/i2c-frames.md) voor de autoritatieve definitie.
- **I2C Status Frame**: Vaste 4-byte antwoord van slave naar master (slave-mode, filamentsensor, lastEvent, lastError). Zie [contracts/i2c-frames.md](contracts/i2c-frames.md) voor de autoritatieve definitie.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Operators kunnen een extra slave-module aansluiten en bruikbaar maken in minder dan 60 seconden in minimaal 95% van de pogingen.
- **SC-002**: Materiaalwissels worden in minimaal 95% van de gevallen succesvol afgerond zonder handmatige herstart van de printtaak.
- **SC-003**: Het systeem detecteert en meldt toevoerfouten binnen 5 seconden na het overschrijden van de veilige toevoertijd in minimaal 99% van de foutgevallen.
- **SC-004**: Minimaal 90% van de operators kan een foutsituatie zelfstandig herstellen met de aangeboden herstelstappen binnen 2 minuten.
- **SC-005**: Onjuiste kanaalactivatie (ander kanaal dan geselecteerd) komt voor in minder dan 1 op 1.000 materiaalwissels.

## Clarifications

### Session 2026-05-26 (Continued 4)

- Q: Hoe wordt een handmatige filament-insteek in een lege slave geïnitieerd — vereist dit een expliciet `L<n>`-commando vanuit Klipper of mag de slave het autonoom oppikken? → A: Autonoom oppikken. Een slave die in `IDLE_AWAITING_LOAD` staat behandelt sensor-activatie als een impliciet `L<n>`: hij grijpt en gaat naar `READY`. Toegestaan uitsluitend wanneer de master in `IDLE` is; tijdens een in-flight `T<nr>`/`L<n>`/`U<n>`/`R` houdt de master alle lege slaves in een passieve mode zodat insteek niet wordt opgepikt. De master neemt de transitie waar via reguliere polling, werkt bookkeeping bij en emit een diagnose-regel (FR-020); er gaat geen aanvullend bericht naar Klipper.

### Session 2026-05-26 (Continued 3)

- Q: How does the master handle I2C bus-level errors (NACK, bus timeout, corrupt status frame) on a command to a slave, as opposed to filament feed errors (FR-007)? → A: Bus-retry the same I2C command up to 3× with short backoff (10/20/40 ms); if still failing, set the channel to `FAULT` (red LED) and report the error to Klipper via FR-003b/FR-003c. Bus retries do not count against the FR-011 feed-retry counter.
- Q: How are mechanical jam / motor-stall conditions detected during active printing (filament present at sensor but wheel slips or motor stalls)? → A: Out-of-scope. Filament-sensor-based feed validation is only applied during load/unload/material-change sequences. Detection of slip, jam, or under-extrusion during continuous printing is left to Klipper / the extruder side; no additional hardware (encoder, current sensing) is required on master or slaves.
- Q: What happens if Klipper sends a new `T<nr>` while the master is still processing the previous one (before the ready/error confirmation has been returned)? → A: This situation cannot occur by protocol contract: every `T<nr>` must be completed with either a ready-confirmation (FR-003a) or an error (FR-003b) before Klipper sends the next tool-change command. The master therefore processes strictly one tool-change at a time; overlapping T-commands are not a runtime concern.
- Q: Is there any diagnostic/log output besides the LED feedback, for development and bug reporting? → A: Yes. The master emits human-readable event/debug lines (timestamp, channel, command, error code, retry attempt) on the same USB-serial channel as supplementary lines that Klipper can ignore. This is for developers/support; LEDs remain the primary operator feedback channel and no Klipper-side parsing is required.
- Q: How are tunables like the FR-007 safe-feed timeout, FR-011 retry count, and backoff intervals configured — runtime-adjustable or compile-time only? → A: Compile-time constants only. Values are baked into the firmware (`#define`/`constexpr`); changing them requires rebuilding and re-flashing. There is no runtime configuration channel, no NVS/flash-stored config, and no per-command configuration from Klipper.

### Session 2026-05-26 (Continued 2)

- Q: How does a Klipper `T<nr>` index map to a specific physical slave? → A: `T<nr>` maps directly to the slave at physical chain position `n` (T0 = first slave after master). Re-enumeration after hot-plug can change which physical slave a given `T<nr>` refers to; no stable per-slave identity and no operator-configured mapping.
- Q: How does a slave determine the "filament in hotend" (blue LED) state, given it has only one sensor after its own clamp? → A: The slave tracks filament location locally as `EMPTY` / `READY` / `LOADED` using its filament sensor combined with master commands. **On reset/power-up the slave performs NO mechanical movement** — slaves have no stepper driver (the shared TMC2209 is master-owned), so the slave classifies purely from the debounced sensor state: sensor active → `READY` (green), sensor inactive → `EMPTY` (LED off). Any post-enumeration normalisation (retract/feed cycle to land in a known position) is the master's responsibility via GRIP + master-driven stepper. On LOAD it feeds toward the hotend (master-driven); while the sensor stays active the slave treats filament as `LOADED` (blue). On UNLOAD the master retracts until sensor deactivates, then feeds until it activates (→ `READY`, green). No separate hotend sensor is required. **Note**: this answer is the authoritative version; it supersedes the earlier wording that described a slave-initiated retract/feed cycle at boot. See FR-010a for the normative rule.
- Q: When an FR-011 auto-retry fires after a feed-error during a material change, what is retried? → A: Only the failing sub-step is replayed on the same target slave (re-engage clamp / re-feed and re-check the sensor within FR-007); the previously-deactivated slave is not touched. The full FR-004a sequence is only re-run if the operator subsequently chooses to switch to an alternative channel.
- Q: How to resolve the "two slaves claim the same channel identity" edge case, given the master-assigned addressing scheme makes it structurally impossible? → A: Remove the edge case entirely; the dynamic EN-signal handshake addressing makes duplicate identities impossible by construction, so no spec language is needed.
- Q: When does the periodic broadcast-query (FR-013) actually run? → A: Only when the master is idle (no active print job, no in-flight material change). The broadcast is suspended for the full duration of a print and resumes when the master returns to idle.

### Session 2026-05-26 (Continued)

- Q: When the master controller reboots (power loss or manual reset), should discovered channels be re-enumerated or restored from persistent state? → A: Re-enumerate all slaves and mark all channels available; any in-progress material state is discarded (clean start). Upon reboot, the system performs full I2C enumeration, clears any orphaned in-progress selections, and starts with all discovered slaves marked as available.
- Q: When a slave encounters a filament feed error (FR-007 timeout), and the operator initiates recovery, what should happen? → A: Auto-retry the same slave up to 3 times with exponential backoff (1s, 2s, 4s), then offer operator manual choice to retry again or switch to an alternative slave.
- Q: What is the exact sequence of master actions during a material change from Slave A to Slave B? → A: Sequential with verification — Deactivate A (release clamp) → verify filament removed via sensor → Activate B (engage clamp) → verify new filament present; the operation fails if either verification times out within the configured safe time limit (FR-007).

### Session 2026-05-26

- Q: Maximaal aantal gelijktijdig verbonden slave-modules? → A: Geen limiet (dynamisch); slaves worden ontdekt zolang systeem resources beschikbaar zijn.
- Q: Klipper-versie ondersteuning? → A: Alle recent maintained Klipper-versies (≥ 0.11); geen expliciete versie-checking door systeem vereist.
- Q: Communicatieprotocol tussen Master en Slaves? → A: Seriepoort (USB) voor Klipper↔Master; I2C-protocol voor Master↔Slaves communicatie.
- Q: I2C-adresschema en slave-detectie? → A: Sequentiële dynamische toewijzing bij opstart via Enable-signaal handshake. Master stuurt broadcast "wie wil adres", één slave met actief EN-signaal reageert, ontvangt adres en activeert vervolgens EN voor volgende slave. Na timeout geen reactie → adrestoewijzing compleet. Bij hot-plug (toevoeg/verwijder): master voert periodiek broadcast uit, ontdekt nieuwe/verdwenen slaves, invalidates adressen van defecte/verwijderde slaves, en voert volledige re-enumeratie uit. Re-enumeration: adressen worden opnieuw toegewezen gebaseerd op fysieke positie in de keten; geen persistentie van adres over disconnects.
- Q: I2C-busfrequentie? → A: 100 kHz (Standard Mode) voor maximale betrouwbaarheid; hardware: SEEED XIAO ESP32C3 voor zowel master als alle slave-modules.
- Q: LED-feedback en identificatie slaves? → A: LED-kleur is **puur state-afhankelijk** (niet positie-afhankelijk): uit = geen filament; wit knipperend = filament wordt ingestoken; groen = filament in slave (ready); blauw = filament in hotend; geel knipperend = actief proces; rood = fault. Fysieke positie-identificatie gebeurt via nummers (1, 2, 3...) op PCB/labels.
- Q: Hoe ontvangt de master materiaalwisselopdrachten van Klipper, en hoe wacht Klipper op gereedheid? → A: Minimale Klipper-wijzigingen via standaard G-code tool-change macros (T0/T1/T2); master ontvangt wisselbevel via USB-serieel en stuurt een gereedheidsbevestiging terug zodra het nieuwe filament beschikbaar is bij de extruder; de Klipper-macro blokkeert totdat deze bevestiging ontvangen is.
- Q: Welk I2C-berichtformaat gebruiken master en slave? → A: Vaste 4-byte frames in beide richtingen — master stuurt 1 opcode byte + 3 payload bytes; slave antwoordt met een 4-byte status frame (slave-mode, filamentsensor, lastEvent, lastError); minimale overhead, eenvoudig te implementeren op ESP32C3. **(De autoritatieve opcode-set en frame-layout staan in [contracts/i2c-frames.md](contracts/i2c-frames.md); een eerdere wording van deze clarification gebruikte de namen `ACTIVATE/DEACTIVATE/QUERY_STATUS/RESET` en een 3-byte status frame en wordt hierbij vervangen.)**
- Q: Gedrag bij re-enumeratie tijdens actieve materiaalwissel? → A: Niet van toepassing — slaves worden nooit losgekoppeld tijdens een actieve print; loskoppelen tijdens print is een out-of-scope situatie met ongedefinieerd gedrag.
- Q: Hoe wordt de master geconfigureerd met kanaalcount/type vóór gebruik? → A: Geen pre-configuratie vereist — master ontdekt slaves volledig automatisch via I2C-enumeratie bij opstart; T<nr> opdrachten worden door Klipper doorgestuurd naar de master; als <nr> een niet-geregistreerde slave adresseert, stuurt de master een foutmelding terug naar Klipper.
- Q: Wat doet Klipper bij een foutmelding van de master na T<nr>? → A: Klipper pauzeert de print automatisch via de `PAUSE` macro en toont de foutmelding; operator hervat handmatig na herstel.
- Q: Hoe wordt operatorfeedback over kanaalstatus gepresenteerd (FR-010)? → A: Uitsluitend via slave LED-kleuren (state-coded); geen master-display of Klipper-statusvariabelen; operator leest fysieke LEDs op de slave-modules.
- Q: Wat is de veilige tijdslimiet voor filamenttoevoerdetectie (FR-007)? → A: Configureerbare firmware-constante, standaard 5 seconden na het filamenttoevoercommando; aanpasbaar per hardware-setup. **(SUPERSEDED by Session 2026-05-26 Continued 3: "compile-time constants only"; "aanpasbaar per hardware-setup" wordt geïnterpreteerd als "aanpasbaar via een nieuwe firmware-build", NIET als runtime/per-setup configuratie. Zie FR-007 voor de normatieve regel.)**

## Assumptions

- De 3D-printeromgeving ondersteunt materiaalwisselopdrachten vanuit de bestaande printworkflow via standaard G-code tool-change macros (T0/T1/T2); Klipper-configuratiewijzigingen worden tot een minimum beperkt.
- De Klipper-macro wacht actief op een gereedheidsbevestiging van de master via USB-serieel vóór hervatting van de print; er is geen timeout-overschrijding verwacht onder normale omstandigheden.
- Het systeem ondersteunt dynamische slave-detectie zonder vaste bovengrens; schaalbarheid wordt begrensd door beschikbare systeemresources (geheugen, I/O-kanalen).
- Hot-plug wordt gegarandeerd voor idle-toestand; tijdens actieve print (inclusief materiaalwissels) wordt loskoppelen van slaves niet ondersteund en valt buiten de scope van het systeem.
- Operators hebben fysieke toegang om modules veilig aan te koppelen en los te koppelen.
- Filamentsensoren leveren binaire detectie-informatie die betrouwbaar genoeg is voor feed-validatie.
- De master vereist geen voorafgaande kanaalconfiguratie; alle slave-kanalen worden automatisch ontdekt via I2C-enumeratie bij opstart en na hot-plug events.
- Hardwareplatform: SEEED XIAO ESP32C3 wordt gebruikt voor master controller en alle slave modules (voorkeur voor één gestandaardiseerde hardware).
- I2C-busfrequentie ingesteld op 100 kHz Standard Mode voor maximale betrouwbaarheid en afstandstoleratie binnen printer-chassis.
- Slave-adressen worden NIET persistent opgeslagen in slave-geheugen; elke reconnect triggert hernieuwde toewijzing op basis van positie.
- Master voert periodieke broadcast-queries uit om topologieveranderingen (nieuwe slaves, verwijderde/defecte slaves) te detecteren.
- Volledige re-enumeratie wordt automatisch gestart zodra een topologieverandering wordt gedetecteerd; bestaande slotallocation wordt bijgewerkt.
- Slaves gebruiken oplopende I2C-adressen vanaf de master-gekoppelde slave; adres-continuïteit is gelijk aan fysieke positie-order.
- Bij filamenttoevoerfout MUST het systeem automatisch tot 3× opnieuw proberen met exponentiële backoff (1s, 2s, 4s); transiënte fouten (sensorruis, voorbijgaande blokkering) worden autonoom afgehandeld zonder operatorinterventie. Na 3 mislukte pogingen beslist de operator over hernieuwde poging of wissel naar alternatief kanaal.
- Materiaalwissel verloopt strikt sequentieel met sensorverificatie (deactiveer → verifieer leeg → activeer → verifieer aanwezig); geen parallelle activatie/deactivatie om materiaalvermenging en mechanische conflicten te voorkomen.
- Tunables zoals de FR-007-tijdsgrens, FR-011 retry-aantal/-backoff en FR-017 bus-retry-backoff zijn compile-time constanten in de firmware; wijzigingen vereisen een nieuwe build en flash. Er is geen runtime-configuratiekanaal en geen persistente configuratieopslag.

