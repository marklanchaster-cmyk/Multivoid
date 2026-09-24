// coop/interactables/repair_sync.cpp -- see repair_sync.h.

#include "coop/interactables/repair_sync.h"

#include "coop/element/portable_identity.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace coop::repair_sync {
namespace {

namespace R = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;

std::atomic<coop::net::Session*> g_session{nullptr};

constexpr uint64_t kPollMs = 100;

enum : uint8_t {
    kRepairServer = 1,
    kRepairRadioTower = 2,
    kRepairGenerator = 3,
};

struct Desc {
    uint8_t target;
    const wchar_t* className;
    const wchar_t* stateNameA;
    const wchar_t* stateNameB;
    bool repairedWhenSet;

    void* cls = nullptr;
    int32_t stateOff = -1;
    uint8_t stateMask = 0;
};

Desc g_descs[] = {
    {kRepairServer,     L"serverBox_C",  L"IsBroken", L"isBroken", false},
    {kRepairRadioTower, L"radiotower_C", L"IsBroken", L"isBroken", false},
    {kRepairGenerator,  L"generator_C",  L"isBroken", L"IsBroken", false},
};

std::unordered_map<std::string, bool> g_lastRepaired;
uint64_t g_lastPoll = 0;

uint64_t NowMs() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void ResolveDesc(Desc& d) {
    if (!d.cls) d.cls = R::FindClass(d.className);
    if (!d.cls || d.stateOff >= 0 || d.stateOff == -2) return;

    if (!R::FindBoolProperty(d.cls, d.stateNameA, d.stateOff, d.stateMask) &&
        d.stateNameB &&
        !R::FindBoolProperty(d.cls, d.stateNameB, d.stateOff, d.stateMask)) {
        UE_LOGW("repair_sync: %ls repair-state bool unresolved (%ls/%ls)",
                d.className, d.stateNameA, d.stateNameB ? d.stateNameB : L"-");
        d.stateOff = -2;
        return;
    }

    UE_LOGI("repair_sync: resolved %ls repair state @0x%04X mask=0x%02X",
            d.className, d.stateOff, d.stateMask);
}

bool ReadRawBool(void* actor, const Desc& d, bool& v) {
    if (!actor || d.stateOff < 0 || d.stateMask == 0) return false;
    const uint8_t b = *(reinterpret_cast<const uint8_t*>(actor) + d.stateOff);
    v = (b & d.stateMask) != 0;
    return true;
}

void WriteRawBool(void* actor, const Desc& d, bool v) {
    if (!actor || d.stateOff < 0 || d.stateMask == 0) return;
    uint8_t* p = reinterpret_cast<uint8_t*>(actor) + d.stateOff;
    if (v) *p |= d.stateMask;
    else   *p &= static_cast<uint8_t>(~d.stateMask);
}

bool IsRepaired(void* actor, const Desc& d, bool& repaired) {
    bool raw = false;
    if (!ReadRawBool(actor, d, raw)) return false;
    repaired = d.repairedWhenSet ? raw : !raw;
    return true;
}

std::string NarrowAscii(const std::wstring& w) {
    std::string s;
    s.reserve(w.size());
    for (wchar_t ch : w)
        s.push_back((ch >= 0 && ch <= 0x7f) ? static_cast<char>(ch) : '?');
    return s;
}

std::string ActorIdentity(void* actor) {
    // b65004: one deterministic identity rule on every peer. Do not fall back
    // to game Keys or UObject instance names that can be process-local.
    return NarrowAscii(coop::element::PortableWireKey(actor));
}

void PutWireKey(coop::net::WireKey& out, const std::string& s) {
    std::memset(&out, 0, sizeof(out));
    const size_t n = std::min<size_t>(31, s.size());
    out.len = static_cast<uint8_t>(n);
    if (n) std::memcpy(out.data, s.data(), n);
}

std::string GetWireKey(const coop::net::WireKey& k) {
    const size_t n = std::min<size_t>(31, k.len);
    return std::string(k.data, k.data + n);
}

void* FindTarget(const coop::net::RepairOutcomePayload& p, Desc** outDesc = nullptr) {
    const std::string want = GetWireKey(p.key);
    if (want.empty()) return nullptr;

    for (auto& d : g_descs) {
        if (d.target != p.target) continue;
        ResolveDesc(d);
        if (!d.cls || d.stateOff < 0) continue;

        for (void* obj : R::FindObjectsByClass(d.className)) {
            if (!obj || !R::IsLive(obj)) continue;
            if (ActorIdentity(obj) != want) continue;
            if (outDesc) *outDesc = &d;
            return obj;
        }
    }
    return nullptr;
}

bool CallNoArg(void* actor, const wchar_t* name) {
    if (!actor) return false;
    void* fn = R::FindFunction(R::ClassOf(actor), name);
    if (!fn) return false;
    ue_wrap::ParamFrame f(fn);
    return f.valid() && ue_wrap::Call(actor, f);
}

bool CallAllByteParams(void* actor, const wchar_t* name, bool value) {
    if (!actor) return false;
    void* fn = R::FindFunction(R::ClassOf(actor), name);
    if (!fn) return false;
    ue_wrap::ParamFrame f(fn);
    if (!f.valid()) return false;

    const uint8_t v = value ? 1u : 0u;
    for (const auto& p : R::FunctionParams(fn)) {
        if (p.size == 1)
            f.SetRaw(p.name.c_str(), &v, 1);
    }
    return ue_wrap::Call(actor, f);
}

bool ApplyRepair(void* actor, Desc& d) {
    bool before = false;
    if (!IsRepaired(actor, d, before)) return false;
    if (before) return true;

    bool called = false;

    if (d.target == kRepairServer) {
        called = CallNoArg(actor, L"fix");

        bool after = false;
        if (!IsRepaired(actor, d, after) || !after) {
            WriteRawBool(actor, d, false);
        }

        // check() refreshes the server box presentation from canonical IsBroken.
        // Run it even when fix() already flipped the bool successfully.
        CallNoArg(actor, L"check");
    } else if (d.target == kRepairRadioTower) {
        called = CallAllByteParams(actor, L"setBroken", false);

        bool after = false;
        if (!IsRepaired(actor, d, after) || !after)
            WriteRawBool(actor, d, false);
    } else if (d.target == kRepairGenerator) {
        // generator_C owns the authoritative transformer repair state.
        // Its native fullFix() completes the panel state, sets cycle=100,
        // clears isBroken, fires turnedOn, and calls upd().
        called = CallNoArg(actor, L"fullFix");

        bool after = false;
        if (!IsRepaired(actor, d, after) || !after) {
            // Fail-safe only: fullFix() is the preferred native path.
            WriteRawBool(actor, d, false);
            CallNoArg(actor, L"upd");
        }
    }

    bool finalState = false;
    const bool ok = IsRepaired(actor, d, finalState) && finalState;
    UE_LOGI("repair_sync[worldauth]: APPLY target=%u id='%s' called=%d repaired=%d",
            static_cast<unsigned>(d.target), ActorIdentity(actor).c_str(),
            called ? 1 : 0, ok ? 1 : 0);
    return ok;
}

std::string BaselineKey(uint8_t target, const std::string& id) {
    return std::to_string(static_cast<unsigned>(target)) + ":" + id;
}

void NoteBaseline(uint8_t target, void* actor, bool repaired) {
    const std::string id = ActorIdentity(actor);
    if (!id.empty())
        g_lastRepaired[BaselineKey(target, id)] = repaired;
}

void SendOutcome(uint8_t target, void* actor) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !actor) return;

    const std::string id = ActorIdentity(actor);
    if (id.empty()) {
        UE_LOGW("repair_sync: target=%u has no portable identity -- outcome not authored",
                static_cast<unsigned>(target));
        return;
    }

    coop::net::RepairOutcomePayload p{};
    p.target = target;
    p.repaired = 1;
    PutWireKey(p.key, id);

    if (s->SendReliable(coop::net::ReliableKind::RepairOutcome, &p, sizeof(p))) {
        UE_LOGI("repair_sync[worldauth]: TX %s target=%u id='%s'",
                s->role() == coop::net::Role::Host ? "host-outcome" : "client-request",
                static_cast<unsigned>(target), GetWireKey(p.key).c_str());
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
}

void Tick() {
    if (!GT::IsGameThread()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;

    const uint64_t now = NowMs();
    if (now - g_lastPoll < kPollMs) return;
    g_lastPoll = now;

    for (auto& d : g_descs) {
        ResolveDesc(d);
        if (!d.cls || d.stateOff < 0) continue;

        for (void* obj : R::FindObjectsByClass(d.className)) {
            if (!obj || !R::IsLive(obj)) continue;

            bool repaired = false;
            if (!IsRepaired(obj, d, repaired)) continue;

            const std::string id = ActorIdentity(obj);
            if (id.empty()) continue;
            const std::string bk = BaselineKey(d.target, id);

            auto it = g_lastRepaired.find(bk);
            if (it == g_lastRepaired.end()) {
                g_lastRepaired.emplace(bk, repaired);
                UE_LOGI("repair_sync[worldauth]: BASELINE target=%u id='%s' repaired=%d",
                        static_cast<unsigned>(d.target), id.c_str(), repaired ? 1 : 0);
                continue;
            }

            const bool was = it->second;
            it->second = repaired;

            if (!was && repaired)
                SendOutcome(d.target, obj);
        }
    }
}

void OnReliable(const coop::net::RepairOutcomePayload& p, uint8_t senderSlot) {
    if (!GT::IsGameThread()) return;

    if (p.repaired != 1 || p.target < kRepairServer || p.target > kRepairGenerator) {
        UE_LOGW("repair_sync: invalid payload target=%u repaired=%u",
                static_cast<unsigned>(p.target), static_cast<unsigned>(p.repaired));
        return;
    }

    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) return;

    UE_LOGI("repair_sync[worldauth]: RX target=%u id='%s' repaired=%u from slot=%u role=%s",
            static_cast<unsigned>(p.target), GetWireKey(p.key).c_str(),
            static_cast<unsigned>(p.repaired), senderSlot,
            s->role() == coop::net::Role::Host ? "host" : "client");

    Desc* d = nullptr;
    void* actor = FindTarget(p, &d);
    if (!actor || !d) {
        UE_LOGW("repair_sync: target=%u id='%s' unresolved",
                static_cast<unsigned>(p.target), GetWireKey(p.key).c_str());
        return;
    }

    if (s->role() == coop::net::Role::Host) {
        if (senderSlot == 0 || senderSlot >= coop::net::kMaxPeers) {
            UE_LOGW("repair_sync: host refused request from slot=%u", senderSlot);
            return;
        }

        if (!ApplyRepair(actor, *d)) {
            UE_LOGW("repair_sync: host failed target=%u id='%s' slot=%u",
                    static_cast<unsigned>(p.target), GetWireKey(p.key).c_str(), senderSlot);
            return;
        }

        NoteBaseline(p.target, actor, true);
        s->SendReliable(coop::net::ReliableKind::RepairOutcome, &p, sizeof(p));
        UE_LOGI("repair_sync[worldauth]: host COMMIT+BROADCAST target=%u id='%s' from slot=%u",
                static_cast<unsigned>(p.target), GetWireKey(p.key).c_str(), senderSlot);
        return;
    }

    if (senderSlot != 0) {
        UE_LOGW("repair_sync: client refused non-host outcome from slot=%u", senderSlot);
        return;
    }

    if (ApplyRepair(actor, *d))
        NoteBaseline(p.target, actor, true);
}

void OnDisconnect() {
    g_lastRepaired.clear();
    g_lastPoll = 0;

    for (auto& d : g_descs) {
        d.cls = nullptr;
        d.stateOff = -1;
        d.stateMask = 0;
    }
}

}  // namespace coop::repair_sync
