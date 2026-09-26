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
