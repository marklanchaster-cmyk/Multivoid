#pragma once
#include <cstdint>
namespace coop::net { class Session; struct EventSnapshotPayload; struct EventAuthorityPayload; }
namespace coop::event_active_sync {
// Host polls activeEvents_senders and assigns each membership a monotonic,
// session-local instance id. Clients keep only this authoritative set.
void Install(coop::net::Session* session);
void Tick();
// Direct event-controller births whose meaningful lifetime begins before they
// enter activeEvents_senders use this exact controller identity. A later poll
// of the registry sees the same pointer and therefore cannot mint a duplicate.
uint64_t HostBeginExternal(void* controller, const char* className, const char* rowName,
                           bool* created = nullptr);
// Refreshes class-specific state on the same instance/reliable revision stream.
// Emits only when the quantized flags changed.
void HostRefreshExternal(void* controller);
void HostEndExternal(void* controller);
// Atomic current-set bracket, including an empty set, at ClientWorldReady.
void SendJoinSnapshotForSlot(int slot);
void OnReliable(const coop::net::EventAuthorityPayload& payload);
// Defensive legacy parser; mixed protocol versions cannot normally reach it.
void OnReliable(const coop::net::EventSnapshotPayload& payload);
void OnDisconnect();
}
