#pragma once

#include <cstdint>
#include <string>

namespace coop::net { class Session; }

namespace coop::event_family_sync {

void Install(coop::net::Session* session);
void Tick();
void HostBegin(uint64_t instanceId, void* controller, const std::string& className);
void HostEnd(uint64_t instanceId, void* controller, const std::string& className);
void HostNativeActive(uint64_t instanceId, void* controller, const std::string& className);
void ClientApplyState(uint64_t instanceId, const std::string& className,
                      const uint8_t* state, uint8_t stateLen, bool snapshot);
void ClientEndState(uint64_t instanceId, const std::string& className);
void ClientApplyCue(const std::string& className, uint8_t cue);
void OnDisconnect();

}  // namespace coop::event_family_sync
