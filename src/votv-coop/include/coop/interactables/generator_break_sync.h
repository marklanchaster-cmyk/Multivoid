#pragma once
#include <cstdint>
namespace coop::net { class Session; struct GeneratorBreakStatePayload; }
namespace coop::generator_break_sync {
void Install(coop::net::Session* session);
void Tick();
void OnReliable(const coop::net::GeneratorBreakStatePayload& payload, uint8_t senderSlot);
void SendJoinSnapshotForSlot(int slot);
void OnDisconnect();
}
