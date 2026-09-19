// coop/voice/radio_item.h -- physical walkie-radio gameplay state.

#pragma once

namespace coop::radio_item {

// Install the hold-E radial confirmation observer.
void Install();

// Poll the local inventory/hand and publish RX/TX eligibility.
void Tick();

// Clear per-session radio power state.
void OnDisconnect();

// Helpers used by other gameplay code.
bool IsRadioActor(void* actor);
bool IsPoweredActor(void* actor);

}  // namespace coop::radio_item
