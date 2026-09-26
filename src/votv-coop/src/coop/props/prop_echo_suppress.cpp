// coop/prop_echo_suppress.cpp -- see header for design.

#include "coop/props/prop_echo_suppress.h"

#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace coop::prop_echo_suppress {
namespace {

// Game-thread-only access (OnSpawn/OnDestroy in remote_prop and the
// Init POST / K2_DestroyActor PRE observers in prop_lifecycle all
// dispatch on the game thread). Capped to bound memory across long
// sessions; on overflow we clear (a one-shot stale lookup is harmless
// -- see header).
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
std::unordered_map<std::wstring, std::chrono::steady_clock::time_point> g_wireDestroyedKeys;
constexpr auto kWireDestroyedKeyTtl = std::chrono::minutes(2);

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

void NoteWireDestroyedKey(const std::wstring& key) {
    if (key.empty()) return;
    if (g_wireDestroyedKeys.size() >= kIncomingCap) g_wireDestroyedKeys.clear();
    g_wireDestroyedKeys[key] = std::chrono::steady_clock::now() + kWireDestroyedKeyTtl;
}

bool ConsumeRecentlyWireDestroyedKey(const std::wstring& key) {
    if (key.empty()) return false;
    const auto it = g_wireDestroyedKeys.find(key);
    if (it == g_wireDestroyedKeys.end()) return false;
    const auto until = it->second;
    g_wireDestroyedKeys.erase(it);
    return std::chrono::steady_clock::now() <= until;
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
