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
| shuffled `comps`, gravity off/on, random velocities | 777-835, 915-1393, 1549-1876 | RESULT_SYNC/ACTOR_MIRROR gap; resulting motion coverage is not proven complete |
| cloak/material timeline and camera light | 537-739, 2551-3159 | PER_PLAYER/TRANSIENT_CUE; host-local |
| `arirHover_Cue` on/off | 740-775, 1880-1916 | TRANSIENT_CUE; host-local |
| `photoTaken` -> `addAriralRep(15)` | 2070-2134, 2165-2258 | RESULT_SYNC persistent reputation gap |
| `lib.setEvent(true/false)` | 15-52, 1952-1988 | authoritative membership bookkeeping |

`agrav` deliberately remains NO-replay and is not yet fully presented on
clients. Its transformer result converges; physics, presentation, and the
conditional reputation side effect remain explicit gaps.

## Event output roadmap

Categories may overlap for one event.

### SAFE_CLIENT_REPLAY

Current allowlist: `treehouse_0..5`, `break_RomeoSierra`, `break_Victor`,
`break_Victor2`, `obelisk`, `looker_*-1`/signal/satellite force-object rows,
`solar`, `call0`, scare arms (`toeStab`, `falseEnter`, `mann`, `vent`, `crys`,
`fakeGrays`, `susArir`), and `arirGraff_0..6`.

### RESULT_SYNC

- `agrav`: generator result implemented; physics and reputation remain.
- prank selection: host chooses the concrete prank; prop variants rely on the
  prop lane, while no-lane variants remain gaps.
- Server minigame variant, rare `mainGamemode` rolls, loot RNG, and radar/tower
  shuffle remain targeted rows in `COOP_RNG_AUTHORITY.md`.

### ACTOR_MIRROR

Known covered/no-replay rows include `piramid`, `wisps`, `arirFollower`,
`ventCrawler`, `vehtp`, picnic/prop rows, and prop-lane pranks. Known actor gaps:
`arirShip`, `ventKnocker`, `tentacleBalls`, `morningGay`, `borgRozital`,
`graysforest`, `graystank`, `arirBuster`, `eggvasion`, `boarwar`, `soltoClean`,
`salt`, `rozitalHole`, `dreambase`, `fallbody/fallcar`, `rockThrow`,
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
  exact red-sky/fog mirror echo scope is active. `RedSky` and fog/weather state
  own the covered outputs; black fog remains an output gap.
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
| `mainGamemode_C` | `spawnBlackFog` | new-day roll/start | black-fog controller | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | Unproven | No client output lane. |
| `mainGamemode_C` | `Spawn Bad Sun` | new-day roll/start | bad-sun controller | EVENT_SELECTOR | AUTHORITY_SAFE | exact ScriptGate | Yes (`setEvent`) | No client output lane. |
| `event_fleshRain_C` | `ReceiveBeginPlay` | direct day/night birth | registers, rolls locations, spawns rain results | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact BeginPlay PRE; inert shell removed at FinishSpawn | Yes | Host output is not mirrored completely. |
| `event_fossilBoarWar_C` | `ReceiveBeginPlay` | direct day/night birth | registers, rolls and spawns boars | EVENT_SELECTOR + HOST_RESULT_SELECTOR | AUTHORITY_SAFE | exact BeginPlay PRE; inert shell removed at FinishSpawn | Yes | Host output is not mirrored. |
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
| rain/fog/lightning | day/night weather controllers | host weather schedulers | RESULT_SYNC / TRANSIENT_CUE | `WeatherState`, fog, `LightningStrike` | Covered weather converges | black-fog presentation has no proved lane | FULLY_SYNCED except black fog |
| red sky | `redSkyEvent_C` | host new-day roll | RESULT_SYNC | `RedSky` | Client creates/applies the commanded red-sky state | none known | FULLY_SYNCED |
| bad sun | `badSun_C` | host new-day roll | RESULT_SYNC + TRANSIENT_CUE | EventAuthority metadata only | Client knows an unmapped event may be active, but does not experience it | state/sky/audio lane | AUTHORITY_SAFE_PRESENTATION_INCOMPLETE |
| flesh rain | `event_fleshRain_C` | host direct day/night birth | RESULT_SYNC + ACTOR_MIRROR + PER_PLAYER | EventAuthority metadata; generic prop coverage unproven | No whole-event replay | spawned result and cue coverage | RESULT_SYNC_GAP |
| fossil boar war | `event_fossilBoarWar_C` | host direct day/night birth | ACTOR_MIRROR + TRANSIENT_CUE | EventAuthority metadata only | Client receives activity metadata, not boars/shake | `grayboar_C` mirror plus cue | ACTOR_MIRROR_GAP |
| UFO timer outputs | `ufo_*` controllers | host `mainGamemode` timers | ACTOR_MIRROR / UNKNOWN | none proved for controller classes | Client does not invent a different UFO | host event may be invisible | ACTOR_MIRROR_GAP |
| funguy ticker | `npc_funguy_C` | host `tickerFunguy` | ACTOR_MIRROR | NPC lane | Host spawn/pose/despawn is mirrored | none known | FULLY_SYNCED |
| body/lake monster/locker/radiotower | respective controllers | host main timers | ACTOR_MIRROR / RESULT_SYNC / CUE | none proved | Client no longer selects independently | controller/state/cue visibility | AUTHORITY_SAFE_PRESENTATION_INCOMPLETE |
| screaming corpse / figura / eg / geometric and rare actors | respective classes above | host main timers | ACTOR_MIRROR / PER_PLAYER cue | none proved | Client no longer selects independently | mirror/cue lane per class | ACTOR_MIRROR_GAP |
| ambient mannequin | `prop_wMannequin_C` | host mannequin marker | ACTOR_MIRROR | PropSpawn/PropDestroy | Host-created prop is mirrored | presentation details unverified | FULLY_SYNCED for identity/lifecycle |
| deer/hexahive/tree/tick/gost/eg | classes above | host world tickers | ACTOR_MIRROR | none of these classes is in the NPC/world-actor allowlists | Client no longer invents its own result | exact actor classes need lanes | ACTOR_MIRROR_GAP |
| bush/beehive | `growingPlant_C`, `beehiveBranch_C` | host exact spawn verbs | RESULT_SYNC / ACTOR_MIRROR | none proved | Client no longer invents its own result | persistent actor/state lane | RESULT_SYNC_GAP |
| timed trigger targets | target-specific | host `triggerTimer.newMinute` | UNKNOWN | target-specific | Selection is host-only | audit each concrete target output | UNKNOWN |
| falling sky | `skyFallingEvent_C` | host overlap, including collision-enabled remote `mainPlayer_C` puppet | SAFE_CLIENT_REPLAY not proven | none proved | Client cannot start a rogue local event; its host puppet can drive the authoritative overlap | output/presentation still unproved | UNKNOWN |
| `agrav` | `trigger_agrav_C` | host scheduled event | RESULT_SYNC / PER_PLAYER / CUE | `GeneratorBreakState` for transformer only | Broken transformer converges | physics, rep, hover/cloak/light | RESULT_SYNC_GAP |

The newly gated selectors intentionally received no generic whole-event replay.
Where Table B says gap, the present behavior is **HOST A / CLIENT nothing or
partial output**, which is authority-correct but not presentation-complete.

### Active instance correlation and short-lived events

`event_active_sync` creates instances only from the live
`activeEvents_senders` registry. Selector gates do not create instances. An
exact post-watch on `lib_C::setEvent` immediately diffs that same registry, so
an ON/OFF event cannot live wholly between 250 ms reconciliation polls. A later
poll of the same sender cannot double-BEGIN it because the map already contains
the concrete sender pointer; the entry also records and validates its
`InternalIndex`. END uses the exact instance ID. Concurrent same-class senders
therefore remain distinct, repeated instances receive new monotonic IDs, and a
class without a `list_events` mapping remains valid with an empty row.

No wire layout or `ReliableKind` changed in this pass; protocol remains 65005.
