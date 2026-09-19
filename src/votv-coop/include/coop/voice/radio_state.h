// coop/voice/radio_state.h -- local walkie-radio state.
//
// The gameplay/item layer publishes whether the local radio is powered
// and whether it is currently being held. The audio thread only reads
// these atomics; it never touches Unreal objects directly.

#pragma once

namespace coop::radio_state {

// Game-thread publishers.
void SetPowered(bool powered);
void SetHeld(bool held);
void SetHeldPowered(bool powered);

// Thread-safe readers.
bool Powered();
bool Held();
bool HeldPowered();
bool CanTransmit();

// 0.0 = clean radio, 1.0 = severe event interference.
void SetInterference(float level);
float Interference();

// World/session teardown.
void Reset();

}  // namespace coop::radio_state
