// coop/prop_echo_suppress.h -- one-shot echo-suppression sets used by the
// host/client prop_lifecycle observers to skip broadcasting a spawn/destroy
// that originated from the OTHER end of the wire (the receiver-side OnSpawn
// / OnDestroy in coop::remote_prop calls Mark*; the symmetric observer in
// coop::prop_lifecycle calls Consume*).
//
// Without this, our own receiver-applied spawn/destroy would re-broadcast
// to the original sender = packet ping-pong.
//
// These are implementation details shared by exactly two TUs
// (prop_lifecycle and remote_prop). They live in a dedicated header
// rather than in coop/remote_prop.h to keep remote_prop's public API
// surface free of internals.
//
// Game-thread-only access. Set capacity is bounded internally; on overflow
// the set is cleared (a one-shot stale lookup on a never-consumed entry
// is harmless -- it lets a wire-induced spawn re-broadcast once, which
// the OTHER side de-dupes via FindByKeyString).

#pragma once

#include <cstdint>
#include <string>

namespace coop::prop_echo_suppress {

void MarkIncomingSpawn(void* actor);
bool ConsumeIncomingSpawn(void* actor);
// Non-destructive membership check (v106 spawn-seam): the FinishSpawningActor
// Func callback must EXCLUDE wire/display mirror spawns (marked before Finish)
// WITHOUT eating the mark the Init-POST observer consumes on its normal path.
bool PeekIncomingSpawn(void* actor);
void MarkIncomingDestroy(void* actor);
bool ConsumeIncomingDestroy(void* actor);

// The game retires a floppy actor when it is inserted, then may reincarnate the
// same logical floppy (same portable save key) on eject much later. Remember
// that lifecycle edge until the session/world ends or the first qualifying
// fresh floppy consumes it. The key marker alone never suppresses a destroy.
void NoteFloppyRetiredForReincarnation(void* actor, const std::wstring& key);

// Receiver-side correlation for an incoming PropDestroy: capture a bounded candidate from the
// dying disc, then promote it to the long-lived marker only when the applied laptop INSERT state
// matches its concrete type/content. This handles cross-lane arrival in either order without
// treating every wire floppy destroy as an insertion.
void NoteWireFloppyRetirementCandidate(void* actor, const std::wstring& key);
void ConfirmWireFloppyInsertRetirement();

// Query used only by the host PropDropIntent materialization seam: if this
// exact portable floppy key is awaiting reincarnation, assign its eid now
// rather than waiting for the normal next-tick watcher. Does not consume.
bool IsFloppyReincarnationAwaiting(void* actor, const std::wstring& key);

// Central fresh-floppy convergence arm. Qualifies the actor as a floppy and
// consumes the portable-key marker only after a valid exact eid exists. An
// invalid/zero eid leaves the marker pending for a later assignment seam.
// Final suppression remains the existing exact actor+eid 500 ms one-shot.
bool TryArmFloppyReincarnation(void* actor, uint32_t wireEid,
                               const std::wstring& key);

// Session/world teardown: discard unconsumed lifecycle markers and exact
// expectations so neither can bleed into a later world.
void ResetFloppyConvergence();

// A freshly materialized HOST floppy mirror can be torn down immediately by
// the receiver's stale local drive/eject cleanup. That display-side teardown
// is not a client-authored world transaction. Arm an exact actor+wire-eid,
// short-lived, one-shot expectation after the mirror bind; the destroy seam
// consumes it instead of sending PropDestroy upstream. This is deliberately
// not a blanket mirror-destroy suppression.
void ExpectHostMirrorConvergenceDestroy(void* actor, uint32_t wireEid);
bool ConsumeHostMirrorConvergenceDestroy(void* actor, uint32_t wireEid);

// ---- the ARBITER-CONSUMED key (2026-08-25) -----------------------------------------------------
// "I, the host, already destroyed the prop with this save key myself, as the authority's half of a
// transaction a client asked for. The client's own destroy for it is following behind me on the same
// lane, and it is an ECHO."
//
// KEYED BY SAVE KEY, not by actor pointer, and that is the whole reason it exists: by the time the
// echo arrives the actor is gone, so there is no pointer left to mark. Without this the echo reaches
// the destroy receiver's key fallback, misses the index (we evicted the key when we destroyed it),
// and pays a full `ue_wrap::prop::FindByKeyString` GUObjectArray walk to discover what the host
// already knew -- turning what used to be an O(1) index hit into a guaranteed cold scan on EVERY
// arbiter-performed transaction (audit IMPORTANT I-2, 2026-08-25, found in the coin gun's sale lane
// right after A50 gave the arbiter its own destroy).
//
// One-shot and capped like the pointer sets above; a stale entry that is never consumed costs one
// missed short-circuit, never a wrong destroy. Game thread only.
void MarkArbiterConsumedKey(const std::wstring& key);
bool ConsumeArbiterConsumedKey(const std::wstring& key);

// Mirror-spawn re-entrancy scope (owner-mirror 2026-07-10). The receiver's
// BeginDeferred UFunction call dispatches through ProcessEvent, so the
// BeginDeferred POST observers (host_spawn_watcher's ambient broadcaster)
// fire INSIDE it -- BEFORE MarkIncomingSpawn can run (the actor only exists
// at Begin-return). Since the ambient broadcaster is peer-symmetric now, a
// mirror spawn of an ambient class would re-broadcast = ping-pong. The
// receiver wraps its spawn call in this scope; the broadcaster checks it.
// Game-thread-only (the POST fires synchronously inside the wrapped call).
class ScopedMirrorSpawn {
 public:
    ScopedMirrorSpawn();
    ~ScopedMirrorSpawn();
    ScopedMirrorSpawn(const ScopedMirrorSpawn&) = delete;
    ScopedMirrorSpawn& operator=(const ScopedMirrorSpawn&) = delete;
};
bool InMirrorSpawnScope();

}  // namespace coop::prop_echo_suppress
