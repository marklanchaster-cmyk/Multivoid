#pragma once
namespace coop::net { struct EntitySpawnPayload; }
namespace coop::fossilhound_birth {
void Capture(void* actor, coop::net::EntitySpawnPayload& payload);
void ApplyBeforeFinish(void* actor, const coop::net::EntitySpawnPayload& payload);
}
