#pragma once

#include <string>

namespace coop::net { struct WorldActorSpawnPayload; }

namespace coop::event_actor_birth {

// These keys are deliberately package-qualified. VotV contains several unrelated
// NewBlueprint5_C classes, so a leaf-name wire identity is ambiguous.
bool RequiresBirth(void* actorClass);
std::wstring WireClassKey(void* actorClass);
bool IsWireClassKey(const std::wstring& key);
void* ResolveWireClass(const std::wstring& key);

// Capture runs after FinishSpawning on the host (live birth and JIP). ApplyBeforeFinish
// restores fields consumed by ReceiveBeginPlay; ApplyAfterFinish restores engine-owned
// lifecycle state which BeginPlay itself overwrites.
bool Capture(void* actor, coop::net::WorldActorSpawnPayload& payload);
bool ApplyBeforeFinish(void* actor, const std::wstring& key,
                       const coop::net::WorldActorSpawnPayload& payload);
bool ApplyAfterFinish(void* actor, const std::wstring& key,
                      const coop::net::WorldActorSpawnPayload& payload);

// These are player-relative presentation actors. Their native Tick is part of the
// effect and must not be replaced by the generic host-pose driver.
bool UsesLocalPresentationTick(const std::string& key);

}  // namespace coop::event_actor_birth
