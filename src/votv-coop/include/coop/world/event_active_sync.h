#pragma once
namespace coop::net { class Session; struct EventSnapshotPayload; struct EventAuthorityPayload; }
namespace coop::event_active_sync {
// Host polls activeEvents_senders and assigns each membership a monotonic,
// session-local instance id. Clients keep only this authoritative set.
void Install(coop::net::Session* session);
void Tick();
// Atomic current-set bracket, including an empty set, at ClientWorldReady.
void SendJoinSnapshotForSlot(int slot);
void OnReliable(const coop::net::EventAuthorityPayload& payload);
// Defensive legacy parser; mixed protocol versions cannot normally reach it.
void OnReliable(const coop::net::EventSnapshotPayload& payload);
void OnDisconnect();
}
