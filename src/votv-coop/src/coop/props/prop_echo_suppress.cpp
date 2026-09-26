// coop/prop_echo_suppress.cpp -- see header for design.

#include "coop/props/prop_echo_suppress.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/devices/laptop.h"

#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace coop::prop_echo_suppress {
namespace {

// Game-thread-only access (OnSpawn/OnDestroy in remote_prop and the
// Init POST / K2_DestroyActor PRE observers in prop_lifecycle all
// dispatch on the game thread). Capped to bound memory across long
// sessions. Pointer echo sets use the existing clear-on-cap behavior; the
// lifecycle marker set preserves its already-recorded one-shots at capacity.
std::unordered_set<void*> g_incomingSpawns;
std::unordered_set<void*> g_incomingDestroys;
constexpr size_t kIncomingCap = 256;

// Mirror-spawn re-entrancy depth (see header). Game-thread-only; a plain int
// because the wrapped BeginDeferred call dispatches synchronously on the same
// thread (nested scopes are fine -- depth counts).
int g_mirrorSpawnDepth = 0;

struct ExpectedConvergenceDestroy {
    uint32_t wireEid = 0;
    std::chrono::steady_clock::time_point until{};
};
std::unordered_map<void*, ExpectedConvergenceDestroy> g_expectedConvergenceDestroys;
constexpr auto kConvergenceDestroyTtl = std::chrono::milliseconds(500);
std::unordered_set<std::wstring> g_floppiesAwaitingReincarnation;

struct WireFloppyCandidate {
    std::wstring key;
    bool zip = false;
    int32_t readWrites = -1;
    std::vector<std::wstring> data;
    std::chrono::steady_clock::time_point until{};
};
std::deque<WireFloppyCandidate> g_wireFloppyCandidates;
constexpr size_t kWireFloppyCandidateCap = 8;
constexpr auto kWireFloppyCandidateTtl = std::chrono::seconds(30);

template <class Set>
void InsertCapped(Set& s, void* actor) {
    if (s.size() >= kIncomingCap) s.clear();
    s.insert(actor);
}

template <class Set>
bool TakeOne(Set& s, void* actor) {
    auto it = s.find(actor);
    if (it == s.end()) return false;
    s.erase(it);
    return true;
}

}  // namespace

void MarkIncomingSpawn(void* actor)     { if (actor) InsertCapped(g_incomingSpawns, actor); }
bool ConsumeIncomingSpawn(void* actor)  { return actor ? TakeOne(g_incomingSpawns, actor) : false; }
bool PeekIncomingSpawn(void* actor)     { return actor && g_incomingSpawns.count(actor) != 0; }
void MarkIncomingDestroy(void* actor)   { if (actor) InsertCapped(g_incomingDestroys, actor); }
bool ConsumeIncomingDestroy(void* actor){ return actor ? TakeOne(g_incomingDestroys, actor) : false; }

static bool IsFloppy(void* actor) {
    return actor && ue_wrap::laptop::EnsureResolved() &&
           ue_wrap::laptop::IsDiscClass(ue_wrap::reflection::ClassOf(actor));
}

void InsertFloppyReincarnationMarker(const std::wstring& key) {
    if (g_floppiesAwaitingReincarnation.count(key) != 0) return;
    if (g_floppiesAwaitingReincarnation.size() >= kIncomingCap) {
        UE_LOGW("floppy convergence marker cap %zu reached -- retaining existing markers; "
                "cannot mark key='%ls'", kIncomingCap, key.c_str());
        return;
    }
    g_floppiesAwaitingReincarnation.insert(key);
    UE_LOGI("floppy convergence marker created key='%ls'", key.c_str());
}

void NoteFloppyRetiredForReincarnation(void* actor, const std::wstring& key) {
    if (key.empty() || key == L"None" || !IsFloppy(actor)) return;
    InsertFloppyReincarnationMarker(key);
}

void ConfirmWireFloppyInsertRetirement() {
    ue_wrap::laptop::SlotState slot;
    ue_wrap::laptop::SlotContent content;
    if (!ue_wrap::laptop::EnsureResolved() || !ue_wrap::laptop::ReadSlot(slot) ||
        slot.floppyType < 0 || !ue_wrap::laptop::ReadSlotContent(content)) return;
    const auto now = std::chrono::steady_clock::now();
    for (auto it = g_wireFloppyCandidates.begin(); it != g_wireFloppyCandidates.end();) {
        if (now > it->until) { it = g_wireFloppyCandidates.erase(it); continue; }
        if (it->zip == slot.zip && it->readWrites == slot.readWrites &&
            it->data == content.data) {
            const std::wstring key = it->key;
            it = g_wireFloppyCandidates.erase(it);
            InsertFloppyReincarnationMarker(key);
            return;
        }
        ++it;
    }
}

void NoteWireFloppyRetirementCandidate(void* actor, const std::wstring& key) {
    if (key.empty() || key == L"None" || !IsFloppy(actor)) return;
    ue_wrap::laptop::DiscContent disc;
    if (!ue_wrap::laptop::ReadDiscContent(actor, disc)) return;
    while (g_wireFloppyCandidates.size() >= kWireFloppyCandidateCap)
        g_wireFloppyCandidates.pop_front();
    g_wireFloppyCandidates.push_back(WireFloppyCandidate{
        key, ue_wrap::laptop::IsZipDiscClass(ue_wrap::reflection::ClassOf(actor)),
        disc.readWrites, std::move(disc.data),
        std::chrono::steady_clock::now() + kWireFloppyCandidateTtl});
    ConfirmWireFloppyInsertRetirement();
}

bool IsFloppyReincarnationAwaiting(void* actor, const std::wstring& key) {
    return !key.empty() && key != L"None" && IsFloppy(actor) &&
           g_floppiesAwaitingReincarnation.count(key) != 0;
}

bool TryArmFloppyReincarnation(void* actor, uint32_t wireEid,
                               const std::wstring& key) {
    if (!IsFloppyReincarnationAwaiting(actor, key)) return false;
    const auto it = g_floppiesAwaitingReincarnation.find(key);
    if (it == g_floppiesAwaitingReincarnation.end()) return false;
    // Do not consume the long-lived one-shot until an exact safe expectation
    // can be formed. The later eid-assignment seam gets another attempt.
    if (wireEid == 0 || wireEid == 0xFFFFFFFFu) return false;
    g_floppiesAwaitingReincarnation.erase(it);
    ExpectHostMirrorConvergenceDestroy(actor, wireEid);
    UE_LOGI("floppy reincarnation marker consumed key='%ls' actor=%p eid=%u",
            key.c_str(), actor, wireEid);
    return true;
}

void ResetFloppyConvergence() {
    g_floppiesAwaitingReincarnation.clear();
    g_wireFloppyCandidates.clear();
    g_expectedConvergenceDestroys.clear();
}

void ExpectHostMirrorConvergenceDestroy(void* actor, uint32_t wireEid) {
    if (!actor || wireEid == 0 || wireEid == 0xFFFFFFFFu) return;
    if (g_expectedConvergenceDestroys.size() >= kIncomingCap)
        g_expectedConvergenceDestroys.clear();
    g_expectedConvergenceDestroys[actor] = {
        wireEid, std::chrono::steady_clock::now() + kConvergenceDestroyTtl};
}

bool ConsumeHostMirrorConvergenceDestroy(void* actor, uint32_t wireEid) {
    if (!actor) return false;
    const auto it = g_expectedConvergenceDestroys.find(actor);
    if (it == g_expectedConvergenceDestroys.end()) return false;
    if (std::chrono::steady_clock::now() > it->second.until) {
        g_expectedConvergenceDestroys.erase(it);
        return false;
    }
    if (wireEid == 0 || wireEid == 0xFFFFFFFFu ||
        wireEid != it->second.wireEid) {
        return false;
    }
    g_expectedConvergenceDestroys.erase(it);
    return true;
}

// ---- the ARBITER-CONSUMED key set (2026-08-25) --------------------------------------------------
// Keyed by save KEY, not by pointer, because the whole point is that the actor is already gone: the
// pointer set above cannot express "a destroy naming THIS KEY is an echo of one I performed myself".
std::unordered_set<std::wstring> g_arbiterConsumedKeys;

void MarkArbiterConsumedKey(const std::wstring& key) {
    if (key.empty()) return;
    if (g_arbiterConsumedKeys.size() >= kIncomingCap) g_arbiterConsumedKeys.clear();
    g_arbiterConsumedKeys.insert(key);
}

bool ConsumeArbiterConsumedKey(const std::wstring& key) {
    if (key.empty()) return false;
    auto it = g_arbiterConsumedKeys.find(key);
    if (it == g_arbiterConsumedKeys.end()) return false;
    g_arbiterConsumedKeys.erase(it);
    return true;
}

ScopedMirrorSpawn::ScopedMirrorSpawn()  { ++g_mirrorSpawnDepth; }
ScopedMirrorSpawn::~ScopedMirrorSpawn() { --g_mirrorSpawnDepth; }
bool InMirrorSpawnScope()               { return g_mirrorSpawnDepth > 0; }

}  // namespace coop::prop_echo_suppress
