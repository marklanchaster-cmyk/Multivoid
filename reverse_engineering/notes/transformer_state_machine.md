# Transformer / generator Blueprint state machine

Date: 2026-09-24  
Scope: cooked Blueprint/Kismet bytecode and current Multivoid source only. No native Ghidra sweep was used for this report.

Status: **READY FOR RUNTIME VALIDATION**. The state transitions and side-effect
paths are statically established; the remaining questions are runtime-instance
facts listed at the end of this report.

## Evidence boundary

The primary sources are `research/bp_reflection/generator.json` and
`research/bp_reflection/transformerMGPanel.json`. For control-flow recovery, the
same already-extracted, read-only cooked assets were rendered into reusable text
CFGs under [`reverse_engineering/exports/kismet-cfg/`](../exports/kismet-cfg/).
Offsets below are Kismet bytecode offsets within the named function, not native
addresses or file offsets.

To find external callers/listeners, all 2,513 already-extracted `.uasset` files
were searched for the relevant names, then every asset that also references
`generator_C` was narrowed to its Kismet CFG. Seventeen assets matched that
dependency test. This establishes direct statically named Blueprint calls and
delegate bindings in the extracted corpus; a runtime call made only through a
string/reflection mechanism would not be visible to that test.

## Result in one diagram

```text
WORKING (generator.isBroken == false, normally cycle > 0)
  |
  | generator.damage() events: cycle := cycle - 1
  | when the new value is <= 0, damage() calls generator.break()
  | OR an explosion/addDamage/scripted event calls break() directly
  v
BROKEN (generator.isBroken == true, cycle == 0)
  |
  | player changes panel controls while isBroken is true
  | setKnobs/setSwitches/setRotators recompute three completion booleans
  v
REPAIRING (logical activity only; there is no separate Blueprint state flag)
  |
  | generator.actionOptionIndex(action == 4) reads all three booleans
  | and, when all are true and isBroken is true, performs the inline repair
  v
WORKING (isBroken := false, cycle := 100, update())
```

`generator_C.fullFix` is **not** the normal human puzzle-completion edge. It is
a game-supported force-repair/convergence operation: the shipped Kerfur Omega
repair flow calls it from an animation notify. The only direct external
Blueprint caller found is that Kerfur Omega path.

## Every write to `generator_C.isBroken`

| Function / entry point | Bytecode write | Assigned value | Conditions and nearby behavior | Evidence |
|---|---:|---|---|---|
| `generator_C::break` | destination `20`, value `29` | `true` | Offset `5` first reads `isBroken`; an already-broken generator returns. Otherwise: set true, play `stationTurnoff`, set `cycle = 0`, call `update`, then `panelObj.randomizeTargets` and `randomizeValues`. No `turnedOff`/`broken` delegate is broadcast. | [`generator.txt`](../exports/kismet-cfg/generator/generator.txt), `FunctionExport break`, offsets 0-199 |
| `generator_C::loadData` | destination `162`, expression `171` | saved `bools[0]` | Persistence restoration. It later restores `index`, `cycle`, `upgradeLevel`, and `cyc`, then calls only `updUpgrades`. This is not the gameplay break transition. | `generator.txt`, `FunctionExport loadData`, offsets 5-811 |
| `ExecuteUbergraph_generator`, entry `actionOptionIndex = 1579` | destination `1154`, value `1163` | `false` | Requires `action == 4`, the activation/look-at path, all three panel completion booleans true, and current `isBroken == true`. Execution order is: broadcast `turnedOn` at 1281, then write false, write `cycle = 100`, call `update`, then optionally invoke `triggerWhenCompleted.runTrigger(self, 0)`. | `generator.txt`, blocks 1579, 3044, 3200, 1271/1276/1281, 1153, 1301-1458 |
| `generator_C::fullFix` | destination `782`, value `791` | `false` | After forcing panel sine/switch/rotator values to their targets, it writes `cycle = 100`, clears `isBroken`, broadcasts `turnedOn`, and calls `upd`. It does not call `update` and does not run `triggerWhenCompleted`. | `generator.txt`, `FunctionExport fullFix`, offsets 5-900 |

There are no other `isBroken` destinations in `generator_C` bytecode. In
particular, the normal gameplay assignment of `true` is centralized in
`generator_C::break`; other gameplay events reach that function rather than
writing the property themselves.

## Every write to `generator_C.cycle`

| Function / entry point | Bytecode write | Assigned value / expression | Conditions and immediate calls | Evidence |
|---|---:|---|---|---|
| `generator_C::break` | destination `99`, value `108` | `0` | Only when not already broken. After the write it calls `update`, then randomizes panel targets and values. | `generator.txt`, `break` 0-199 |
| `generator_C::damage` | destination `126`, value `135` | `cycle - 1` | Only if old `cycle > 0`. If new value `<= 0`, calls `break` at 192; otherwise calls `panelObj.randomizeSines` at 233. Intermediate damage does **not** call `update`/`upd`. | `generator.txt`, `damage` 0-248 |
| `ExecuteUbergraph_generator`, `actionOptionIndex = 1579`, broken branch | destination `1173`, value `1182` | `100` | All three completion flags true and `isBroken == true`; immediately follows `isBroken = false`, then calls `update`. | `generator.txt`, blocks 3044, 3200, 1153 |
| `ExecuteUbergraph_generator`, `actionOptionIndex = 1579`, already-working branch | destination `1468`, value `1477` | `100` | All three completion flags true but `isBroken == false`; then plays the `turnon` sound and calls `panelObj.updateAll`. It does not broadcast `turnedOn` or call `update`/`upd` on this branch. | `generator.txt`, blocks 3044, 3200, 1459 |
| `generator_C::loadData` | destination `472`, expression `481` | saved integer entry 1 | Persistence restore. `getData` stores integer fields in the order `[index, cycle, upgradeLevel]`. | `generator.txt`, `loadData` 335-636 and `getData` |
| `generator_C::fullFix` | destination `767`, value `776` | `100` | After panel force-completion; followed by `isBroken = false`, `turnedOn`, and `upd`. | `generator.txt`, `fullFix` 758-825 |

`cycle` is not continuously ticked. Its mutation is event-driven: `damage()`
subtracts exactly one per call, `break()` forces zero, and repair/load paths set
or restore it. One normal source of periodic *damage attempts* is
`generatorFuckuper_C`: BeginPlay installs a repeating 30-second
`timer_transformers`; if `gameRules.enableTransformerDecay`, generator selection,
`lib.isGeneratorsFine`, `cycle > 0`, `powerRatio`, upgrade-weight, and day-weight
gates pass, it calls the selected generator's `damage` at offset 1068. Thus the
timer is periodic, but actual `cycle` changes are discrete and probabilistic.
The miner also has a 0.15-weight path that chooses a random generator and calls
`damage` at offset 1284.

Evidence: [`generatorFuckuper.txt`](../exports/kismet-cfg/generatorFuckuper/generatorFuckuper.txt),
BeginPlay offsets 15-209, `timer_transformers` entry 1743, damage path 374-1082;
[`prop_miner.txt`](../exports/kismet-cfg/prop_miner/prop_miner.txt), offsets 1146-1298.

## Calls to `fullFix`, `upd`, and `update`

### `fullFix`

- No call exists in `transformerMGPanel_C`.
- No normal human generator puzzle-completion path calls it.
- The only direct external Blueprint call found is
  `kerfurOmega_C::OnNotifyBegin_E233AD08424FBDA61025C2915B5291B0`. The wrapper
  enters `ExecuteUbergraph_kerfurOmega` at 5547, reaches a zero-second latent
  continuation at 2747, plays `break_drive_cue`, then calls
  `transformer.fullFix` at 2865. Evidence:
  [`kerfurOmega.txt`](../exports/kismet-cfg/kerfurOmega/kerfurOmega.txt), blocks
  5631, 2747, and the notify wrapper.
- This caller is material semantic evidence: `fullFix` is the game's own
  supported non-puzzle repair verb, not a debugging-only convenience.
- Current Multivoid is an additional runtime/native caller via reflected
  `CallNoArg(actor, L"fullFix")`; that is not a game Blueprint call site.

### `generator_C::upd`

Direct self-call sites in `generator_C` are:

| Caller | Call offset | Purpose |
|---|---:|---|
| `ExecuteUbergraph_generator`, `ReceiveBeginPlay = 881` | 891 | Initial state/power/panel refresh. |
| `ExecuteUbergraph_generator`, delayed `intComs_gamemodeBeginPlay = 4167` path | 535 | Refresh after gamemode/panel initialization. |
| `generator_C::update` | 179 | State-change refresh after choosing/activating the on/off sound. |
| `generator_C::fullFix` | 811 | Force-repair refresh without the `update` audio wrapper. |

At `upd` offset 73 there is another method named `upd`, but its receiver is
`gamemode.generatorCond`; it is not a recursive generator call. No direct
external Blueprint call to `generator_C.upd` was found in the 17-asset
generator-reference closure.

### `generator_C::update`

There are two direct call sites:

| Caller | Call offset | Context |
|---|---:|---|
| `generator_C::break` | 113 | After `isBroken = true`, shutdown sound, and `cycle = 0`. |
| `ExecuteUbergraph_generator`, `actionOptionIndex = 1579` | 1187 | Normal solved-puzzle repair, after `isBroken = false` and `cycle = 100`. |

`update` chooses `turnoff` or `turnon` from `isBroken`, sets and activates the
`turnon` AudioComponent, then calls `upd` at 179. No direct external Blueprint
call to this generator method was found.

## Transition details

### WORKING -> BROKEN

Authoritative function: `generator_C::break`.

- Reads: `isBroken` guard.
- Writes: `isBroken = true` at 20/29; `cycle = 0` at 99/108.
- Immediately invokes: `update` at 113, then
  `panelObj.randomizeTargets` at 149 and `panelObj.randomizeValues` at 185.
- Through `update -> upd`, invokes: `updUpgrades`, `generatorCond.upd`, the
  broken branch `powerControl.solar`, `powerControl.sendPower`,
  `panelObj.updateAll`, and finally broadcasts `updated`.
- Direct dispatcher: none in `break`; the common `updated` dispatcher is emitted
  by `upd` after all refresh work.
- Likely authoritative object: `generator_C`; it owns both canonical fields and
  the transition function. The panel is randomized as a consequence.

Proven callers/causes in the targeted dependency closure include:

- `generator_C::damage` when the decremented cycle is `<= 0` (offset 192).
- Generator `exploded` entry 3328 and `addDamage` entry 3247, both direct calls.
- A TV-remote hit path inside `actionOptionIndex` reaches `break` at 3029.
- `generatorFuckuper_C`'s decay path calls `damage` at 1068.
- `prop_miner_C` calls a random generator's `damage` at 1284 after a 0.15 gate.
- `grayEventController_C` binds `turnedOn`, then initially calls
  `transformer.break` at 3189.
- `lightningStrike_C` calls a hit generator building's transformer `break` at
  3616, or a random generator's `break` at 4031.
- `trigger_fuckUpTransformer_C::runTrigger` selects its indexed generator and
  calls `break` at 113.
- `trigger_agrav_C` chooses a random generator and calls `break` at 403.
- `trigger_eventer_C::runEvent`, on the `tentacleBalls` named branch, calls
  `gamemode.generators[2].break` at 6602.
- `ufoDropper_pig_C` and `ufoDropper_tank_C` each call
  `gamemode.generators[0].break` at ReceiveBeginPlay offset 119.

The regular decay mechanism is the `damage`/30-second-timer path. The others are
direct damage, event, or scripted forced-break paths. All converge on the same
canonical `break` implementation.

### BROKEN -> REPAIRING

There is no explicit `REPAIRING` enum or boolean in these Blueprints. This is a
logical state meaning "the generator remains broken while the player changes
panel values."

- `clicked_rotataors` enters the panel Ubergraph at 3757 and immediately requires
  `transformer.isBroken`. If `isMoving` is false, it increments the selected
  `rotators_states` byte modulo 4 and calls `moveRotator` at 4065.
- `moveRotator` entry 4677 also rejects while `isMoving`; it plays audio, sets
  `isMoving = true`, animates for 0.2 seconds, then continuation 56 clears
  `isMoving` and calls `setRotators` at 67.
- `clicked_switchers` enters at 4090 and likewise requires
  `transformer.isBroken`. If not moving, it toggles the selected
  `switches_states` element and calls `moveSwitch` at 4341.
- `moveSwitch` entry 4365 animates for 0.1 seconds; continuation 16 clears
  `isMoving`, calls `setKnobs` at 27, then `setSwitches` at 41.
- Slider/knob motion updates sine values and calls `setKnobs`; `releaseSlider`
  entry 5186 clears the knob-button chain and uses `TickUntilStop`. The completion
  boolean is recomputed by `setKnobs`, not by a repair call in `releaseSlider`.

These interactions do not write generator `isBroken` or `cycle`, do not invoke
`fullFix`, and do not broadcast a generator delegate.

Transition record:

- Properties read: `transformer.isBroken`, `isMoving`, the selected live panel
  value, and the applicable target data.
- Properties written: `rotators_states`, `switches_states`, sine live values,
  `isMoving`, and (through the setters) `isRotatorsComplete`,
  `isSwitchesComplete`, or `isSineComplete`.
- Functions invoked: `moveRotator -> setRotators`, `moveSwitch -> setKnobs ->
  setSwitches`, and sine motion -> `setKnobs`.
- Dispatchers broadcast: none in the panel completion paths.
- Likely authoritative object: the panel owns the in-progress puzzle data, but
  `generator_C.isBroken` remains the gate and canonical broken state.
- Evidence: `transformerMGPanel.txt`, Ubergraph entries
  3757/4090/4365/4677/5186 and the three setter functions.

### REPAIRING -> WORKING: normal puzzle completion

The three panel conditions do **not** converge inside
`transformerMGPanel_C`. They converge in the generator's activation handler:

1. `generator_C::actionOptionIndex` entry 1579 requires `action == 4` and the
   activation/look-at route.
2. At 3044-3186 it computes
   `(panelObj.isRotatorsComplete && panelObj.isSineComplete) &&
   panelObj.isSwitchesComplete`.
3. False plays `denied` at 3205.
4. True reads `generator.isBroken` at 1262.
5. On the broken branch it broadcasts `turnedOn` at 1281, then writes
   `isBroken = false` at 1154, writes `cycle = 100` at 1173, calls `update` at
   1187, and optionally calls `triggerWhenCompleted.runTrigger(self, 0)` at
   1438.

Important ordering fact: on this normal path, `turnedOn` is broadcast **before**
the state writes. In `fullFix`, it is broadcast **after** the writes.

The authoritative repaired state is the generator's `isBroken`/`cycle`; the
panel booleans are the gate that permits the normal transition. They are not
discardable while a player is repairing. After completion, generator `upd`
calls `panelObj.updateAll`, so panel presentation follows generator refresh.

Transition record:

- Properties read: all three panel completion booleans, `generator.isBroken`,
  and validity/type of `triggerWhenCompleted`.
- Properties written: `generator.isBroken = false` and `generator.cycle = 100`.
- Dispatchers broadcast: `turnedOn` directly, then `updated` indirectly through
  `update -> upd`.
- Functions invoked: `update -> upd`, all `upd` power/panel/indicator children,
  and optionally `triggerWhenCompleted.runTrigger(self, 0)`.
- Likely authoritative object: `generator_C`; the panel supplies the completion
  predicate but does not perform the transition.
- Evidence: `generator.txt`, `actionOptionIndex` entry 1579 and blocks
  3044-3246, 1271-1458, 1153-1201.

## Panel completion writes

| Property | Sole write site | Assigned expression | How it is reached |
|---|---:|---|---|
| `isSineComplete` | `transformerMGPanel_C::setKnobs`, destination 2753/value 2762 | `(sine_offset == targetSine_offset) && (sine_frequency == targetSine_frequency) && (sine_amplitude == targetSine_amplitude)` | Recomputed after knob/slider changes and from refresh paths. `moveSwitch` also calls `setKnobs` before `setSwitches`. |
| `isSwitchesComplete` | `setSwitches`, destination 671/value 680 | `!compareArray.Contains(false)`, where the eight entries compare `switches_states[0..7]` with bits 1..8 of `switches_target` | Recomputed after the switch animation completes and by panel refresh. |
| `isRotatorsComplete` | `setRotators`, destination 29/value 38 | return value of `checkColors` | Recomputed after the rotator animation completes and by refresh paths. |

Evidence: [`transformerMGPanel.txt`](../exports/kismet-cfg/transformerMGPanel/transformerMGPanel.txt),
`setKnobs` 2562-2762, `setSwitches` 28-680, `setRotators` 5-38, and Ubergraph
entries 3757/4090/4365/4677/5186.

## `fullFix`, `upd`, and power effects

`fullFix` force-copies all three sine target values into their live values,
rebuilds `switches_states` from `switches_target`, and overwrites every
`rotators_states` element with the function's temporary byte value. It then
sets `cycle = 100`, clears `isBroken`, broadcasts `turnedOn`, and calls `upd`.
The CFG does not show an explicit initializer for that temporary rotator byte,
so this report does not claim a numeric value for it.

`generator_C::upd` performs the following ordered fan-out:

1. `updUpgrades`.
2. `gamemode.generatorCond.upd`, which refreshes each generator's red/green
   particle indicator from its `isBroken` value.
3. Branch on `isBroken`:
   - broken: `powerControl.solar`, then `powerControl.sendPower`;
   - working: `powerControl.buttonsVisibility`, then `powerControl.sendPower`.
4. `panelObj.updateAll`.
5. broadcast `updated`.

On the broken branch, `powerControl.solar` clears `press_downl`, `press_coord`,
`press_play`, `press_calc`, and `press_light`, calls `buttonsVisibility`, runs
the light-root triggers with index 2, deactivates light roots, and applies
`setActiveTrigger(false)` to base sockets. `sendPower` calls
`gamemode.setPower` with the five press flags, runs the light-root triggers,
sets light roots active according to `press_light`, and applies
`setActiveTrigger(press_light)` to each base socket.

Therefore both normal `break -> update -> upd` and normal repair
`update -> upd` change global/world power-manager state. `fullFix -> upd` also
produces the power, indicator, panel, and `updated` side effects automatically.
Those operations should not be separately replayed if `fullFix` successfully
runs. What `fullFix` omits relative to normal player completion is the
`update` audio wrapper and the optional `triggerWhenCompleted` call.

Evidence: `generator.txt`, `upd` 5-391 and `fullFix` 758-825;
[`powerControl.txt`](../exports/kismet-cfg/powerControl/powerControl.txt),
`solar` 0-1113 and `sendPower` 0-1141.

## Delegates and listeners

`generator_C` declares only `turnedOn` and `updated` multicast dispatchers in
the relevant state logic. No corresponding `turnedOff`, `broken`, or `failure`
dispatcher was found. Failure notification uses `update -> upd -> updated`.

Static direct listeners found:

- `grayEventController_C` binds its local `turnedon` event to
  `transformer.turnedOn` at 3103-3158. The listener entry 3388 enables
  `triggerbox` collision. The controller initially calls `transformer.break` at
  3189. This is a gameplay-affecting re-arm listener, not merely UI.
- `generatorBuilding_C` binds `trUpdated` to `transformer.updated` at 785-840.
  Listener entry 5854 assigns `noPower = transformer.isBroken`, then enters the
  shared collision-refresh block at 849, which changes the building zapper's
  collision mode. This is also gameplay-affecting.

Evidence: [`grayEventController.txt`](../exports/kismet-cfg/grayEventController/grayEventController.txt)
and [`generatorBuilding.txt`](../exports/kismet-cfg/generatorBuilding/generatorBuilding.txt).

## GAME DOES vs MULTIVOID CURRENTLY DOES

| GAME DOES | MULTIVOID CURRENTLY DOES |
|---|---|
| `damage()` decrements `cycle`; the scheduled decay path may do this every 30 seconds after probabilistic gates. | Does not read, send, or reconcile `cycle`. |
| `break()` is the authoritative working-to-broken operation: writes true/zero, runs `update -> upd`, then randomizes puzzle targets and values. | Does not detect or send working-to-broken transitions. Its 100 ms poll only sends when repaired changes from false to true. |
| Panel switch/rotator/sine interactions mutate live panel state and recompute three completion booleans. | Sends no panel interactions, panel arrays/targets, or completion flags; protocol comments explicitly treat puzzle internals as local/cosmetic. |
| Ordinary puzzle completion happens inline in `actionOptionIndex`: `turnedOn`, state writes, `update`, optional completion trigger. | Detects the resulting `isBroken: true -> false` edge and sends a reliable `RepairOutcome { target=3, repaired=1, portable key }`. It does not identify which game path caused it. |
| The shipped Kerfur Omega repair path uses `fullFix` as the game's force-repair/convergence operation. | On an accepted generator repair outcome, calls reflected `fullFix` on the target generator. |
| Successful `fullFix` normalizes panel values and automatically runs power, indicator, panel, and `updated` side effects through `upd`. | Correctly does not replay those child operations separately. |
| Normal puzzle completion calls `update` and may run `triggerWhenCompleted`; `fullFix` does neither. | Replays `fullFix`, so remote peers do not reproduce those two normal-completion details. |
| If the preferred force-complete call failed, no game path is equivalent to only clearing one bool. | Fallback writes raw `isBroken = false` and calls `upd`; it does not set `cycle = 100` or complete panel state. |
| `turnedOn` and `updated` have gameplay listeners. | Relies on `fullFix`/`upd` to emit them on repair, which is correct for those two dispatchers. |

Current-code evidence: [`repair_sync.cpp`](../../src/votv-coop/src/coop/interactables/repair_sync.cpp)
lines 31, 51-55, 180-229, 250-269, 279-369; and
[`protocol.h`](../../src/votv-coop/include/coop/net/protocol.h) lines 2855-2859
and 4277-4286.

## MULTIVOID IMPLICATIONS

These are design implications only; no synchronization code was changed.

| Transition | Safest likely network operation | Why / remaining caveat |
|---|---|---|
| WORKING -> BROKEN by direct event | Execute `generator_C.break` on the host, then replicate/converge the host's resulting randomized panel target/live state | A raw property write misses power, delegates, collision, indicators, and randomization. **Do not independently call `break` on every peer:** each call runs `randomizeTargets`/`randomizeValues` and can create different puzzles. |
| WORKING -> BROKEN through decay | Authoritatively replicate the discrete `damage` events or authoritative `cycle`, and use `break` for the zero-crossing; also synchronize the random panel result at break | Raw `cycle` alone misses `randomizeSines` on nonterminal damage and the entire break fan-out. Blindly replaying random operations may diverge RNG results. |
| BROKEN -> REPAIRING | Prefer replaying authoritative interactions if shared puzzle motion matters; otherwise replicate complete panel live/target arrays plus recomputed flags | Replicating only the three booleans would not reproduce control positions, targets, or visuals and would be easy to spoof/desynchronize. |
| REPAIRING -> WORKING, exact normal completion | Replay the generator activation after authoritative panel completion, or explicitly reproduce the inline sequence including optional trigger | There is no dedicated normal `repair()` UFunction. `fullFix` reaches the canonical repaired state but is not bytecode-equivalent to `actionOptionIndex`. |
| REPAIRING -> WORKING, canonical-state convergence only | Calling `fullFix` is the smallest existing Blueprint operation; do not separately replicate `solar`, `sendPower`, `generatorCond.upd`, panel refresh, or `updated` | `fullFix` already performs those via `upd`. Confirm whether missing `update` audio and `triggerWhenCompleted` are acceptable for placed generators. |
| Any emergency fallback | Replicate both `isBroken` and `cycle`, normalize panel state, then call the proper refresh path; a raw bool alone is insufficient | Current fallback can leave `cycle` and panel state inconsistent with `isBroken`. |

## Pending host-authoritative repair design (do not implement before probe review)

Kerfur Omega establishes `fullFix` as a shipped game repair path. That supports,
but does not by itself finish validating, this multiplayer design:

1. A client observes its successful local human-repair edge.
2. The client sends an intent identifying the generator.
3. The host resolves the authoritative generator.
4. The host calls `fullFix`.
5. The host broadcasts the canonical repaired outcome.
6. Each receiving client calls `fullFix` only if its local generator is not
   already converged.

The client-local state change is evidence of an attempted/successful local
interaction, not authority over shared world state. Host resolution, validation,
execution, and broadcast remain the commit point.

After the runtime trace is collected, compare this operation with replaying
`actionOptionIndex(action=4)` using these gates:

- `fullFix` must produce every gameplay-relevant power, indicator, collision,
  panel, `turnedOn`, and `updated` side effect needed on remote peers;
- record whether each normal placed generator has a live
  `triggerWhenCompleted` target;
- determine whether the missing `update` wrapper changes only audio/presentation;
- determine whether omitting `triggerWhenCompleted` suppresses any required
  gameplay transition;
- retain the static fact that Kerfur Omega itself reaches `fullFix` directly and
  therefore structurally omits the human `actionOptionIndex` completion-trigger
  tail. Runtime evidence can establish its effects, but should not speculate
  about designer intent beyond that control flow.

No synchronization change is authorized until those gates are evaluated.

## Remaining unknowns and smallest runtime diagnostic

The Kismet evidence proves the state writes and static calls. The material
remaining unknowns are runtime-instance facts: whether placed generators have a
non-null `triggerWhenCompleted`; whether any delegate is bound dynamically by a
string/reflection-only route; and whether independently executing `break`
produces observably divergent panel random state across peers.

The smallest useful diagnostic is one read-only `ProcessEvent` observer scoped
to `generator_C` and its referenced `powerControl_C`, with no mutation and no
new synchronization behavior. For each matching generator identity, log:

- entry/return for `damage`, `break`, `actionOptionIndex`, `update`, `upd`, and
  `fullFix`;
- before/after `isBroken`, `cycle`, all three completion booleans, sine
  live/target values, switch live/target state, and rotator state;
- `turnedOn` and `updated` broadcasts;
- whether `triggerWhenCompleted` is valid and the class/name of its target;
- calls to `powerControl.solar`, `buttonsVisibility`, `sendPower`, and
  `mainGamemode.setPower` with the five power flags.

One host-side break-to-repair session using the normal player puzzle, followed
by one forced `fullFix`, would confirm the remaining runtime distinctions. No
interaction replay or property write is necessary for that diagnostic.
