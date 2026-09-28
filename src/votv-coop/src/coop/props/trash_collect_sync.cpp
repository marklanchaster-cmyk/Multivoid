// coop/trash_collect_sync.cpp -- see coop/trash_collect_sync.h.
//
// One function: EnsureHeldItemBroadcast. net_pump's held-prop send calls it on
// the new-held edge. The collect itself (trashBitsPile_C::playerTryToCollect)
// is BP-internal (ProcessInternal) so it can't be observed; we instead act on
// the grabbing_actor the send already resolves -- a freshly-spawned, auto-
// grabbed Aprop_C with Key=None gets a stable Key + a PropSpawn here, then the
// existing pose stream mirrors it into the collector's hands.

#include "coop/props/trash_collect_sync.h"

#include "coop/dev/spawn_order_probe.h"  // Phase 1 step 1A: client load-spawn coverage probe (read-only)
#include "coop/props/save_identity_bind.h"     // Phase 1 step 2b: client eid-range bind (mutating)
#include "coop/props/save_identity_map.h"      // Family
#include "coop/creatures/kerfur_entity.h"  // K-5: IsKerfurActor (the held-kerfur class-gate); IsKerfurPropClass (off-prop)
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/element/portable_identity.h"
#include "coop/element/registry.h"
#include "coop/element/intent_authority.h"
#include "coop/element/quiescence_drain.h"   // ArmGhostSweep (v106b: E-press on an unbound native arms the wholesale ghost reconcile)
#include "coop/props/prop_element_tracker.h"
#include "coop/props/prop_sound.h"           // client-own-grab pickup cue (native grab suppressed -> synthesize locally)
#include "coop/props/prop_synth_key.h"
#include "coop/props/remote_prop.h"        // ResolveMirrorEidByActor (the pile-grab hook mirror eid resolve)
#include "coop/props/remote_prop_spawn.h"
#include "coop/props/join_membership_sweep.h"  // anti-smear 2026-06-30: claim+sweep extracted out of remote_prop_spawn
#include "coop/save/save_transfer.h"      // docs/piles/09: RecordGrabTimePileXform (grab-edge save-time key)
#include "coop/props/trash_channel.h"      // NoteClumpBorn (v106: the clump birth certificate; docs/piles/08)
#include "coop/props/trash_proxy.h"        // EidForAimedPileProxy (Increment 2: client-grab camera-ray cone recognition)
#include "coop/props/trash_use_intercept.h"  // B1a: the InpActEvt_use client-grab interceptor (Install/OnDisconnect delegate here)
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/game_thread.h"     // RegisterPreObserver (the InpActEvt_use pile-grab observer)
#include "ue_wrap/core/log.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"     // MainPlayerClass + MainPlayerUseInputEventFn
#include "ue_wrap/core/types.h"
#include "ue_wrap/core/ufunction_hook.h"  // the deterministic re-pile: BeginDeferred Func-patch (docs/piles/08)
#include "ue_wrap/core/cached_obj_ref.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <string>

namespace coop::trash_collect_sync {
namespace {

namespace R  = ue_wrap::reflection;
namespace PT = coop::prop_element_tracker;
namespace GT = ue_wrap::game_thread;
namespace P  = ue_wrap::profile;

// Cached Session for the pile-grab observer (the PRE callback can't take a param). Set by Install
// (re-cached every call for reconnect). Game-thread read inside the observer.
std::atomic<coop::net::Session*> g_session{nullptr};

// RULE 1+2 (2026-06-21, docs/piles/08): the v81 MORPH (proximity FindNearestChipPile land-detect) is FULLY
// RETIRED -- it mis-bound on a NEIGHBOUR pile in a dense cluster. The host-grab sync is now: the InpActEvt
// clump's identity travels via the v106 BIRTH CERTIFICATE (trash_channel::NoteClumpBorn at the BeginDeferred
// thunk); the held-edge adopts the clump onto that eid; identity is the host eid end-to-end (POSITION IS NEVER IDENTITY).
// (An earlier design caught the spawn at a host_spawn_watcher BeginDeferred POST -- RETIRED 2026-06-21: the
// chipPile/clump spawn is dispatched EX_CallMath, INVISIBLE to our ProcessEvent hook; it never fired.)

// (The proximity RE-PILE death-watch -- WatchClumpForRepile / Tick / FindNearestUntrackedChipPile_ /
// g_watchedClumps -- is RETIRED 2026-06-21, RULE 2. It converted on the clump's DEATH by a nearest-untracked
// pile search; the deterministic UFunction::Func thunk (OnBeginDeferredSpawnObserve) replaced it after a
// hands-on validated the thunk's *Result is ptr-for-ptr the same pile the death-watch found. The thunk
// converts the EXACT spawned pile the same tick -- no proximity, no reaper race. A consumed clump (no
// re-pile spawn) now just dies untracked -> the normal prop reaper PropDestroys it.)

bool g_repileThunkInstalled = false;          // process-lifetime Func-patch latch

struct AdoptionCapture {
    ue_wrap::CachedObjRef captured;
    ue_wrap::FVector capturedLocation{};
};
AdoptionCapture g_adoptionCapture;

struct PendingAdoption {
    uint64_t id = 0;
    ue_wrap::CachedObjRef actor;
    std::wstring requestedClass;
    bool ackClaimed = false;
    bool released = false;
    uint32_t hostEid = 0;
    coop::net::WireKey canonicalKey{};
    ue_wrap::FVector linearVelocity{};
    ue_wrap::FVector angularVelocity{};
    uint8_t finalPoseTicks = 0;
    std::chrono::steady_clock::time_point deadline{};
};
std::deque<PendingAdoption> g_pendingAdoptions;
constexpr size_t kMaxPendingAdoptions = 8;
constexpr auto kAdoptionTtl = std::chrono::seconds(30);
uint64_t g_nextAdoptionId = 1;

void PrunePendingAdoptions_(std::chrono::steady_clock::time_point now) {
    for (auto it = g_pendingAdoptions.begin(); it != g_pendingAdoptions.end();) {
        if (now >= it->deadline || !it->actor.Alive()) {
            UE_LOGW("prop_identity: CLIENT retiring adoption id=%llu (%s)",
                    static_cast<unsigned long long>(it->id),
                    now >= it->deadline ? "ACK timeout" : "actor died");
            it = g_pendingAdoptions.erase(it);
        } else {
            ++it;
        }
    }
}

uint64_t NextAdoptionId_() {
    for (;;) {
        const uint64_t candidate = g_nextAdoptionId++;
        if (g_nextAdoptionId == 0) g_nextAdoptionId = 1;
        if (candidate == 0) continue;
        bool inUse = false;
        for (const auto& pending : g_pendingAdoptions)
            if (pending.id == candidate) { inUse = true; break; }
        if (!inUse) return candidate;
    }
}

// THE DETERMINISTIC RE-PILE (docs/piles/08 "thunk") -- the convert, validated GREEN 2026-06-21 (host log:
// many CLEAN [REPILE], the thunk's *Result ptr-for-ptr == the death-watch's FindNearest pile every isolated
// re-pile). On a host re-pile the clump's `EX_CallMath BeginDeferredActorSpawnFromClass(self=clump, pile)`
// fires our UFunction::Func patch -> (srcObj = FFrame::Object = the re-piling clump @0x18; newActor =
// *Result = the new chipPile). If the clump is a TRACKED trash entity (eid E) we convert E onto the new pile
// IN PLACE, the SAME tick the pile is constructed -- zero proximity, zero death-watch, no reaper race (so the
// old ~5s vanish-return is gone by construction). The GRAB case (srcObj=pile, newActor=clump) is NOT a clump
// source -> ignored here; the host grab stays on the InpActEvt PRE + held-edge adopt path. A wild srcObj (a
// wrong offset -- ruled out, IDA-pinned + validated) would fault in IsGarbageClump and the facility's
// RunCbSEH absorbs it -> no convert. Game thread (BeginDeferred is GT-only); host-only.
void OnBeginDeferredSpawnObserve(void* /*context*/, void* srcObj, void* newActor) {
    auto* s = g_session.load(std::memory_order_acquire);
    // Phase 1 step 1A PROBE (read-only, CLIENT-side, before the host gate): record every keyless-family
    // load-spawn this thunk sees, so EmitVerdictAtQuiescence can test whether spawn-order catches every
    // surviving native (build plan 1A; docs/COOP_STABLE_ID_SIDECAR.md S3.2). Observe-only, mutates nothing.
    // Phase 1 step 2b BIND (CLIENT, before the host gate): same keyless-family classification feeds the 1A
    // probe (read-only) AND the eid-range bind (mutating) -- the bind binds the k-th keyless load-spawn to the
    // k-th received-map entry's host eid (save_identity_bind). Both are independently gated; one classification.
    if (newActor && s && s->connected() && s->role() == coop::net::Role::Client) {
        const bool probeOn = coop::dev::spawn_order_probe::IsEnabled();
        const bool bindOn = coop::save_identity_bind::IsEnabled();
        if (probeOn || bindOn) {
            // [18:45 wrong-type root fix 2026-07-04] A SAVE-LOAD spawn is a spawn from the GAMEMODE's
            // own frame: loadObjects/loadPrimitives run in mainGamemode's BP functions, so the thunk's
            // srcObj (FFrame::Object) is the gamemode instance (276 BeginDeferred call sites in the
            // gamemode BP). WITHOUT this gate, EVERY client chipPile spawn while connected consumed the
            // keyless ordinal cursor -- including our own wire-driven convert-LAND nativizations and
            // mirror spawns -- shifting every subsequent bind by one (client pile #k bound to host
            // eid #k+n): the mass identity misalignment behind the 18:45 wrong-type morphs. The k-th
            // GAMEMODE-sourced keyless spawn == primitivesData[k] by construction; nothing else is.
            // Class resolve is latched; a MISS is retried at most every 2s (perf-audit W-d: FindClass
            // is a full GUObjectArray walk -- an unthrottled null-retry would run it PER SPAWN while
            // mainGamemode_C is not yet loaded). Until it resolves, spawns are treated as non-load
            // (no cursor consumption) -- correct: a save-load spawn cannot precede its own gamemode.
            static void* sGmCls = nullptr;
            if (!sGmCls) {
                static std::chrono::steady_clock::time_point sNextGmClsTry{};
                const auto now = std::chrono::steady_clock::now();
                if (now >= sNextGmClsTry) {
                    sNextGmClsTry = now + std::chrono::seconds(2);
                    sGmCls = R::FindClass(L"mainGamemode_C");
                }
            }
            void* srcCls = srcObj ? R::ClassOf(srcObj) : nullptr;
            const bool fromGamemode =
                srcCls && sGmCls && R::IsDescendantOfAny(srcCls, &sGmCls, 1);
            int fam = -1;  // 0 = chipPile, 1 = kerfurOff, -1 = not a keyless family / not a load spawn
            if (fromGamemode) {
                if (ue_wrap::prop::IsChipPile(newActor)) fam = 0;
                else if (void* c = R::ClassOf(newActor); c && coop::kerfur_entity::IsKerfurPropClass(c)) fam = 1;
            }
            if (fam >= 0) {
                if (probeOn)
                    coop::dev::spawn_order_probe::NoteKeylessSpawn(
                        newActor, fam == 0 ? coop::dev::spawn_order_probe::Family::ChipPile
                                           : coop::dev::spawn_order_probe::Family::KerfurOff);
                if (bindOn)
                    coop::save_identity_bind::OnSaveLoadSpawn(
                        newActor, fam == 0 ? coop::save_identity_map::Family::ChipPile
                                           : coop::save_identity_map::Family::KerfurOff);
            }
        }
    }
    if (!s || !s->connected() || s->role() != coop::net::Role::Host) return;  // host authors converts
    if (!srcObj || !newActor) return;
    // GRAB direction (v106, 2026-07-07): EVERY clump is born from a chipPile's own
    // BeginDeferred (bytecode: actorChipPile toClump @141 + ubergraph @3493; srcObj =
    // the source pile, ALIVE at this POST -- it self-destructs after). Record the
    // birth certificate here: this is the DETERMINISTIC grab seam that fires for
    // E-press AND use-HOLD grabs (canBeUsedHold repeats with no new InpActEvt
    // dispatch -- the 10:19:03 mis-bind root: the InpActEvt PRE seam missed the
    // grab and the held-edge heuristic bound the clump to the WRONG open carry).
    // The held-edge (local_streams) consumes the certificate; the InpActEvt PRE
    // seam keeps only diagnostics + the client-side routing.
    if (ue_wrap::prop::IsChipPile(srcObj) && ue_wrap::prop::IsGarbageClump(newActor)) {
        coop::element::ElementId E = PT::GetPropElementIdForActor(srcObj);
        if (E == coop::element::kInvalidId)
            E = coop::remote_prop::ResolveMirrorEidByActor(srcObj);
        if (E == coop::element::kInvalidId) {
            // docs/piles/09 SELF-SEED (moved here from the E-press PRE, ONE owner):
            // an UNTRACKED pile grabbed in the post-blob purge gap mints its eid at
            // the very seam its clump is born (register-only, no broadcast).
            PT::MarkPropElement(srcObj, L"", R::ClassNameOf(srcObj), PT::EnrollSource::kExpressSeam);
            E = PT::GetPropElementIdForActor(srcObj);
            if (E != coop::element::kInvalidId)
                UE_LOGI("[PILE-09] HOST self-seeded UNTRACKED grabbed pile %p -> eid=%u "
                        "(thunk seam; eid-0-at-grab gap closed)",
                        srcObj, static_cast<unsigned>(E));
        }
        if (E != coop::element::kInvalidId) {
            // docs/piles/09: freeze the pile's PRE-GRAB position as the save-time key
            // (the pile has not moved yet -- it dies in place after this spawn).
            coop::save_transfer::RecordGrabTimePileXform(
                E, ue_wrap::engine::GetActorLocation(srcObj));
            coop::trash_channel::NoteClumpBorn(newActor, E, ue_wrap::prop::GetChipType(srcObj));
        }
        return;  // grab direction fully handled (the held-edge adopts)
    }
    if (!ue_wrap::prop::IsGarbageClump(srcObj)) return;   // re-pile source must be a clump
    if (!ue_wrap::prop::IsChipPile(newActor)) return;     // ... spawning a chipPile
    const coop::element::ElementId E = PT::GetPropElementIdForActor(srcObj);
    if (E == coop::element::kInvalidId) return;           // an UNTRACKED clump (grab-adopt miss; the eid=0 gap) -> skip
    // The thunk fires at the BeginDeferred POST, which is PRE-FinishSpawningActor -> newActor (the pile) is NOT
    // positioned yet: GetActorLocation(newActor) here is (0,0,0) and GetActorRotation is identity. take-31
    // proved this (host log: every "BROADCAST ToPile ... at (0.0,0.0,0.0)") -- reading newActor's transform
    // snapped every derived pile to the world origin = "disappeared". (take-30's rotation "worked" only because
    // an identity rotation LOOKS correct for a flat pile, masking the unset transform; loc came from the clump
    // then so it stayed visible.) So pass the CLUMP's transform (srcObj IS a fully-spawned, positioned actor)
    // as the FALLBACK, and re-read the PILE's REAL transform at the land-settle COMMIT (TickCarry, post-
    // FinishSpawning) in trash_channel -- that is where loc/rot/scale become valid. The clump's loc is
    // ~clump-radius high + its rotation is the tumble, so srcObj is ONLY the fallback (a committing settle
    // always has a live settled pile to re-read). [the original "not positioned yet" comment was RIGHT.]
    const ue_wrap::FVector  loc      = ue_wrap::engine::GetActorLocation(srcObj);
    const ue_wrap::FRotator rot      = ue_wrap::engine::GetActorRotation(srcObj);
    const uint8_t           chipType = ue_wrap::prop::GetChipType(srcObj);
    UE_LOGI("[PILE] HOST RE-PILE(thunk) eid=%u clump=%p -> chipPile=%p convert IN PLACE (deterministic, same "
            "tick as the spawn -- no death-watch, no proximity)", static_cast<unsigned>(E), srcObj, newActor);
    coop::trash_channel::OnHostConvert(*s, E, coop::net::propconvert_kind::kToPile, newActor, loc, rot, chipType);
}

// (The read-only dropGrabObject diagnostic thunk is RETIRED, 2026-07-10. Its
// question closed 2026-06-22: the clump release fires NEITHER simulateDrop NOR
// dropGrabObject -- the release edge is STREAM-THROUGH, not a verb (see
// local_streams.cpp "THROW ARC"), so the planned latch-close FLIP here can
// never happen. The thunk only logged noise on unrelated PHC drops and held a
// fixed-capacity Func-hook slot.)

}  // namespace

void CaptureAdoptionCandidate(void* actor) {
    g_adoptionCapture.captured.Reset();
    if (!actor || !R::IsLive(actor) || !ue_wrap::prop::IsKeyedInteractable(actor)) return;
    g_adoptionCapture.captured.Set(actor);
    g_adoptionCapture.capturedLocation = ue_wrap::engine::GetActorLocation(actor);
}

void* ConsumePendingAdoptionAck(uint64_t adoptionId, const std::wstring& cls) {
    if (adoptionId == 0) return nullptr;
    const auto now = std::chrono::steady_clock::now();
    for (auto it = g_pendingAdoptions.begin(); it != g_pendingAdoptions.end(); ++it) {
        if (it->id != adoptionId) continue;
        if (now >= it->deadline) {
            UE_LOGI("prop_identity: CLIENT ignored expired adoption ACK id=%llu",
                    static_cast<unsigned long long>(adoptionId));
            g_pendingAdoptions.erase(it);
            return nullptr;
        }
        if (it->ackClaimed) return nullptr;  // duplicate ACK
        void* actor = it->actor.Get();
        if (!actor || cls != it->requestedClass) {
            UE_LOGW("prop_identity: CLIENT adoption ACK id=%llu failed pending actor/class validation "
                    "-- retiring transaction", static_cast<unsigned long long>(adoptionId));
            g_pendingAdoptions.erase(it);
            return nullptr;
        }
        it->ackClaimed = true;
        return actor;
    }
    return nullptr;  // stale ACK: its transaction was retired/evicted
}

void ConsumePendingAdoptionRefusal(uint64_t adoptionId) {
    if (adoptionId == 0) return;
    const auto now = std::chrono::steady_clock::now();
    for (auto it = g_pendingAdoptions.begin(); it != g_pendingAdoptions.end(); ++it) {
        if (it->id != adoptionId) continue;
        if (now >= it->deadline) {
            UE_LOGI("prop_identity: CLIENT ignored expired adoption refusal id=%llu",
                    static_cast<unsigned long long>(adoptionId));
            g_pendingAdoptions.erase(it);
            return;
        }
        if (it->ackClaimed) {
            // A terminal success already won this transaction. A contradictory
            // late/duplicate refusal is stale and must not cancel its deferred release.
            UE_LOGI("prop_identity: CLIENT ignored stale adoption refusal id=%llu after ACK",
                    static_cast<unsigned long long>(adoptionId));
            return;
        }
        UE_LOGW("prop_identity: CLIENT adoption refused id=%llu -- transaction retired",
                static_cast<unsigned long long>(adoptionId));
        g_pendingAdoptions.erase(it);
        return;
    }
    // Missing means an already-retired/evicted transaction: duplicate and stale
    // refusals are deliberately side-effect free.
}

void CompletePendingAdoptionAck(uint64_t adoptionId, void* actor,
                                uint32_t hostEid, const coop::net::WireKey& canonicalKey) {
    for (auto it = g_pendingAdoptions.begin(); it != g_pendingAdoptions.end(); ++it) {
        if (it->id != adoptionId || !it->ackClaimed || it->actor.Get() != actor) continue;
        if (!it->released) {
            g_pendingAdoptions.erase(it);
            return;
        }
        it->hostEid = hostEid;
        it->canonicalKey = canonicalKey;
        it->finalPoseTicks = 2;
        return;
    }
}

bool DeferPendingAdoptionRelease(void* actor,
                                 float linX, float linY, float linZ,
                                 float angX, float angY, float angZ) {
    if (!actor) return false;
    for (auto& pending : g_pendingAdoptions) {
        if (pending.actor.Raw() != actor) continue;
        pending.released = true;
        pending.linearVelocity = {linX, linY, linZ};
        pending.angularVelocity = {angX, angY, angZ};
        return true;
    }
    return false;
}

void TickPendingAdoptionReleases(coop::net::Session* session) {
    const auto now = std::chrono::steady_clock::now();
    PrunePendingAdoptions_(now);
    if (!session || session->role() != coop::net::Role::Client) return;
    for (auto it = g_pendingAdoptions.begin(); it != g_pendingAdoptions.end(); ++it) {
        if (!it->released || !it->ackClaimed || it->hostEid == 0) continue;
        void* actor = it->actor.Get();
        if (!actor) { g_pendingAdoptions.erase(it); return; }
        if (it->finalPoseTicks != 0) {
            coop::net::PropPoseSnapshot pp{};
            pp.key = it->canonicalKey;
            pp.elementId = it->hostEid;
            const auto loc = ue_wrap::engine::GetActorLocation(actor);
            const auto rot = ue_wrap::engine::GetActorRotation(actor);
            pp.x = loc.X; pp.y = loc.Y; pp.z = loc.Z;
            pp.pitch = ue_wrap::NormalizeAxis(rot.Pitch);
            pp.yaw = ue_wrap::NormalizeAxis(rot.Yaw);
            pp.roll = ue_wrap::NormalizeAxis(rot.Roll);
            session->SetLocalPropPose(true, pp);
            --it->finalPoseTicks;
            return;  // one pose stream: oldest ready release owns this tick
        }
        session->SetLocalPropPose(false, {});
        session->SendPropRelease(it->canonicalKey,
                                 it->linearVelocity.X, it->linearVelocity.Y, it->linearVelocity.Z,
                                 it->angularVelocity.X, it->angularVelocity.Y, it->angularVelocity.Z,
                                 it->hostEid, /*ctx=*/0);
        UE_LOGI("prop_identity: CLIENT completed deferred release adoptionId=%llu eid=%u",
                static_cast<unsigned long long>(it->id), it->hostEid);
        g_pendingAdoptions.erase(it);
        return;
    }
}

bool HostAuthorizeAdoptionTarget(void* actor, uint8_t senderSlot) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || session->role() != coop::net::Role::Host || !actor || senderSlot == 0)
        return false;
    // Generic Aprop pickup uses mainPlayer.armLength=200. IntentTarget expands
    // this by the target bounds and the shared pose-staleness allowance, and
    // fails closed when the host has no live puppet for the sender.
    constexpr float kPropGrabReachUU = 200.0f;
    const auto token = coop::element::IntentTarget::ForClientIntent(
        *session, senderSlot, kPropGrabReachUU);
    const coop::element::IntentSubject subject = token.Authorize(actor);
    if (subject) return true;
    UE_LOGW("prop_identity: HOST adoption target REFUSED slot=%u reason=%s "
            "dist=%.0f allowed=%.0f", senderSlot,
            coop::element::OutcomeName(subject.outcome), subject.distUU, subject.reachUU);
    return false;
}

bool HostSendAdoptionRefusal(uint8_t requesterSlot, uint64_t adoptionId) {
    auto* session = g_session.load(std::memory_order_acquire);
    if (!session || session->role() != coop::net::Role::Host ||
        requesterSlot == 0 || requesterSlot >= coop::net::kMaxPeers || adoptionId == 0)
        return false;
    coop::net::PropAdoptionRefusalPayload refusal{};
    refusal.adoptionId = adoptionId;
    return session->SendReliableToSlot(requesterSlot,
                                       coop::net::ReliableKind::PropAdoptionRefused,
                                       &refusal, sizeof(refusal));
}

bool EnsureHeldItemBroadcast(void* heldActor, coop::net::Session* s) {
    if (!heldActor || !s || !s->connected()) return false;
    if (!R::IsLive(heldActor)) return false;
    // K-5 kerfur class-gate: a kerfur prop is a host-owned entity (the host owns its KerfurId + its
    // host-range eid). A client must NEVER express/mint a peer-range eid for it -- that was the
    // grab-dupe root (redesign Failures #1/#3/#6: the minted peer-range eid is dropped by the host's
    // host-range conversion gate, then re-mirrored as a fresh client entity -> the dupe-and-drop loop).
    // Never mint here; the held-pose stream (local_streams) instead carries the kerfur prop MIRROR by
    // its host-range eid (kerfur_entity::GetKerfurMirrorEidForActor), which the host's remote_prop
    // receiver resolves to the authoritative kerfur prop + kinematic-drives -- no PropSpawn needed. On
    // the HOST this is redundant (its kerfur prop is tracker-known -> the express skip below already
    // returns false), but gating up-front is role-agnostic + explicit.
    if (coop::kerfur_entity::IsKerfurActor(heldActor)) return false;
    // docs/piles/08 class-gate: a garbageClump is a host-authoritative trash entity -- its identity is the
    // source pile's eid E, bound by trash_channel::OnHostConvert at the convert-spawn POST; the held-pose
    // stream then carries E (local_streams). The clump must NEVER be expressed here as a FRESH-eid keyless
    // PropSpawn (that second cross-peer entity = the dupe). A clump that reaches here (a stray pickup, or a
    // pile that had no eid to convert) is simply not authored -- carry-only, no dupe.
    if (ue_wrap::prop::IsGarbageClump(heldActor)) return false;
    // Any KEYED interactable: Aprop_C trash items AND the non-Aprop_C
    // garbageClump/chipPile (the actual trash the player carries). CRASH-SAFETY
    // is enforced on the RECEIVER, not by excluding them here: GetStaticMesh
    // returns null for non-Aprop_C, so the peer never runs physics on the clump
    // mirror -- it spawns physics-free and is driven KINEMATICALLY (the inverse
    // of the reverted-2a UAF). [[project-bug-trash-chippile-uaf-crash]]
    if (!ue_wrap::prop::IsKeyedInteractable(heldActor)) return false;

    // A client never allocates a prop identity. A host-range mirror binding is
    // already the completed handshake, so the held pose can use it immediately.
    // Any local/peer-range row is provisional baggage from an older lifecycle and
    // must be replaced by the host's handback below.
    if (s->role() == coop::net::Role::Client) {
        for (auto it = g_pendingAdoptions.begin(); it != g_pendingAdoptions.end(); ++it) {
            if (it->actor.Raw() != heldActor) continue;
            it->released = false;
            it->finalPoseTicks = 0;
            // ACK already bound this actor; this re-grab supersedes the queued
            // release transaction completely.
            if (it->ackClaimed && it->hostEid != 0)
                g_pendingAdoptions.erase(it);
            break;
        }
        const coop::element::ElementId bound =
            coop::element::Registry::Get().EidForActor(heldActor);
        if (bound != coop::element::kInvalidId &&
            coop::element::Registry::IsAllowedHostAllocatedEid(bound)) {
            if (auto* e = coop::element::Registry::Get().Get(bound); e && e->IsMirror())
                return false;
        }
    }
    // PART 2 (2026-06-18) CLIENT host-authority gate for SHARED TRASH (chipPile / garbageClump /
    // trashBitsPile -- the non-Aprop_C "world litter"). These are HOST-OWNED shared entities. When a
    // CLIENT grabs a host pile, its BP locally morphs it to a clump (EX_CallMath, un-hookable) and this
    // path would SendPropSpawn that clump -> the host fresh-spawns a DUPLICATE, and on the clump's later
    // land-convert (BroadcastConvertNear, reachable only if we WatchClump below) the host spawns yet
    // another pile = "a new pile born from the host's original, from the host's perspective." The client
    // must NEVER author a shared trash entity -- the exact host-authority rule the K-5 kerfur gate above
    // enforces. Suppressing here ALSO disarms the clump-convert watch (WatchClump is only reached past
    // this return), so neither the clump PropSpawn nor the PropConvert ever leaves the client -> the dupe
    // is killed at the SOURCE. The client's grab already removed the host's original via OnPileGrabPre's
    // PropDestroy (eid-resolved on the mirror), so the original IS removed -- no orphan. The client holds
    // the clump locally; a client-THROWN ground pile staying client-local is a known phase-2 gap (the
    // host-request spawn model) -- a benign divergence, NOT a dupe. Aprop_C items (IsDescendantOfProp)
    // are unaffected (per-player carry stays). The HOST still authors its own held trash below.
    // (Audit M1, 2026-06-18: this suppresses all 3 shared-trash bases but OnPileGrabPre's host-original
    // PropDestroy covers only chipPile -- safe because the other two are never carried-as-a-host-mirror:
    // a trashBitsPile dispenses an Aprop item + a separately-synced counter (not a held clump), and a
    // garbageClump only ever exists as a chipPile's morph product, whose chipPile grab already fired
    // OnPileGrabPre. So no host original is ever left orphaned.)
    if (s->role() == coop::net::Role::Client && !ue_wrap::prop::IsDescendantOfProp(heldActor)) {
        UE_LOGI("trash_collect: CLIENT holds shared trash cls='%ls' -- NOT authoring it (host owns world "
                "piles/clumps; the grab's PropDestroy already removed the host original). No dupe.",
                R::ClassNameOf(heldActor).c_str());
        return false;
    }

    // A divergence sweep is pending and this held actor is one of its candidates
    // (an in-universe, unclaimed save-loaded local the host did NOT express -- a
    // ghost awaiting adjudication, e.g. the save-transfer kerfur the host turned
    // ON before this client joined). Do NOT express it: a SendPropSpawn below
    // would fresh-spawn a host duplicate, and the RecordClaimIfTracking after it
    // would claim the ghost past the pending sweep (permanently rescuing it). Let
    // the deferred sweep destroy it. A genuinely client-originated drop is claimed
    // via the takeObj path, so it is NOT a candidate and streams normally here.
    if (coop::join_membership_sweep::IsPendingSweepCandidate(heldActor)) {
        UE_LOGI("trash_collect: held item %p is a pending divergence-sweep candidate "
                "(unclaimed ghost) -- NOT expressing (the deferred sweep will destroy it)", heldActor);
        return false;
    }

    // Pre-quiescence JOIN WINDOW (2026-06-16 hands-on, the "kerfur off+grab dupe"). The guard above
    // only fires once the divergence sweep is ARMED (g_sweepPending). But a save-loaded in-universe
    // keyed prop the client grabs in the ~25 s window BEFORE the snapshot bracket opens slips past it
    // (g_sweepPending still false) -- and expressing it here SendPropSpawns a host duplicate the
    // client-side sweep can NEVER reclaim (it ran: host fresh-spawned a kerfur prop, eid 43938,
    // ownerSlot=1, permanent). Same divergence-universe membership, just not yet sweep-armed: until
    // load-tail quiescence the world is NOT reconciled, so a grabbed un-adjudicated local is not ours
    // to announce. Keep holding it locally (no pop); the host expresses its OWN copy in the bracket
    // (our held copy is then key/fuzzy-matched + claimed -> the held-pose stream mirrors it, no dupe),
    // or the sweep destroys our copy if the host converted it away. HasLoadTailQuiesced flips true the
    // instant the sweep fires, so this gates ONLY the join window -- never steady-state gameplay.
    // CLIENT-ONLY (v105 root-fix companion, 2026-07-06): the divergence sweep is a CLIENT join
    // mechanism -- on the HOST g_sweepFired never flips, so this gate held FOREVER and silenced
    // every untracked keyed prop the host ever held (the never-mirrored-hotbar-hand log,
    // 12:40:05 'grabbed PRE-QUIESCENCE' on a 40-minute-old host session). The host's world IS
    // the reconciled authority by definition -- the un-adjudicated-local concept only exists on
    // a joining client. Hand items themselves no longer reach here (coop/player/hand_item), but
    // the express-if-unknown self-heal below must work for a host-grabbed world straggler.
    if (s->role() == coop::net::Role::Client &&
        !coop::join_membership_sweep::HasLoadTailQuiesced() &&
        coop::join_membership_sweep::IsInDivergenceUniverseUnclaimed(heldActor)) {
        UE_LOGI("trash_collect: held item %p cls='%ls' grabbed PRE-QUIESCENCE (join window not yet "
                "reconciled) -- NOT expressing (host expresses its own / the sweep adjudicates ours; "
                "prevents the off+grab host dupe)", heldActor, R::ClassNameOf(heldActor).c_str());
        return false;
    }

    // Express-if-unknown (ghost-twin fix, 2026-06-10 hands-on forensics). The
    // old predicate skipped ANY keyed held actor as "a normal world prop the
    // peer already has" -- but "has a key" is NOT "peer has it": a prop that
    // materialized in the world-load's late tail (keyless at sweep/seed time,
    // key self-minted by its Init AFTER both) is keyed yet completely unknown
    // to the wire -- never expressed, never bound, eid=0. The user carrying
    // one was invisible to the host (127 'no local match' PropPose warns) and
    // the wall stack diverged one-way. The correct skip test is TRACKER-KNOWN:
    // a prop with a live Element (seed-walked / Init-announced / previously
    // expressed) is genuinely shared -- the pose stream alone mirrors it. An
    // untracked one gets expressed RIGHT HERE, key and all, the moment a
    // player first touches it -- the self-heal for any straggler class.
    // (The peer's OnSpawn may fuzzy-bind it to a nearby same-class prop; for a
    // genuine EXTRA actor that is bounded weirdness vs the strictly-worse
    // one-way invisibility. The structural fix for the late-tail stragglers
    // themselves is tracked separately -- see the sweep's keyless histogram.)
    std::wstring keyStr = ue_wrap::prop::GetInteractableKeyString(heldActor);
    if (!keyStr.empty() && keyStr != L"None" &&
        PT::GetPropElementIdForActor(heldActor) != coop::element::kInvalidId &&
        s->role() != coop::net::Role::Client) {
        // [ROCK-DROP DIAG 2026-07-08, RULE-2-exempt] "pose stream suffices" is a promise
        // that ONLY holds while the prop is ACTIVELY held+streamed. A caller expressing a
        // no-longer-streamed prop (a just-released/E-grab-then-idle prop) gets a false
        // promise here -- nothing re-arms the stream. Log key+eid so a repro can tell a
        // tracker-known DECLINE (this) from a never-received (client-spawn skip elsewhere).
        UE_LOGI("[ROCK-DROP] EnsureHeldItemBroadcast DECLINE (tracker-known): cls='%ls' key='%ls' eid=%u "
                "-- 'pose stream suffices' assumes an active stream; false if the prop is no longer held",
                R::ClassNameOf(heldActor).c_str(), keyStr.c_str(),
                static_cast<unsigned>(PT::GetPropElementIdForActor(heldActor)));
        return false;  // keyed AND tracker-known: the peer has it; pose stream suffices
    }

    const std::wstring cls = R::ClassNameOf(heldActor);

    // CLIENT IDENTITY REQUEST. This reuses PropSpawn's complete class/physics
    // description and the already-proven host handback, but elementId=0 plus
    // kAdoptionRequest changes its meaning from "allocate my entity" to "name
    // this existing world actor". No client Element is minted, and no gameplay
    // Key is force-created. The pre-grab position and cooked-level locator let
    // the host resolve the concrete counterpart even when UE minted different
    // per-process Keys. Pose/release remain silent until the host PropSpawn ACK
    // binds a host-range eid.
    if (s->role() == coop::net::Role::Client) {
        PrunePendingAdoptions_(std::chrono::steady_clock::now());
        // A rapid re-grab of the same actor reuses its still-live transaction;
        // reliable delivery already owns retransmission and a second token would
        // create two valid ACKs for one actor.
        for (auto it = g_pendingAdoptions.begin(); it != g_pendingAdoptions.end(); ++it) {
            if (it->actor.Raw() == heldActor) {
                if (it->ackClaimed && it->hostEid != 0) {
                    // The authoritative mirror already exists. Re-grabbing
                    // cancels the queued deferred release and retires only this
                    // completed transaction; the held-edge EID lookup below
                    // resumes normal pose streaming on the bound identity.
                    UE_LOGI("prop_identity: CLIENT re-grab canceled deferred release id=%llu eid=%u",
                            static_cast<unsigned long long>(it->id), it->hostEid);
                    g_pendingAdoptions.erase(it);
                } else {
                    it->released = false;
                    it->finalPoseTicks = 0;
                }
                return true;
            }
        }
        coop::net::PropSpawnPayload req{};
        for (size_t i = 0; i < cls.size() && i < 63; ++i)
            req.className.data[req.className.len++] = static_cast<char>(cls[i]);
        if (keyStr != L"None") {
            for (size_t i = 0; i < keyStr.size() && i < 31; ++i)
                req.key.data[req.key.len++] = static_cast<char>(keyStr[i]);
        }
        const std::wstring levelKey = coop::element::PortableLevelActorWireKey(heldActor);
        for (size_t i = 0; i < levelKey.size() && i < 31; ++i)
            req.propName.data[req.propName.len++] = static_cast<char>(levelKey[i]);
        ue_wrap::FVector origin = ue_wrap::engine::GetActorLocation(heldActor);
        if (g_adoptionCapture.captured.Get() == heldActor)
            origin = g_adoptionCapture.capturedLocation;
        // The locator is copied into this transaction now; never let the
        // single input-edge capture leak into a later re-grab/request.
        g_adoptionCapture.captured.Reset();
        req.hasMatchPos = 1;
        req.matchX = origin.X; req.matchY = origin.Y; req.matchZ = origin.Z;
        const auto loc = ue_wrap::engine::GetActorLocation(heldActor);
        const auto rot = ue_wrap::engine::GetActorRotation(heldActor);
        const auto scl = ue_wrap::engine::GetActorScale3D(heldActor);
        req.locX = loc.X; req.locY = loc.Y; req.locZ = loc.Z;
        req.rotPitch = ue_wrap::NormalizeAxis(rot.Pitch);
        req.rotYaw = ue_wrap::NormalizeAxis(rot.Yaw);
        req.rotRoll = ue_wrap::NormalizeAxis(rot.Roll);
        req.scaleX = scl.X; req.scaleY = scl.Y; req.scaleZ = scl.Z;
        req.physFlags = coop::net::propspawn_flags::kAdoptionRequest;
        req.elementId = 0;
        req.adoptionId = NextAdoptionId_();
        if (g_pendingAdoptions.size() >= kMaxPendingAdoptions) {
            UE_LOGW("prop_identity: CLIENT pending adoption cap=%zu -- refusing new request id=%llu "
                    "without evicting an in-flight transaction",
                    kMaxPendingAdoptions, static_cast<unsigned long long>(req.adoptionId));
            return false;
        }
        PendingAdoption pending{};
        pending.id = req.adoptionId;
        pending.actor.Set(heldActor);
        pending.requestedClass = cls;
        pending.deadline = std::chrono::steady_clock::now() + kAdoptionTtl;
        g_pendingAdoptions.push_back(pending);
        if (!s->SendPropSpawn(req)) {
            g_pendingAdoptions.pop_back();
            UE_LOGW("prop_identity: CLIENT adoption REQUEST id=%llu could not enter reliable lane",
                    static_cast<unsigned long long>(req.adoptionId));
            return false;
        }
        UE_LOGI("prop_identity: CLIENT adoption REQUEST id=%llu actor=%p cls='%ls' keyHint='%ls' "
                "levelKey='%ls' match=(%.1f,%.1f,%.1f); pose held until host identity ACK",
                static_cast<unsigned long long>(req.adoptionId), heldActor,
                cls.c_str(), keyStr.c_str(), levelKey.c_str(),
                origin.X, origin.Y, origin.Z);
        return true;
    }

    // Aprop_C trash items get a force-minted Key (their UCS holds it). The non-Aprop_C
    // trash CLUMP (prop_garbageClump_C) is NON-KEYABLE -- setKey doesn't stick (the
    // source re-reads None, autotest 33e7f25) -- so it rides the SAME prop pipeline but
    // is identified by our EID instead: broadcast with key=None + the eid, and the
    // receiver resolves its mirror by eid (PropPoseSnapshot.elementId, v26). The clump
    // renders on its own (bare spawn = the 'dirtball' mesh), so no mesh transfer needed.
    // [[project-bug-trash-chippile-uaf-crash]]
    const bool isAprop = ue_wrap::prop::IsDescendantOfProp(heldActor);
    keyStr = coop::prop_synth_key::EnsureKeyForBroadcast(heldActor, keyStr, /*mintForAprop=*/isAprop);
    if (keyStr.empty() || keyStr == L"None") {
        if (isAprop) {
            UE_LOGW("trash_collect: Aprop item %p cls='%ls' Key still None after force-mint -- cannot mirror",
                    heldActor, cls.c_str());
            return false;
        }
        keyStr.clear();  // clump: wire key stays None; the eid is the cross-peer identity
    }
    // Create the Prop Element shadow + dedupe latch so the item's eventual
    // K2_DestroyActor unwinds it through the normal destroy path.
    PT::MarkProcessedInit(heldActor);
    PT::MarkPropElement(heldActor, keyStr, cls, PT::EnrollSource::kExpressSeam);

    coop::net::PropSpawnPayload p{};
    p.className.len = 0;
    for (size_t i = 0; i < cls.size() && i < 63; ++i)
        p.className.data[p.className.len++] = static_cast<char>(cls[i]);
    p.key.len = 0;
    for (size_t i = 0; i < keyStr.size() && i < 31; ++i)
        p.key.data[p.key.len++] = static_cast<char>(keyStr[i]);

    const ue_wrap::FVector  loc = ue_wrap::engine::GetActorLocation(heldActor);
    const ue_wrap::FRotator rot = ue_wrap::engine::GetActorRotation(heldActor);
    p.locX = loc.X; p.locY = loc.Y; p.locZ = loc.Z;
    p.rotPitch = ue_wrap::NormalizeAxis(rot.Pitch);
    p.rotYaw   = ue_wrap::NormalizeAxis(rot.Yaw);
    p.rotRoll  = ue_wrap::NormalizeAxis(rot.Roll);
    // v54: real scale (was hardcoded 1,1,1) + identity row below.
    const ue_wrap::FVector scl = ue_wrap::engine::GetActorScale3D(heldActor);
    p.scaleX = scl.X; p.scaleY = scl.Y; p.scaleZ = scl.Z;
    // Carry the trash VARIANT so the mirror shows the same chip/clump type the owner
    // grabbed (else a bare spawn defaults to variant 0 = the wrong-type bug). 0 for any
    // non-trash actor (GetChipType is a reflection no-op without a chipType property).
    p.chipType = ue_wrap::prop::GetChipType(heldActor);
    p.physFlags = coop::net::propspawn_flags::kSimulatePhysics;
    // IsHeavy/IsFrozen read Aprop_C struct offsets -- only meaningful on Aprop_C.
    // For a non-Aprop_C clump the receiver ignores physFlags (kinematic mirror),
    // so don't read stray bytes off it.
    p.propName.len = 0;
    if (ue_wrap::prop::IsDescendantOfProp(heldActor)) {
        if (ue_wrap::prop::IsHeavy(heldActor))  p.physFlags |= coop::net::propspawn_flags::kIsHeavy;
        if (ue_wrap::prop::IsFrozen(heldActor)) p.physFlags |= coop::net::propspawn_flags::kFrozen;
        if (ue_wrap::prop::IsStatic(heldActor)) p.physFlags |= coop::net::propspawn_flags::kStatic;
        if (ue_wrap::prop::IsSleeping(heldActor)) p.physFlags |= coop::net::propspawn_flags::kSleep;
        if (ue_wrap::prop::ReadRemoveWOrespawn(heldActor)) {
            p.physFlags |= coop::net::propspawn_flags::kRemoveWOrespawn;
        }
        // v54 identity: the list_props row -- a held GENERIC prop_C (e.g. a
        // cubicle gib panel) must mirror as itself, not the CDO 'cube'.
        const std::wstring nm = ue_wrap::prop::GetPropNameString(heldActor);
        for (size_t i = 0; i < nm.size() && i < 31; ++i) {
            p.propName.data[p.propName.len++] = static_cast<char>(nm[i]);
        }
    }
    p.initLinVelX = p.initLinVelY = p.initLinVelZ = 0.f;
    p.initAngVelX = p.initAngVelY = p.initAngVelZ = 0.f;
    {
        const coop::element::ElementId eid = PT::GetPropElementIdForActor(heldActor);
        p.elementId = (eid == coop::element::kInvalidId) ? 0u : eid;
    }
    UE_LOGI("trash_collect: BROADCAST held untracked item cls='%ls' key='%ls' loc=(%.1f,%.1f,%.1f) "
            "-- held-pose stream now mirrors it into the collector's hands",
            cls.c_str(), keyStr.c_str(), p.locX, p.locY, p.locZ);
    s->SendPropSpawn(p);
    // Fork B 2c: self-claim -- this peer just wire-expressed the held item;
    // an open bracket's sweep must not destroy it as "unclaimed".
    coop::join_membership_sweep::RecordClaimIfTracking(heldActor);
    // (v81 MORPH V2: the former clump WatchClump enroll is GONE -- clumps return early above; the
    // bind-model pile_morph owns the clump's identity + its land conversion.)
    return true;
}

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);  // re-cache every call (reconnect)

    // The deterministic re-pile thunk (READ-ONLY observe pass this cut): patch
    // BeginDeferredActorSpawnFromClass's UFunction::Func so we catch the clump's chipPile spawn
    // (EX_CallMath -> invisible to ProcessEvent). Independent latch -> retries until GameplayStatics
    // resolves, regardless of the grab-observer state. host_spawn_watcher's POST observer on the SAME
    // UFunction is unaffected (separate mechanism; our forwarder is transparent + the pinecone path's
    // result is not a chipPile/clump, so our cb ignores it).
    if (!g_repileThunkInstalled) {
        void* gsCls = R::FindClass(P::name::GameplayStaticsClass);
        void* bdFn  = gsCls ? R::FindFunction(gsCls, P::name::BeginDeferredSpawnFn) : nullptr;
        if (bdFn) {
            ue_wrap::ufunction_hook::InstallPostHook(bdFn, &OnBeginDeferredSpawnObserve);
            g_repileThunkInstalled = true;  // latch on attempt (InstallPostHook logs success/refusal)
            UE_LOGI("trash_collect: re-pile Func-patch armed on BeginDeferred -- the DETERMINISTIC converter "
                    "(clump re-pile -> OnHostConvert ToPile the same tick as the spawn; death-watch retired)");
        }
        // bdFn null -> GameplayStatics not loaded yet; retry next world-gated Install call.
    }

    // (The dropGrabObject diagnostic thunk install is RETIRED, 2026-07-10 -- see the
    // retirement note above OnBeginDeferredSpawnObserve's sibling comment: the clump
    // release uses no catchable verb; the stream-through design replaced it.)

    // The InpActEvt_use interceptor family (client-grab bridge on _41, the _38/_42 use_deny
    // suppressors, and the _58/_59 LMB hard-throw bridge) lives in trash_use_intercept
    // (modularization B1a). Delegate its install -- it caches its own session + latches
    // independently and retries the same way until mainPlayer_C is loaded.
    coop::trash_use_intercept::Install(session);
}

void OnDisconnect() {
    g_session.store(nullptr, std::memory_order_release);
    OnClientWorldReady();
    coop::trash_use_intercept::OnDisconnect();  // clears its cached session + gesture-pairing latch
}

void OnClientWorldReady() {
    g_adoptionCapture.captured.Reset();
    if (!g_pendingAdoptions.empty()) {
        UE_LOGI("prop_identity: CLIENT world reset retired %zu pending adoption transaction(s)",
                g_pendingAdoptions.size());
    }
    g_pendingAdoptions.clear();
}

bool DebugSendGrabIntent(uint32_t eid) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running() || s->role() != coop::net::Role::Client) {
        UE_LOGW("trash_collect: DebugSendGrabIntent eid=%u -- no running client session", eid);
        return false;
    }
    coop::trash_channel::SendGrabIntent(*s, eid);
    return true;
}

bool DebugSendThrowIntent(uint32_t eid) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->running() || s->role() != coop::net::Role::Client) {
        UE_LOGW("trash_collect: DebugSendThrowIntent eid=%u -- no running client session", eid);
        return false;
    }
    coop::trash_channel::SendThrowIntent(*s, eid, coop::net::throw_mode::kRelease, ue_wrap::FVector{});
    return true;
}

}  // namespace coop::trash_collect_sync
