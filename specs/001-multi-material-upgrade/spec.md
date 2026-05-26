# Feature Specification: Multi-material upgrade voor Klipper 3D-printer

**Feature Branch**: `001-create-feature-branch`

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
- **FR-003a**: De master MUST na voltooiing van een materiaalwissel een gereedheidsbevestiging terugsturen naar Klipper via USB-serieel, zodat de printuitvoering pas hervat nadat het nieuwe filament beschikbaar is bij de extruder.
- **FR-003b**: De master MUST een foutmelding terugsturen naar Klipper wanneer een T<nr> opdracht een kanaal adresseert dat niet als actieve slave geregistreerd is.
- **FR-003c**: Bij ontvangst van een foutmelding van de master MUST de Klipper-macro de print automatisch pauzeren (via `PAUSE`) en de foutmelding tonen; de operator hervat de print handmatig na herstel.
- **FR-004**: Het systeem MUST bij materiaalwissel uitsluitend het geselecteerde slave-kanaal activeren en andere kanalen gedeactiveerd houden.
- **FR-004a**: Het systeem MUST een materiaalwissel uitvoeren als sequentieel proces met sensorverificatie: (1) deactiveer huidig actief kanaal (klem loslaten), (2) verifieer via filamentsensor dat filament verwijderd is, (3) activeer doelkanaal (klem aandrukken), (4) verifieer via filamentsensor dat nieuw filament aanwezig is. Indien stap 2 of 4 niet binnen de veilige tijdsgrens (FR-007) bevestigd wordt, MUST de wissel falen en een toevoerfout volgens FR-011 worden gestart.
- **FR-005**: Het systeem MUST per slave de klemstatus beheersen zodat filament alleen wordt aangedrukt wanneer dat kanaal actief is.
- **FR-006**: Het systeem MUST de filament sensorstatus na het klemmechanisme gebruiken om te bevestigen dat filament daadwerkelijk wordt doorgevoerd.
- **FR-007**: Het systeem MUST een toevoerfout detecteren en melden wanneer filamentdetectie uitblijft binnen een veilige tijdsgrens (standaard: 5 seconden na het toevoercommando). Deze tijdsgrens, evenals retry-aantal (FR-011) en backoff-tijden (FR-011/FR-017), zijn compile-time constanten in de firmware; aanpassen vereist een nieuwe firmware-build en flash. Er is geen runtime-configuratiekanaal.
- **FR-008**: Het systeem MUST hot-plug van slaves ondersteunen in idle-toestand, inclusief automatische registratie bij aansluiten en deregistratie bij loskoppelen.
- **FR-009**: Het systeem MUST voorkomen dat een losgekoppeld of defect kanaal geselecteerd wordt voor nieuwe materiaalwissels.
- **FR-010**: Het systeem MUST duidelijke operatorfeedback tonen over kanaalstatus (beschikbaar, actief, fout, losgekoppeld) uitsluitend via LED-kleuren op de slave-modules (geen master-display, geen Klipper-statusvariabelen); de LED-codering is puur state-afhankelijk: uit = geen filament, wit knipperend = filament wordt ingestoken, groen = filament in slave (ready), blauw = filament in hotend, geel knipperend = actief proces, rood = fault.
- **FR-010a**: Elke slave MUST zijn eigen filament-locatietoestand bijhouden (`EMPTY`, `READY` = filament in slave, `LOADED` = filament in hotend) op basis van zijn lokale filamentsensor in combinatie met door de master ontvangen commando's; er is geen aparte hotend-sensor vereist. Toestandstransities:
    - **Reset / power-up**: slave retracteert filament tot de filamentsensor deactiveert, voert vervolgens weer aan tot de sensor activeert; eindtoestand `READY` (LED groen). Detecteert de sensor in het geheel geen filament, dan blijft de toestand `EMPTY` (LED uit).
    - **LOAD-commando (filament naar hotend)**: slave voert filament naar de hotend; gedurende de aanvoer en zolang de filamentsensor actief blijft, beschouwt de slave het filament als `LOADED` (LED blauw).
    - **UNLOAD-commando (filament terug uit hotend)**: slave retracteert tot de filamentsensor deactiveert, voert dan weer aan tot de sensor activeert; eindtoestand `READY` (LED groen).
    - **Sensor-uitval tijdens een operatie** (sensor wijzigt niet binnen FR-007 tijdsgrens): slave gaat naar `FAULT` (LED rood) en meldt de fout via I2C status volgens FR-011.
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
- **FR-019**: Het USB-serial protocol tussen Klipper en master is strikt request/response per T-commando: elk `T<nr>` MUST worden afgesloten met hetzij een gereedheidsbevestiging (FR-003a) hetzij een foutmelding (FR-003b) voordat Klipper een volgend T-commando verstuurt. De master gaat ervan uit dat T-commando's nooit overlappen en hoeft geen queue of cancel-logica te implementeren.
- **FR-020**: De master MUST mens-leesbare diagnose-/event-regels uitvoeren op het bestaande USB-serial-kanaal (timestamp, kanaal-ID, commando, foutcode, retry-poging) als aanvullende regels die Klipper kan negeren. Dit kanaal is bedoeld voor ontwikkeling en bug-rapportage; er worden GEEN eisen gesteld aan Klipper-zijde om deze regels te parsen of te tonen, en het vervangt of dupliceert de LED-feedback (FR-010) niet als primaire operatorfeedback.

### Key Entities *(include if feature involves data)*

- **Master Controller**: Centrale eenheid die kanaalselectie, activatievolgorde, foutafhandeling en statusdistributie beheert.
- **Slave Module**: Modulair kanaal dat één filament toevoert en status levert over koppeling, klemactivering en sensorresultaat.
- **Filament Channel**: Logische representatie van een selecteerbaar materiaalpad met unieke identiteit, beschikbaarheid en actuele toestand.
- **Drive Coupling State**: Toestandsinformatie over mechanische en elektrische koppeling tussen master en slave, gebruikt voor inzetbaarheid.
- **Feed Validation Event**: Gebeurtenis die vastlegt of filament na activatie succesvol gedetecteerd werd binnen de veilige tijdsgrens.
- **Recovery Action**: Door operator gekozen herstelstap na fout (opnieuw proberen, ander kanaal kiezen, taak pauzeren).
- **I2C Command Frame**: Vaste-lengte frame van master naar slave: 1 command byte (bijv. ACTIVATE, DEACTIVATE, QUERY_STATUS, RESET) + optionele payload bytes.
- **I2C Status Frame**: Vaste-lengte antwoord van slave naar master: 1 statusbyte (toestandscode), 1 bit filamentsensor, 1 foutcode byte.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Operators kunnen een extra slave-module aansluiten en bruikbaar maken in minder dan 60 seconden in minimaal 95% van de pogingen.
- **SC-002**: Materiaalwissels worden in minimaal 95% van de gevallen succesvol afgerond zonder handmatige herstart van de printtaak.
- **SC-003**: Het systeem detecteert en meldt toevoerfouten binnen 5 seconden na het overschrijden van de veilige toevoertijd in minimaal 99% van de foutgevallen.
- **SC-004**: Minimaal 90% van de operators kan een foutsituatie zelfstandig herstellen met de aangeboden herstelstappen binnen 2 minuten.
- **SC-005**: Onjuiste kanaalactivatie (ander kanaal dan geselecteerd) komt voor in minder dan 1 op 1.000 materiaalwissels.

## Clarifications

### Session 2026-05-26 (Continued 3)

- Q: How does the master handle I2C bus-level errors (NACK, bus timeout, corrupt status frame) on a command to a slave, as opposed to filament feed errors (FR-007)? → A: Bus-retry the same I2C command up to 3× with short backoff (10/20/40 ms); if still failing, set the channel to `FAULT` (red LED) and report the error to Klipper via FR-003b/FR-003c. Bus retries do not count against the FR-011 feed-retry counter.
- Q: How are mechanical jam / motor-stall conditions detected during active printing (filament present at sensor but wheel slips or motor stalls)? → A: Out-of-scope. Filament-sensor-based feed validation is only applied during load/unload/material-change sequences. Detection of slip, jam, or under-extrusion during continuous printing is left to Klipper / the extruder side; no additional hardware (encoder, current sensing) is required on master or slaves.
- Q: What happens if Klipper sends a new `T<nr>` while the master is still processing the previous one (before the ready/error confirmation has been returned)? → A: This situation cannot occur by protocol contract: every `T<nr>` must be completed with either a ready-confirmation (FR-003a) or an error (FR-003b) before Klipper sends the next tool-change command. The master therefore processes strictly one tool-change at a time; overlapping T-commands are not a runtime concern.
- Q: Is there any diagnostic/log output besides the LED feedback, for development and bug reporting? → A: Yes. The master emits human-readable event/debug lines (timestamp, channel, command, error code, retry attempt) on the same USB-serial channel as supplementary lines that Klipper can ignore. This is for developers/support; LEDs remain the primary operator feedback channel and no Klipper-side parsing is required.
- Q: How are tunables like the FR-007 safe-feed timeout, FR-011 retry count, and backoff intervals configured — runtime-adjustable or compile-time only? → A: Compile-time constants only. Values are baked into the firmware (`#define`/`constexpr`); changing them requires rebuilding and re-flashing. There is no runtime configuration channel, no NVS/flash-stored config, and no per-command configuration from Klipper.

### Session 2026-05-26 (Continued 2)

- Q: How does a Klipper `T<nr>` index map to a specific physical slave? → A: `T<nr>` maps directly to the slave at physical chain position `n` (T0 = first slave after master). Re-enumeration after hot-plug can change which physical slave a given `T<nr>` refers to; no stable per-slave identity and no operator-configured mapping.
- Q: How does a slave determine the "filament in hotend" (blue LED) state, given it has only one sensor after its own clamp? → A: The slave tracks filament location locally as `EMPTY` / `READY` / `LOADED` using its filament sensor combined with master commands. On reset it retracts until the sensor deactivates, then feeds until it activates again (→ `READY`, green). On LOAD it feeds toward the hotend; while the sensor stays active the slave treats filament as `LOADED` (blue). On UNLOAD it retracts until sensor deactivates, then feeds until it activates (→ `READY`, green). No separate hotend sensor is required.
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
- Q: Welk I2C-berichtformaat gebruiken master en slave? → A: Vaste-lengte byteframes — master stuurt 1 command byte (ACTIVATE, DEACTIVATE, QUERY_STATUS, RESET) + payload; slave antwoordt met 1 statusbyte (toestandscode) + 1 sensorbit + 1 foutcode byte; minimale overhead, eenvoudig te implementeren op ESP32C3.
- Q: Gedrag bij re-enumeratie tijdens actieve materiaalwissel? → A: Niet van toepassing — slaves worden nooit losgekoppeld tijdens een actieve print; loskoppelen tijdens print is een out-of-scope situatie met ongedefinieerd gedrag.
- Q: Hoe wordt de master geconfigureerd met kanaalcount/type vóór gebruik? → A: Geen pre-configuratie vereist — master ontdekt slaves volledig automatisch via I2C-enumeratie bij opstart; T<nr> opdrachten worden door Klipper doorgestuurd naar de master; als <nr> een niet-geregistreerde slave adresseert, stuurt de master een foutmelding terug naar Klipper.
- Q: Wat doet Klipper bij een foutmelding van de master na T<nr>? → A: Klipper pauzeert de print automatisch via de `PAUSE` macro en toont de foutmelding; operator hervat handmatig na herstel.
- Q: Hoe wordt operatorfeedback over kanaalstatus gepresenteerd (FR-010)? → A: Uitsluitend via slave LED-kleuren (state-coded); geen master-display of Klipper-statusvariabelen; operator leest fysieke LEDs op de slave-modules.
- Q: Wat is de veilige tijdslimiet voor filamenttoevoerdetectie (FR-007)? → A: Configureerbare firmware-constante, standaard 5 seconden na het filamenttoevoercommando; aanpasbaar per hardware-setup.

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

