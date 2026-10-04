# Multivoid Agent Instructions

## Project

Multivoid is a cooperative multiplayer mod for Voices of the Void (VotV).

The repository contains both the mod source and a substantial amount of existing
reverse-engineering material. Reuse existing research and tooling before doing
fresh reverse engineering.

Primary source tree:

    src/votv-coop/

Important supporting directories:

    reverse_engineering/
    research/
    tools/
    docs/
    reference/

Build artifact directories and *.bak / *.backup files may contain useful prior
versions or test builds. Treat them as evidence/reference unless explicitly told
to modify or remove them.

## Working Tree Safety

THIS REPOSITORY HAS EXISTING UNCOMMITTED AND UNTRACKED WORK.

Do not assume an untracked or modified file is disposable.

Never run:

    git clean
    git reset --hard
    git checkout -- .
    git restore .
    git add -A
    git add .

Do not delete, overwrite, revert, stage, commit, or otherwise modify unrelated
existing work.

Before editing files, inspect:

    git status --short --branch
    git diff -- <relevant-file>

Stage only explicitly intended files by exact path.

Never commit or push unless explicitly instructed by the user.

## Protected Existing File

Do not modify, overwrite, restore, stage, or delete:

    tools/bp_reflect.py

unless the user explicitly instructs you to modify that file.

There are backup and historical copies of files throughout the repository.
Do not remove them merely because they appear redundant.

## Reverse Engineering

Before performing new reverse engineering, search and inspect existing material
in:

    reverse_engineering/
    research/
    tools/
    docs/
    reference/

Existing Ghidra installation:

    /home/matt/Tools/ghidra-12.1.4

Headless Ghidra executable:

    /home/matt/Tools/ghidra-12.1.4/support/analyzeHeadless

Existing analyzed VotV Ghidra project:

    /home/matt/GhidraProjects/VotV-headless.gpr
    /home/matt/GhidraProjects/VotV-headless.rep

Reuse the existing Ghidra project. Do not create or re-import a fresh VotV
project unless explicitly instructed.

Use headless Ghidra, PyGhidra, command-line analysis, and the repository's
existing RE scripts.

Do not automate the Ghidra GUI.

When reporting reverse-engineering findings, distinguish confirmed static or
runtime evidence from inference.

## VotV Runtime

Primary VotV executable:

    /home/matt/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/VotV-Win64-Shipping.exe

Primary Multivoid runtime log:

    /home/matt/Desktop/a09n/WindowsNoEditor/VotV/Binaries/Win64/multivoid.log

Treat the VotV installation as READ-ONLY unless the user explicitly instructs
otherwise.

Reading the executable, Blueprint data, logs, and other game files for analysis
is allowed.

Do not patch, replace, rename, delete, or modify game-install files.

## Build Workflow

The established project build workflow uses GitHub Actions / Windows MSVC.

Do not invent a replacement build pipeline merely because the local machine is
Linux.

Local syntax checks, static analysis, repository searches, and narrowly scoped
compile checks may be used when appropriate, but do not represent them as the
authoritative Windows build.

## Network / Multiplayer Architecture

Prefer host-authoritative multiplayer behavior.

The host is the authoritative simulator for world-significant event behavior,
RNG outcomes, persistent state changes, authoritative actors, and gameplay
consequences.

Clients should generally reproduce authoritative host outputs rather than
independently simulating world-significant event logic.

Different outputs may require different replication mechanisms:

    actor / NPC output        -> actor mirror / puppet
    physics prop              -> authoritative prop identity/state
    global world state        -> state/result replication
    environment presentation  -> replicated presentation parameters
    sound / transient cue     -> authoritative cue
    player-specific effect    -> directed client effect

Do not assume that reproducing the same Blueprint event independently on each
machine will produce deterministic multiplayer behavior.

## Identity

Network identity must be explicit and host-authoritative where applicable.

Do not rely solely on class name, approximate location, or local process-generated
keys to establish durable multiplayer identity.

Prefer stable host-issued event-instance and entity identifiers.

Never print, expose, copy into reports, or commit private identity-key contents.

## Investigation Style

For substantial changes:

1. Inspect existing implementation and documentation.
2. Search existing reverse-engineering results.
3. Identify the actual current behavior.
4. Trace relevant callers / producers / consumers.
5. Explain the proposed change before making broad architectural changes.
6. Keep changes narrowly scoped.
7. Run appropriate static checks.
8. Review the final diff for unrelated changes.

Do not replace a general synchronization problem with a one-off special case
without explaining why the general architecture cannot handle it.

Do not claim a runtime behavior is fixed solely because the code compiles.

## User Control

If the user asks only for analysis, investigation, review, or architectural
advice, do not modify files.

If the user asks to implement something, modification is permitted only within
the requested scope.

When uncertain whether an action is destructive or outside scope, stop and ask
instead of guessing.
