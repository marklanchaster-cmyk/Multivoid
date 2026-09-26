// coop/weather_event_births.cpp -- see coop/weather_event_births.h.

#include "coop/world/weather_event_births.h"

#include "coop/net/session.h"
#include "coop/world/weather_fog.h"
#include "coop/world/weather_redsky.h"
#include "ue_wrap/core/fname_utils.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"

#include <atomic>
#include <chrono>
#include <cstdint>

namespace coop::weather_event_births {
namespace {

namespace P  = ue_wrap::profile;
namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace E  = ue_wrap::engine;

std::atomic<coop::net::Session*> g_session{nullptr};
std::atomic<bool> g_isClient{false};

bool g_hookInstalled = false;   // FinishSpawningActor POST hook (process-lifetime)
bool g_namesMinted   = false;   // suppressed class FNames resolved (GT-only mint)

// These controllers are born directly from day/night graphs, bypassing
// trigger_eventer. Their ReceiveBeginPlay bodies are the exact start seam.
// Cancel it for an organic connected-client birth; the FinishSpawning hook
// below then removes the inert shell. Commanded red-sky/fog mirrors pass while
// their existing echo scope is active. This is intentionally narrower than
// cancelling daynightCycle.Tick.
struct StartGate {
    const wchar_t* cls;
    GT::UFunctionInterceptor cb;
    bool registered;
    uint32_t attempts;
};

bool SuppressDirectEventStart(const wchar_t* tag, void* self) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!g_isClient.load(std::memory_order_acquire) || !s || !s->connected()) return false;
    static std::atomic<uint32_t> count{0};
    const uint32_t n = count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n <= 6 || (n % 25) == 0) {
        UE_LOGW("random_event_auth: CLIENT suppressed selector producer=%ls "
                "function=ReceiveBeginPlay actor=%p", tag, self);
    }
    return true;
}

bool OnRedSkyBeginPlay(void* self, void*) {
    if (coop::weather_redsky::ApplyEchoActive()) return false;
    return SuppressDirectEventStart(L"redSkyEvent_C", self);
}
bool OnWeatherFogBeginPlay(void* self, void*) {
    if (coop::weather_fog::MirrorEchoActive()) return false;
    return SuppressDirectEventStart(L"weatherFogController_C", self);
}
bool OnBlackFogBeginPlay(void* self, void*) {
    return SuppressDirectEventStart(L"blackFog_C", self);
}
bool OnFleshRainBeginPlay(void* self, void*) {
    return SuppressDirectEventStart(L"event_fleshRain_C", self);
}
bool OnFossilBoarWarBeginPlay(void* self, void*) {
    return SuppressDirectEventStart(L"event_fossilBoarWar_C", self);
}

StartGate g_startGates[] = {
    {P::name::RedSkyEventClass,          &OnRedSkyBeginPlay,          false, 0},
    {P::name::WeatherFogControllerClass, &OnWeatherFogBeginPlay,      false, 0},
    {P::name::BlackFogClass,             &OnBlackFogBeginPlay,        false, 0},
    {L"event_fleshRain_C",     &OnFleshRainBeginPlay,     false, 0},
    {L"event_fossilBoarWar_C", &OnFossilBoarWarBeginPlay, false, 0},
};
std::chrono::steady_clock::time_point g_nextStartResolve{};

void InstallStartGates() {
    bool all = true;
    for (const auto& gate : g_startGates) all = all && gate.registered;
    if (all || std::chrono::steady_clock::now() < g_nextStartResolve) return;
    g_nextStartResolve = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (auto& gate : g_startGates) {
        if (gate.registered) continue;
        ++gate.attempts;
        void* cls = R::FindClass(gate.cls);
        void* fn = cls ? R::FindFunction(cls, L"ReceiveBeginPlay") : nullptr;
        if (!fn || !GT::RegisterInterceptor(fn, gate.cb)) {
            if (gate.attempts == 5 || (gate.attempts % 30) == 0) {
                UE_LOGW("random_event_auth: direct event-start gate unresolved after %u "
                        "attempts -- %ls::ReceiveBeginPlay; retrying",
                        gate.attempts, gate.cls);
            }
            continue;
        }
        gate.registered = true;
        UE_LOGI("random_event_auth: direct event-start gate armed -- %ls::ReceiveBeginPlay",
                gate.cls);
    }
}

// The suppressed birth classes, matched by FName index (int compares on the
// hot path -- FinishSpawningActor fires for EVERY actor spawn, so no string
// build here). The first three are weather selectors already owned by weather
// state. The final three are direct daynightCycle event-controller births
// which bypass trigger_eventer.runEvent entirely.
constexpr int kNumClasses = 6;
R::FName g_classNames[kNumClasses] = {};
const wchar_t* const kClassNameStrs[kNumClasses] = {
    P::name::RedSkyEventClass,          // L"redSkyEvent_C"
    P::name::WeatherFogControllerClass, // L"weatherFogController_C"
    P::name::BlackFogClass,             // L"blackFog_C"
    L"badSun_C",
    L"event_fleshRain_C",
    L"event_fossilBoarWar_C",
};

// Rate-latched suppression log (a stuck roll cannot spam the log).
uint32_t g_suppressed[kNumClasses] = {};

// FinishSpawningActor POST: destroy an UNCOMMANDED weather-event birth on a
// CLIENT. Chains after host_spawn_watcher / prop_drop_intent on the same
// UFunction (role-disjoint with the former, class-disjoint with both).
void OnFinishSpawnPost(void* /*context*/, void* /*srcObj*/, void* result) {
    if (!g_isClient.load(std::memory_order_acquire)) return;
    if (!GT::IsGameThread()) return;
    void* actor = result;
    if (!actor) return;
    void* cls = R::ClassOf(actor);
    if (!cls) return;
    const R::FName& n = R::NameOf(cls);
    int match = -1;
    for (int i = 0; i < kNumClasses; ++i) {
        if (n.ComparisonIndex == g_classNames[i].ComparisonIndex &&
            n.Number == g_classNames[i].Number) { match = i; break; }
    }
    if (match < 0) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    // Wire-commanded mirror births pass: each lane raises its echo flag around
    // its own reflected spawn Call. The no-lane classes have no legitimate
    // client birth yet, so every client instance of those is organic.
    if (match == 0 && coop::weather_redsky::ApplyEchoActive()) return;
    if (match == 1 && coop::weather_fog::MirrorEchoActive()) return;
    if (!R::IsLive(actor)) return;
    E::DestroyActor(actor);
    const uint32_t nSup = ++g_suppressed[match];
    if (nSup <= 5 || (nSup % 25) == 0) {
        UE_LOGW("random_event_auth: CLIENT suppressed uncommanded %ls birth #%u "
                "(the day/night roll is host-owned RNG; EX_Local caller is "
                "PE-invisible -- destroyed at FinishSpawn)",
                kClassNameStrs[match], nSup);
    }
}

}  // namespace

bool Install(coop::net::Session* session, bool isHost) {
    g_session.store(session, std::memory_order_release);
    g_isClient.store(!isHost, std::memory_order_release);
    if (!GT::IsGameThread()) return false;  // FName mint dispatches ProcessEvent
    InstallStartGates();
    if (g_hookInstalled && g_namesMinted) return true;
    if (!g_namesMinted) {
        bool all = true;
        for (int i = 0; i < kNumClasses; ++i) {
            g_classNames[i] = ue_wrap::fname_utils::StringToFName(kClassNameStrs[i]);
            if (g_classNames[i].ComparisonIndex == 0) all = false;  // "None" = mint failed
        }
        if (!all) return false;
        g_namesMinted = true;
    }
    if (!g_hookInstalled) {
        void* statics = R::FindClassDefaultObject(P::name::GameplayStaticsClass);
        void* cls = statics ? R::ClassOf(statics) : nullptr;
        void* fn = cls ? R::FindFunction(cls, P::name::FinishSpawningActorFn) : nullptr;
        if (!fn) return false;
        if (!ue_wrap::ufunction_hook::InstallPostHook(fn, &OnFinishSpawnPost)) {
            UE_LOGW("weather_births: FinishSpawningActor POST hook install FAILED -- retrying");
            return false;
        }
        g_hookInstalled = true;
        UE_LOGI("weather_births: FinishSpawningActor POST hook installed "
                "(client birth-catch for 3 weather + 3 direct day-event controllers)");
    }
    InstallStartGates();
    return true;
}

void OnDisconnect() {
    // Hook + minted names persist (process-lifetime; self-gated on session).
    for (int i = 0; i < kNumClasses; ++i) g_suppressed[i] = 0;
    // Already-registered process-lifetime gates stay; unresolved late-loaded
    // classes continue their throttled per-target retries next session.
    g_nextStartResolve = {};
}

}  // namespace coop::weather_event_births
