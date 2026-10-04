# VotV transformer/power headless findings

Prepared 2026-09-24 from a dedicated Ghidra 12.1.4 headless project and the
repository's existing UAssetAPI Blueprint exports.

## Scope and target integrity

- Headless project: `/home/matt/GhidraProjects/VotV-headless.gpr` with data in
  `/home/matt/GhidraProjects/VotV-headless.rep` (about 1.2 GiB).
- Input executable:
  `/home/matt/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/VotV-Win64-Shipping.exe`
- SHA-256: `ad478218ec5513cc4c1682937db3214cbf2694d1dcd0583eb423447c049dd3ae`
- Size/format: 84,751,360 bytes; PE32+ Windows GUI; x86-64; preferred image
  base `0x140000000`.
- The expected PDB was unavailable. Findings therefore use Ghidra-generated
  names, runtime-derived anchors, strings, xrefs, and Blueprint metadata.
- The game executable was only read. The open GUI project was not used by the
  headless workflow. Final extraction passes used `-readOnly -noanalysis`.

## Native executable result

The decisive result is negative: the game-specific transformer state machine
is not compiled as named native code in this executable. It remains Kismet
bytecode in cooked Blueprint assets. Native Ghidra analysis is still valuable
for Unreal reflection and dispatch internals used by instrumentation.

The defined-string scan found 187 case-insensitive substring matches; the raw
ASCII/UTF-16LE scan found 35. The apparent hits were eliminated as follows:

| Search | Defined hits | Raw hits | Interpretation |
|---|---:|---:|---|
| `transformer` | 1 | 0 | `Animation_TransformError`, unrelated |
| `generator_C` | 5 | 0 | substrings of `EnvQueryGenerator_Composite` and related engine classes |
| `generator` | 88 | 8 | water, Niagara, EQS, audio, and crypto generators |
| `isBroken` | 1 | 0 | native string `IsBroken` in the PhysicsConstraint API |
| `cycle` | 16 | 7 | generic engine uses |
| `updated` | 67 | 11 | generic engine uses |
| `BlueprintGeneratedClass` | 8 | 8 | useful engine reflection anchor |
| `UFunction` | 1 | 1 | useful CoreUObject registration anchor |

The following high-value exact terms have no native match:
`transformerMGPanel`, `prop_transformerUpgrade`, `upg_transofrmer`, `fullFix`,
`turnedOn`, `switches_states`, `switches_target`, `rotators_states`,
`clicked_switchers`, `clicked_rotataors`, `moveRotator`, `moveSwitch`,
`setRotators`, `setSwitches`, `setKnobs`, `randomizeTargets`, `randomizeSines`,
`solveColorGrid`, `checkColors`, `attemptIgnite`, `powerControl`,
`powerChanged`, and the five `press_*` breaker events.

The lone `IsBroken` string is at VA `0x1441f5068`. Its only analyzed xref is
data at `0x1441f8370`, with no owning function. Its raw neighborhood contains
`BreakConstraint`, `GetConstraintForce`, `GetCurrentSwing1`,
`GetCurrentSwing2`, `GetCurrentTwist`, and `SetAngularBreakable`; it is the
PhysicsConstraint property, not `generator_C.isBroken`.

## Native anchors and xrefs

| Anchor | VA | Inbound refs | Result |
|---|---:|---:|---|
| `FName::ToString` | `0x14127d870` | 1,598 | decompiled; accesses name-pool state around `DAT_144d535c0` and appends numbered-name suffixes |
| `ProcessEvent` | `0x141465930` | 1,769 | decompiled; reaches the chunked object array and script/native dispatch paths |
| `GUObjectArray` | `0x144d8f910` | 85 | mapped data anchor; object-array item stride visible as `0x18` in the dispatch path |
| `UGameplayStatics::OpenLevel` | `0x142b530b0` | 5 | decompiled; calls the name-to-string and travel paths |
| D3D11 present/resize | `0x1416f4ba0` / `0x141703750` | 3 / 4 | runtime hook anchors verified in analyzed functions |
| D3D12 present/resize | `0x14177e0e0` / `0x14177e8b0` | 2 / 5 | runtime hook anchors verified in analyzed functions |

Targeted string-xref traversal produced two useful native functions:

- `FUN_140690930` registers `UFunction` in `/Script/CoreUObject` through the
  native class-registration path.
- `FUN_1412e0310` resolves and validates Blueprint-related exports, using the
  names `BlueprintGeneratedClass`, `DynamicClass`, `Function`, and
  `DelegateFunction`; it also reaches `FName::ToString` for diagnostics.

No developer symbols are needed for either conclusion, but names beyond these
observable roles should remain provisional.

## Blueprint transformer state map

The repository's decoded cooked assets are the primary evidence for game logic:
`research/bp_reflection/generator.json` and
`research/bp_reflection/transformerMGPanel.json`.

| Stage | Observed symbolic behavior |
|---|---|
| `transformerMGPanel_C.initiate` | Initializes the widget and dynamic materials, collects switch/rotator/slider components, reads difficulty, then calls `init`, `initValues`, and hint/status paths. |
| `randomizeTargets` | Chooses target sine amplitude/frequency/offset in the 0–15 range, builds an eight-bit `switches_target`, calls `solveColorGrid`, then refreshes rotators, knobs, and switches. |
| `randomizeValues` | Randomizes live sine values, `rotators_states`, and `switches_states`, then calls all three setters. |
| `setKnobs` | Clamps the three live sine values to 0–15, compares them with targets, updates sine materials/slider transforms, and writes `isSineComplete`. |
| `setSwitches` | Converts `switches_target` to bits, compares against `switches_states`, updates switch transforms/text/materials, and writes `isSwitchesComplete`. |
| `setRotators` | Calls `checkColors`, stores its solved result in `isRotatorsComplete`, updates rotator transforms, and reports status. |
| `solveColorGrid` | Constructs/shuffles the color-grid puzzle and assigns per-side colors/materials. |
| `checkColors` | Traverses grid neighbors and rotated color edges, returning a `solved` result used by `setRotators`. |
| `updateAll` / `initValues` | Both call `setKnobs`, `setRotators`, and `setSwitches`. |
| `clicked_switchers`, `clicked_rotataors`, `moveSwitch`, `moveRotator`, `releaseSlider` | Thin event wrappers that jump into `ExecuteUbergraph_transformerMGPanel` at bytecode offsets 4090, 3757, 4365, 4677, and 5186 respectively. |

`generator_C.fullFix` is 903 bytes of decoded Kismet bytecode. Its symbolic
operations copy the panel's target sine settings into its live settings,
derive the live switch array from `switches_target`, touch/normalize the
rotator-state array, set `cycle` to 100, clear `isBroken`, broadcast the
`turnedOn` multicast delegate, and invoke `upd`.

`generator_C.upd` fans out through `updUpgrades`, generator/power update paths,
the panel's `updateAll`, UI visibility, and the `updated` delegate.
`loadData`/`getData` confirm persistence of `isBroken`, `cycle`,
`upgradeLevel`, and the panel sine-completion bit. These relationships support
the current outcome-only repair design in `repair_sync.cpp`: authority on
`generator_C.isBroken`, with `fullFix()` used to produce the game's own
completion side effects. `powerControl_C` and its five breaker events are a
separate power subsystem.

The reports inventory symbols and expressions, not recovered Blueprint source.
Exact branch ordering inside `ExecuteUbergraph_transformerMGPanel` still needs
a dedicated Kismet control-flow decoder if live cooperative puzzle-state sync
is required.

## Reusable outputs

- `exports/ghidra-headless/defined_string_hits.tsv`: defined strings and xrefs.
- `exports/ghidra-headless/raw_term_hits.tsv`: raw ASCII/UTF-16 term hits.
- `exports/ghidra-headless/anchor_xrefs.tsv`: runtime anchors and inbound refs.
- `exports/ghidra-headless/decompiled/`: seven anchor decompilations.
- `exports/ghidra-headless/targeted_xref_graph.tsv`: focused reflection-string graph.
- `exports/ghidra-headless/targeted_decompiled/`: the two registration/resolution functions.
- `exports/blueprint/generator_functions.md`: selected generator bytecode symbols.
- `exports/blueprint/transformer_panel_functions.md`: selected panel bytecode symbols.

## Reproduction

Run each Ghidra pass sequentially; do not open the headless project for write in
another Ghidra process at the same time.

```bash
/home/matt/Tools/ghidra-12.1.4/support/analyzeHeadless \
  /home/matt/GhidraProjects VotV-headless \
  -process VotV-Win64-Shipping.exe -readOnly -noanalysis \
  -scriptPath /home/matt/Projects/Multivoid/reverse_engineering/ghidra_scripts \
  -postScript VotVHeadlessReport.java \
  /home/matt/Projects/Multivoid/reverse_engineering/exports/ghidra-headless

/home/matt/Tools/ghidra-12.1.4/support/analyzeHeadless \
  /home/matt/GhidraProjects VotV-headless \
  -process VotV-Win64-Shipping.exe -readOnly -noanalysis \
  -scriptPath /home/matt/Projects/Multivoid/reverse_engineering/ghidra_scripts \
  -postScript VotVTargetedXrefs.java \
  /home/matt/Projects/Multivoid/reverse_engineering/exports/ghidra-headless
```

Regenerate the Blueprint summaries with
`reverse_engineering/summarize_blueprint_bytecode.py`; its positional arguments
are an input JSON export, output Markdown path, and one or more function names.

## Recommended next analysis

1. Decode `ExecuteUbergraph_transformerMGPanel` into basic blocks keyed by the
   five event-entry offsets above, preserving Kismet code offsets.
2. Trace writes to the three completion booleans and the point where their
   conjunction reaches `generator_C.fullFix` or another completion event.
3. If live cooperative manipulation is desired, derive a minimal replicated
   state packet for the three sine values, switch bitset, and rotator array,
   with update echo suppression. If only repaired/not-repaired outcome matters,
   the existing `isBroken` plus `fullFix()` route is the smaller and safer path.
