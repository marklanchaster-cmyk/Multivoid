// coop/voice/radio_item.cpp -- physical walkie-radio controller.

#include "coop/voice/radio_item.h"

#include "coop/player/hand_item.h"
#include "coop/voice/radio_state.h"

#include "ue_wrap/actors/inventory.h"
#include "ue_wrap/actors/prop.h"
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

constexpr const wchar_t* kRadioClass = L"prop_walkie_radio_C";
constexpr const wchar_t* kUnkeyedRadio = L"@multivoid_walkie_unkeyed";

bool g_installed = false;

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
    coop::radio_state::Reset();
}

}  // namespace coop::radio_item
