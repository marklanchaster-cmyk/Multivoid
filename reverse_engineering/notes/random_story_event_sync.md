# Random/story event authority and `agrav` result synchronization

## Static scheduler path

```text
daynightCycle_C.ReceiveTick
  -> saveSlot_C.settime
     -> iterate saveSlot.allEvents (list_events rows)
     -> compare row time; skip names already in passEvents
     -> append selected row to passEvents
     -> mainGamemode.eventer.runEvent(rowName, row.specialTrigger)
```

This is the scheduled/story-row scheduler. `runEvent` does not append
`passEvents`. The distinct prank selector is:

```text
trigger_eventer_C.runEvent(..., specialTrigger=ariralPrank)
  -> trigger_eventer_C.summonArirPrank
     -> choose a reputation-tier candidate array
     -> KismetArrayLibrary.Array_Random
     -> trigger_eventer_C.runSpecialEvent(concretePrank)
```

There is no second universal controller for every game RNG source. Ambient
ticker spawners and rare `mainGamemode_C` rolls are independent producers; their
partial coverage is tracked in `docs/COOP_RNG_AUTHORITY.md` and
`spawn_authority`. They are not aliases for the list-events scheduler.

Connected-client suppression now has two layers. `allEvents.Num=0` prevents the
normal `settime` walk. Exact ScriptGate PRE watches cancel organic client bodies
for `trigger_eventer_C.runEvent`, `runSpecialEvent`, and `summonArirPrank`.
Calls from Multivoid's allowlisted replay path carry `fromOurCode` and remain
permitted. The host runs the original Blueprint paths unchanged.

## Authoritative active-event registry

`event_active_sync` remains the sole owner. The host polls
`mainGamemode_C.activeEvents_senders`, assigns a monotonic `instanceId`, and
sends `EventAuthority` BEGIN/END records. The class-to-row map remains
evidence-based; an unknown class remains unmapped.

At `ClientWorldReady`, the host sends `SNAPSHOT_BEGIN(revision)`, zero or more
current `SNAPSHOT_ITEM` records, then `SNAPSHOT_END(revision)`. The client swaps
the staged set only at the matching end bracket. Clients accept this kind only
from slot 0 and never author it.

The active registry is bookkeeping, not permission to execute an entire event.
It does not replay a Blueprint on BEGIN or from a join snapshot. This prevents
an unfocused client from draining BEGIN+END and resurrecting an ended event.
Outputs still use `EventFire` only where its allowlist proves replay safe, or a
dedicated state/entity/cue lane. No safe generic shutdown verb is proven for a
rogue event already active before suppression; cleanup remains event-specific
and UNKNOWN.

Reliable ordering is explicit. `Session::SendReliable` selects
`LaneForKind`, and GameNetworkingSockets guarantees that reliable messages on
the same lane are delivered in send order (`isteamnetworkingsockets.h:475`).
`EventAuthority` is pinned to Normal. `GeneratorBreakState` and the existing
`RepairOutcome` are also pinned to Normal, so a queued break cannot overtake a
later repaired commit when a frozen client resumes.

## `generator_C::break` field audit

`generator_C::break` is guarded by `isBroken`. On the working-to-broken edge it:

1. writes `isBroken=true`;
2. plays `stationTurnoff`;
3. writes `cycle=0`;
4. calls `update` (which reaches `upd` and power-control effects);
5. calls `panelObj.randomizeTargets()`;
6. calls `panelObj.randomizeValues()`.

| Owner | Field | Exact representation |
|---|---|---|
| generator | `isBroken` | reflected bool byte+mask |
| generator | `cycle` | 32-bit integer |
| generator | `panelObj` | object pointer; identity is not transmitted |
| panel | `sine_offset/frequency/amplitude` | three 32-bit integers |
| panel | `targetSine_offset/frequency/amplitude` | three 32-bit integers |
| panel | `switches_states` | exactly 8 native bool bytes; packed into one wire byte |
| panel | `switches_target` | byte |
| panel | `rotators_states` | exactly 9 bytes |
| panel | `rotators_colorGrid` | 9 four-byte `struct_generatorRotator` values (`top/right/bottom/left`) |
| panel | three `is*Complete` fields | reflected bool byte+masks; packed into three bits |

The 8/9/9 lengths are not arbitrary caps: `setSwitches` directly indexes 0..7,
the grid logic indexes 0..8, and the cooked rotator struct has exactly four byte
properties. Capture/apply fail closed if live lengths differ.

The host exact-watches reflected `generator_C::break`. PRE records an organic
host call whose generator was not broken; POST requires that exact object now be
broken and captures after both RNG functions return. This central seam covers
`agrav` and all other canonical game callers.

`GeneratorBreakState` carries the portable key, per-generator revision, and all
fields above. Clients accept only slot 0. An unresolved object queues the newest
state for up to 30 seconds. Apply calls `break()` for shipped shutdown sound and
power/update side effects, immediately overwrites every randomized field, calls
`setRotators`, `setKnobs`, `setSwitches`, restores completion bits, calls
`generator.update`, and verifies the complete state. Static bytecode shows no
irreversible actor spawn/delegate mutation in `break`; its local randomization is
fully overwritten. The game's guard makes this call a no-op if already broken.

A join snapshot sends this same payload for every current `isBroken=true`
generator. Replaying `agrav` is not needed or allowed. Repair remains the
existing host-authoritative `RepairOutcome -> fullFix` path.

## `agrav` output audit

| Output | Evidence | Classification/current result |
|---|---|---|
| random generator then `generator.break()` | trigger CFG offsets 300-403 | RESULT_SYNC; implemented by post-break snapshot |
| shuffled `comps`, gravity off/on, random velocities | 777-835, 915-1393, 1549-1876 | RESULT_SYNC via `AgravState`; host target/RNG only |
| cloak/material timeline and camera light | 537-739, 2551-3159 | SAFE_CLIENT_REPLAY (`cloak` only) + PER_PLAYER/TRANSIENT_CUE reconciliation |
| `arirHover_Cue` on/off | 740-775, 1880-1916 | TRANSIENT_CUE reconciliation from authoritative BEGIN/END |
| `photoTaken` -> `addAriralRep(15)` | 2070-2134, 2165-2258 | RESULT_SYNC persistent reputation gap |
| `lib.setEvent(true/false)` | 15-52, 1952-1988 | authoritative membership bookkeeping |

`agrav` deliberately remains NO-replay. Runtime logs identified it as the Day
16 01:00 scheduled row and confirmed the former client behavior explicitly:
`event_fire: 'agrav' NOT replayed -- physics divergence (by-design host-local)`.
That explained both the connected-client and join-in-progress failures:
EventAuthority described the controller but owned none of its output.

The exact start path is `daynightCycle_C.ReceiveTick -> saveSlot_C.settime ->
trigger_eventer_C.runEvent("agrav", ...) -> trigger_agrav_C.runTrigger`. The
controller immediately registers itself through `lib_C::setEvent(true, false,
self)`. It does not spawn a separate craft actor: the visible craft is the
controller's authored `StaticMesh`, driven by dynamic materials and its `b`
timeline (`cloak`). `PointLight` is attached to the local camera;
`PointLight1` intensity is timeline-driven; `arirHover_Cue` is the loop. `gaher`
collects nearby `prop_C` root components, then `buf` is shuffled and drained
while gravity and velocities change. Cleanup restores gravity, stops the cue,
destroys the host `PointLight` component, reverses cloak, unregisters through
`setEvent(false, ...)`, and removes the photo delegate. No sky mutation or
separate UFO/craft spawn occurs in this Blueprint.

Protocol 65006 adds the narrow `AgravState` result lane. On the host,
EventAuthority's exact sender pointer/instance ID arms native post-observation
of `PrimitiveComponent::SetEnableGravity` and
`SetPhysicsAngularVelocityInDegrees`; only calls sourced by that active
`trigger_agrav_C` are captured. The payload names the concrete host-selected
prop by key plus current eid, carries gravity phase and concrete linear/angular
velocity, and never asks a client to gather or shuffle targets. The client
replays only `cloak(forward)`, hover audio, the camera-attached point light, and
the received prop physics. It never invokes `runTrigger`. The presentation
controller is resolved through the placed `trigger_eventer_C.event_agrav`
object reference. `ReceiveBeginPlay` has already initialized the authored
`StaticMesh` dynamic materials; `PointLight`, `PointLight1`, and
`arirHover_Cue` are authored components rather than objects created by
`runTrigger`. If distinct eventers reference distinct agrav controllers,
presentation is declined because the wire instance does not identify one.

Join snapshots remain decomposed: EventAuthority first reconstructs the active
instance/presentation at its current elapsed age, then `AgravState` sends only
the host's currently gravity-disabled target set. Unresolved prop identities
park and retry every 250 ms until their normal prop snapshot lands or the exact
event instance ends. END restores all targets still tracked for that instance
and hides presentation. Exact monotonic instance IDs plus per-target sequence
tracking prevent a late pending target from resurrecting an ended or newer
state. Result state is instance-scoped; presentation is deliberately limited
to the one shipped `trigger_eventer.event_agrav` controller and does not claim
simultaneous same-class presentation isolation.

The conditional `photoTaken -> addAriralRep(15)` remains an explicit persistent
result gap; no client-side event replay is used to manufacture it.

## Event output roadmap

Categories may overlap for one event.

### SAFE_CLIENT_REPLAY

Current allowlist: `treehouse_0..5`, `break_RomeoSierra`, `break_Victor`,
`break_Victor2`, `obelisk`, `looker_*-1`/signal/satellite force-object rows,
`solar`, `call0`, scare arms (`toeStab`, `falseEnter`, `mann`, `vent`, `crys`,
`fakeGrays`, `susArir`), and `arirGraff_0..6`.

### RESULT_SYNC

- `agrav`: generator and selected-prop physics implemented; reputation remains.
- prank selection: host chooses the concrete prank; prop variants rely on the
  prop lane, while no-lane variants remain gaps.
- Server minigame variant, rare `mainGamemode` rolls, loot RNG, and radar/tower
  shuffle remain targeted rows in `COOP_RNG_AUTHORITY.md`.

### ACTOR_MIRROR

Known covered/no-replay rows include `piramid`, `wisps`, `arirFollower`,
`ventCrawler`, `ventKnocker`, `morningGay`, `borgRozital`, `graystank`,
`eggvasion`, `fallbody/fallcar`, `vehtp`, picnic/prop rows, and
prop-lane pranks. The 2026-09-26 output pass closed the previously invisible
`EX_CallMath` birth seam for the newly listed rows; exact scope is recorded
below. Remaining actor gaps include `arirShip`, `tentacleBalls`, `graysforest`,
`arirBuster`, `boarwar`, `salt`, `rozitalHole`, `dreambase`, `rockThrow`,
`hillRoller`, `alienJump`, and `trashBase`.

### TRANSIENT_CUE

`starRain` has the cue lane. `alienSounds` is a known sound gap. `agrav`
light/audio/cloak and missing sounds on converged result lanes need targeted
cues, not whole-event replay.

### PER_PLAYER

Scare arms are intentionally replayed so each player can experience its own
overlap. `treehouseSleep` is a per-player teleport and remains no-replay. The
camera-relative part of `agrav` is per-player in shape but currently host-only.

### UNKNOWN

Any row absent from both policy tables, any unmapped active sender class, and
already-active rogue-event shutdown defaults to no whole-event replay.

Highest-priority remaining gaps are no-lane actor spawners, `agrav` component
and reputation results, and runtime verification of independent ambient/main-
gamemode rolls marked NEEDS-PROBE. These need targeted Blueprint work or the
existing RNG census, not another broad native scan.

## `blackFog_C` complete output synchronization (2026-09-26)

The exact organic start is an hourly branch in
`daynightCycle_C::ReceiveTick`: after `saveSlot.settime` reports a new hour and
game mode is not 6, `RandomBoolWithWeight(0.0005)` calls
`mainGamemode_C::spawnBlackFog`. That function is only an idempotent deferred
spawn at `(0,0,0)` and stores the result in `mainGamemode.blackFog`. Connected
clients cannot independently select it: the exact `spawnBlackFog` ScriptGate
cancels organic client calls, `blackFog_C::ReceiveBeginPlay` is separately
PRE-gated, and the FinishSpawning fallback removes an uncommanded inert shell.

The cooked controller has no height-fog, sky, light, particle, inventory,
reward, reputation, or persistent-save mutation. Its complete active behavior
is:

- `ReceiveBeginPlay` creates a dynamic `Inst_pp_blackFog`, puts it in the
  authored PostProcess component's first weighted blendable, and ramps float
  `a` by `WorldDeltaSeconds / spd`; the CDO has `spd=300`.
- `set()` clamps `a` to `[0,1]`, writes material scalar `alpha`, writes
  `gamemode.birber.noBirb = (a > 0.5)`, and calls `setVol2(1-a)` on every
  `gamemode.ambMaster.ambience_triggers` entry.
- Two local presentation loops play a camera-relative
  `blackFog_whispers_Cue` after random 5--30 second delays and
  `blackFog_thump` every five seconds. Their position/timing is deliberately
  per-player presentation RNG, not shared world state.
- `ReceiveTick` detects the `a >= 0.9` edge. It activates the named
  `blackfog` reverb and calls `lib.setEvent(true,false,self)` on entry; it
  deactivates/unregisters on the falling edge.
- A 40-second looping timer calls `spawnGhost`, whose guarded body requires
  `a >= 1`, chooses a world angle/nav point, and deferred-spawns `eyer_C` with
  concrete `distance` plus `ignoreDaySpawn=true` and `fog=true`. Under the
  shipped 300-second ramp, its normal ticks are at 40-second multiples through
  280 while `a<1`; the controller enters its approximately one-second terminal
  fade at 300 and is gone before 320. Thus the default graph has no qualifying
  ghost tick. The client mirror nevertheless suppresses `spawnGhost` so a
  changed schedule cannot create client-authored world RNG; any real host
  `eyer_C` birth remains a concrete OwnerEntity spawn/pose/destroy result.
- At full intensity the graph destroys every actor in `eyes`, then subtracts
  delta from `a`, calls `set()` until zero, and destroys itself. The apparent
  `a=180` assignment is immediately clamped to 1 by `set()`, so it is not a
  180-second hold. `ReceiveDestroyed` clears `mainGamemode.blackFog` and calls
  `setEvent(false,false,self)`.

The synchronization reuses `EventAuthority`; no new kind or payload was
needed. `black_fog_sync` watches the exact live `mainGamemode.blackFog` pointer
and asks `event_active_sync` to BEGIN that controller at birth. This early
entry owns **presentation lifetime only**. The normal game does not enter
`activeEvents_senders` until alpha 0.9 (about 270 seconds), and only that native
membership contributes to radio interference. The later poll sees the same
pointer, marks that instance natively active rather than minting another, and
clears the radio contribution again when native membership ends during the
terminal fade. Repeated controllers get new IDs and simultaneous same-class
identity remains pointer + `InternalIndex` scoped.

On connected clients the authoritative BEGIN calls the shipped
`spawnBlackFog` inside the black-fog mirror echo scope. The normal Blueprint
therefore owns post-process, ambience, bird, whisper, thump, and reverb behavior
without replaying the host selector. The client `spawnGhost` callback is
cancelled. For JIP, the already-existing `EventAuthority.flags` byte has a
narrow class-specific interpretation: bit 7 is terminal-fade direction, bit 6
is native `activeEvents_senders` membership, and bits 0--5 are host alpha. A
snapshot creates the controller once and applies that current alpha before its
next latent update. Connected clients also receive ordered UPDATEs on the same
instance whenever those quantized authoritative flags change, including during
terminal fade. An UPDATE neither rerolls nor respawns the controller. Elapsed
age remains diagnostic, not phase authority: missing-presentation retries use
the last host alpha and never derive completion from `steady_clock`, so a
paused/tabbed client cannot complete or refuse reconstruction based on time it
did not simulate.

END is ordered on the same Normal lane as BEGIN/snapshot. Before destroying a
still-live client presentation, the module writes alpha zero and calls the
shipped `set()` once; this restores ambient volumes and `noBirb` even if local
timing diverged. `ReceiveDestroyed` then deactivates reverb and unregisters.
The instance is removed before any later retry can recreate it, snapshot
replacement ends omitted IDs, and disconnect performs the same cleanup. This
covers connected clients, JIP current phase, END, and repeated instances.

Development compatibility note: this branch deliberately remains protocol
65006, but its EventAuthority UPDATE operation and black-fog flag interpretation
are not compatible with older 65006 binaries. Black-fog tests must use the same
branch build on every peer. The release compatibility bump is deferred until
the event-output branch is ready to merge.

Evidence: `research/bp_reflection/blackFog.json`,
`reverse_engineering/exports/kismet-cfg/blackFog/blackFog.txt`, and
`reverse_engineering/exports/kismet-cfg/daynightCycle/daynightCycle.txt`.

## Completed producer census and authority pass (2026-09-26)

This pass treats two questions independently:

- **Selection authority:** can a connected client decide that an event exists,
  choose its variant/target, or start its controller?
- **Output synchronization:** after the host makes that decision, what lane
  makes the client actually experience the result?

An output mirror is not evidence of selection suppression. Conversely, an
authority-safe selector can still leave a `HOST event / CLIENT nothing` gap.

### Proof for systems already gated

- **Weather `timerRain`:** exact `daynightCycle_C::timerRain` PRE interceptor
  skips the Blueprint body while connected as a client; `WeatherState` owns the
  resulting rain scalars/state.
- **Weather `timerLightning`:** exact scheduler PRE interceptor prevents the
  client roll; `LightningStrike` carries each host-selected strike.
- **Weather `fogEvent`:** exact scheduler PRE interceptor prevents the client
  roll; the fog state/controller apply path owns the result.
- **Weather `superFogEvent`:** exact scheduler PRE interceptor prevents the
  client roll; the fog state/controller apply path owns the result.
- **Weather `permaRain_timer`:** exact scheduler PRE interceptor prevents the
  client roll; `WeatherState` owns the resulting rain state.
- **Red-sky/fog births:** exact class-specific `ReceiveBeginPlay` PRE gates
  prevent an organic client controller from starting; commanded red-sky/fog
  mirrors pass only inside their existing echo scopes. `FinishSpawningActor`
  then destroys the inert uncommanded
  `redSkyEvent_C`, `weatherFogController_C`, or `blackFog_C` birth unless the
  exact event-specific mirror echo scope is active. `RedSky`, fog/weather
  state, and `black_fog_sync` own their respective outputs.
- **Server breaking:** the connected client disables the one
  `ticker_serverBreaker_C` actor Tick, so it cannot select a server to break;
  `ServerBoxState` is the authoritative output. Tick is restored on disconnect.
- **Roaches:** both `cockroachMaster_C` and `ticker_roachSummoner_C` are parked
  on clients, and exact PRE gates also cancel `summonRoach`, `addRoachTimer`,
  `spawnNestTimer`, and `CustomEvent`; paged `RoachState` owns the population
  and `RoachConsumed` is only an intent to the host.
- **Sky wisps:** exact `ticker_wispSpawner_C::ReceiveTick` PRE cancellation
  prevents the absolute-world roll; NPC spawn/pose/despawn owns all nine wisp
  variants.
- **Yellow wisps:** exact
  `ticker_yellowWispSpawner_C::ReceiveTick` PRE cancellation prevents the
  nav-world roll; the killer-wisp NPC lane owns the output.
- **Scheduled rows:** `saveSlot_C::settime` cannot walk candidates after the
  client-side `allEvents.Num=0`, and exact ScriptGate PRE watches independently
  cancel organic `trigger_eventer_C::runEvent`; `EventFire` replays only the
  statically safe rows while result/entity lanes own known no-replay rows.
- **Pranks:** exact ScriptGate PRE watches cancel organic
  `summonArirPrank` (the `Array_Random` selector) and `runSpecialEvent` (the
  direct-start bypass). A cancelled `runSpecialEvent` explicitly writes its
  bool OUT parameter `return=false`. Extracted callers are only
  `runEvent -> summonArirPrank -> runSpecialEvent`; no alternate direct caller
  was found in the reflection corpus. Prop/NPC/vehicle lanes own covered prank
  results; several sound/spawner pranks remain output gaps.

All process-lifetime gates test the live connected-client role at call time.
The `allEvents` length and parked actor ticks are restored on disconnect, so
single-player selection resumes.

Suppression return semantics are deterministic. The 19 gated
`mainGamemode_C` entries, ticker `ReceiveTick` entries, `spawnBush`, beehive
`spawn`, trigger-minute entry, overlap entry, and controller BeginPlay entries
are void. `runSpecialEvent` and `wMannequinSpawn_C::spawn` each expose a bool
OUT parameter literally named `return`; both cancellation paths write the
callee frame and caller out storage to `false` before skipping the body.

### Table A — producers

| Producer class | Function | Trigger/cadence | Selection/result | Classification | Client authority status | Suppression seam | activeEvents registration? | Notes |
|---|---|---|---|---|---|---|---|---|
| `saveSlot_C` / `trigger_eventer_C` | `settime -> runEvent` | game-clock crossing | scheduled `list_events` row | EVENT_SELECTOR | AUTHORITY_SAFE | `allEvents.Num=0` plus exact `runEvent` ScriptGate | Controller-dependent | Host native scheduler unchanged. |
| `trigger_eventer_C` | `summonArirPrank -> runSpecialEvent` | prank row/special call | concrete prank | EVENT_SELECTOR | AUTHORITY_SAFE | exact gates on both selector and direct start | Usually no | Safe false return on cancelled `runSpecialEvent`. |
| `daynightCycle_C` | `timerRain` | timer | rain start/parameters | EVENT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | `WeatherState` output. |
| `daynightCycle_C` | `timerLightning` | timer | strike occurrence/location | EVENT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | `LightningStrike` output. |
| `daynightCycle_C` | `fogEvent` | timer | fog start | EVENT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | Fog state output. |
| `daynightCycle_C` | `superFogEvent` | timer | super-fog start | EVENT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | Fog state output. |
| `daynightCycle_C` | `permaRain_timer` | timer | permanent-rain transition | EVENT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | `WeatherState` output. |
| `mainGamemode_C` | `spawnRedSky` | new-day roll/start | red-sky controller | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No | `RedSky` owns output. |
| `mainGamemode_C` | `spawnBlackFog` | new-hour 0.0005 roll/start | black-fog controller | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate + BeginPlay/birth fallback | Yes, natively only at alpha >=0.9; Multivoid begins the same pointer at birth | Complete local presentation/JIP/end through EventAuthority + `black_fog_sync`; client ghost RNG suppressed. |
| `mainGamemode_C` | `Spawn Bad Sun` | new-day roll/start | bad-sun controller | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | Yes (`setEvent`) | No client output lane. |
| `event_fleshRain_C` | `ReceiveBeginPlay` | direct day/night birth | registers, rolls locations, spawns rain results | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact BeginPlay PRE; inert shell removed at FinishSpawn | Yes | Host output is not mirrored completely. |
| `event_fossilBoarWar_C` | `ReceiveBeginPlay` | direct day/night birth | registers, rolls and spawns fossilhound/boar results | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact BeginPlay PRE; inert shell removed at FinishSpawn | Yes | Host output is not mirrored; fossilhound has an exposed float birth write. |
| `mainGamemode_C` | `ufo_midas` | timer delegate | selected target `runTrigger` | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | Unknown | Output is unresolved. |
| `mainGamemode_C` | `ufo_pill` | timer delegate | `ufo_pillfo_C` birth | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Actor-mirror gap. |
| `mainGamemode_C` | `ufo_boo` | timer delegate | `ufo_boofo_spawn1_C` birth | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Actor-mirror gap. |
| `mainGamemode_C` | `ufo_joel` | timer delegate | `ufo_joel_C` birth | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Actor-mirror gap. |
| `mainGamemode_C` | `ufo_ball` | timer delegate | `ufo_ballfo_C` birth | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Actor-mirror gap. |
| `mainGamemode_C` | `tickerFunguy` | timer/RNG | `npc_funguy_C` birth | EVENT_SELECTOR | EXISTING_LANE | exact ScriptGate | No | NPC lane owns output. |
| `mainGamemode_C` | `tickerBody` | timer/RNG/trace | `theBody_C` birth | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No | Actor-mirror gap. |
| `mainGamemode_C` | `ticker_lakeMonsert` | timer/RNG | `event_lakeglow.setActive` | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Result-state gap. |
| `mainGamemode_C` | `ticker_lockerhead` | timer/RNG | `lockerCorpse_C` at selected locker | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No | Actor-mirror gap. |
| `mainGamemode_C` | `ticker_radiotowerPoof` | timer/RNG | `radiotowerPoof_C` birth | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No | Actor-mirror/cue gap. |
| `mainGamemode_C` | `CustomEvent_0` | 10 s timer/RNG | `screamingCorpseController_C` birth | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | Unknown | Actor/controller gap. |
| `mainGamemode_C` | `CustomEvent_3` | 1800 s timer/RNG | `figura_C` around player | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Actor-mirror gap. |
| `mainGamemode_C` | `CustomEvent_5` | 120–300 s timer/RNG | `eg_C` or `geomOcta_C` | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Actor-mirror gap. |
| `mainGamemode_C` | `CustomEvent_6` | 600 s timer/RNG | `NewBlueprint5_C` | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Actor-mirror gap. |
| `mainGamemode_C` | `CustomEvent_7` | 21600 s timer/RNG | `NewBlueprint19_C` | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No evidence | Actor-mirror gap. |
| `mainGamemode_C` | `gorelockertest` | explicit/debug entry | random locker + `lockerCorpse_C` | HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | No | Prevents a callable bypass. |
| `ticker_deerSpawner_C` | `ReceiveTick` | 60–300 s self-rearm | gate, position, deer variant | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | Graph is selector/spawn only; actor gap. |
| `ticker_hexahiveSpawner_C` | `ReceiveTick` | 40–60 min self-rearm | gate/nav position + `hexahiveSpawner_C` | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | Graph is selector/spawn only; actor gap. |
| `ticker_treeSpawner_C` | `ReceiveTick` | self-rearm | gate/position + `walkingTree_C` | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | Graph is selector/spawn only; actor gap. |
| `ticker_tick_C` | `ReceiveTick` | self-rearm | gate + `NewBlueprint17_C` | EVENT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | Graph is selector/spawn only; actor gap. |
| `ticker_gost_C` | `ReceiveTick` | self-rearm | bounding-box position + `poolwalker_C` | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | Graph is selector/spawn only; actor gap. |
| `ticker_egSpawner_C` | `ReceiveTick` | self-rearm | gate/absolute position + `eg_C` | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact PRE | No | Graph is selector/spawn only; actor gap. |
| `ticker_mannequinSpawner_C` / `wMannequinSpawn_C` | `spawn` | 1–3 h ticker chooses marker | selected marker spawns `prop_wMannequin_C` | EVENT_SELECTOR + HOST_RESULT_SELECTOR | EXISTING_LANE | exact ScriptGate on marker `spawn`; ticker/cleanup remains live | No | Prop lane owns the spawned prop; cancelled bool OUT `return=false`. |
| `ticker_bushSpawning_C` | `spawnBush` | Tick calls exact verb | location + `growingPlant_C` | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate on `spawnBush` | No | Actor/save-state gap. |
| `ticker_beehiveSpawner_C` | `spawn` | timer delegate | branch selection + `beehiveBranch_C` | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate on `spawn` | No | Actor/save-state gap. |
| `triggerTimer_C` | `newMinute` | game-minute notification | chooses matching targets and calls `runTrigger` | EVENT_SELECTOR | AUTHORITY_SAFE | exact PRE | Target-dependent | Time/list storage remains untouched; output depends on target. |
| `triggerFallingSky_C` | overlap delegate | player overlap | starts `skyFallingEvent_C` | deterministic story start | AUTHORITY_SAFE | exact overlap PRE | Host-puppet overlap | The host puppet is a collision-enabled `mainPlayer_C`; the trigger sphere overlaps `Pawn`, and the overlap graph does not inspect `OtherActor`. A remote client walking into it can therefore drive the host overlap. |
| `ticker_bp7Spawner_C` | `ReceiveTick` | ticker | camera/player-relative birth | PER_PLAYER | PER_PLAYER | none | No | Not shared-world authority. |
| `ticker_susHoleSpawner_C` | `ReceiveTick` | ticker | camera/player-relative birth | PER_PLAYER | PER_PLAYER | none | No | Not shared-world authority. |
| `ticker_eyers_C` | `ReceiveTick` | night ticker | local-player stalker | PER_PLAYER | EXISTING_LANE | none | No | OwnerEntity mirrors each peer's own eyer. |
| `ticker_fireflySpawner_C` | `ReceiveTick` | ticker | local presentation emitter | PRESENTATION_RANDOM | PRESENTATION_ONLY | none | No | `firefly_sync` is intentionally peer-symmetric. |
| `mainGamemode_C` | `CustomEvent_1` | timer/call | toggles local `isSnowyFootsteps_0` | PRESENTATION_RANDOM | PRESENTATION_ONLY | none | No | No world actor/state mutation. |
| `mainGamemode_C` | `CustomEvent_2` | timer/call | random UI text justification | PRESENTATION_RANDOM | PRESENTATION_ONLY | none | No | UI only. |
| `mainGamemode_C` | `CustomEvent_4` | sleep/wakeup path | local sleep-conditioned hallucination | PER_PLAYER | PER_PLAYER | none | No evidence | Kept local by design. |
| `grayBoarSpawner_C` | mixed `ReceiveTick` | conditional controller | boar encounter selection plus combat cleanup | HOST_RESULT_SELECTOR | UNKNOWN | none; whole-Tick gate rejected | No evidence | No independent instance appeared in the measured world census and no external constructor was found in extracted BPs. If a level-placed instance exists in another map, an exact internal seam or probe is required. |

For known connected, world-significant selectors:
`STILL_CLIENT_RANDOM_CAPABLE = 0`. `grayBoarSpawner_C::ReceiveTick = UNKNOWN`,
not silently declared safe; its mixed Tick was not disabled.

### Table B — event outputs

| Row/output | Implementation/controller class | Authoritative producer | Output classification | Existing lane | Current client-visible behavior | Remaining gap | Priority/status |
|---|---|---|---|---|---|---|---|
| scheduled safe rows | `trigger_eventer_C` targets | host `settime/runEvent` | SAFE_CLIENT_REPLAY | `EventFire` | Allowed rows replay through `fromOurCode` | Per-row gaps remain listed in the earlier roadmap | FULLY_SYNCED for allowlist |
| prank result | `trigger_eventer_C::runSpecialEvent` cases | host prank selector | ACTOR_MIRROR / TRANSIENT_CUE / UNKNOWN | prop/NPC/vehicle where applicable | Covered spawned entities appear; no reroll occurs | `rockThrow`, `hillRoller`, `alienJump`, `trashBase`, `alienSounds` | ACTOR_MIRROR_GAP / TRANSIENT_CUE_GAP |
| rain/fog/lightning | day/night weather controllers | host weather schedulers | RESULT_SYNC / TRANSIENT_CUE | `WeatherState`, fog, `LightningStrike` | Covered weather converges | none known | FULLY_SYNCED |
| red sky | `redSkyEvent_C` | host new-day roll | RESULT_SYNC | `RedSky` | Client creates/applies the commanded red-sky state | none known | FULLY_SYNCED |
| black fog | `blackFog_C` | host hourly roll | SAFE_CLIENT_REPLAY + RESULT_SYNC + PER_PLAYER cues | early exact-pointer `EventAuthority`; shipped local controller; OwnerEntity only for any concrete host eyer | Connected client receives native PP/ambience/bird/audio/reverb behavior without rerolling world output; JIP snaps alpha/direction | runtime smoke validation | FULLY_SYNCED (static evidence) |
| bad sun | `badSun_C` | host new-day roll | RESULT_SYNC + TRANSIENT_CUE | EventAuthority metadata only | Client knows an unmapped event may be active, but does not experience it | state/sky/audio lane | AUTHORITY_SAFE_PRESENTATION_INCOMPLETE |
| flesh rain | `event_fleshRain_C` | host direct day/night birth | RESULT_SYNC + ACTOR_MIRROR + PER_PLAYER | EventAuthority metadata; generic prop coverage unproven | No whole-event replay | spawned result and cue coverage | RESULT_SYNC_GAP |
| fossil boar war | `event_fossilBoarWar_C` | host direct day/night birth | ACTOR_MIRROR + TRANSIENT_CUE + birth state | EventAuthority metadata only | Client receives activity metadata, not fossilhound/boars/shake | fossilhound exposed float, `grayboar_C`, and cues | ACTOR_MIRROR_GAP |
| UFO timer outputs | `ufo_pillfo_C`, `ufo_boofo_spawn1_C`, `ufo_joel_C`, `ufo_ballfo_C` | host `mainGamemode` timers | ACTOR_MIRROR | WorldActor birth/pose/destroy/snapshot | Connected clients and joiners receive the exact host-selected class and transform | class-specific non-transform state not carried | PARTIAL: ACTOR LIFECYCLE SYNCED |
| funguy ticker | `npc_funguy_C` | host `tickerFunguy` | ACTOR_MIRROR | NPC lane | Host spawn/pose/despawn is mirrored | none known | FULLY_SYNCED |
| body/locker/radiotower | `theBody_C`, `lockerCorpse_C`, `radiotowerPoof_C` | host main timers | ACTOR_MIRROR | WorldActor birth/pose/destroy/snapshot | Exact host actor is reconstructed for connected clients and joiners | class-specific cues not independently carried | PARTIAL: ACTOR LIFECYCLE SYNCED |
| lake monster | level `lakeglow_C` | host `ticker_lakeMonsert` | RESULT_SYNC / CUE | none | Client no longer selects independently | `setActive` state, phase, cleanup | AUTHORITY_SAFE_PRESENTATION_INCOMPLETE |
| figura / eg / geometric actors | `figura_C`, `eg_C`, `geomOcta_C` | host main timers | ACTOR_MIRROR / PER_PLAYER cue | NPC birth/pose/despawn/snapshot | Exact host actor lifecycle reaches connected clients and joiners | class-specific non-transform state/cues | PARTIAL: ACTOR LIFECYCLE SYNCED |
| screaming corpse and rare `NewBlueprint*` actors | `screamingCorpse_C`, `NewBlueprint5_C`, `NewBlueprint19_C`, `NewBlueprint17_C` | host main timer/controller or ticker | ACTOR_MIRROR + birth state | none | Client no longer selects independently | deferred-window bool/string birth fields are not carried by WorldActorSpawn | ACTOR_MIRROR_GAP |
| ambient mannequin | `prop_wMannequin_C` | host mannequin marker | ACTOR_MIRROR | PropSpawn/PropDestroy | Host-created prop is mirrored | presentation details unverified | FULLY_SYNCED for identity/lifecycle |
| deer/tree/gost/eg | `deer_C`, `walkingTree_C`, `poolwalker_C`, `eg_C` | host world tickers | ACTOR_MIRROR | NPC birth/pose/despawn/snapshot | Exact host-selected actor reaches connected clients and joiners | class-specific non-transform state | PARTIAL: ACTOR LIFECYCLE SYNCED |
| tick ticker | `NewBlueprint17_C` | host world ticker | ACTOR_MIRROR + birth state | none | Client no longer invents its own result | deferred-window bool and string fields must be carried before actor mirroring is safe | ACTOR_MIRROR_GAP |
| hexahive | `hexahiveSPawner_C` and descendants | host world ticker | ACTOR_MIRROR / RESULT_SYNC | none | Client no longer invents its own result | generated hive actors/state | ACTOR_MIRROR_GAP |
| bush/beehive | `growingPlant_C`, `beehiveBranch_C` | host exact spawn verbs | RESULT_SYNC / ACTOR_MIRROR | none proved | Client no longer invents its own result | persistent actor/state lane | RESULT_SYNC_GAP |
| timed trigger targets | target-specific | host `triggerTimer.newMinute` | UNKNOWN | target-specific | Selection is host-only | audit each concrete target output | UNKNOWN |
| falling sky | `skyFallingEvent_C` | host overlap, including collision-enabled remote `mainPlayer_C` puppet | SAFE_CLIENT_REPLAY not proven | none proved | Client cannot start a rogue local event; its host puppet can drive the authoritative overlap | output/presentation still unproved | UNKNOWN |
| `agrav` | `trigger_agrav_C` | host scheduled event | RESULT_SYNC / PER_PLAYER / CUE | `GeneratorBreakState` + `AgravState` + authored-controller presentation reconciliation | Transformer and exact floating props converge; the shipped singleton craft/cloak/hover/camera-light path is reconstructed live and on join | conditional photo reputation; simultaneous presentation controllers unsupported | PARTIAL: MAJOR EXPERIENCE SYNCED FOR SHIPPED SINGLETON |

The newly gated selectors intentionally received no generic whole-event replay.
Where Table B says gap, the present behavior is **HOST A / CLIENT nothing or
partial output**, which is authority-correct but not presentation-complete.

Targeted follow-up decoded `badSun_C` plus `ui_badSun_C` rather than treating
it as another black-fog-style presentation actor. `daynightCycle` starts it on
the new-day path (game mode 7 or December 24 guarantees it; otherwise the
proved branch includes a 0.001 roll, subject to the achievement condition).
`mainGamemode."Spawn Bad Sun"` only deferred-spawns the controller and stores
`gamemode.badSun`. After parent `actor_save.ReceiveBeginPlay`, the controller
finds the RuntimeVirtualTexture volume, installs a 15-second invalidation
timer, stores itself in `gamemode.badSun`, waits until hour 7--18, registers
through `setEvent`, and creates `ui_badSun_C` with `sunObj=self` and its `super`
flag. Its `siren()` owns a local Audio component/timeline plus a random 50--60
second delay and three pitched thumps. `remove()` unregisters, removes the
widget, clears `gamemode.badSun`, awards the `badsun` achievement, and destroys
the controller.

The companion widget is not presentation-only: its Tick traces the local
camera against the sun, calls `Add Player Damage` with a random 2--3 result,
spawns blood/cues, can drive player control rotation, writes sky-sphere sun
intensity/color, calls `gamemode.setDryness`, reads/writes the dry-state fields,
and contains achievement/save/game-over work. Therefore replaying the whole
widget on a client without separating per-player exposure/damage from shared
dryness/current phase would be an authority leak, while replaying only the
controller would omit the event. Bad sun deliberately remains
**OUTPUT-MISSING** pending a narrow current-state design; no unsafe replay was
added in this pass. Evidence is
`reverse_engineering/exports/kismet-cfg/{badSun,ui_badSun}/`.

## Complete event/output inventory (event-output-sync-pass)

### Inventory accounting

The cooked `trigger_eventer_C` switch contains **66 `runEvent` names** and
**42 `runSpecialEvent` names**. Twelve names are accepted by both dispatchers,
leaving **96 unique eventer actions**. The developer event catalog exposes those
96 actions plus **8 ambient/weather verbs**, for **104 triggerable inventory
entries**. Table A separately enumerates **47 producer/start seams** outside or
under those actions. Producer seams and event names intentionally are not added
together: for example, `mainGamemode.spawnRedSky` is both an ambient verb and a
producer, while the five weather schedulers share output state.

Evidence is `research/bp_reflection/trigger_eventer.json`,
`reverse_engineering/exports/kismet-cfg/trigger_eventer/trigger_eventer.txt`,
the 104-entry table in `coop/dev/event_trigger.cpp`, and the 47 rows in Table A.
The following matrices account for every event name and every Table A producer.
An entity row marked lifecycle-synced means birth, streamed transform, destroy,
and current live actors on join; it does **not** silently claim arbitrary
class-specific fields or transient cues.

### Dispatcher actions: complete or output-owned

| Exact event names | Meaningful output(s) | Classification | Exact owner/lane | Connected / JIP / end |
|---|---|---|---|---|
| `treehouse_0..5` | deterministic authored build-stage mutation | SAFE_CLIENT_REPLAY | `EventFire -> runEvent` | connected replay; save carries current stage on join; native graph owns permanence |
| `break_RomeoSierra`, `break_Victor`, `break_Victor2` | selected server broken state and native side effects | SAFE_CLIENT_REPLAY | `EventFire`; server state later reconciles through `ServerBoxState` | connected replay; save/server snapshot supplies current state; native repair owns end |
| `obelisk` | authored trigger state, alarm/radar presentation | SAFE_CLIENT_REPLAY | `EventFire -> runEvent` | connected replay; save carries authored state; native event cleanup |
| `looker_0-1..4-1`, `peace`, `arirSignal`, `arirSpk`, `picSignal`, `arirSat_0..2`, `piramid_sig` | deterministic `forceObjects`/signal progression | SAFE_CLIENT_REPLAY | `EventFire -> runEvent` | connected replay; transferred save supplies persistent array; no transient instance |
| `solar`, `call0` | deterministic sky/light/sound or end-state verb | SAFE_CLIENT_REPLAY | `EventFire`; light state also has its normal lane | connected replay; persistent flags come from save; native verb ends its cues |
| `toeStab`, `falseEnter`, `mann`, `vent`, `crys`, `fakeGrays`, `susArir` | per-player scare arm/presentation | PER_PLAYER + SAFE_CLIENT_REPLAY | `EventFire -> runEvent` | each connected player receives the arm; not reconstructed for a joiner after the moment has passed |
| `arirGraff_0..6` | exact deterministic decal variant | SAFE_CLIENT_REPLAY | `EventFire -> runSpecialEvent` | connected replay; persistent/JIP lifetime remains the game's decal/save behavior |
| `starRain` | shooting-star particle/cue | TRANSIENT_CUE | `event_cue_sync` cue 0 | connected start/stop; active particle component re-sent on join; native stop clears it |
| `piramid` | `piramid2_C`, four killer wisps, pose/brain/gather behavior | ACTOR_MIRROR | WorldActor + NPC + `piramid_sync` | connected and JIP actor snapshots; destroy lanes close all tracked actors |
| `wisps` | exact host-created wisp variants | ACTOR_MIRROR | NPC birth/pose/despawn/snapshot | connected and JIP complete for creature identity/lifecycle; host deaths remove mirrors |
| `arirFollower` | follower creature | ACTOR_MIRROR | NPC birth/pose/despawn/snapshot | connected and JIP lifecycle; host despawn owns end |
| `ventCrawler` | `ventCrawler_C` | ACTOR_MIRROR | NPC lane; this pass adds the missing `trigger_eventer_C` EX-spawn source | connected and JIP lifecycle; host despawn owns end |
| `ventKnocker` | `kocker_C` | ACTOR_MIRROR | WorldActor lane; this pass adds the missing EX-spawn source | connected and JIP lifecycle; host destroy owns end |
| `morningGay`, `borgRozital`, `eggvasion` | respectively `morningUfo_C`, `rozitBorg_C`, `superEgger_C` | ACTOR_MIRROR | WorldActor lane; exact host class/transform, no client reroll | connected and JIP lifecycle; host destroy owns end |
| `fallbody_0`, `fallbody_1`, `fallcar_0`, `graystank` | host-selected `ufoDropper_body/car/tank/pig_C` craft variants | ACTOR_MIRROR | WorldActor lane; `ufoDropper_pig_C` and EX source added this pass | connected and JIP craft lifecycle; separately dropped payload uses its normal prop/vehicle lane where supported |
| `picnic`, `destroyPicnic`, `enasus`, `enacros`, `cookier`, `paperGray`, `arirEgg` | concrete prop placement/removal/arming | PROP_MIRROR | PropSpawn/PropDestroy/PropPose plus keyed save state | connected lifecycle; prop snapshot/save supplies JIP; PropDestroy closes it |
| `food`, `drive`, `poisonFood`, `expDrive`, `cookiebox`, `trashPiles`, `vaccine`, `oil`, `begos`, `gascans`, `bombBox` | host-created concrete prop/result actors | PROP_MIRROR | prop lifecycle and pose lanes | connected and JIP for enrolled props; destroy lane closes them; controller-only cues remain unclaimed |
| `atvFuel`, `atvFix`, `atvExplode` | ATV fuel/condition/trap result | RESULT_SYNC | ATV authoritative state lane | connected state convergence and JIP current state; later native state supersedes it |
| `vehtp` | authoritative ATV transform | RESULT_SYNC | ATV lane | connected and JIP current vehicle pose/state; no event replay |
| `console`, `lightswitch`, `keypadGuess` | device interaction result | RESULT_SYNC | existing console/light/door device lanes | connected and JIP according to each device's state owner; no client prank reroll |

The twelve dual-dispatch names (`falseEnter`, `fakeGrays`, `paperGray`, `vent`,
`mann`, `crys`, `bedEvent`, `susArir`, `vehtp`, `agrav`, `arirFollower`, and
`arirEgg`) retain the same output verdict regardless of which dispatcher called
them; they are counted once in the 96-name total.

### Dispatcher actions: partial, authority-only, or unknown

| Exact event names | Meaningful output(s) | Classification/current owner | Exact remaining gap |
|---|---|---|---|
| `agrav` | generator break; exact lifted props; craft cloak/light/audio; conditional `addAriralRep(15)` | RESULT_SYNC + PER_PLAYER + TRANSIENT_CUE via `GeneratorBreakState`, `AgravState`, and presentation reconciliation | reputation write remains RESULT_SYNC gap; concurrent controller presentation unsupported |
| `arirShip` | authored overlap arm; ship, alarm lamp, possible NPC leaves | ACTOR_MIRROR / RESULT_SYNC | later overlap spawn source and alarm/result coupling are not enrolled by the eventer EX source |
| `tentacleBalls` | activates the level-placed `tentacleBallsFollower_C` via `runTrigger` | RESULT_SYNC | placed-controller phase/state and cues; spawning a duplicate WorldActor is explicitly not a solution |
| `arirBuster` | `arirBusterSpawner_C` and its selected products | ACTOR_MIRROR / UNKNOWN | controller/product census and lifecycle lane |
| `soltoClean` | `soltomiaCleaning_C` plus exposed `doorJam2`/`doorJam3` references | ACTOR_MIRROR + birth state | the two references are written between Begin/Finish and are not carried; the new EX catch deliberately excludes this pair |
| `graysforest` | `grayEventController_C`, grays and encounter state | ACTOR_MIRROR + RESULT_SYNC | controller phase plus exact creature births/deaths |
| `boarwar` | `boarInvasion_C`, boars, long-lived phase/timer | ACTOR_MIRROR + RESULT_SYNC | boar identity/pose, current phase on join, and 90-minute cleanup |
| `salt` | `saltpile_C` persistent save actor | RESULT_SYNC / ACTOR_MIRROR | reconcile against save-loaded twin; generic fresh actor mirror risks duplication |
| `rozitalHole`, `dreambase` | persistent controller/save actors and child pivots | RESULT_SYNC / ACTOR_MIRROR | current phase/children plus save-twin adoption and cleanup |
| `rockThrow`, `hillRoller`, `alienJump`, `trashBase` | controller birth; selected thrown food/rocks/trash; local audio/overlap behavior | PROP_MIRROR + ACTOR_MIRROR + TRANSIENT_CUE | child props may enroll normally, but controller lifecycle/cues and exact output census are not complete |
| `alienSounds` | three `noiser_C` actors and alien audio | ACTOR_MIRROR + TRANSIENT_CUE | NPC pose alone would not reproduce its timer/audio; needs an authored sound/result design |
| `earthTp` | teleport of the intended triggering player | PER_PLAYER | current host replay/pose behavior does not prove the intended remote-player recipient semantics |
| `bedEvent`, `treehouseSleep` | player sleep/dream/teleport presentation | PER_PLAYER | remote recipient, phase, and reconnect semantics require targeted sleep-state evidence |

### Non-eventer selectors/controllers and each output family

| Producers (Table A rows) | Outputs | Classification and exact lane | Remaining output/JIP/end result |
|---|---|---|---|
| `timerRain`, `permaRain_timer` | rain enabled/amount/timing | RESULT_SYNC: `WeatherState` | connected and JIP current state; later host weather state ends it |
| `timerLightning` | exact strike occurrence/location and cue | RESULT_SYNC + TRANSIENT_CUE: `LightningStrike` | connected strikes; intentionally no JIP replay for an expired strike |
| `fogEvent`, `superFogEvent` | fog state/controller | RESULT_SYNC: fog/weather modules | connected and JIP current fog; host clear ends it |
| `spawnRedSky` / `redSkyEvent_C` | red-sky state and controller birth | RESULT_SYNC: `RedSky`; client organic BeginPlay suppressed | connected and JIP current state; host clear/destroy ends it |
| `spawnBlackFog` / `blackFog_C` | local black PP; ambience/bird suppression; whispers/thumps; reverb; guarded eyer output | SAFE_CLIENT_REPLAY + RESULT_SYNC + PER_PLAYER; concrete eyer uses OwnerEntity if one occurs | connected BEGIN materializes the shipped local presentation; JIP applies current alpha + fade direction; END forces `set(0)` then destroys; client `spawnGhost` RNG is cancelled |
| `Spawn Bad Sun` / `badSun_C` | sky/light/fog mutation and siren/audio | RESULT_SYNC + TRANSIENT_CUE | EventAuthority metadata only; client presentation, current phase, and cleanup are missing |
| `event_fleshRain_C` | `prop_garbageClump_C`; two local camera shakes; two 2D sounds | PROP_MIRROR + PER_PLAYER + TRANSIENT_CUE | clump `Init` feeds the existing prop lane; shake/sound are missing and are not replayed on JIP after expiry |
| `event_fossilBoarWar_C` | `fossilhound_C`, `grayboar_C`, camera shake, sound | ACTOR_MIRROR + birth state + PER_PLAYER + TRANSIENT_CUE | fossilhound receives a pre-Finish float property, so both actors/cues/current phase remain missing rather than creating an incorrect generic mirror |
| `ufo_midas` | selected target controller `runTrigger` | RESULT_SYNC / UNKNOWN | exact target and resulting state need targeted BP decode |
| `ufo_pill`, `ufo_boo`, `ufo_joel`, `ufo_ball` | exact UFO actor class and transform | ACTOR_MIRROR: WorldActor | connected/JIP birth+pose+destroy now covered; class-specific fields/cues remain partial |
| `tickerFunguy` | `npc_funguy_C` | ACTOR_MIRROR: NPC | the missing mainGamemode EX source is added; connected/JIP/despawn covered |
| `tickerBody`, `ticker_lockerhead`, `ticker_radiotowerPoof` | `theBody_C`, `lockerCorpse_C`, `radiotowerPoof_C` | ACTOR_MIRROR: WorldActor | connected/JIP lifecycle covered; one-shot local cues not independently guaranteed |
| `CustomEvent_0` / `screamingCorpseController_C` | controller plus `screamingCorpse_C` children | ACTOR_MIRROR + birth state | host controller remains host-only; child `SetStringPropertyByName` in the deferred window is uncarried, so no unsafe generic mirror was added |
| `CustomEvent_3`, `CustomEvent_5` | `figura_C`; `eg_C`/`geomOcta_C` | ACTOR_MIRROR: NPC | connected/JIP lifecycle now covered; per-class cues/fields remain partial |
| `CustomEvent_6`, `CustomEvent_7` | `NewBlueprint5_C`; `NewBlueprint19_C` | ACTOR_MIRROR + birth state | each has a deferred-window `SetBoolPropertyByName`; no mirror until the exact birth field is represented |
| `ticker_lakeMonsert` | level `lakeglow.setActive` phase/pull effect | RESULT_SYNC + PER_PLAYER | no state lane, JIP phase, or proved end reconciliation |
| deer/tree/gost/eg ticker rows | `deer_C`, `walkingTree_C`, `poolwalker_C`, `eg_C` | ACTOR_MIRROR: NPC | connected/JIP/despawn now covered by exact EX sources |
| tick ticker row | `NewBlueprint17_C` plus deferred bool/string inputs | ACTOR_MIRROR + birth state | output remains missing until both pre-BeginPlay fields are represented |
| hexahive ticker | `hexahiveSPawner_C` then generated hive state | ACTOR_MIRROR + RESULT_SYNC | controller/child generation and cleanup remain missing |
| mannequin ticker | `prop_wMannequin_C` | PROP_MIRROR | connected/JIP/destroy covered by prop lane |
| bush ticker / `growingPlant_C` | persistent plant actor, growth/fruit/save state | RESULT_SYNC + ACTOR_MIRROR | no authoritative growth/state lane or save-twin convergence |
| beehive ticker / `beehiveBranch_C` | persistent branch/bees/save state | RESULT_SYNC + ACTOR_MIRROR | no authoritative state/child lane or save-twin convergence |
| `triggerTimer.newMinute` | target-specific `runTrigger` | UNKNOWN per target | selection is host-only; each concrete target still needs enumeration and an output owner |
| `triggerFallingSky` / `skyFallingEvent_C` | falling-sky actor, sky/particle/audio/player effects | ACTOR_MIRROR + TRANSIENT_CUE / UNKNOWN | host puppet preserves remote triggering; output phase and cleanup remain unproved |
| `grayBoarSpawner.ReceiveTick` | gray-boar encounter and combat cleanup | UNKNOWN | selector seam itself remains UNKNOWN; do not classify it safe or disable the mixed Tick |
| `ticker_bp7Spawner`, `ticker_susHoleSpawner`, `ticker_eyers`, `ticker_fireflySpawner`, `CustomEvent_1/2/4` | player/camera-relative actors, emitter, UI/footstep/hallucination | PER_PLAYER / PRESENTATION_RANDOM | intentionally local; not shared-world output and no host result lane required |
| server breaker, roach master/ticker, sky/yellow wisps | servers; roach population; exact wisps | RESULT_SYNC / ACTOR_MIRROR | `ServerBoxState`, `RoachState`, NPC lanes already provide connected/JIP/current cleanup |

### Implementation added by this pass

The native deferred-spawn UFunction has a second observation path for Blueprint
`EX_CallMath`. Previously that path accepted only four broad source classes, so
an allowlisted event product could still be invisible. This pass adds **24 exact
source+product pairs** for `trigger_eventer_C`, `mainGamemode_C`,
and the deer/tree/gost/eg tickers. The concrete output
must also match the curated NPC or WorldActor allowlist. Pair gating is
intentional: a broad `mainGamemode_C` row would accidentally enroll save-loaded
antibreathers as transient actors, and the `trigger_eventer_C ->
soltomiaCleaning_C` pair has uncarried exposed-on-spawn references.
The apparently easy `event_fossilBoarWar_C -> fossilhound_C` pair was also
excluded because that graph writes a float property before FinishSpawning.

New NPC products are `deer_C`, `figura_C`, `eg_C`, `geomOcta_C`,
`walkingTree_C`, and `poolwalker_C`. New WorldActor products are
`ufoDropper_pig_C`, `theBody_C`, `lockerCorpse_C`, `radiotowerPoof_C`,
`ufo_pillfo_C`, `ufo_boofo_spawn1_C`, `ufo_joel_C`, and `ufo_ballfo_C`.
`NewBlueprint5_C`, `NewBlueprint19_C`, `NewBlueprint17_C`, and
`screamingCorpse_C` were deliberately excluded after the deferred-window audit
found uncarried bool/string writes before `FinishSpawningActor`. Parent classes
were verified from the extracted cooked exports before choosing a lane: the
first admitted set are `Character`; the second are `Actor` (or an existing
Actor-derived allowlisted family).

No payload or reliable kind changed, so protocol remains **65006**. Existing
NPC/WorldActor sender-slot rules remain host-only. Both lanes already provide
live-actor join snapshots, monotonic host EIDs, streamed transforms, and exact
destroy cleanup. This closes entity lifecycle, not every internal property or
one-shot cue; those residuals remain explicitly PARTIAL above.

### Active instance correlation and short-lived events

`event_active_sync` normally creates instances from the live
`activeEvents_senders` registry. The one proven earlier lifetime seam is
`mainGamemode.blackFog`: its exact controller pointer is admitted at birth,
then correlated with that same pointer when the shipped graph later registers
at alpha 0.9. Other selector gates do not create instances. An
exact post-watch on `lib_C::setEvent` immediately diffs that same registry, so
an ON/OFF event cannot live wholly between 250 ms reconciliation polls. A later
poll of the same sender cannot double-BEGIN it because the map already contains
the concrete sender pointer; the entry also records and validates its
`InternalIndex`. END uses the exact instance ID. Concurrent same-class senders
therefore remain distinct, repeated instances receive new monotonic IDs, and a
class without a `list_events` mapping remains valid with an empty row.

`AgravState` adds reliable kind 134 and an 80-byte host-only payload. Protocol
is 65006; EventAuthority and AgravState both use the ordered Normal lane.

### Output-status counts

Using the 22 grouped rows in Table B as the accounting unit (so one grouped row
is not inflated by every named variant), the current matrix is:

- **COMPLETE: 6** -- scheduled safe rows, weather rain/fog/lightning, red sky,
  black fog, funguy, and mannequin lifecycle.
- **PARTIAL: 6** -- prank results, UFO actors, body/locker/radiotower, the
  figura/eg group, deer/tree/gost/eg, and `agrav`.
- **OUTPUT-MISSING: 8** -- bad sun, flesh rain, fossil boar war, lake monster,
  screaming/rare actors, tick ticker, hexahive, and bush/beehive.
- **UNKNOWN: 2** -- timed-trigger targets and falling-sky output semantics.

These counts measure meaningful output coverage, not merely selection safety
or actor birth. `grayBoarSpawner_C::ReceiveTick` remains a producer-authority
UNKNOWN outside the 22 Table B output groups.
