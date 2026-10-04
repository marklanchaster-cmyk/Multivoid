# Multivoid synchronization audit

Date: 2026-09-24  
Scope: current repository, current `multivoid.log`, existing reverse-engineering
notes, and targeted Blueprint/Kismet reflection exports. No broad native scan,
GUI automation, game-file modification, or synchronization-code change was
performed for this audit.

## Evidence and classification

The current runtime log at
`~/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/multivoid.log` contains only
153 startup/installation lines. It has no transformer, door, drone, light,
container, or world-event session transitions. It can establish that hooks and
subsystems started, but it cannot confirm or refute any present host/client
gameplay failure. Runtime conclusions below therefore cite older recorded tests
where available and otherwise say `UNKNOWN`.

Failure categories used below:

- **A** — interaction/event does not reach authority
- **B** — authoritative state changes but is not replicated
- **C** — receiver invokes the wrong game function
- **D** — object identity mismatch
- **E** — state arrives but required game side effects are skipped
- **F** — host/client both execute, causing echo or double execution
- **G** — world/global-manager state is missing
- **H** — initialization or late-join state is missing
- **I** — currently unknown

`KNOWN` means proved by bytecode, source, or a recorded runtime result. `LIKELY`
means a constrained inference from those facts. `UNKNOWN` means a runtime fact
or uncovered Blueprint path, not a guess.

## Final transformer conclusion — READY FOR RUNTIME VALIDATION

The transformer state machine is sufficiently understood to design a narrow
runtime validation. It is **READY FOR RUNTIME VALIDATION**, not yet ready for an
unqualified synchronization rewrite.

1. **WORKING -> BROKEN is proven.** `generator_C::damage` subtracts one from
   `cycle`; when the resulting value is `<= 0`, it calls
   `generator_C::break`. Direct damage/script/event callers also call `break`.
   `break` is the sole normal-gameplay writer of `isBroken = true`: it guards
   against an already-broken generator, writes `isBroken = true`, plays
   `stationTurnoff`, writes `cycle = 0`, calls `update`, then calls
   `transformerMGPanel_C.randomizeTargets` and `randomizeValues`.
2. **BROKEN/REPAIRING -> WORKING is proven.** Panel interactions leave the
   generator broken while `setKnobs`, `setSwitches`, and `setRotators` derive
   `isSineComplete`, `isSwitchesComplete`, and `isRotatorsComplete`. They
   converge in `generator_C::actionOptionIndex`, not in the panel and not in
   `fullFix`. On the `action == 4` activation path, all three true plus
   `isBroken == true` causes `turnedOn.Broadcast`, `isBroken = false`,
   `cycle = 100`, `update`, and, when present,
   `triggerWhenCompleted.runTrigger(self, 0)`.
3. **Exact game functions/events.** Breakdown is centralized in `damage ->
   break` or direct `break`. Normal human repair is inline in
   `actionOptionIndex`. `fullFix` is a game-supported force-repair/convergence
   operation: the direct external Blueprint caller found in the extracted
   corpus is the shipped `kerfurOmega_C` animation-notify repair path.
4. **Authoritative properties.** `generator_C.isBroken` and
   `generator_C.cycle` are the canonical generator state. The panel arrays,
   targets, and three completion booleans are authoritative for repair progress,
   but there is no separate `REPAIRING` flag. `cycle` changes discretely on
   events; it is not continuously decremented.
5. **Required side effects.** Both break and normal repair reach `update ->
   upd`. `upd` refreshes upgrades and `generatorCond`, runs the appropriate
   `powerControl` branch (`solar` when broken, then `sendPower`; otherwise
   `buttonsVisibility`, then `sendPower`), refreshes the panel, and broadcasts
   `updated`. `turnedOn` has a gameplay listener in `grayEventController_C`,
   and `updated` changes generator-building power/collision state. No
   corresponding `turnedOff`/`broken` dispatcher was found.
6. **What Multivoid currently does.** `repair_sync.cpp` polls only the derived
   repaired edge (`isBroken: true -> false`) every 100 ms and sends
   `RepairOutcome(target=3, repaired=1, portable key)`. The accepted replay
   calls `generator_C.fullFix`. It does not synchronize break, `cycle`, panel
   progress, or randomized puzzle state. Its emergency fallback clears only
   `isBroken` and calls `upd`, leaving `cycle` and panel state potentially
   inconsistent.
7. **Smallest proposed synchronization operation.** For repaired-state
   convergence, one reflected `generator_C.fullFix()` on the authoritative
   target is the smallest existing game-supported Blueprint operation. Its `upd` already
   supplies power, panel, indicator, and `updated` side effects; those should
   not be separately replicated. This is canonical convergence, not exact
   replay of normal completion, because it omits `update` audio and
   `triggerWhenCompleted`. For WORKING -> BROKEN, use `generator_C.break()` for
   execute `generator_C.break()` only on the host, then converge the host's
   randomized panel result. Never independently call `break` on every peer:
   `break` itself invokes `randomizeTargets` and `randomizeValues`.
8. **Remaining UNKNOWN runtime facts.** Whether each placed generator has a
   live `triggerWhenCompleted`; whether dynamic/reflection-only delegate
   listeners exist; whether the normal-path `update` sound or optional trigger
   is gameplay-significant; and how visibly panel randomization diverges when
   `break` runs independently. The smallest probe is a read-only observer for
   `damage`, `break`, `actionOptionIndex`, `update`, `upd`, `fullFix`, and the
   power calls, logging before/after generator and panel fields plus the optional
   trigger target during one normal break/repair and one forced `fullFix`.

Full bytecode evidence and offsets are in
[`notes/transformer_state_machine.md`](notes/transformer_state_machine.md).

## Feature audit

### Doors / interactable doors

| Field | Finding |
|---|---|
| **FEATURE** | Hinged `door_C` interactable doors, including locks/jams, movement timeline, sensor/autoclose, and client-held-open behavior. |
| **GAME CLASS / BLUEPRINT** | `door_C`; relevant entries are `actionOptionIndex`, `doorOpen`, `doorClose`, `checkSensor`, and `move__FinishedFunc`. `active`, `jammed`, and `superClosed` gate opening; `isMoving` plus movement direction describes in-flight intent; `isOpened` becomes settled state at timeline completion. |
| **RELEVANT MULTIVOID SOURCE** | `src/votv-coop/src/coop/interactables/interactable_sync.cpp`, `include/coop/interactables/interactable_channel.h`, `src/votv-coop/src/ue_wrap/devices/door.cpp`, and `src/votv-coop/src/coop/dev/door_probe.cpp`. |
| **CURRENT SYNC STRATEGY** | Host-authoritative keyed channel. Client `mainPlayer.InpActEvt_use` PRE temporarily closes the door's native `Active` gate; POST restores it, resolves the aimed door, debounces press/release, and sends a toggle request. Host validates the door gates, applies it, holds autoclose while requested, and broadcasts authoritative state. Receivers use `SmartApply`/force completion because distant Blueprint timelines can freeze. Connect snapshots seed state. |
| **AUTHORITATIVE GAME STATE** | Host `door_C` open intent/settled state, lock/jam/power gates, and host-side hold/autoclose register. `TryReadOpenIntent` deliberately uses movement direction while moving rather than lagging `isOpened`. |
| **KNOWN GAME FUNCTIONS / EVENTS** | `doorOpen`, `doorClose`, `move__FinishedFunc`, `checkSensor`, and `actionOptionIndex`. `checkSensor` can close after its delay. The native interaction verbs are Blueprint-internal and invisible to the ProcessEvent seam. |
| **NETWORK MESSAGE / REPLAY PATH** | Client `DoorOpenRequest(KeyedTogglePayload)` -> host `Channel::OnRequest` -> host `SmartApply` -> host poll -> `DoorState(KeyedTogglePayload)` -> keyed `SmartApply` on clients. Connect replay sends `DoorState`. |
| **WHAT CURRENTLY FAILS** | The source records earlier failures—client input never reaching host, client-native plus host echo double execution, press/release double toggle, far-away timeline freeze, and unindexed identity. The current implementation explicitly addresses each. The current log has no door interaction, so no present residual failure is demonstrated. |
| **CLASSIFICATION** | Historical **A/C/F/D/E**; current residual is **I** until reproduced. Identity remains a plausible **D** at genuinely unindexed instances. |
| **KNOWN** | Blueprint state/verbs and the authority path are established. Door lane is recorded `[V]` in `docs/COOP_SYNC_MAP.md`. The current code warns loudly when an aimed door is not indexed. |
| **LIKELY** | A present “press does nothing” report is more likely an index/look-at timing failure than a missing door function, because the authority/replay path now covers the known Blueprint behavior. |
| **UNKNOWN** | Which door class/instance currently fails, whether it has a portable identity, and whether failure occurs before request, at host validation, or during apply. |
| **RECOMMENDED NEXT INVESTIGATION** | **Runtime probe**, not more RE. Enable `interactable_log`; perform client open/close on one ordinary keyed door, one locked door, one host-distant door, and one sensor/autoclose door. Correlate client input/key/request, host RX/deny/apply, `DoorState`, and both peers' `isMoving/isOpened/Active`. Then late-join while open. |

### Drone body and delivery state

| Field | Finding |
|---|---|
| **FEATURE** | Delivery drone flight/body mirror, arrival gates, cargo relationship, dust, alarm cue, and signal light. |
| **GAME CLASS / BLUEPRINT** | `drone_C`; relevant graph/functions include `ReceiveTick`, `triggerFly`, `sendShop`, `compileOrder`, `dropSack`, `actionOptionIndex`, and arrival/alarm paths. |
| **RELEVANT MULTIVOID SOURCE** | `src/votv-coop/src/coop/interactables/drone_sync.cpp`, `src/votv-coop/src/ue_wrap/devices/drone.cpp`, `src/votv-coop/src/coop/dev/drone_probe.cpp`, plus the separate order and prop/container lanes. |
| **CURRENT SYNC STRATEGY** | Host simulates the singleton drone. While `Active`, it sends transform at about 20 Hz and one inactive edge. Client disables the drone's own tick, interpolates the host pose, and replays non-tick effects from state bits. A connect snapshot adopts pose immediately. The wire payload includes `active`, but current `OnReliable` never reads or writes that field. |
| **AUTHORITATIVE GAME STATE** | Host drone actor transform and `Active`; mirrored interaction/effect state includes `canTakeOff`, `hasSack`, dust-active plus dust anchor. Cargo objects and contents are authoritative in their prop/container lanes, not in `DroneState`. |
| **KNOWN GAME FUNCTIONS / EVENTS** | `ReceiveTick` is the flight integrator. `triggerFly(console)` is the actual console result. `canTakeOff` is the parked/interaction gate; `hasSack` is an option prerequisite and tells the mirror to repoint its container. |
| **NETWORK MESSAGE / REPLAY PATH** | Host -> clients `DroneState` with pose, `active`, FX/gate bits, dust anchor, and connect `adopt`. Client suppresses tick, drives pose, writes the two gates, repoints cargo, replays dust and arrival cue/light. |
| **WHAT CURRENTLY FAILS** | Static audit found a concrete omission: `DroneState.active` is populated and transmitted, including the falling edge and connect snapshot, but is never consumed by `OnReliable`; the client actor's Blueprint `Active` field is therefore not converged by this lane. The current log cannot show its gameplay impact. Flight, arrival, cargo, and late join otherwise lack current-session evidence. Any missing order/cargo result may belong to `OrderRequest`, prop birth, or container contents rather than pose sync. |
| **CLASSIFICATION** | Proven **B** for the client `Active` field; possible **E/H** consequences require runtime evidence. Other reported drone symptoms remain **I** until separated by subsystem. |
| **KNOWN** | Host/client simulation ownership, pose path, and explicitly mirrored non-tick effects are code-proven. A repository-wide use search shows `p.active` is written when packing/logging but `payload.active` is never read; `SuppressTick` only disables actor tick and does not update the Blueprint field. |
| **LIKELY** | If pose moves correctly but sack/contents do not, the defect is downstream identity/birth/contents rather than flight authority. |
| **UNKNOWN** | The observable consequences of stale client `Active`, plus hands-on status of current `canTakeOff`/`hasSack`, arrival cue, signal light, cargo repoint, and join-during-flight. |
| **RECOMMENDED NEXT INVESTIGATION** | **Runtime probe, then a small implementation decision.** Host launches drone and client watches departure/arrival; log host/client Blueprint `Active`, payload `active`, `canTakeOff`, `hasSack`, pose, dust bit/anchor, cargo pointer/eid, and each state edge. Join once in flight and once parked with a sack. This determines whether the smallest fix is writing the mirrored `Active` field or replaying an existing Blueprint state verb. Do not decode native code unless a reflected native effect call itself fails. |

### Drone console / console interaction

| Field | Finding |
|---|---|
| **FEATURE** | Client use of `droneConsole_C` to command the authoritative drone. |
| **GAME CLASS / BLUEPRINT** | `droneConsole_C::actionOptionIndex` reaches `drone.triggerFly(self)` at Kismet offset 745. `droneConsole_C::player_use` enters its Ubergraph at 512, which is only a pop/no-op for this purpose. Target operation is `drone_C::triggerFly(console)`, whose wrapper enters the drone Ubergraph at 14617. |
| **RELEVANT MULTIVOID SOURCE** | `src/votv-coop/src/coop/interactables/drone_sync.cpp`; targeted CFGs in `reverse_engineering/exports/kismet-cfg/droneConsole/` and `drone/`. |
| **CURRENT SYNC STRATEGY** | Client observes the ProcessEvent-visible main-player E input after execution, checks `lookAtActor` for `droneConsole_C`, debounces 400 ms, and sends command `1`. Host resolves singleton drone and console and directly calls `drone.triggerFly(console)`. |
| **AUTHORITATIVE GAME STATE** | Host drone state and the host's `triggerFly` result. Console press is an intent, not authoritative state. |
| **KNOWN GAME FUNCTIONS / EVENTS** | `droneConsole_C::actionOptionIndex`; `drone_C::triggerFly`. Calling `console.player_use` or replaying the host player's E input is the wrong seam and has been replaced in current source. |
| **NETWORK MESSAGE / REPLAY PATH** | Client `DroneCommandRequest(1)` -> host validation -> reflected `drone_C::triggerFly(console)` -> resulting host `DroneState` stream. |
| **WHAT CURRENTLY FAILS** | No current runtime evidence. Unlike doors, the client observer is POST-only and does not suppress the client's native console action. Thus the client may run local `triggerFly` and also request host `triggerFly`; client tick suppression and later host state should converge pose, but transient local state/effects have not been proven harmless. |
| **CLASSIFICATION** | Previous implementation was **C** (wrong replay function). Current remaining risk is **F**, with present outcome **I**. Singleton/first-object lookup is a potential **D** only if more than one live console exists. |
| **KNOWN** | The exact Blueprint destination and current host call now match. The request is debounced and host-only on receive. |
| **LIKELY** | A failure where the request logs but the host never flies is a `triggerFly` precondition/state issue, not an input visibility issue. A client-only flicker or duplicate cue would implicate unsuppressed local execution. |
| **UNKNOWN** | Whether native local console use actually calls `triggerFly` on the client in the tested interaction route; whether it produces observable double side effects; whether multiple consoles can be live. |
| **RECOMMENDED NEXT INVESTIGATION** | **Runtime probe plus targeted Blueprint check.** On one client press, count `droneConsole.actionOptionIndex` and `drone.triggerFly` on both peers and snapshot drone fields before/after. Expected: one local organic call, one host requested call, then host state convergence. If the local call is harmful, the next design decision is a narrow client gate/cancel—not a broader drone scan. |

### Transformers / generators

| Field | Finding |
|---|---|
| **FEATURE** | Generator decay, break, transformer minigame, repair, and world-power fan-out. |
| **GAME CLASS / BLUEPRINT** | `generator_C`, `transformerMGPanel_C`, `generatorFuckuper_C`, `powerControl_C`; relevant functions are `damage`, `break`, `actionOptionIndex`, `update`, `upd`, `fullFix`, panel setters/interactions, `solar`, and `sendPower`. |
| **RELEVANT MULTIVOID SOURCE** | `src/votv-coop/src/coop/interactables/repair_sync.cpp`, `include/coop/net/protocol.h`; full finding in `reverse_engineering/notes/transformer_state_machine.md`. |
| **CURRENT SYNC STRATEGY** | Poll repaired boolean edge only; send host-authorized `RepairOutcome`; call `fullFix` on accepted peers. No break, cycle, or puzzle-progress replication. |
| **AUTHORITATIVE GAME STATE** | `generator_C.isBroken` and `cycle`; panel target/live arrays and three completion flags for progress; shared world power through `powerControl`/gamemode. |
| **KNOWN GAME FUNCTIONS / EVENTS** | Break: `damage -> break` or direct `break`. Normal completion: generator `actionOptionIndex`, not panel `fullFix`. Force convergence: `fullFix`. Refresh: `update -> upd`; delegates `turnedOn` and `updated`. |
| **NETWORK MESSAGE / REPLAY PATH** | `RepairOutcome {portable key,target=3,repaired=1}` -> host acceptance/broadcast -> target `fullFix`; fallback raw clear plus `upd`. |
| **WHAT CURRENTLY FAILS** | Break/cycle/puzzle state is not synchronized (**B/G**). Repair replay is canonical but not byte-for-byte normal completion; fallback skips canonical fields/side effects (**E**). No current runtime log demonstrates user-visible impact. |
| **CLASSIFICATION** | **B, E, G**, plus runtime **I** around optional normal-completion effects. |
| **KNOWN** | Complete state writes, callers, side effects, panel convergence, power fan-out, and present Multivoid path are bytecode/source-proven. |
| **LIKELY** | `fullFix` is the smallest safe repaired-state convergence verb. Break synchronization will require both `break` and authoritative randomized panel state. |
| **UNKNOWN** | Placed `triggerWhenCompleted`, dynamic delegate bindings, audio significance, and cross-peer random puzzle divergence. |
| **RECOMMENDED NEXT INVESTIGATION** | **Runtime validation** described in the transformer conclusion. Do not change synchronization until that trace is captured. |

### World events

| Field | Finding |
|---|---|
| **FEATURE** | Scheduled/story event firing, client scheduler suppression, replay policy, cosmetic emitter cues, and join-during-event recovery. |
| **GAME CLASS / BLUEPRINT** | `saveSlot_C::settime`, `trigger_eventer_C::runEvent` / `runSpecialEvent`, `mainGamemode_C.activeEvents` and `activeEvents_senders`; individual event classes named in `event_active_sync.cpp`. |
| **RELEVANT MULTIVOID SOURCE** | `src/votv-coop/src/coop/world/event_fire_sync.cpp`, `event_active_sync.cpp`, `event_cue_sync.cpp`, plus event-specific lanes such as `piramid_sync.cpp` and `alarm_sync.cpp`. |
| **CURRENT SYNC STRATEGY** | Host polls growth of `saveSlot.passEvents`, the scheduler's proven fire record. Client scheduler is disabled by setting `allEvents.Num=0` and restored on disconnect. Allowlisted deterministic rows replay through reflected `runEvent`/`runSpecialEvent`; lane-owned rows are deliberately skipped. Host polls the active-event sender registry and sends mapped in-flight snapshots at join. Cosmetic particle cues use a separate host poll/replay lane. |
| **AUTHORITATIVE GAME STATE** | Host scheduler and `passEvents`; global active-event refcount/sender registry; then per-event output lanes. No single event packet can represent every spawned actor, manager change, cue, or phase. |
| **KNOWN GAME FUNCTIONS / EVENTS** | `settime` appends `passEvents`; `runEvent` itself does not. `runEvent(name,None)` / `runSpecialEvent(name)` are the replay verbs. `lib_C::setEvent` maintains `activeEvents` plus sender membership. |
| **NETWORK MESSAGE / REPLAY PATH** | `EventFire` for a new fire; `EventSnapshot(class,row,elapsed)` for in-flight join; `EventCue(cueId,location)` for registered emitter cues. Per-row policy either replays the Blueprint verb or relies on a dedicated state/entity lane. |
| **WHAT CURRENTLY FAILS** | Known policy gaps are explicitly marked no-replay/no-lane: `arirShip`, several creature/save-actor/dropper spawns, prank spawners, and `alienSounds`. Unknown rows default to no replay. Active class-to-row coverage is partial; unmapped in-flight classes log and skip. `elapsedSec` is diagnostic only, so a replayed long-running event restarts its local phase. Current log has no event run. |
| **CLASSIFICATION** | Missing event outputs are chiefly **G**; unmapped/phase-less late join is **H**; rows with dedicated lanes must avoid **F**; unclassified new rows are **I**. |
| **KNOWN** | Scheduler authority and the core replay seam are Blueprint-proven. Recorded autonomous tests passed forced obelisk mid-join and star-rain cue replay; event-specific coverage is intentionally incomplete. |
| **LIKELY** | A generic “events do not sync” report is a per-row output-lane gap, not failure of `EventFire` itself. Replaying every row would create client-local duplicates for host-random or lane-owned events. |
| **UNKNOWN** | The exact currently failing row, its output classes/effects, whether its active sender maps to a row, and whether failure is initial fire or mid-join phase. |
| **RECOMMENDED NEXT INVESTIGATION** | **Targeted Blueprint analysis** per failing row. Capture the `EventFire`/policy line and active sender class first. Then decode only `trigger_eventer_C::runEvent`'s named case and the spawned event class's BeginPlay/timers/end path. Runtime test: force one row on host, observe one client from before fire and one joining mid-event; census output actors, global fields, cues, and BEGIN/END registry edges. |

### Light switches

| Field | Finding |
|---|---|
| **FEATURE** | Physical light-switch presentation and player press propagation. |
| **GAME CLASS / BLUEPRINT** | `lightswitch_C`; `actionOptionIndex` calls `use`. `use` calls the associated `trigger_lightRoot_C.runTrigger(self,0)` when available, then unconditionally plays the switch sound, writes `a = !a`, and updates the mesh. |
| **RELEVANT MULTIVOID SOURCE** | `src/votv-coop/src/coop/interactables/interactable_sync.cpp`, `src/votv-coop/src/ue_wrap/devices/lightswitch.cpp`, and `src/votv-coop/src/coop/dev/lightswitch_probe.cpp`. |
| **CURRENT SYNC STRATEGY** | Symmetric keyed polling of switch presentation bit `a`. Host receiver calls full `use`, converting a client edge into authoritative group behavior. Client receiver calls presentation-only `use` while temporarily shutting the group gate. Client local E PRE similarly gates the group so local input changes only presentation until host group state arrives. |
| **AUTHORITATIVE GAME STATE** | Switch `a` is presentation/click state, not lamp state and not save-persistent. Lamp state belongs to the separate light-group authority below. |
| **KNOWN GAME FUNCTIONS / EVENTS** | `lightswitch_C::use`, `actionOptionIndex`, and group `runTrigger`. The switch verb is Blueprint-internal; state polling is the reliable observation seam. |
| **NETWORK MESSAGE / REPLAY PATH** | `LightState(KeyedTogglePayload)` keyed by switch; host full-use, client presentation-only use. The host's resulting group mutation is carried separately by `LightGroupState`. |
| **WHAT CURRENTLY FAILS** | The original one-lane design could agree on `a` while lamps diverged; the new group lane addresses that. Current log does not exercise it. A failed gate suppression can visibly double-move the group before correction. Unkeyed switch/root relationships remain outside the key lane. |
| **CLASSIFICATION** | Historical **B/G**; residual identity **D**, possible transient **F**, otherwise **I** pending hands-on validation. |
| **KNOWN** | `a` toggles both ways unconditionally; it cannot serve as authoritative lamp state. Full host use and client presentation-only use match the two-state Blueprint semantics. |
| **LIKELY** | If the switch mesh/click agrees but lamps do not, investigate the associated group identity/gate rather than `LightState`. |
| **UNKNOWN** | Hands-on result for current paired switch+group lanes and any unkeyed placed switch involved in a report. |
| **RECOMMENDED NEXT INVESTIGATION** | **Runtime probe.** Press from client with group gate both on and off; log switch key/`a`, root key/`active`/`isActive`, `LightState`, `LightGroupState`, and suppression restore. Repeat after late join. |

### Light groups

| Field | Finding |
|---|---|
| **FEATURE** | Authoritative state and fan-out of a group of lamps/ambient lights. |
| **GAME CLASS / BLUEPRINT** | `trigger_lightRoot_C`. `runTrigger(1)` sets `isActive=true`; `runTrigger(2)` sets false; both call `updLig`. Index 0 is an `active`-gated toggle. `setActive` changes only the gate. `updLig` fans state to `lights[]` and `ambs[]`. Persistence stores `isActive` and `active`. |
| **RELEVANT MULTIVOID SOURCE** | `interactable_sync.cpp`, `ue_wrap/devices/lightswitch.cpp`, and `coop/dev/light_group_census.cpp`. |
| **CURRENT SYNC STRATEGY** | Host-authoritative source-agnostic poll of `isActive`; clients do not author it. Apply uses absolute, ungated `runTrigger(root, 1 or 2)` so `updLig` executes. Connect snapshots are included. |
| **AUTHORITATIVE GAME STATE** | Host `trigger_lightRoot_C.isActive`; `active` is a separate interaction gate and is not replaced by lamp state. |
| **KNOWN GAME FUNCTIONS / EVENTS** | `runTrigger`, `updLig`, `setActive`, BeginPlay/load/save paths. Thirteen Blueprint sources can change groups, including power/world-event actors, which is why the group lane cannot be symmetric. |
| **NETWORK MESSAGE / REPLAY PATH** | Host `LightGroupState(KeyedTogglePayload)` -> client key resolution -> no-op if equal, otherwise `runTrigger(1/2)`. |
| **WHAT CURRENTLY FAILS** | The repository records only 15 of 42 level light roots as cross-peer-key-stable; 27 child-actor roots are unreachable by this key lane. A smoke saw 15 client applies but was not hands-on. Gate suppression failure can cause a transient double move. |
| **CLASSIFICATION** | Proven coverage gap **D**; historical missing authoritative group state **B/G** is implemented but needs validation; possible transient **F**. |
| **KNOWN** | `runTrigger(1/2)` is the correct absolute side-effectful apply; `setActive` is not. Key coverage is incomplete by census. |
| **LIKELY** | Any failure confined to one of the 27 child-actor roots will persist until identity is solved; changing the replay verb will not fix it. |
| **UNKNOWN** | Which problematic light belongs to which identity population and whether 15 keyed groups are visually correct in current two-peer play. |
| **RECOMMENDED NEXT INVESTIGATION** | **Runtime probe first** with `lightgroup_census=1`, keyed-group press, world/power-driven group change, and late join. If the failure is an unkeyed child actor, perform **targeted Blueprint/level-ownership analysis** of `trigger_lightRoot_C` child-actor construction and derive a stable parent/component path; no native scan is indicated. |

### Container / cabinet lids

| Field | Finding |
|---|---|
| **FEATURE** | Swinging storage/cabinet lid only—not inventory contents. |
| **GAME CLASS / BLUEPRINT** | `prop_swinger_C`. `open` gates on `locked`, writes `opened=true`, drives constraint/rotation/audio, and broadcasts `doorOpened`; `close` writes false, drives the closing effects, and broadcasts `doorClosed`. |
| **RELEVANT MULTIVOID SOURCE** | `src/votv-coop/src/coop/interactables/interactable_sync.cpp`, `src/votv-coop/src/ue_wrap/actors/swinger.cpp`, and targeted CFG `reverse_engineering/exports/kismet-cfg/prop_swinger/`. |
| **CURRENT SYNC STRATEGY** | Symmetric keyed poll of `opened`; receiver calls the actual `open(false)` or `close()` Blueprint function. Connect snapshot included. |
| **AUTHORITATIVE GAME STATE** | Keyed `prop_swinger_C.opened`, subject to the Blueprint lock gate. This lane does not own any `GObjStack` contents. |
| **KNOWN GAME FUNCTIONS / EVENTS** | `open`, `close`, `doorOpened`, `doorClosed`; no autoclose writer was found, so the simpler symmetric channel is used. |
| **NETWORK MESSAGE / REPLAY PATH** | `ContainerState(KeyedTogglePayload)` -> resolve by prop key -> call `open(false)` or `close()`. |
| **WHAT CURRENTLY FAILS** | No current evidence of a lid failure. Historical generic risks are missing/unmatched key and late identity resolution; raw state would skip physics/audio/delegates, but current replay uses verbs. |
| **CLASSIFICATION** | Potential **D/H**; present status **I**. Current verb apply avoids **E**. |
| **KNOWN** | State and side-effect functions are Blueprint-proven; recorded sync map marks the generic interactable lane verified. |
| **LIKELY** | A lid-only mismatch is identity/index timing unless logs show a failed `open`/`close` apply. |
| **UNKNOWN** | The class/key of any currently problematic lid and current late-join behavior. |
| **RECOMMENDED NEXT INVESTIGATION** | **Runtime probe.** Open and close a known keyed swinger from each peer, test locked refusal, then join while open. Log key, `opened`, message direction, and `doorOpened/doorClosed`. |

### Container contents and extracted items

| Field | Finding |
|---|---|
| **FEATURE** | World-container inventory contents and the item created when a player extracts an entry. |
| **GAME CLASS / BLUEPRINT** | `prop_container_C`, `propInventory_C`, `saveSlot_C.GObjStack`, and extraction/spawn paths. Content mutations use `addObject` and `takeObj`, which are `EX_LocalVirtualFunction` and invisible to ProcessEvent. |
| **RELEVANT MULTIVOID SOURCE** | `src/votv-coop/src/coop/props/container_contents_sync.cpp`, `prop_container_extract.cpp`, `prop_drop_intent.cpp`, `src/votv-coop/src/coop/dev/container_selftest.cpp`, and `include/coop/net/protocol.h`. |
| **CURRENT SYNC STRATEGY** | VM opcode `0x45` brackets `addObject`/`takeObj` and dirties the container eid. After 250 ms, the presser sends the whole content slice. Host arbitrates with a published/base content hash, applies, and relays canonical state excluding the author. Receiver raw-writes its local `GObjStack` slice, then calls `updateVolumesAndMass` and `recalculateNames`. Unresolved eids park across the join bracket. Personal inventories are fail-closed; nested-container indices are neutralized. |
| **AUTHORITATIVE GAME STATE** | The host-published content slice for a world-container eid and its compare-and-swap hash. The optimistic extracted object in a player's personal inventory/world is a related but separate birth/rollback concern. |
| **KNOWN GAME FUNCTIONS / EVENTS** | `propInventory_C.addObject`, `takeObj`, `prop_container_C.updateVolumesAndMass`, and `propInventory_C.recalculateNames`. `checkObjectsVolume` is not a refresh verb; it can eject objects. |
| **NETWORK MESSAGE / REPLAY PATH** | Chunked `ContainerContents` blob `[op,eid,baseHash,n,records...]`; client authors to host, host accepts and relays or rejects and republishes canonical contents. Extracted actor birth also depends on the prop spawn/drop-intent pipeline. |
| **WHAT CURRENTLY FAILS** | Sequential two-peer contents synchronization was hands-on verified. A later autonomous simultaneous-take test is recorded as a confirmed residual: both peers optimistically take the same item, host rejects the stale slice, but the losing peer's personal extracted copy is not rolled back (`sum=2`, expected 1). Earlier notes also flag extracted-item birth and `currVol` re-derivation concerns. |
| **CLASSIFICATION** | Confirmed concurrent residual **E/F** (authoritative rejection lacks loser rollback); extracted-birth issues may be **A/B/D** depending the observed seam; parked unresolved eids are **D/H** when they expire. |
| **KNOWN** | VM visibility, global-store layout, sequential behavior, CAS rejection behavior, and simultaneous duplicate residual are measured. Personal inventory must not be authored by this lane. |
| **LIKELY** | Fixing only the world-container slice cannot remove a losing peer's already-materialized personal/world item; rejection needs an explicit rollback/convergence operation tied to that extraction. |
| **UNKNOWN** | Current-build result of the exact race, whether extracted birth is still missing after later prop-pipeline changes, and whether `currVol` remains stale on extraction. |
| **RECOMMENDED NEXT INVESTIGATION** | **Runtime probe/acceptance rerun**, not more broad RE: run the existing simultaneous `ctakerace` barrier test and two sequential controls; log both content hashes, `CONFLICT`, extracted actor eid/key, personal-inventory count, prop birth/adoption, volume/mass/names before and after. The required design work is then a narrow rejection rollback for the losing extraction. |

## Prioritized next work

### READY FOR IMPLEMENTATION/TEST

- **Container simultaneous-take residual:** the failure and acceptance criterion
  are already known. Re-run the existing race on the current build, then design
  only the losing-extraction rollback. No additional broad reverse engineering
  is justified.
- **Drone `Active` omission:** the packet field is sent but not consumed. The
  code defect is statically established; run the narrow state probe to choose
  raw-field convergence versus an existing Blueprint state verb.
- **Doors, keyed lid state, and keyed light switch/group paths:** code and
  Blueprint operations are established. Run the narrow host/client and
  late-join tests above before changing implementation.

### NEEDS TARGETED BLUEPRINT ANALYSIS

- **World event rows that actually fail.** Exact target:
  `trigger_eventer_C::runEvent`/`runSpecialEvent` named case, followed only by
  that case's concrete event class BeginPlay, timer handlers, state writes, and
  end path. Start from the runtime row/class; do not rescan all events.
- **Unkeyed light groups.** Exact target: `trigger_lightRoot_C` as instantiated
  by the owning child-actor Blueprint/level component, including parent/component
  identity and save/load construction. The `runTrigger` behavior is already
  solved.
- **Drone console only if the runtime count shows harmful local execution.**
  Exact functions: `droneConsole_C::actionOptionIndex` around the `triggerFly`
  call and `drone_C::triggerFly` entry 14617 through its state writes/effects.

### NEEDS RUNTIME PROBE

- **Transformer:** one normal break/puzzle repair plus one `fullFix`, using the
  read-only trace specified above. Status: **READY FOR RUNTIME VALIDATION**.
- **Drone body/console:** one client console press, host flight and arrival,
  plus join in flight/parked. Count `triggerFly` calls per peer and correlate
  state stream, cargo, and FX.
- **Doors:** client request, locked denial, host-distant apply, autoclose/hold,
  and late join with `interactable_log`.
- **Lights:** keyed switch with powered/unpowered group, world-driven group
  edge, census identity, and late join.
- **Containers:** current-build sequential and simultaneous extraction test,
  including the losing personal item and re-derived volume.
- **World events:** one live-from-start client and one mid-event joiner for the
  exact reported row.

### NEEDS NATIVE ANALYSIS

- **None presently.** Every identified decision point is explained by Kismet,
  current runtime infrastructure, or a missing runtime observation. Native
  analysis becomes justified only if a specific reflected/native engine call
  fails after the Blueprint path and parameters are confirmed.

### BLOCKED / INSUFFICIENT EVIDENCE

- The phrase “currently problematic” cannot be mapped to exact instances or
  event rows from the current `multivoid.log`; it contains no gameplay session.
  This does not block the targeted probes above, but it does block claiming a
  present root cause for doors, drone, lights, lids, or a particular world
  event.
- World-event coverage remains deliberately incomplete until a failing row is
  named. Replaying every skipped row is not a safe fallback because several
  are host-random or already owned by entity/state lanes.

## Evidence index

- Transformer: `reverse_engineering/notes/transformer_state_machine.md` and
  `reverse_engineering/exports/kismet-cfg/{generator,transformerMGPanel,powerControl}/`
- Doors/lights/lids: `reverse_engineering/exports/kismet-cfg/{door,lightswitch,trigger_lightRoot,prop_swinger}/`
- Drone/console: `reverse_engineering/exports/kismet-cfg/{drone,droneConsole}/`
- Sync inventory and recorded verification: `docs/COOP_SYNC_MAP.md`
- Blueprint dispatch visibility: `docs/COOP_DISPATCH_VISIBILITY.md`
- Current implementations: the source files named in each feature section
