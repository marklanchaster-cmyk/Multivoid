// Host-authored lifetime + safe local presentation for blackFog_C.
//
// Cooked Blueprint ground truth (blackFog CFG):
//   ReceiveBeginPlay creates a dynamic Inst_pp_blackFog material, installs it
//   in PostProcess.Settings.WeightedBlendables[0], then runs three local loops:
//     a += WorldDeltaSeconds / spd (CDO spd=300), set();
//     random camera-relative whisper every 5..30 s;
//     local thump every 5 s.
//   set() clamps a to [0,1], writes material scalar `alpha`, writes
//   gamemode.birber.noBirb=(a>0.5), and calls setVol2(1-a) on every ambient
//   trigger. ReceiveTick toggles the named `blackfog` reverb at a>=0.9 and
//   registers/unregisters itself through lib.setEvent. At a>=1 it destroys its
//   eyes array, fades for about one second, then destroys itself. The 40 s
//   spawnGhost timer contains RNG and is never replayed on a client mirror;
//   any host eyer_C result remains owned by the existing OwnerEntity lane.

#include "coop/world/black_fog_sync.h"

#include "coop/net/session.h"
#include "coop/world/event_active_sync.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/engine/engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace coop::black_fog_sync { namespace {
namespace R = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace E = ue_wrap::engine;

std::atomic<coop::net::Session*> g_session{nullptr};
std::atomic<bool> g_mirrorEcho{false};
std::atomic<bool> g_isClient{false};

void* g_gmCls = nullptr;
void* g_fogCls = nullptr;
void* g_spawnFn = nullptr;
void* g_setFn = nullptr;
void* g_spawnGhostFn = nullptr;
int32_t g_offGmBlackFog = -1;
int32_t g_offAlpha = -1;
bool g_spawnGhostGate = false;
std::chrono::steady_clock::time_point g_nextResolve{};
uint32_t g_resolveAttempts = 0;

void* g_gm = nullptr;
int32_t g_gmIdx = -1;
void* g_hostController = nullptr;
int32_t g_hostControllerIdx = -1;

struct HostPhase {
    int32_t index = -1;
    float lastAlpha = 0.0f;
    bool sawFull = false;
    bool fading = false;
};
std::unordered_map<void*, HostPhase> g_hostPhases;

struct ClientInstance {
    void* actor = nullptr;
    int32_t index = -1;
    bool terminalFade = false;
    float fadeAlpha = 0.0f;
    std::chrono::steady_clock::time_point fadeAt{};
    std::chrono::steady_clock::time_point nextSpawnRetry{};
};
std::unordered_map<uint64_t, ClientInstance> g_clients;

float DecodeAlpha(uint8_t flags) {
    return static_cast<float>(flags & 0x3f) / 63.0f;
}

void* Gamemode() {
    if (g_gm && R::IsLiveByIndex(g_gm, g_gmIdx)) return g_gm;
    g_gm = nullptr;
    g_gmIdx = -1;
    for (void* obj : R::FindObjectsByClass(L"mainGamemode_C")) {
        if (!obj || !R::IsLive(obj) || R::NameStartsWith(R::NameOf(obj), L"Default__")) continue;
        g_gm = obj;
        g_gmIdx = R::InternalIndexOf(obj);
        break;
    }
    return g_gm;
}

bool SuppressClientSpawnGhost(void*, void*) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!g_isClient.load(std::memory_order_acquire) || !s || !s->connected()) return false;
    static uint32_t count = 0;
    ++count;
    if (count <= 3 || (count % 25) == 0) {
        UE_LOGI("black_fog: CLIENT suppressed local spawnGhost RNG #%u; host concrete eyer output "
                "remains OwnerEntity-owned", count);
    }
    return true;
}

void ResolvePass() {
    if (g_offGmBlackFog >= 0 && g_offAlpha >= 0 && g_spawnFn && g_setFn &&
        g_spawnGhostGate) return;
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextResolve) return;
    g_nextResolve = now + std::chrono::seconds(2);
    if (!g_gmCls) g_gmCls = R::FindClass(L"mainGamemode_C");
    if (!g_fogCls) g_fogCls = R::FindClass(L"blackFog_C");
    if (g_gmCls) {
        if (g_offGmBlackFog < 0) g_offGmBlackFog = R::FindPropertyOffset(g_gmCls, L"blackFog");
        if (!g_spawnFn) g_spawnFn = R::FindFunction(g_gmCls, L"spawnBlackFog");
    }
    if (g_fogCls) {
        if (g_offAlpha < 0) g_offAlpha = R::FindPropertyOffset(g_fogCls, L"a");
        if (!g_setFn) g_setFn = R::FindFunction(g_fogCls, L"set");
        if (!g_spawnGhostFn) g_spawnGhostFn = R::FindFunction(g_fogCls, L"spawnGhost");
        if (g_spawnGhostFn && !g_spawnGhostGate)
            g_spawnGhostGate = GT::RegisterInterceptor(g_spawnGhostFn, &SuppressClientSpawnGhost);
    }
    if (g_offGmBlackFog >= 0 && g_offAlpha >= 0 && g_spawnFn && g_setFn &&
        g_spawnGhostGate) {
        UE_LOGI("black_fog: resolved (mainGamemode.blackFog=0x%X blackFog.a=0x%X; "
                "spawn/set=yes spawnGhost client gate=yes)", g_offGmBlackFog, g_offAlpha);
        return;
    }
    ++g_resolveAttempts;
    if (g_resolveAttempts == 5 || (g_resolveAttempts % 30) == 0) {
        UE_LOGW("black_fog: resolution incomplete after %u attempts; retrying late-loaded class",
                g_resolveAttempts);
    }
}

void ApplyAlpha(void* actor, float alpha) {
    if (!actor || !R::IsLive(actor) || g_offAlpha < 0 || !g_setFn) return;
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(actor) + g_offAlpha) = alpha;
    ue_wrap::ParamFrame frame(g_setFn);
    if (frame.valid()) ue_wrap::Call(actor, frame);
}

void DestroyPresentation(ClientInstance& inst) {
    if (!inst.actor || !R::IsLiveByIndex(inst.actor, inst.index)) return;
    // ReceiveDestroyed only unregisters/deactivates reverb. The shipped graph
    // normally reaches set(0) before destruction; preserve that cleanup when a
    // host END or disconnect cuts a locally divergent controller short.
    ApplyAlpha(inst.actor, 0.0f);
    E::DestroyActor(inst.actor);
}

void* SpawnPresentation() {
    ResolvePass();
    // Never materialize the shipped controller while its world-RNG callback
    // could still run locally. The EventAuthority instance remains alive and
    // TickClient retries after the exact interceptor becomes available.
    if (!g_spawnGhostGate) return nullptr;
    void* gm = Gamemode();
    if (!gm || g_offGmBlackFog < 0 || !g_spawnFn) return nullptr;
    void*& slot = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gm) + g_offGmBlackFog);
    if (slot && R::IsLive(slot)) return slot;
    g_mirrorEcho.store(true, std::memory_order_release);
    ue_wrap::ParamFrame frame(g_spawnFn);
    if (frame.valid()) ue_wrap::Call(gm, frame);
    g_mirrorEcho.store(false, std::memory_order_release);
    return slot && R::IsLive(slot) ? slot : nullptr;
}

void TickHost() {
    ResolvePass();
    void* gm = Gamemode();
    if (!gm || g_offGmBlackFog < 0) return;
    void* current = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(gm) + g_offGmBlackFog);
    if (current && !R::IsLive(current)) current = nullptr;
    if (current != g_hostController) {
        if (g_hostController) {
            coop::event_active_sync::HostEndExternal(g_hostController);
            g_hostPhases.erase(g_hostController);
        }
        g_hostController = current;
        g_hostControllerIdx = current ? R::InternalIndexOf(current) : -1;
        if (current) {
            HostPhase phase{};
            phase.index = g_hostControllerIdx;
            if (g_offAlpha >= 0)
                phase.lastAlpha = std::clamp(*reinterpret_cast<float*>(
                    reinterpret_cast<uint8_t*>(current) + g_offAlpha), 0.0f, 1.0f);
            g_hostPhases[current] = phase;
            const uint64_t id = coop::event_active_sync::HostBeginExternal(
                current, "blackFog_C", nullptr);
            UE_LOGI("black_fog: HOST controller birth actor=%p instance=%llu",
                    current, static_cast<unsigned long long>(id));
        }
    }
    if (!current || g_offAlpha < 0) return;
    auto it = g_hostPhases.find(current);
    if (it == g_hostPhases.end()) return;
    const float raw = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(current) + g_offAlpha);
    const float alpha = std::clamp(raw, 0.0f, 1.0f);
    if (raw > 1.0f || alpha >= 0.999f) it->second.sawFull = true;
    constexpr float kDecreaseEpsilon = 1.0e-6f;
    if (it->second.sawFull && alpha < it->second.lastAlpha - kDecreaseEpsilon)
        it->second.fading = true;
    it->second.lastAlpha = alpha;
    coop::event_active_sync::HostRefreshExternal(current);
}

void TickClient() {
    const auto now = std::chrono::steady_clock::now();
    for (auto& [id, inst] : g_clients) {
        (void)id;
        const float dt = std::clamp(
            std::chrono::duration<float>(now - inst.fadeAt).count(), 0.0f, 0.1f);
        if (inst.terminalFade) {
            inst.fadeAlpha = std::max(0.0f, inst.fadeAlpha - dt);
            inst.fadeAt = now;
            if (inst.actor && R::IsLiveByIndex(inst.actor, inst.index)) {
                ApplyAlpha(inst.actor, inst.fadeAlpha);
            } else if (now >= inst.nextSpawnRetry) {
                inst.nextSpawnRetry = now + std::chrono::milliseconds(500);
                inst.actor = SpawnPresentation();
                inst.index = inst.actor ? R::InternalIndexOf(inst.actor) : -1;
                if (inst.actor) ApplyAlpha(inst.actor, inst.fadeAlpha);
            }
            continue;
        }
        if (inst.actor && R::IsLiveByIndex(inst.actor, inst.index)) continue;
        // The last host phase is authoritative. Never advance reconstruction
        // from wall time: an unfocused/frozen process may resume much later.
        // END removes the instance before this retry can run again.
        if (now < inst.nextSpawnRetry) continue;
        inst.nextSpawnRetry = now + std::chrono::milliseconds(500);
        inst.actor = SpawnPresentation();
        inst.index = inst.actor ? R::InternalIndexOf(inst.actor) : -1;
        if (inst.actor) ApplyAlpha(inst.actor, inst.fadeAlpha);
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    g_isClient.store(session && session->role() != coop::net::Role::Host,
                     std::memory_order_release);
    ResolvePass();
}

void Tick() {
    if (!GT::IsGameThread()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) return;
    // Authority/tracking is connection-independent: a fog event may begin
    // while the host is alone and must already exist for a later JIP snapshot.
    if (s->role() == coop::net::Role::Host) TickHost();
    else if (s->connected()) TickClient();
}

uint8_t HostFlags(void* controller, bool nativeActive) {
    if (!controller || g_offAlpha < 0 || !R::IsLive(controller)) return 0;
    const float alpha = std::clamp(*reinterpret_cast<float*>(
        reinterpret_cast<uint8_t*>(controller) + g_offAlpha), 0.0f, 1.0f);
    const auto it = g_hostPhases.find(controller);
    const bool fading = it != g_hostPhases.end() && it->second.fading;
    const auto q = static_cast<uint8_t>(std::lround(alpha * 63.0f));
    return static_cast<uint8_t>((fading ? 0x80u : 0u) |
                                (nativeActive ? 0x40u : 0u) |
                                std::min<uint8_t>(q, 63u));
}

bool NativeActive(uint8_t flags) { return (flags & 0x40u) != 0; }

void ClientBegin(uint64_t instanceId, uint16_t elapsedSec, uint8_t flags,
                 bool fromSnapshot) {
    if (!GT::IsGameThread() || !instanceId) return;
    const float alpha = DecodeAlpha(flags);
    const bool fade = (flags & 0x80u) != 0;
    auto it = g_clients.find(instanceId);
    if (it == g_clients.end()) {
        // mainGamemode permits only one blackFog pointer. Ordered EventAuthority
        // normally ENDs the prior instance first; this is defensive cleanup for
        // a world-reset/revision replacement.
        for (auto& [oldId, old] : g_clients) {
            (void)oldId;
            DestroyPresentation(old);
        }
        g_clients.clear();
        void* actor = SpawnPresentation();
        ClientInstance inst{};
        inst.actor = actor;
        inst.index = actor ? R::InternalIndexOf(actor) : -1;
        it = g_clients.emplace(instanceId, inst).first;
    }
    ClientInstance& inst = it->second;
    if ((!inst.actor || !R::IsLiveByIndex(inst.actor, inst.index)) && !fade) {
        inst.actor = SpawnPresentation();
        inst.index = inst.actor ? R::InternalIndexOf(inst.actor) : -1;
    }
    if (inst.actor && R::IsLiveByIndex(inst.actor, inst.index)) ApplyAlpha(inst.actor, alpha);
    inst.terminalFade = fade;
    inst.fadeAlpha = alpha;
    inst.fadeAt = std::chrono::steady_clock::now();
    inst.nextSpawnRetry = inst.fadeAt + std::chrono::milliseconds(500);
    UE_LOGI("black_fog: CLIENT %s instance=%llu elapsed=%us phase=%s alpha=%.3f actor=%p",
            fromSnapshot ? "SNAPSHOT" : "BEGIN",
            static_cast<unsigned long long>(instanceId), elapsedSec,
            fade ? "fade" : "ramp", alpha, inst.actor);
}

void ClientPhase(uint64_t instanceId, uint8_t flags) {
    const auto it = g_clients.find(instanceId);
    if (it == g_clients.end()) return;
    ClientInstance& inst = it->second;
    inst.terminalFade = (flags & 0x80u) != 0;
    inst.fadeAlpha = DecodeAlpha(flags);
    inst.fadeAt = std::chrono::steady_clock::now();
    if (inst.actor && R::IsLiveByIndex(inst.actor, inst.index))
        ApplyAlpha(inst.actor, inst.fadeAlpha);
}

void ClientEnd(uint64_t instanceId) {
    const auto it = g_clients.find(instanceId);
    if (it == g_clients.end()) return;
    DestroyPresentation(it->second);
    g_clients.erase(it);
    UE_LOGI("black_fog: CLIENT END instance=%llu -- alpha zeroed and presentation destroyed",
            static_cast<unsigned long long>(instanceId));
}

bool MirrorEchoActive() {
    return g_mirrorEcho.load(std::memory_order_acquire);
}

void OnDisconnect() {
    for (auto& [id, inst] : g_clients) {
        (void)id;
        DestroyPresentation(inst);
    }
    g_clients.clear();
    g_hostPhases.clear();
    g_hostController = nullptr;
    g_hostControllerIdx = -1;
    g_gm = nullptr;
    g_gmIdx = -1;
    g_mirrorEcho.store(false, std::memory_order_release);
    g_isClient.store(false, std::memory_order_release);
    g_session.store(nullptr, std::memory_order_release);
}

}  // namespace coop::black_fog_sync
