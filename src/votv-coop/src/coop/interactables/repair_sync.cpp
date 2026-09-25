// coop/interactables/repair_sync.cpp -- see repair_sync.h.

#include "coop/interactables/repair_sync.h"

#include "coop/element/portable_identity.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"

#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/vm_dispatch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::repair_sync {
namespace {

namespace R = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace VM = ue_wrap::vm_dispatch;

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

enum NativeVerbId : int {
    kVerbServerFix = 1,
    kVerbGeneratorActionOptionIndex = 2,
    kVerbGeneratorFullFix = 3,
};

struct PendingNativeCompletion {
    void* actor = nullptr;
    int32_t internalIndex = -1;
    uint8_t target = 0;
};

// Game-thread-only. One pending check per object folds nested/duplicate watched
// verbs (for example an actionOptionIndex path that invokes fullFix) into one
// repair request/outcome.
std::unordered_map<void*, PendingNativeCompletion> g_pendingNative;
bool g_nativeRegistrationAttempted = false;
bool g_serverNativeReady = false;
bool g_generatorNativeReady = false;

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

Desc* DescForTarget(uint8_t target) {
    for (auto& d : g_descs) {
        if (d.target == target) return &d;
    }
    return nullptr;
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

bool CallRadioTowerSetBroken(void* actor, bool broken, bool pullLever) {
    if (!actor) return false;

    void* fn = R::FindFunction(R::ClassOf(actor), L"setBroken");
    if (!fn) return false;

    ue_wrap::ParamFrame f(fn);
    if (!f.valid()) return false;

    // radiotower_C::tryToFix() success path calls:
    //
    //     setBroken(false, true)
    //
    // Do not use the old "set every byte param to false" helper here:
    // setBroken has TWO bool parameters with different native values.
    if (!f.Set<bool>(L"isBroken", broken))
        return false;
    if (!f.Set<bool>(L"pullLever", pullLever))
        return false;

    return ue_wrap::Call(actor, f);
}

bool ApplyRepair(void* actor, Desc& d) {
    bool wasRepaired = false;
    if (!IsRepaired(actor, d, wasRepaired)) return false;

    // Do not turn an already-repaired local bool into an implicit success.  A
    // host handling client intent and a client handling the host's commit both
    // replay the canonical gameplay verb, then verify the resulting state.
    // This also makes the originating client converge through the same verb
    // when its authoritative host commit comes back.

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
        // Native successful radiotower_C::tryToFix() path:
        //
        //     setBroken(false, true)
        //     updPuzzle()
        //
        // setBroken updates canonical isBroken, fires stateChanged and
        // refreshes the tower's blink timer. updPuzzle then paints the
        // repaired panel presentation (green when isBroken == false).
        called = CallRadioTowerSetBroken(actor, false, true);

        bool after = false;
        if (!IsRepaired(actor, d, after) || !after) {
            // Fail-safe only: the native setBroken path is preferred.
            WriteRawBool(actor, d, false);
        }

        // Safe even on a peer whose individual puzzle/fuse actions were not
        // mirrored: repaired isBroken=false selects the completed presentation.
        CallNoArg(actor, L"updPuzzle");
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
    UE_LOGI("repair_sync[worldauth]: APPLY target=%u id='%s' was_repaired=%d called=%d repaired=%d",
            static_cast<unsigned>(d.target), ActorIdentity(actor).c_str(),
            wasRepaired ? 1 : 0, called ? 1 : 0, ok ? 1 : 0);
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

bool SendOutcome(uint8_t target, void* actor) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !actor) return false;

    const std::string id = ActorIdentity(actor);
    if (id.empty()) {
        UE_LOGW("repair_sync: target=%u has no portable identity -- outcome not authored",
                static_cast<unsigned>(target));
        return false;
    }

    coop::net::RepairOutcomePayload p{};
    p.target = target;
    p.repaired = 1;
    PutWireKey(p.key, id);

    const bool sent = s->SendReliable(coop::net::ReliableKind::RepairOutcome, &p, sizeof(p));
    if (sent) {
        UE_LOGI("repair_sync[worldauth]: TX %s target=%u id='%s'",
                s->role() == coop::net::Role::Host ? "host-outcome" : "client-request",
                static_cast<unsigned>(target), GetWireKey(p.key).c_str());
    }
    return sent;
}

void CheckNativeCompletion(PendingNativeCompletion pending) {
    // A posted task can be drained by a nested ProcessEvent while the original
    // EX_LocalVirtualFunction is still executing. Leave it in g_pendingNative
    // for Tick() rather than reposting from inside the pump (which drains until
    // empty and would otherwise spin on the same task).
    if (VM::CurrentThreadVerb().active) return;

    const auto it = g_pendingNative.find(pending.actor);
    if (it == g_pendingNative.end() ||
        it->second.internalIndex != pending.internalIndex ||
        it->second.target != pending.target) return;
    g_pendingNative.erase(it);

    if (!R::IsLiveByIndex(pending.actor, pending.internalIndex)) return;
    Desc* const d = DescForTarget(pending.target);
    if (!d) return;
    ResolveDesc(*d);
    if (!d->cls || d->stateOff < 0 || R::ClassOf(pending.actor) != d->cls) return;

    bool repaired = false;
    if (!IsRepaired(pending.actor, *d, repaired) || !repaired) return;

    auto* const s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;
    const std::string id = ActorIdentity(pending.actor);
    if (id.empty()) {
        UE_LOGW("repair_sync: native completion target=%u has no portable identity",
                static_cast<unsigned>(pending.target));
        return;
    }

    // Baseline first: polling is maintenance-only for native-observed targets,
    // but this also prevents a future fallback configuration from double-sending.
    NoteBaseline(pending.target, pending.actor, true);
    UE_LOGI("repair_sync[worldauth]: NATIVE-COMPLETE %s target=%u id='%s'",
            s->role() == coop::net::Role::Host ? "host-outcome" : "client-request",
            static_cast<unsigned>(pending.target), id.c_str());
    SendOutcome(pending.target, pending.actor);
}

void DrainNativeCompletions() {
    if (g_pendingNative.empty() || VM::CurrentThreadVerb().active) return;

    // CheckNativeCompletion erases entries, so snapshot the small pending set
    // before walking it. A normally posted check will already have consumed
    // its entry; this drain exists for the nested-ProcessEvent early-drain case.
    std::vector<PendingNativeCompletion> pending;
    pending.reserve(g_pendingNative.size());
    for (const auto& entry : g_pendingNative)
        pending.push_back(entry.second);
    for (const PendingNativeCompletion& completion : pending)
        CheckNativeCompletion(completion);
}

void OnNativeVerbEntry(const VM::Bracket& bracket) {
    if (!bracket.ctx || R::InCoopDispatch()) return;
    auto* const s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;

    uint8_t target = 0;
    if (bracket.verbId == kVerbServerFix) {
        if (!g_serverNativeReady) return;
        target = kRepairServer;
    } else if (bracket.verbId == kVerbGeneratorActionOptionIndex ||
               bracket.verbId == kVerbGeneratorFullFix) {
        if (!g_generatorNativeReady) return;
        target = kRepairGenerator;
    } else {
        return;
    }

    Desc* const d = DescForTarget(target);
    if (!d) return;
    ResolveDesc(*d);
    // VM dispatch matches globally by verb name. The exact live class check is
    // therefore mandatory before reading any target-specific state.
    if (!d->cls || d->stateOff < 0 || !R::IsLive(bracket.ctx) ||
        R::ClassOf(bracket.ctx) != d->cls) return;

    bool repaired = false;
    if (!IsRepaired(bracket.ctx, *d, repaired) || repaired) return;

    const int32_t internalIndex = R::InternalIndexOf(bracket.ctx);
    if (internalIndex < 0) return;

    PendingNativeCompletion pending{bracket.ctx, internalIndex, target};
    if (!g_pendingNative.emplace(bracket.ctx, pending).second) return;
    GT::Post([pending] { CheckNativeCompletion(pending); });
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (!g_nativeRegistrationAttempted) {
        g_nativeRegistrationAttempted = true;
        g_serverNativeReady =
            VM::RegisterVirtualVerb(L"fix", kVerbServerFix, &OnNativeVerbEntry);
        const bool actionReady = VM::RegisterVirtualVerb(
            L"actionOptionIndex", kVerbGeneratorActionOptionIndex, &OnNativeVerbEntry);
        const bool fullFixReady = VM::RegisterVirtualVerb(
            L"fullFix", kVerbGeneratorFullFix, &OnNativeVerbEntry);
        g_generatorNativeReady = actionReady && fullFixReady;
        UE_LOGI("repair_sync[worldauth]: native verb detection server=%d generator=%d "
                "(fix/actionOptionIndex/fullFix)",
                g_serverNativeReady ? 1 : 0, g_generatorNativeReady ? 1 : 0);
    }
}

void Tick() {
    if (!GT::IsGameThread()) return;
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || !s->connected()) return;

    VM::TickResolvePending();
    VM::SetEnabled(true);
    DrainNativeCompletions();

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

            const bool nativeDetectorReady =
                (d.target == kRepairServer && g_serverNativeReady) ||
                (d.target == kRepairGenerator && g_generatorNativeReady);
            const auto pending = g_pendingNative.find(obj);
            const bool nativeCheckPending = nativeDetectorReady &&
                pending != g_pendingNative.end() &&
                pending->second.internalIndex == R::InternalIndexOf(obj);
            // Native detection owns the edge while its post-call check is
            // pending. Polling remains a fallback if registration/resolution
            // failed or an unexpected dispatch path never produced a bracket.
            if (!was && repaired && !nativeCheckPending)
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

    const bool isHost = s->role() == coop::net::Role::Host;
    if (isHost) {
        // On the host this packet is only repair intent.  It is never accepted
        // as proof of repaired world state.
        if (senderSlot == 0 || senderSlot >= coop::net::kMaxPeers) {
            UE_LOGW("repair_sync: host refused request from slot=%u", senderSlot);
            return;
        }
    } else if (senderSlot != 0) {
        // Only slot 0 can author the commit applied by a client.
        UE_LOGW("repair_sync: client refused non-host outcome from slot=%u", senderSlot);
        return;
    }

    UE_LOGI("repair_sync[worldauth]: RX %s target=%u id='%s' repaired=%u from slot=%u",
            isHost ? "client-request" : "host-outcome", static_cast<unsigned>(p.target),
            GetWireKey(p.key).c_str(), static_cast<unsigned>(p.repaired), senderSlot);

    Desc* d = nullptr;
    void* actor = FindTarget(p, &d);
    if (!actor || !d) {
        UE_LOGW("repair_sync: target=%u id='%s' unresolved",
                static_cast<unsigned>(p.target), GetWireKey(p.key).c_str());
        return;
    }

    if (isHost) {
        if (!ApplyRepair(actor, *d)) {
            UE_LOGW("repair_sync: host failed target=%u id='%s' slot=%u",
                    static_cast<unsigned>(p.target), GetWireKey(p.key).c_str(), senderSlot);
            return;
        }

        NoteBaseline(p.target, actor, true);
        // Rebuild the commit from the host-resolved actor.  Never echo the
        // client's packet as though it were authoritative state.
        if (SendOutcome(p.target, actor)) {
            UE_LOGI("repair_sync[worldauth]: host COMMIT+BROADCAST target=%u id='%s' "
                    "from slot=%u",
                    static_cast<unsigned>(p.target), ActorIdentity(actor).c_str(), senderSlot);
        } else {
            UE_LOGW("repair_sync: host repaired target=%u id='%s' but broadcast enqueue failed",
                    static_cast<unsigned>(p.target), ActorIdentity(actor).c_str());
        }
        return;
    }

    if (ApplyRepair(actor, *d))
        NoteBaseline(p.target, actor, true);
}

void OnDisconnect() {
    g_lastRepaired.clear();
    g_pendingNative.clear();
    g_lastPoll = 0;
    VM::SetEnabled(false);

    for (auto& d : g_descs) {
        d.cls = nullptr;
        d.stateOff = -1;
        d.stateMask = 0;
    }
}

}  // namespace coop::repair_sync
