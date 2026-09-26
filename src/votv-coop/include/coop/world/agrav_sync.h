#pragma once

#include <cstdint>

namespace coop::net { class Session; struct AgravStatePayload; }

namespace coop::agrav_sync {

void Install(coop::net::Session* session);
void Tick();
void HostBegin(uint64_t instanceId, void* controller);
void HostEnd(uint64_t instanceId, void* controller);
void ClientBegin(uint64_t instanceId, uint16_t elapsedSec, bool fromSnapshot);
void ClientEnd(uint64_t instanceId);
void SendJoinSnapshotForSlot(int slot);
void OnReliable(const coop::net::AgravStatePayload& payload);
void OnDisconnect();

}  // namespace coop::agrav_sync
