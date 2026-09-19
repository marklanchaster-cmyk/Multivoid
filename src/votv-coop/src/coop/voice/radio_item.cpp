// coop/voice/radio_item.cpp -- physical walkie-radio controller.

#include "coop/voice/radio_item.h"

#include "coop/player/hand_item.h"
#include "coop/player/players_registry.h"
#include "coop/voice/radio_state.h"

#include "ue_wrap/actors/inventory.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/asset_load.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/sdk_profile.h"

#include <chrono>
#include <string>
#include <unordered_set>

namespace coop::radio_item {
namespace {

namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace P  = ue_wrap::profile;
namespace E  = ue_wrap::engine;

constexpr const wchar_t* kRadioClass = L"prop_walkie_radio_C";
constexpr const wchar_t* kUnkeyedRadio = L"@multivoid_walkie_unkeyed";

bool g_installed = false;

// TEMPORARY visual probe. Removed once the cooked walkie mesh is proven in-game.
bool g_meshProbeSpawned = false;

// Local power-state table. The normal case uses the prop's persistent Key.
// The unkeyed fallback keeps the first prototype usable before we finish
// persistence/network identity for runtime-spawned radios.
std::unordered_set<std::wstring> g_powered;

using Clock = std::chrono::steady_clock;
Clock::time_point g_lastPoll{};

std::wstring NormalizeKey(std::wstring key) {
    if (key.empty() || key == L"None")
        return kUnkeyedRadio;
    return key;
}

std::wstring ActorIdentity(void* actor) {
    if (!actor)
        return {};
    return NormalizeKey(ue_wrap::prop::GetInteractableKeyString(actor));
}

std::wstring RecordIdentity(const ue_wrap::save_record::SaveRecord& rec) {
    return NormalizeKey(rec.key);
}

bool IsPoweredIdentity(const std::wstring& id) {
    return !id.empty() && g_powered.find(id) != g_powered.end();
}

// Temporary one-shot cooked-mesh smoke test.
//
// Spawns our pak asset as a bare AStaticMeshActor in front of the local player.
// This proves the full path:
//   mounted pak -> LoadObject -> UStaticMesh -> runtime component -> renderer.
void TrySpawnMeshProbe() {
    if (g_meshProbeSpawned)
        return;

    void* local = coop::players::Registry::Get().Local();
    if (!local || !R::IsLive(local))
        return;

    void* mesh = ue_wrap::asset_load::LoadObjectByPath(
        L"/Game/Mods/VOTVCoop/walkie/"
        L"atvRadio_radio_prop.atvRadio_radio_prop");

    if (!mesh)
        return;

    void* actorClass = R::FindClass(L"StaticMeshActor");
    if (!actorClass) {
        UE_LOGW("walkie: mesh probe -- StaticMeshActor class unresolved");
        return;
    }

    ue_wrap::FVector loc = E::GetActorLocation(local);
    const ue_wrap::FVector fwd = E::GetActorForwardVector(local);

    // About 1.5 m ahead and slightly elevated so it's impossible to miss.
    loc.X += fwd.X * 150.f;
    loc.Y += fwd.Y * 150.f;
    loc.Z += 40.f;

    void* actor = E::SpawnActor(actorClass, loc, false);
    if (!actor) {
        UE_LOGW("walkie: mesh probe -- StaticMeshActor spawn failed");
        return;
    }

    void* comp = E::GetStaticMeshComponent(actor);
    if (!comp) {
        UE_LOGW("walkie: mesh probe -- StaticMeshComponent missing");
        E::DestroyActor(actor);
        return;
    }

    // Runtime mesh swaps silently fail on Static mobility.
    E::SetComponentMobility(comp, 2);  // Movable

    if (!E::SetStaticMesh(comp, mesh)) {
        UE_LOGW("walkie: mesh probe -- SetStaticMesh failed");
        E::DestroyActor(actor);
        return;
    }

    // Our Blender model's long axis currently maps to UE Y.
    // Roll 90 degrees so the handheld stands upright for the visual test.
    E::SetActorRotation(actor, ue_wrap::FRotator{0.f, 0.f, 90.f});

    // Visual probe only; don't let it interfere with player collision.
    E::SetActorRootCollisionEnabled(actor, 0);

    g_meshProbeSpawned = true;

    UE_LOGI("walkie: MESH PROBE SPAWNED actor=%p mesh=%p at (%.1f, %.1f, %.1f)",
            actor, mesh, loc.X, loc.Y, loc.Z);
}

// Hold-E radial confirmation.
//
// The cooked radio Blueprint will expose exactly one radial option:
//     index 0 = Power
//
// releaseEToUse is true only on the radial confirmation edge, so ordinary
// E pickup/grab presses do not toggle the radio.
void OnUseInputPre(void* self, void* /*function*/, void* /*params*/) {
    if (!self)
        return;

    bool release = false;
    int32_t actionIndex = -1;
    if (!ue_wrap::engine::ReadMainPlayerRadialSelect(
            self, release, actionIndex))
        return;

    if (!release || actionIndex != 0)
        return;

    void* actor = ue_wrap::engine::ReadMainPlayerLookAtActor(self);
    if (!IsRadioActor(actor))
        return;

    const std::wstring id = ActorIdentity(actor);
    if (id.empty())
        return;

    bool powered = false;

    auto it = g_powered.find(id);
    if (it == g_powered.end()) {
        g_powered.insert(id);
        powered = true;
    } else {
        g_powered.erase(it);
        powered = false;
    }

    UE_LOGI("walkie: ground radio %p key='%ls' -> %s",
            actor,
            id.c_str(),
            powered ? "POWER ON" : "power off");
}

}  // namespace

bool IsRadioActor(void* actor) {
    if (!actor || !R::IsLive(actor))
        return false;

    return R::ClassNameOf(actor) == kRadioClass;
}

bool IsPoweredActor(void* actor) {
    if (!IsRadioActor(actor))
        return false;

    return IsPoweredIdentity(ActorIdentity(actor));
}

void Install() {
    if (g_installed)
        return;

    void* cls = R::FindClass(P::name::MainPlayerClass);
    if (!cls)
        return;  // retry next subsystem Install once gameplay content is loaded

    void* fn = R::FindFunction(cls, P::name::MainPlayerUseInputEventFn);
    if (!fn) {
        UE_LOGW("walkie: InpActEvt_use not found -- power radial unavailable");
        return;
    }

    if (!GT::RegisterPreObserver(fn, &OnUseInputPre)) {
        UE_LOGW("walkie: failed to register hold-E power observer");
        return;
    }

    g_installed = true;
    UE_LOGI("walkie: hold-E power observer installed");
}

void Tick() {
    // Reading the live personal store walks/copies inventory records, so do
    // this at 5 Hz rather than every render/game tick.
    const auto now = Clock::now();
    if (g_lastPoll.time_since_epoch().count() != 0 &&
        now - g_lastPoll < std::chrono::milliseconds(200))
        return;
    g_lastPoll = now;

    TrySpawnMeshProbe();

    // Hand state is a fresh mainPlayer.holding_actor read.
    void* held = coop::hand_item::LocalHandActor();
    const bool heldRadio = IsRadioActor(held);
    const bool heldPowered = heldRadio && IsPoweredActor(held);

    coop::radio_state::SetHeld(heldRadio);
    coop::radio_state::SetHeldPowered(heldPowered);

    // RX is available when ANY powered radio is actually carried by this
    // player. A held radio counts even if VOTV temporarily moves it out of
    // the personal-store array while equipped.
    bool canReceive = heldPowered;

    ue_wrap::inventory::LivePersonalStore store;
    if (ue_wrap::inventory::ReadLivePersonalStore(store)) {
        for (const auto& rec : store.records) {
            if (rec.className != kRadioClass)
                continue;

            if (IsPoweredIdentity(RecordIdentity(rec))) {
                canReceive = true;
                break;
            }
        }
    }

    coop::radio_state::SetPowered(canReceive);
}

void OnDisconnect() {
    g_powered.clear();
    g_lastPoll = {};
    g_meshProbeSpawned = false;
    coop::radio_state::Reset();
}

}  // namespace coop::radio_item
