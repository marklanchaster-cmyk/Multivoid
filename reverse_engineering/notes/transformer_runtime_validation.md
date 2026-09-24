# Transformer runtime validation

This is the hands-on validation companion to
[`transformer_state_machine.md`](transformer_state_machine.md). The probe is
read-only and opt-in. It does not alter transformer synchronization or invoke any
gameplay function.

## Enable, build, and deploy

On **both** test peers, add this to `multivoid.ini` beside that peer's game
executable:

```ini
[dev]
transformer_probe=1
```

Leave the `[dev] enabled` master switch unset or set to `1`.

Build from a Windows developer shell in the repository root:

```powershell
cmake --build build/votv-coop --config Release
```

The result is `build/votv-coop/Release/main.dll`. Deploy the same DLL to every
configured test copy with:

```powershell
.\tools\deploy-all.ps1
```

For a single manual installation, copy `main.dll` to
`VotV\Binaries\Win64\Mods\Multivoid\dlls\main.dll`. Do not replace the game
executable or any cooked asset. Restart both game processes after changing the DLL
or INI.

At session start, require one line per peer resembling:

```text
[transformer_probe] INSTALLED read-only targeted probe role=HOST PE-pairs=10/10 VM-entries=10/10 ...
```

`PE-ENTER`/`PE-EXIT` bracket calls that crossed `ProcessEvent`. `VM-ENTER` catches
the named Blueprint-internal local-virtual call seam (entry only). `BASELINE` is
one initial snapshot per generator, and `EDGE` is emitted only when captured state
changes. The VM and ProcessEvent lines can both describe one call; compare state,
ordering, and identity rather than treating them as distinct gameplay executions.

Every `SNAP` includes:

- `role`, `phase`, `fn`, and `action`;
- `key`, readable `ident`, and `gen` pointer;
- `broken`, `cycle`, and `panel` pointer;
- `complete=S#/W#/R#` for sine/switch/rotator completion;
- `sine=O:live/target,F:live/target,A:live/target`;
- `switches=[...]n=N`, `target=0xNN`, and `rotators=[...]n=N`;
- generator `power` pointer;
- `trigger`, `trigger_populated`, `trigger_live`, `trigger_name`, and
  `trigger_class`.

Use `key` as the cross-process identity. Pointers are intentionally logged to prove
which local instance and panel were involved; pointer equality across processes is
neither expected nor meaningful.

## Controlled setup

Use the same save/world on both peers. Record the target generator's `key` from its
baseline before each test. For Tests B and C, begin with that generator broken on
**both** peers (for example, start both from the same saved broken state). This is
important because the current code does not synchronize the break transition, and
`ApplyRepair` intentionally skips `fullFix` when its local generator is already
repaired.

Keep other generator activity idle during each capture. Split host and client logs
by process and filter on:

```text
transformer_probe|repair_sync\[worldauth\]
```

## TEST A — host performs a normal break

1. Start with the selected generator working on both peers (`broken=0`).
2. On the host, cause that generator to break through ordinary gameplay. Do not
   invoke a Multivoid diagnostic driver.
3. Wait several seconds and capture both logs.

For the same `key`, compare:

| Expected evidence | Host | Client |
|---|---|---|
| Cause | `fn=generator.damage` if decay/damage caused it, or a direct `fn=generator.break` | ordinarily no matching call with current sync |
| Canonical transition | `generator.break` enter has old state; exit/edges reach `broken=1 cycle=0` | determine whether it remains `broken=0` |
| Refresh fan-out | `generator.update`, `generator.upd`, `power.solar`, `power.sendPower` entries where observable | determine whether any appear |
| Puzzle randomization | `panel.randomizeTargets` and `panel.randomizeValues` entries where observable | determine whether any appear |
| Resulting puzzle | compare `panel`, all `complete`, `sine`, `switches/target`, and `rotators` fields | compare the same fields |
| Optional trigger | record `trigger_populated/live/name/class` | compare by the same `key` |

The static expectation is that the host transitions and the client does not. The
runtime result, not that expectation, is the verdict.

## TEST B — host repairs normally through the puzzle

1. Start a fresh session with the selected generator already broken on both peers.
2. On the host, solve the panel normally and activate the generator with action 4.
3. Wait for the reliable repair outcome and capture both logs.

Compare, for one `key`:

| Evidence | Host, normal gameplay path | Client, current replay path |
|---|---|---|
| Completion gate | panel `EDGE` lines end at `complete=S1/W1/R1` | progress need not match before replay |
| Normal completion | `fn=generator.actionOptionIndex action=4`; result `broken=0 cycle=100` | no local normal action expected |
| Network edge | `repair_sync[worldauth]: TX host-outcome target=3 id='<key>'` | matching `RX ... role=client` |
| Replay operation | ordinarily no host `fullFix` for its own already-repaired state | `PE-ENTER fn=generator.fullFix`, followed by `PE-EXIT` |
| Required side effects | `generator.update`, `generator.upd`, power calls; final panel state | `generator.upd`, `power.sendPower`, and the working power branch; final panel state |
| Final convergence | `broken=0 cycle=100` plus final `complete/sine/switches/rotators` | compare the same fields; pointers may differ |
| Known semantic difference | normal path may use `triggerWhenCompleted` and the `update` audio wrapper | `fullFix` is not expected to invoke either |

Also require the client's existing line:

```text
repair_sync[worldauth]: APPLY target=3 id='<key>' called=1 repaired=1
```

## TEST C — client repair exercises host-authoritative `fullFix`

1. Start another fresh session with the selected generator broken on both peers.
2. Solve and activate it normally on the **client**.
3. Capture the client request, host commit/replay, and the host broadcast.

Compare:

| Evidence | Client | Host |
|---|---|---|
| Initiating path | `generator.actionOptionIndex action=4`, final `broken=0 cycle=100` | no normal action expected |
| Network edge | `TX client-request target=3 id='<key>'` | matching `RX ... role=host` |
| Authoritative replay | local actor is already repaired, so no second `fullFix` is required after broadcast | `PE-ENTER/PE-EXIT fn=generator.fullFix` and `APPLY ... called=1 repaired=1` |
| Fan-out | normal `update -> upd` path | forced `fullFix -> upd`, including power calls where observable |
| Commit | receives host outcome; final state remains repaired | `host COMMIT+BROADCAST target=3 id='<key>'` |
| Final fields | `broken=0 cycle=100`; capture all panel and trigger fields | compare all fields to client and to Test B's replay receiver |

The key comparison between Tests B and C is the peer on which `fullFix` actually
ran. Its exit/edge snapshot should converge to the same canonical generator and
panel state as the normally repaired peer, while the trace should retain the known
path difference (`update` and possible completion trigger only on normal completion).

## Decision record after the run

For each test, retain the shortest contiguous log excerpt containing the pre-state,
named calls, network TX/RX/APPLY lines, and final edge. Mark these runtime questions:

- Does normal `break` remain local, as current source predicts?
- Do `solar`/`sendPower` occur under `break -> update -> upd` and does the replayed
  `fullFix -> upd` produce the necessary working-side power update?
- Does `fullFix` make all panel live/target fields and completion flags converge?
- Is `triggerWhenCompleted` populated on the tested placed generator, and does the
  normal path produce an observable effect absent from `fullFix`?
- Which named calls are PE-visible, VM-visible, or visible only through state edges?
