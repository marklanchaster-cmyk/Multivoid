// coop/drive_sync.h -- v119 L5: the drive-chain lanes.
//
//   DriveSlotState (109) -- per-slot intent/result lines (desk play/comp +
//     eraser slots; slot actors have NO eids -> keyed by role). A client's
//     organic edge terminates at the host; the host validates and broadcasts
//     the canonical reflected putDriveIn/drivePulledOut result.
//   DrivePayload (110) -- prop_drive.data_0 rows ({u32 eid} + the v65
//     signal_wire codec sans image, BlobChunkPayload chunks). 0x45
//     dirty-marks + a 1 Hz diff-gated baseline poll; client rows terminate at
//     the host and host-authored rows are the only results clients apply.
//   (RackState (111) lives in drive_rack_sync -- extracted 2026-07-18,
//    votv-rack-extraction-DESIGN-2026-07-18.md. This module keeps ALL the
//    0x45 verb registration and forwards rack marks.)
//
// Design of record: votv-drive-chain-L5-impl-DESIGN-2026-07-18.md (7-round
// /qf). The slotted-latch of that design is SATISFIED BY the existing
// frozen/static pose gate (remote_prop.cpp "frozen/static non-attachable --
// ignored") plus an insertion-time ClearAnyDriveFor handoff: putDriveIn
// freezes the drive, the handoff retires an already-active pose cache, and a
// genuinely later straggler must resolve afresh through the frozen gate.
//
// Game thread throughout.

#pragma once

#include "coop/net/protocol.h"

#include <cstdint>

namespace coop::net { class Session; }

namespace coop::drive_sync {

// Install once per session (latched; safe per net-pump tick).
void Install(coop::net::Session* session);

// Per net-pump tick: verb-name resolution, the dirty-mark barrier drain,
// the 1 Hz sweeps, pending-apply retries, the connect broadcast queue.
void Tick();

// Router entries (event_dispatch_signal.cpp).
void OnDriveSlotState(const coop::net::DriveSlotStatePayload& p, uint8_t senderSlot);
void OnDrivePayloadChunk(const coop::net::BlobChunkPayload& p, uint8_t senderSlot);

// HOST: queue the L5 connect seed (slot lines + drive payloads; the rack
// canonicals ride drive_rack_sync's seed right after) for a peer that just
// reached world-ready.
void QueueConnectBroadcastForSlot(int peerSlot);

// CLIENT JIP bracket: unresolved slot/payload applies do not age while the
// host's prop snapshot is still materializing their referenced drive actors.
// SnapshotComplete re-stamps the bounded queue; its TTL is then only a leak
// guard for genuinely missing identities.
void NoteJoinSnapshotBracket(bool open);

// (The v118-style slot-only birth reap was audit-rejected for drives -- one
// shared class, no byte discriminator, false positives on multi-take play.
// The reap is CONTENT-correlated instead: a denied take's ghost identifies
// itself by its adoption payload's row hash; drive_sync reaps it there.)

// CLIENT (prop_drop_intent, the freshBirth drain): note that `actor` is a
// LOCALLY-AUTHORED drive birth -- its payload broadcasts at adoption (first
// eid sight). Without the note a client's first sight stays prime-only: a
// joiner's save-loaded drives materialize AFTER the connect prime and must
// NOT be re-authored (measured in the first v119 smoke: the joiner
// re-broadcast the host's own connect-seed rows + 2 unmatched-eid strays).
void NoteLocalDriveBirth(void* actor);

// Full teardown (the OnDisconnect fanout) -- clears slot/payload baselines,
// dirty marks, pending applies, noted births; the deny/taken rings are
// drive_rack_sync's (its own OnDisconnect). Re-run implicitly at next start.
void OnDisconnect();

}  // namespace coop::drive_sync
