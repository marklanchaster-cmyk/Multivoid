#pragma once

#include <cstdint>

namespace coop::net {
class Session;
struct EventOutputIntentPayload;
struct EventOutputResultPayload;
struct EventOutputStatePayload;
}

namespace coop::event_output_sync {

// Wire-level output families. ActorMirror, EnvironmentState, TransientCue and
// EventState have concrete adapters. Unknown/reserved families stay inert.
enum class OutputType : uint8_t {
    Unknown = 0,
    ActorMirror = 1,
    EnvironmentState = 2,
    ResultSync = 3,
    TransientCue = 4,
    PerPlayer = 5,
    SafeClientReplay = 6,
    EventState = 7,
};

void Install(coop::net::Session* session);
void Tick();

// Host-owned output identity. backingEid is the independent actor mirror
// materialization identity; it never replaces the returned output id.
uint64_t HostBeginActor(uint64_t eventInstanceId, void* actor, uint32_t backingEid,
                        const char* className, bool interactive);
uint64_t HostBeginEnvironment(uint64_t eventInstanceId, const char* className,
                              uint8_t state);
void HostUpdateEnvironment(uint64_t eventInstanceId, uint64_t outputId, uint8_t state);
uint64_t HostBeginState(uint64_t eventInstanceId, const char* className,
                        const void* state, uint8_t stateLen);
void HostUpdateState(uint64_t eventInstanceId, uint64_t outputId,
                     const void* state, uint8_t stateLen);
void HostEmitTransient(uint64_t eventInstanceId, const char* className,
                       uint8_t cue);
void HostEndOutput(uint64_t eventInstanceId, uint64_t outputId);
void HostEndInstance(uint64_t eventInstanceId);

void SendJoinSnapshotForSlot(int slot);
void ClientEndInstance(uint64_t eventInstanceId);
void OnReliable(const coop::net::EventOutputStatePayload& payload);
void OnIntent(const coop::net::EventOutputIntentPayload& payload, uint8_t senderSlot,
              uint32_t senderGeneration);
void OnResult(const coop::net::EventOutputResultPayload& payload);
// Host-side per-occupant cleanup. Must run on every slot departure/replacement,
// not only on full session teardown.
void OnPeerLeft(uint8_t slot);
void OnDisconnect();

// Explicit console diagnostic. Reads only this module's bounded registries.
void LogDiagnostics();

}  // namespace coop::event_output_sync
