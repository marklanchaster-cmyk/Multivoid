// coop/interactables/repair_sync.h -- cooperative repair-outcome sync.
//
// Compound repair minigames remain local/cosmetic. When a peer's solved-state flips to
// repaired, it sends one revision-bound RepairOutcome request. The host applies the
// repaired outcome to its real actor, then broadcasts the authoritative result. Host
// repaired/broken edges and join snapshots also use this lane so stale requests cannot
// overwrite a newer host state.
//
// Covered:
//   serverBox_C                         IsBroken -> false
//   radiotower_C                      isBroken -> false (native setBroken + updPuzzle)
//   generator_C                       isBroken -> false (native fullFix)
//
// This intentionally syncs the OUTCOME, not each fuse/switch/rotator movement.

#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct RepairOutcomePayload;
}

namespace coop::repair_sync {

void Install(coop::net::Session* session);
void Tick();
void OnReliable(const coop::net::RepairOutcomePayload& payload, uint8_t senderSlot);
void SendJoinSnapshotForSlot(int slot);
void OnDisconnect();

}  // namespace coop::repair_sync
