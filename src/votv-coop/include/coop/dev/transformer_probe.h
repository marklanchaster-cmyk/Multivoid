// coop/dev/transformer_probe.h -- read-only runtime validation for generator_C.
//
// Gated by [dev] transformer_probe=1.  The probe observes only the named
// transformer functions and polls their reflected state for edges; it never calls a
// gameplay function and never writes an engine property.

#pragma once

namespace coop::dev::transformer_probe {

// Game-thread/session tick.  Installs the targeted observers after the relevant
// Blueprint classes load, then emits a baseline and state-change-only snapshots.
void Tick(bool connected, bool isHost);

}  // namespace coop::dev::transformer_probe
