// coop/creatures/npc_sync_install.cpp -- the staged reflection resolve + observer/interceptor
// registration bootstrap of the NPC spawn seam, plus its readiness/refs surface.
//
// EXTRACTED from npc_sync.cpp 2026-07-19 (s28 modular cut; the TU was 989 LOC, past the 800
// soft cap). Owns the install-side state: the g_installed latch, the resolve caches, the
// observer latches (accessor-with-its-state, the s27 session_runtime shape). The five shared
// globals the seam TU's hot paths read (3 param offsets + g_npcAllowlist +
// g_npcSyncDisabledThisProcess) are DEFINED here (the writer TU) and extern-declared in
// npc_sync_internal.h; the three ProcessEvent callbacks this TU registers are defined in
// npc_sync.cpp and declared there too. Public declarations (Install / IsInstalled /
// GetDevSpawnRefs / IsHostNpcSyncDisabled) stay in npc_sync.h; bodies are verbatim except the
// ONE enumerated edit: Install's first line calls the public SetSession(session) (whose body
// is byte-for-byte the g_session_ptr store it replaces -- g_session_ptr stays private to
// npc_sync.cpp).
//
// Game-thread only (Install runs from the net-pump tick).

#include "coop/creatures/npc_sync.h"
#include "npc_sync_internal.h"  // the 5 shared globals (defined below) + the 3 callback decls

#include "coop/creatures/npc_mirror.h"      // ClientRefs / SetClientRefs (receiver-side cache push)
#include "coop/creatures/npc_world_enum.h"  // InstallExSpawnCatch (the EX_CallMath spawn catch)
#include "coop/element/object_scan_hub.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace coop::npc_sync {

namespace P = ue_wrap::profile;
namespace R = ue_wrap::reflection;

// ---- install-owned state (moved verbatim from npc_sync.cpp; the five globals below that
// ---- npc_sync_internal.h externs are the SHARED subset -- defined here, read by the seam TU).

// Idempotency: once installed (or permanently failed) we short-circuit.
// std::atomic for memory-model correctness -- Install runs on the game
// thread, the interceptor/observer paths can read this indirectly via
// other latches that depend on Install having completed.
std::atomic<bool> g_installed{false};

// NPC UClass* pointers (the 12 enemy classes -- zombie/kerfur/krampus/funguy/
// goreSlither/insomniacs/fossilhounds/antibreathers/orborbs + 3 ariral variants
// -- plus the additions killerwisp_C/ventCrawler_C), resolved at install
// from the kNpcAllowlist names in sdk_profile.h. Sized by kNpcAllowlistSize
// so a future addition to the allowlist constant binds the array length too.
std::atomic<void*> g_npcAllowlist[ue_wrap::profile::name::kNpcAllowlistSize] = {};

void* g_npcSpawnFn = nullptr;
int32_t g_npcSpawnActorClassParamOff = -1;
int32_t g_npcSpawnReturnParamOff = -1;
int32_t g_npcSpawnXformParamOff = -1;  // SpawnTransform; cached at Install time

// Install bookkeeping for the receiver-side cache (M-1 2026-05-29 split):
// the client-side UFunction refs LIVE in coop::npc_mirror via
// SetClientRefs(). These two locals are kept here only because the
// resolution happens in two stages (GameplayStatics primitives first,
// then AActor::K2_DestroyActor once the AActor class binds), and we
// need to push the FULL set together when k2DestroyFn lands. Receivers
// in npc_mirror DO NOT read these; the canonical copy is on the
// receiver side.
void* g_installCacheFinishSpawnFn = nullptr;
void* g_installCacheGsCdo = nullptr;

// K2_DestroyActor PRE observer handle bookkeeping (so a second Install()
// doesn't double-register). std::atomic so the host PRE interceptor can
// read these from a parallel-anim worker without UB while the game thread
// writes them in Install().
std::atomic<bool> g_destroyObserverInstalled{false};
std::atomic<bool> g_spawnPostObserverInstalled{false};
// True after Install permanently gave up registering one of the lifecycle
// observers (table full). PRE gates host-side EntitySpawn broadcasts on
// `!g_npcSyncDisabledThisProcess` so a partial-lifecycle install never
// leaks Npc Elements (allocated by PRE, never bound or destroyed).
std::atomic<bool> g_npcSyncDisabledThisProcess{false};

namespace {

std::atomic<size_t> g_allowlistResolvedCount{0};

struct AllowlistResolution {
    size_t resolved = 0;
    size_t missing = 0;
    long long elapsedMs = 0;
};

AllowlistResolution ResolveInitialAllowlistClasses() {
    const auto started = std::chrono::steady_clock::now();
    AllowlistResolution result{};

    for (size_t i = 0; i < P::name::kNpcAllowlistSize; ++i) {
        if (g_npcAllowlist[i].load(std::memory_order_acquire)) {
            ++result.resolved;
            continue;
        }

        void* const cls = R::FindClass(P::name::kNpcAllowlist[i]);
        if (cls) {
            g_npcAllowlist[i].store(cls, std::memory_order_release);
            ++result.resolved;
        } else {
            ++result.missing;
            UE_LOGI("npc-suppress: allowlist class '%ls' is not loaded -- awaiting sliced discovery",
                    P::name::kNpcAllowlist[i]);
        }
    }

    result.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - started).count();
    g_allowlistResolvedCount.store(result.resolved, std::memory_order_release);
    return result;
}

size_t ResolvedAllowlistCount() {
    return g_allowlistResolvedCount.load(std::memory_order_acquire);
}

bool HubReady() { return true; }
bool HubMatchesNoInstances(void*) { return false; }
void HubPassBegin(void*, bool) {}
void HubMatch(void*, void*) {}
size_t HubPassComplete(void*, bool, uint32_t) { return ResolvedAllowlistCount(); }

void HubClassSeen(void*, void* cls) {
    if (!cls || ResolvedAllowlistCount() == P::name::kNpcAllowlistSize) return;
    const auto& name = R::NameOf(cls);
    for (size_t i = 0; i < P::name::kNpcAllowlistSize; ++i) {
        if (g_npcAllowlist[i].load(std::memory_order_acquire)) continue;
        if (!R::NameEquals(name, P::name::kNpcAllowlist[i])) continue;
        void* expected = nullptr;
        if (g_npcAllowlist[i].compare_exchange_strong(
                expected, cls, std::memory_order_release, std::memory_order_acquire)) {
            g_allowlistResolvedCount.fetch_add(1, std::memory_order_release);
            UE_LOGI("npc-suppress: late-loaded allowlist class '%ls' resolved @ %p",
                    P::name::kNpcAllowlist[i], cls);
        }
    }
}

void RegisterLateAllowlistDiscovery() {
    static bool sRegistered = false;
    if (sRegistered) return;
    sRegistered = true;
    coop::element::scan_hub::Register(coop::element::scan_hub::Consumer{
        "npc_allowlist_classes", nullptr, &HubReady, &HubMatchesNoInstances,
        &HubPassBegin, &HubMatch, &HubPassComplete, /*settleScans*/ 2, &HubClassSeen});
}

}  // namespace

bool IsInstalled() {
    // True once the core spawn seam + lifecycle install attempt completed. Lazy allowlist
    // classes may still be null and resolve later; already-resolved entries are fully usable.
    // Deliberately ignores g_npcSyncDisabledThisProcess (interceptor-disabled but class matching
    // still usable): callers must not block unrelated prop/door replay on this lifecycle seam.
    return g_installed.load(std::memory_order_acquire);
}

void Install(coop::net::Session* session) {
    SetSession(session);  // cache (caller guarantees outlives us)
    if (g_installed.load(std::memory_order_acquire)) return;
    // Preserve the startup guard for the core GameplayStatics dependency. This is normally
    // present once worldUp is true, but if it is not, its FindClass miss is also a full object
    // walk and must not run every frame. This pre-install retry remains tick-based because no
    // interceptor or initial allowlist pass can become usable until the core seam exists.
    static int s_coreInstallRetryCountdown = 0;
    if (s_coreInstallRetryCountdown > 0) {
        --s_coreInstallRetryCountdown;
        return;
    }
    // Cache the core reflection seam once resolved so a partial startup attempt never repeats
    // those lookups. Lazy allowlist entries have their independent steady-clock deadline below.
    if (!g_npcSpawnFn) {
        void* gsCls = R::FindClass(P::name::GameplayStaticsClass);
        if (!gsCls) {
            s_coreInstallRetryCountdown = 60;
            return;
        }
        void* fn = R::FindFunction(gsCls, P::name::BeginDeferredSpawnFn);
        if (!fn) {
            UE_LOGW("npc-suppress: %ls.%ls UFunction not found -- disabled permanently",
                    P::name::GameplayStaticsClass, P::name::BeginDeferredSpawnFn);
            g_installed.store(true, std::memory_order_release);
            return;
        }
        const int32_t classOff = R::FindParamOffset(fn, L"ActorClass");
        if (classOff < 0) {
            UE_LOGW("npc-suppress: %ls.%ls 'ActorClass' param not found (BP recook?) -- disabled",
                    P::name::GameplayStaticsClass, P::name::BeginDeferredSpawnFn);
            g_installed.store(true, std::memory_order_release);
            return;
        }
        const int32_t retOff = R::FindParamOffset(fn, L"ReturnValue");
        if (retOff < 0) {
            UE_LOGW("npc-suppress: %ls.%ls 'ReturnValue' param not found -- disabled",
                    P::name::GameplayStaticsClass, P::name::BeginDeferredSpawnFn);
            g_installed.store(true, std::memory_order_release);
            return;
        }
        const int32_t xformOff = R::FindParamOffset(fn, L"SpawnTransform");
        if (xformOff < 0) {
            UE_LOGW("npc-suppress: %ls.%ls 'SpawnTransform' param not found -- "
                    "EntitySpawn broadcasts will lack position/rotation",
                    P::name::GameplayStaticsClass, P::name::BeginDeferredSpawnFn);
            // Don't bail -- position-less spawns still work.
        }
        // Inc3 receiver side (now in coop::npc_mirror): resolve
        // FinishSpawningActor + GameplayStatics CDO so OnEntitySpawn can
        // materialize mirrors without re-walking GUObjectArray on every
        // host broadcast. Non-fatal if missing (OnEntitySpawn null-checks
        // each field and logs).
        void* finishFn = R::FindFunction(gsCls, P::name::FinishSpawningActorFn);
        if (!finishFn) {
            UE_LOGW("npc-sync[receiver]: %ls.%ls UFunction not found -- client mirror "
                    "materialization will be disabled (host EntitySpawn packets will be "
                    "logged + dropped)",
                    P::name::GameplayStaticsClass, P::name::FinishSpawningActorFn);
        }
        void* gsCdo = R::FindClassDefaultObject(P::name::GameplayStaticsClass);
        if (!gsCdo) {
            UE_LOGW("npc-sync[receiver]: %ls CDO not found -- client mirror materialization "
                    "will be disabled",
                    P::name::GameplayStaticsClass);
        }
        // Commit the core reflection cache before the one initial allowlist pass below.
        g_npcSpawnFn = fn;
        g_npcSpawnActorClassParamOff = classOff;
        g_npcSpawnReturnParamOff = retOff;
        g_npcSpawnXformParamOff = xformOff;
        // Stash for the second push (where k2DestroyFn lands together);
        // also push the partial set NOW so the receiver can degrade
        // gracefully (drop with warning) if the install retries before
        // K2_DestroyActor resolves.
        g_installCacheFinishSpawnFn = finishFn;
        g_installCacheGsCdo         = gsCdo;
        coop::npc_mirror::ClientRefs refs{};
        refs.spawnFn             = fn;
        refs.finishSpawnFn       = finishFn;
        refs.gsCdo               = gsCdo;
        refs.spawnReturnParamOff = retOff;
        refs.k2DestroyFn         = nullptr;  // resolved + re-pushed below
        coop::npc_mirror::SetClientRefs(refs);
    }
    void* const fn = g_npcSpawnFn;
    const int32_t classOff = g_npcSpawnActorClassParamOff;
    const int32_t retOff = g_npcSpawnReturnParamOff;
    const int32_t xformOff = g_npcSpawnXformParamOff;

    // Resolve every class currently loaded once, without making a lazy event class an
    // all-or-nothing install gate. Missing slots are later filled only by the shared sliced hub.
    const AllowlistResolution allowlist = ResolveInitialAllowlistClasses();
    RegisterLateAllowlistDiscovery();
    UE_LOGI("npc-suppress: initial allowlist resolution complete: resolved=%zu/%zu missing=%zu "
            "lookup=%lld ms%s",
            allowlist.resolved, P::name::kNpcAllowlistSize, allowlist.missing,
            allowlist.elapsedMs,
            allowlist.missing ? "; late rows delegated to sliced discovery" : "");

    // Cache the function pointer + offsets. The loaded allowlist subset is enough to make
    // the interceptor useful; lazy null slots do not weaken matching for resolved classes.
    // Lifecycle observers go in FIRST -- if either RegisterX fails (observer
    // table full), we set g_npcSyncDisabledThisProcess and SKIP the
    // RegisterInterceptor call below, so we don't burn a permanent interceptor
    // slot for a system that can't function.
    g_npcSpawnFn = fn;
    g_npcSpawnActorClassParamOff = classOff;
    g_npcSpawnReturnParamOff = retOff;
    g_npcSpawnXformParamOff = xformOff;  // may be -1 if param missing; interceptor null-checks

    // ATOMIC two-observer registration: if EITHER lifecycle observer
    // registration fails, the other is rolled
    // back so we don't burn a permanent slot for a system that's disabled.
    // Pre-resolve K2_DestroyActor's reflection dependencies FIRST -- if they
    // fail, we skip POST entirely (no rollback needed).
    if (!g_spawnPostObserverInstalled.load(std::memory_order_acquire) ||
        !g_destroyObserverInstalled.load(std::memory_order_acquire)) {
        void* actorCls = R::FindClass(P::name::ActorClassName);
        void* destroyFn = actorCls ? R::FindFunction(actorCls, P::name::DestroyActorFn) : nullptr;
        if (!actorCls || !destroyFn) {
            g_npcSyncDisabledThisProcess.store(true, std::memory_order_release);
            UE_LOGE("npc-sync: cannot resolve %ls.%ls (actorCls=%p destroyFn=%p) -- NPC sync "
                    "DISABLED for process lifetime (Element lifecycle cannot close)",
                    P::name::ActorClassName, P::name::DestroyActorFn, actorCls, destroyFn);
        } else {
            // Promote K2_DestroyActor into the Inc3 receiver-side cache
            // owned by coop::npc_mirror (OnEntityDestroy reuses it on
            // every host teardown -- skipping the GUObjectArray walk per
            // packet). Re-push the full ClientRefs so the receiver sees
            // spawnFn/finishFn/gsCdo/returnOff (cached during the
            // GameplayStatics resolution block above) together with the
            // newly resolved k2DestroyFn.
            {
                coop::npc_mirror::ClientRefs refs{};
                refs.spawnFn             = g_npcSpawnFn;
                refs.finishSpawnFn       = g_installCacheFinishSpawnFn;
                refs.gsCdo               = g_installCacheGsCdo;
                refs.spawnReturnParamOff = g_npcSpawnReturnParamOff;
                refs.k2DestroyFn         = destroyFn;
                coop::npc_mirror::SetClientRefs(refs);
            }
            // POST observer first; if it succeeds, register K2_DestroyActor PRE.
            // If K2 fails, roll back the POST so a half-installed state doesn't
            // burn an observer-table slot.
            const bool postOk = ue_wrap::game_thread::RegisterPostObserver(fn, &NpcSpawn_POST);
            if (!postOk) {
                g_npcSyncDisabledThisProcess.store(true, std::memory_order_release);
                UE_LOGE("npc-sync: RegisterPostObserver FAILED (observer table full) -- "
                        "NPC sync DISABLED for process lifetime");
            } else if (!ue_wrap::game_thread::RegisterPreObserver(destroyFn, &NpcDestroy_PRE)) {
                // K2 failed after POST succeeded -- roll back POST so we don't
                // leave it firing forever for a disabled system.
                ue_wrap::game_thread::UnregisterObservers(fn, &NpcSpawn_POST);
                g_npcSyncDisabledThisProcess.store(true, std::memory_order_release);
                UE_LOGE("npc-sync: RegisterPreObserver FAILED for K2_DestroyActor "
                        "(observer table full) -- NPC sync DISABLED + rolled back POST "
                        "registration to free its slot");
            } else {
                g_spawnPostObserverInstalled.store(true, std::memory_order_release);
                g_destroyObserverInstalled.store(true, std::memory_order_release);
                UE_LOGI("npc-sync: registered POST observer for %ls.%ls (binds AActor* into Npc Element)",
                        P::name::GameplayStaticsClass, P::name::BeginDeferredSpawnFn);
                UE_LOGI("npc-sync: registered K2_DestroyActor PRE observer (Npc Element lifecycle close)");
            }
        }
    }

    // Register the PRE interceptor LAST and only if the lifecycle observers
    // both succeeded. RegisterInterceptor consumes a slot in the
    // kMaxInterceptors table -- if NPC sync is disabled for the session, we
    // leave the slot free for other subsystems.
    // The client-side suppression that the interceptor implements is also
    // useless without the host-side broadcast pipeline being functional.
    g_installed.store(true, std::memory_order_release);
    if (g_npcSyncDisabledThisProcess.load(std::memory_order_acquire)) {
        UE_LOGW("npc-suppress: lifecycle observer install FAILED -- skipping interceptor "
                "registration entirely (NPC sync disabled for process lifetime; see prior "
                "[Error] lines)");
        return;
    }
    ue_wrap::game_thread::RegisterInterceptor(fn, &NpcSuppress_Interceptor);
    UE_LOGI("npc-suppress: installed interceptor on %ls.%ls @ %p (ActorClass@%d, ReturnValue@%d, SpawnTransform@%d, %zu/%zu NPC classes resolved + lifecycle observers live)",
            P::name::GameplayStaticsClass, P::name::BeginDeferredSpawnFn,
            fn, classOff, retOff, xformOff,
            allowlist.resolved, P::name::kNpcAllowlistSize);
    // 2026-07-03 wisp lane: the EX_CallMath spawn catch (Func-thunk, source-gated) -- installed
    // under the SAME lifecycle gate as the interceptor (an Element it enrolls gets the identical
    // K2_DestroyActor close). Idempotent across Install retries.
    coop::npc_world_enum::InstallExSpawnCatch(fn);
    for (size_t i = 0; i < P::name::kNpcAllowlistSize; ++i) {
        UE_LOGI("npc-suppress: allowlist[%zu] '%ls' = %p",
                i, P::name::kNpcAllowlist[i],
                g_npcAllowlist[i].load(std::memory_order_acquire));
    }
}

bool GetDevSpawnRefs(DevSpawnRefs& out) {
    // Valid only once Install resolved the GameplayStatics UFunctions + CDO
    // (g_installCacheFinishSpawnFn / g_installCacheGsCdo cached in Install's
    // resolution block; g_npcSpawnFn is the BeginDeferred the interceptor hooks).
    if (!g_installed.load(std::memory_order_acquire) || !g_npcSpawnFn ||
        !g_installCacheFinishSpawnFn || !g_installCacheGsCdo) {
        return false;
    }
    out.beginDeferredFn = g_npcSpawnFn;
    out.finishSpawnFn   = g_installCacheFinishSpawnFn;
    out.gsCdo           = g_installCacheGsCdo;
    return true;
}

bool IsHostNpcSyncDisabled() {
    // True iff Install permanently disabled the host NPC lifecycle this process (observer table
    // full -> no guaranteed POST/destroy observer). The world-enum (coop/npc_world_enum) gates on
    // this + IsInstalled() before allocating any Npc Element -- the SAME gate the interceptor uses,
    // so it never leaks an Element that has no destroy observer to close it.
    return g_npcSyncDisabledThisProcess.load(std::memory_order_acquire);
}

}  // namespace coop::npc_sync
