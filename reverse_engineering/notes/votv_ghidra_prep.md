# VotV Ghidra preparation

> Status update (2026-09-24): the GUI analysis subsequently completed, and the
> user directed all automation to a separate headless project. The historical
> cautions below describe the preparation phase. Current automation uses
> `/home/matt/GhidraProjects/VotV-headless` only; the GUI project remains a
> human-inspection copy. See `votv_transformer_headless_findings.md`.

Prepared 2026-09-24 while the GUI was performing initial analysis. Do not run
headless analysis, import into, script against, or otherwise modify the open
project until the user reports that GUI analysis is finished.

## Target identity

Two byte-identical executable copies were found:

- `/home/matt/VOTV/WindowsNoEditor/VotV/Binaries/Win64/VotV-Win64-Shipping.exe`
- `/home/matt/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/VotV-Win64-Shipping.exe`

Verified metadata:

- SHA-256: `ad478218ec5513cc4c1682937db3214cbf2694d1dcd0583eb423447c049dd3ae`
- size: 84,751,360 bytes
- format: PE32+ Windows GUI executable, x86-64, 9 sections
- image base: `0x140000000`
- entry RVA: `0x395441c` (VA `0x14395441c`)
- PE timestamp: 2026-05-16 07:48:13
- linker: MSVC 14.43; large-address-aware, ASLR, NX
- runtime file version: 4.27.2.0; game reports UE 4.27 / Alpha 0.9.0-n

`ghidra_scripts/check_votv_target.py` rechecks both copies without writing to
them. Re-run it immediately before using any prepared RVA.

## Active Ghidra installation

- Ghidra: 12.1.4 PUBLIC, build 2026-09-21
- launcher: `/home/matt/Tools/ghidra-12.1.4/ghidraRun`
- headless interface (do not run yet):
  `/home/matt/Tools/ghidra-12.1.4/support/analyzeHeadless`
- Python support: PyGhidra, Python 3.9 through 3.14
- script roots exist for Base, PyGhidra, Decompiler, PDB, Debugger, BSim,
  FunctionID, MicrosoftCodeAnalyzer, and other shipped modules

The active Java process holds `/home/matt/VotV.lock~` and files below
`/home/matt/VotV.rep`; this identifies the open project. Nothing in that
project was opened for write or scripted during preparation.

Repo-local scripts prepared for the GUI Script Manager after analysis:

- `VotVRuntimeAnchors.py`: report the function/memory block at known runtime RVAs
- `VotVSearchTerms.py`: report matching analyzed symbols, functions, and defined strings

Both are deliberately report-only. They create no labels and save nothing.

## Current runtime log

Log: `/home/matt/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/multivoid.log`

The inspected log is a 153-line, menu-only run from 2026-09-23 22:34-22:38,
mod build b65004. It reaches `HEALTH: PASS`, installs the ProcessEvent and
render hooks, opens the multiplayer browser, then shuts down cleanly. It is not
a transformer gameplay capture: there are no transformer, generator, breaker,
power, or repair lines.

Useful native anchors from that exact executable:

| Name | RVA | VA at preferred image base |
|---|---:|---:|
| `FName::ToString` | `0x127d870` | `0x14127d870` |
| `ProcessEvent` | `0x1465930` | `0x141465930` |
| D3D11 `PresentChecked` | `0x16f4ba0` | `0x1416f4ba0` |
| D3D11 resize | `0x1703750` | `0x141703750` |
| D3D12 `PresentInternal` | `0x177e0e0` | `0x14177e0e0` |
| D3D12 resize | `0x177e8b0` | `0x14177e8b0` |
| `UGameplayStatics::OpenLevel` | `0x2b530b0` | `0x142b530b0` |
| `GUObjectArray` | `0x4d8f910` | `0x144d8f910` |

Other instrumentation evidence:

- `NumObjects()` grows from 17,770 at health check to 179,605 at the menu.
- `FStructProperty::Struct` calibrates to `+0x78`.
- `FBoolProperty` payload calibrates to `+0x78`.
- final cppmod dispatch tally is
  `[1:38499 2:1 3:1 4:1 6:6 8:6 13:65 15:1]`.
- the run warns that UE4SS Lua/default mods and two foreign LogicMods are loaded;
  account for that environment when comparing runtime behavior.

Use `../summarize_multivoid_log.py` for a repeatable read-only summary.

## What “transformer” means here

The target is the electrical generator/transformer minigame, not Unreal's
geometric `FTransform`. Searching `SetActorTransform` would mostly lead away
from this task.

The repo already contains cooked-Blueprint reflection outputs:

- `research/bp_reflection/transformerMGPanel.json` and `.functions.txt`
- `research/bp_reflection/generator.json` and `.functions.txt`
- `research/bp_reflection/trigger_eventer.json`
- `tools/walkie_radio/work/votv_pak_list.txt`

This is important: `transformerMGPanel_C` and `generator_C` are Blueprint
classes stored in cooked `.uasset/.uexp` assets. Their names and logic may not
occur in `VotV-Win64-Shipping.exe` at all. Ghidra is useful for native UE4
dispatch/reflection implementation and runtime anchors; the Blueprint JSON is
the primary source for the transformer state machine.

### Highest-value classes and assets

- `/Game/objects/misc/transformerMGPanel`, class `transformerMGPanel_C`
- `/Game/objects/generator`, class `generator_C`
- `/Game/objects/prop_transformerUpgrade`, class `prop_transformerUpgrade_C`
- mesh family `/Game/meshes/generator/2/transformerpanel_*`
- `uiwindow_transformerScreens_C`
- `actor_save`, `mainGamemode_C`, and `trigger_eventer_C` as owners/consumers

### State, completion, and events

Search exact spelling, including mistakes present in the assets:

- generator completion: `fullFix`, `isBroken`, `cycle`, `upd`, `update`
- generator delegates: `turnedOn`, `turnedOn__DelegateSignature`, `updated`,
  `updated__DelegateSignature`
- puzzle arrays: `switches_states`, `switches_target`, `rotators_states`
- puzzle actions: `clicked_switchers`, `clicked_rotataors` (sic),
  `moveRotator`, `moveSwitch`, `setRotators`, `setSwitches`, `setKnobs`
- initialization/solution: `randomizeTargets`, `randomizeValues`,
  `randomizeSines`, `solveColorGrid`, `checkColors`, `initValues`, `initiate`
- input/UI: `lmb`, `releaseSlider`, `mouseDelta`, `playerscrollUp`,
  `playerscrollDown`, and the `BndEvt__transformerMGPanel_*` delegate functions
- upgrade path: `updUpgrades`, `getUpgrades`, `upgradeTake`,
  `prop_transformerUpgrade_C`, and `upg_transofrmer` (sic)
- repair input: `toolboxFix`, `toolboxCanFix`, `wallFixer_fix`, `playerUsedOn`
- ignition/damage: `attemptIgnite`, `ignite`, `broken`, `broken_fire`

### Existing Multivoid instrumentation relevant to the task

- `repair_sync.cpp` treats `generator_C::isBroken` as authoritative and applies
  the native `fullFix()` path. Its comment records that `fullFix()` completes
  the panel, sets `cycle=100`, clears `isBroken`, fires `turnedOn`, and calls
  `upd()`.
- `power_sync.cpp` is a different system: it mirrors the five breakers on
  `powerControl_C` (`press_coord`, `press_downl`, `press_play`, `press_calc`,
  `press_light`) and intentionally avoids downstream fan-out.
- an older review/diff named `transformerMGPanel_C.fixed/Fixed` as repair state;
  current code correctly moved authority to `generator_C.isBroken`. Treat the
  older class/property pair as historical evidence, not the current answer.
- `docs/piles/findings/votv-all-interactables-sweep-catalog-2026-06-08.md`
  classifies the transformer panel as a compound minigame whose full state is
  the switch/rotator arrays; outcome-only synchronization is the smaller path.

## Original planned next steps (superseded by the headless workflow)

1. Confirm the program name, image base `0x140000000`, language x86-64, target
   size/hash, and that auto-analysis has no running tasks.
2. Add `reverse_engineering/ghidra_scripts` as a Script Manager source
   directory; do not use headless import for this already-open project.
3. Run `VotVRuntimeAnchors.py`. At minimum, `ProcessEvent`, `FName::ToString`,
   and `OpenLevel` should land in executable memory and inside analyzed
   functions; `GUObjectArray` should land in mapped data.
4. Inspect `ProcessEvent` callers and the `UFunction`/`FName` paths used by the
   repo instrumentation. Use runtime addresses as evidence before naming.
5. Run `VotVSearchTerms.py`. Expect engine terms to be more productive than
   Blueprint-specific terms. Record zero-hit Blueprint terms rather than
   forcing unrelated matches.
6. In parallel with native inspection, read the already-extracted Blueprint
   bytecode around `generator_C::fullFix` and the transformer panel's
   `clicked_*`, `set*`, `checkColors`, and `solveColorGrid` functions.
7. Build a state-transition table: input event -> array/property mutation ->
   completion predicate -> `generator_C.fullFix` -> `turnedOn` delegate.
8. Decide which question the implementation needs to answer:
   outcome-only sync (`isBroken/fullFix`) is already instrumented, while live
   cooperative puzzle sync requires array/knob/slider state plus echo control.
9. Only after evidence is recorded, add labels/comments in Ghidra and export a
   candidate-address report under `reverse_engineering/notes/`.

## Safety status during preparation

Preparation used read-only metadata/log/repository inspection. No executable,
game file, Ghidra project file, or Ghidra installation file was changed. No
headless analyzer was launched, and no script was run against the open Ghidra
project.
