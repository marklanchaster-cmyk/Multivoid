// coop/interactables/repair_sync.h -- cooperative repair-outcome sync.
//
// Compound repair minigames remain local/cosmetic. When a peer's solved-state flips to
// repaired, it sends one RepairOutcome request. The host applies the repaired outcome to
// its real actor, then broadcasts the authoritative result to every client.
//
// Covered:
//   serverBox_C                         IsBroken -> false
//   radiotower_C                        IsBroken/isBroken -> false
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
void OnDisconnect();

}  // namespace coop::repair_sync
