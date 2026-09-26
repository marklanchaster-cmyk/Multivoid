#pragma once

#include <cstdint>

namespace coop::net { class Session; }

namespace coop::black_fog_sync {

// blackFog_C is a local presentation controller whose shared lifetime is
// host-owned. EventAuthority carries the instance identity/lifetime; this
// module materializes and phase-reconciles the safe local presentation.
void Install(coop::net::Session* session);
void Tick();

// Called by event_active_sync for blackFog_C entries. `flags` is narrowly
// class-specific: bit 7 = terminal fade, bit 6 = native
// activeEvents_senders membership, bits 0..5 = current alpha [0,63].
uint8_t HostFlags(void* controller, bool nativeActive);
bool NativeActive(uint8_t flags);
void ClientBegin(uint64_t instanceId, uint16_t elapsedSec, uint8_t flags,
                 bool fromSnapshot);
void ClientPhase(uint64_t instanceId, uint8_t flags);
void ClientEnd(uint64_t instanceId);

// Lets weather_event_births distinguish our commanded mirror from an organic
// client-side day-roll birth.
bool MirrorEchoActive();

void OnDisconnect();

}  // namespace coop::black_fog_sync
