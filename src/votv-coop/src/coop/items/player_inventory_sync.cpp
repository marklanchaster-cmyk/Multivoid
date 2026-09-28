// coop/player_inventory_sync.cpp -- see coop/player_inventory_sync.h.

#include "coop/items/player_inventory_sync.h"

#include "coop/net/blob_chunks.h"
#include "ue_wrap/core/paths.h"
#include "coop/config/config.h"
#include "coop/items/inventory_wire.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/element/intent_authority.h"
#include "coop/session/player_handshake.h"
#include "coop/text/utf8_codec.h"
#include "coop/save/save_transfer.h"
#include "ue_wrap/actors/begin_equipment.h"  // RULE-1 first-join: the game's own getData->AddEquipment equip
#include "ue_wrap/engine/engine.h"      // Inc 4: SetSaveObjectReadyHook -- the pre-materialize apply point
#include "ue_wrap/actors/inventory.h"
#include "ue_wrap/actors/vitals.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/log.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace coop::player_inventory_sync {
namespace {

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

std::atomic<coop::net::Session*> g_session{nullptr};

// ---- HOST: per-slot received-blob state + the reassembler ----
coop::blob_chunks::Assembler g_assembler;
struct HostEntry {
    std::string          guid;
    std::string          nick;                // SNAPSHOT at receive (game thread); FlushSlot must
                                              // NOT call the GT-only NicknameForSlot, because
                                              // FlushAllToDisk runs on the WINDOW thread at shutdown
    std::vector<uint8_t> blob;
    uint64_t             hash = 0;
    bool                 dirty = false;       // received, not yet flushed to disk
    Clock::time_point    lastWrite{};         // 15 s rate-limit
    ue_wrap::inventory::PlayerInventory state;
    bool                 loaded = false;
    struct PendingPickup {
        std::wstring key;
        std::wstring className;
        Clock::time_point deadline{};
    };
    std::deque<PendingPickup> pendingPickups;
};
std::array<HostEntry, coop::net::kMaxPeers> g_hostBySlot;
// Normal host mutations are game-thread-only. Shutdown's window-thread durability barrier is the
// one cross-thread reader/writer, so serialize it with the ledger. Recursive only because existing
// lifecycle functions compose FlushSlot with broader ledger operations; there is no normal-thread
// contention. DllMain never touches this state.
std::recursive_mutex g_hostMutex;

// ---- CLIENT: send-dedup state ----
uint64_t          g_lastSentHash = 0;
uint32_t          g_sendSeq = 0;
Clock::time_point g_lastPoll{};
Clock::time_point g_lastSweep{};
Clock::time_point g_lastStreamLog{};

constexpr auto kClientPoll  = std::chrono::seconds(1);
constexpr auto kWriteRate   = std::chrono::seconds(15);
constexpr auto kAsmTtl      = std::chrono::seconds(30);
constexpr auto kPickupTtl   = std::chrono::seconds(10);
constexpr size_t kMaxPendingPickups = 16;
constexpr float kInventoryReachUU = 200.f;

using SaveRecord = ue_wrap::save_record::SaveRecord;
using PlayerInventory = ue_wrap::inventory::PlayerInventory;

void RecordsOf(const PlayerInventory& inv, std::vector<const SaveRecord*>& out) {
    out.clear();
    out.reserve(inv.inventory.size() + inv.equipment.size() + inv.hold.size());
    for (const auto& r : inv.inventory) out.push_back(&r);
    for (const auto& r : inv.equipment) if (!r.data.className.empty()) out.push_back(&r.data);
    for (const auto& r : inv.hold) if (!r.data.className.empty()) out.push_back(&r.data);
}

std::wstring IdentityOf(const SaveRecord& r) {
    return r.key + L"\x1f" + r.className;
}

std::vector<std::wstring> IdentitySet(const PlayerInventory& inv) {
    std::vector<const SaveRecord*> records;
    RecordsOf(inv, records);
    std::vector<std::wstring> ids;
    ids.reserve(records.size());
    for (const SaveRecord* r : records) ids.push_back(IdentityOf(*r));
    std::sort(ids.begin(), ids.end());
    return ids;
}

void SanitizeVitals(PlayerInventory::Vitals& v) {
    if (!std::isfinite(v.maxHealth)) v.maxHealth = 100.f;
    v.maxHealth = std::clamp(v.maxHealth, 1.f, 10000.f);
    if (!std::isfinite(v.health)) v.health = v.maxHealth;
    if (!std::isfinite(v.food)) v.food = 100.f;
    if (!std::isfinite(v.stamina)) v.stamina = 100.f;
    // A persisted corpse is not a valid reconnect state: native death/respawn
    // owns zero-health transitions. Restore a live pawn within its real cap.
    v.health = std::clamp(v.health, 1.f, v.maxHealth);
    v.food = std::clamp(v.food, 0.f, 100.f);
    v.stamina = std::clamp(v.stamina, 0.f, 100.f);
}

bool ValidateClientInventory(HostEntry& e, const PlayerInventory& proposed,
                             bool& ownershipChanged) {
    std::vector<const SaveRecord*> oldRecords, newRecords;
    RecordsOf(e.state, oldRecords);
    RecordsOf(proposed, newRecords);
    std::set<std::wstring> nonEmptyKeys;
    for (const SaveRecord* r : newRecords) {
        if (r->className.empty()) return false;
        if (!r->key.empty() && r->key != L"None" && !nonEmptyKeys.insert(r->key).second)
            return false;
    }
    std::vector<bool> used(oldRecords.size(), false);
    const auto now = Clock::now();
    auto pending = e.pendingPickups;
    while (!pending.empty() && pending.front().deadline < now)
        pending.pop_front();
    for (const SaveRecord* incoming : newRecords) {
        size_t found = oldRecords.size();
        for (size_t i = 0; i < oldRecords.size(); ++i) {
            if (used[i]) continue;
            const SaveRecord& old = *oldRecords[i];
            const bool same = (!incoming->key.empty() && incoming->key != L"None")
                ? (incoming->key == old.key && incoming->className == old.className)
                : ((old.key.empty() || old.key == L"None") &&
                   incoming->className == old.className);
            if (same) { found = i; break; }
        }
        if (found != oldRecords.size()) { used[found] = true; continue; }

        auto permit = std::find_if(pending.begin(), pending.end(),
            [&](const HostEntry::PendingPickup& p) {
                if (p.deadline < now || p.className != incoming->className) return false;
                return p.key.empty() || p.key == L"None" || p.key == incoming->key;
            });
        if (permit == pending.end()) return false;
        pending.erase(permit);  // one host-approved world item -> one addition
    }
    e.pendingPickups = std::move(pending);  // commit permits only after full validation
    ownershipChanged = IdentitySet(e.state) != IdentitySet(proposed);
    return true;
}

// ---- The live per-player apply on join (host->client push + the SaveObjectReadyHook) ----
// The host pushes each joiner its persisted inventory (coop_players/<slot>/<guid>.json); the client
// buffers it (g_pendingApply) and the pre-materialize SaveObjectReadyHook substitutes it into the
// freshly-loaded save object BEFORE the native loadObjects() builds the live world from it -- so the
// joiner gets THEIR OWN per-player items, not the host's save inventory. This IS the inventory
// behaviour now (no rollout flag -- RULE 2: a flag that kept the old v56 host-save inheritance as a
// parallel path was migration baggage, retired once the apply was verified end-to-end). The only
// fallback is the SaveObjectReadyHook no-op below, for the degenerate case where the apply blob
// genuinely never arrived -- it KEEPS the loaded inventory rather than wiping.

// CLIENT: the host's on-join apply blob, reassembled here (separate Assembler from the host's
// receive path -- host->client is a distinct stream direction). Deserialized into g_pendingApply.
coop::blob_chunks::Assembler g_clientAssembler;
ue_wrap::inventory::PlayerInventory g_pendingApply;
std::atomic<bool> g_hasPendingApply{false};

// HOST: per-slot send sequence for the on-join inventory push (independent of the client stream's
// g_sendSeq space; assembler keys are per-(sender,seq) so they never collide cross-direction).
std::array<uint32_t, coop::net::kMaxPeers> g_hostSendSeq{};
// HOST: per-slot "apply blob already pushed this connection" latch. The push must land in the
// joiner's PRE-WORLD window (the ConnectReplayForSlot edge fires at world-ready, too late), so it
// is driven from the host tick the instant the slot is connected AND its Join GUID has arrived --
// which is well before the client finishes its save transfer + loads. Reset on disconnect.
std::array<bool, coop::net::kMaxPeers> g_applySentToSlot{};

// Build <gameDir>/coop_players/<hostSlot>/<guid>.json. Empty path if any piece is missing.
// User request (2026-06-15): store in the GAME folder (next to the payload DLL / multivoid.ini/.log), NOT in
// AppData -- so the per-player inventory jsons are easy to find + hand-edit. Still keyed per host
// SAVE SLOT (a subfolder under coop_players) so different worlds keep separate inventories.
fs::path PlayerFilePath(const std::string& guid) {
    // Defense in depth (the wire boundary already validates): a GUID that is not exactly 32 hex
    // chars must NEVER become a path component -- return empty so every write path no-ops cleanly,
    // foreclosing path traversal even if a non-hex guid ever reaches here. (Adversarial-verify HIGH.)
    if (!coop::player_handshake::IsValidGuid(guid)) return {};
    const std::wstring base = ue_wrap::paths::ExeDir();
    if (base.empty()) return {};
    const std::wstring slot = coop::save_transfer::HostSlot();
    if (slot.empty() || slot == L"." || slot == L".." || slot.size() > 64 ||
        !std::all_of(slot.begin(), slot.end(), [](wchar_t c) {
            return std::iswalnum(c) != 0 || c == L'_' || c == L'-' || c == L'.';
        })) return {};
    return fs::path(base) / L"coop_players" / slot / (std::wstring(guid.begin(), guid.end()) + L".json");
}

std::string Hex(const std::vector<uint8_t>& b) {
    static const char k[] = "0123456789abcdef";
    std::string s;
    s.reserve(b.size() * 2);
    for (uint8_t c : b) { s.push_back(k[c >> 4]); s.push_back(k[c & 0xF]); }
    return s;
}

// The nick goes into a JSON string field below. ENCODING is the codec's job
// (until 2026-07-28 this dropped every non-ASCII byte, so a Cyrillic player's
// record carried an empty name); ESCAPING is this site's, because the container
// is JSON. Raw UTF-8 is valid inside a JSON string -- only the two structural
// metacharacters have to go, and they cannot appear inside a multi-byte sequence
// (continuation bytes are all >= 0x80), so dropping them cannot corrupt one.
std::string NickForJson(const std::wstring& w) {
    std::string s = coop::text::CapUtf8Bytes(coop::text::ToUtf8(w), coop::text::kNickMaxBytes);
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](char c) { return c == '"' || c == '\\'; }),
            s.end());
    return s;
}

bool ParseBlobFile(const fs::path& file, std::vector<uint8_t>& outBlob);

// Persist `blob` to `file` ROBUSTLY: magic + FNV integrity + a readable nick/lastSeen, written
// atomically (tmp + rename) and keeping a .bak of the last-good file so a corrupt edit can be
// recovered (the user-tinkering case). Returns false on I/O failure (logged).
bool WriteBlobFile(const fs::path& file, const std::vector<uint8_t>& blob,
                   const std::string& nick, bool mirrorNewToBackup = false) {
    if (file.empty()) return false;
    if (blob.size() > coop::blob_chunks::MaxBlobBytes()) {
        UE_LOGW("player_inventory: profile blob %zu exceeds transport/persistence cap %zu -- refusing",
                blob.size(), coop::blob_chunks::MaxBlobBytes());
        return false;
    }
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    if (ec) {
        UE_LOGW("player_inventory: create_directories('%ls') failed: %s",
                file.parent_path().c_str(), ec.message().c_str());
        return false;
    }
    // Keep only a VERIFIED generation as <file>.bak; never replace a good
    // backup with a corrupt/truncated primary.
    std::vector<uint8_t> verifiedOld;
    if (!mirrorNewToBackup && fs::exists(file, ec) && ParseBlobFile(file, verifiedOld))
        fs::copy_file(file, fs::path(file).concat(L".bak"), fs::copy_options::overwrite_existing, ec);
    const uint64_t fnv = coop::blob_chunks::Fnv64(blob);
    const long long epoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    char fnvhex[17] = {};
    std::snprintf(fnvhex, sizeof(fnvhex), "%016llx", static_cast<unsigned long long>(fnv));
    std::string json = "{\"magic\":\"VCPI\",\"ver\":2,\"fnv\":\"";
    json += fnvhex;
    json += "\",\"nick\":\"";
    json += nick;
    json += "\",\"lastSeen\":";
    json += std::to_string(epoch);
    json += ",\"blob\":\"";
    json += Hex(blob);
    json += "\"}\n";
    const auto writeAtomically = [&](const fs::path& target) {
        const fs::path tmp = fs::path(target).concat(L".part");
        FILE* raw = nullptr;
#ifdef _WIN32
        raw = _wfopen(tmp.c_str(), L"wb");
#else
        raw = std::fopen(tmp.c_str(), "wb");
#endif
        if (!raw || std::fwrite(json.data(), 1, json.size(), raw) != json.size() ||
            std::fflush(raw) != 0) {
            if (raw) std::fclose(raw);
            UE_LOGE("player_inventory: write failed ('%ls')", tmp.c_str());
            return false;
        }
#ifdef _WIN32
        const bool durable = _commit(_fileno(raw)) == 0;
#else
        const bool durable = ::fsync(fileno(raw)) == 0;
#endif
        const bool closed = std::fclose(raw) == 0;
        if (!durable || !closed) {
            UE_LOGE("player_inventory: flush/close failed ('%ls')", tmp.c_str());
            return false;
        }
#ifdef _WIN32
        if (!::MoveFileExW(tmp.c_str(), target.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            UE_LOGE("player_inventory: atomic replace('%ls') failed: winerr=%lu",
                    target.c_str(), static_cast<unsigned long>(::GetLastError()));
            return false;
        }
#else
        fs::rename(tmp, target, ec);
        if (ec) {
            UE_LOGE("player_inventory: atomic rename('%ls') failed: %s",
                    target.c_str(), ec.message().c_str());
            return false;
        }
#endif
        return true;
    };
    if (!writeAtomically(file)) return false;
    if (mirrorNewToBackup &&
        !writeAtomically(fs::path(file).concat(L".bak"))) {
        UE_LOGE("player_inventory: exclusion checkpoint primary committed but backup mirror failed "
                "('%ls') -- refusing ownership transfer", file.c_str());
        return false;
    }
    return true;
}

// Decode a lowercase/upper hex string to bytes. False on an odd length or a non-hex digit.
bool UnHex(const std::string& s, std::vector<uint8_t>& out) {
    if (s.size() & 1) return false;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    out.clear();
    out.reserve(s.size() / 2);
    for (size_t i = 0; i < s.size(); i += 2) {
        const int hi = nib(s[i]), lo = nib(s[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

// Extract the value of a "key":"..." string field from our flat JSON (no nesting/escapes used).
bool JsonStr(const std::string& doc, const char* key, std::string& out) {
    std::string needle = std::string("\"") + key + "\":\"";
    const size_t k = doc.find(needle);
    if (k == std::string::npos) return false;
    const size_t v = k + needle.size();
    const size_t e = doc.find('"', v);
    if (e == std::string::npos) return false;
    out = doc.substr(v, e - v);
    return true;
}

bool JsonInt(const std::string& doc, const char* key, int& out) {
    const std::string needle = std::string("\"") + key + "\":";
    const size_t k = doc.find(needle);
    if (k == std::string::npos) return false;
    const size_t v = k + needle.size();
    size_t used = 0;
    try { out = std::stoi(doc.substr(v), &used); }
    catch (...) { return false; }
    return used != 0;
}

// Parse one persisted inventory file at `file` into `outBlob`. Defensive against host tinkering
// (the user-edit case): requires magic "VCPI", a parseable hex blob, and a matching FNV (the
// blob's stored integrity hash). False (caller tries .bak, then EMPTY) on any failure -- never
// throws, never returns unverified bytes.
bool ParseBlobFile(const fs::path& file, std::vector<uint8_t>& outBlob) {
    std::error_code ec;
    if (file.empty() || !fs::exists(file, ec)) return false;
    constexpr uintmax_t kMaxProfileFileBytes =
        static_cast<uintmax_t>(coop::blob_chunks::MaxBlobBytes() * 2 + 4096);
    const uintmax_t fileBytes = fs::file_size(file, ec);
    if (ec || fileBytes > kMaxProfileFileBytes) {
        UE_LOGW("player_inventory: '%ls' exceeds bounded profile size -- refusing", file.c_str());
        return false;
    }
    std::ifstream f(file, std::ios::binary);
    if (!f) return false;
    std::string doc((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string magic;
    if (!JsonStr(doc, "magic", magic) || magic != "VCPI") {
        UE_LOGW("player_inventory: '%ls' missing/bad magic -- treating as corrupt", file.c_str());
        return false;
    }
    int schema = 0;
    if (!JsonInt(doc, "ver", schema) || schema != 2) {
        UE_LOGW("player_inventory: '%ls' profile schema mismatch (have=%d want=2) -- refusing",
                file.c_str(), schema);
        return false;
    }
    std::string blobHex, fnvHex;
    if (!JsonStr(doc, "blob", blobHex) || !JsonStr(doc, "fnv", fnvHex)) return false;
    if (blobHex.size() > coop::blob_chunks::MaxBlobBytes() * 2) return false;
    std::vector<uint8_t> blob;
    if (!UnHex(blobHex, blob)) {
        UE_LOGW("player_inventory: '%ls' blob hex unparseable -- corrupt", file.c_str());
        return false;
    }
    char want[17] = {};
    std::snprintf(want, sizeof(want), "%016llx",
                  static_cast<unsigned long long>(coop::blob_chunks::Fnv64(blob)));
    if (fnvHex != want) {
        UE_LOGW("player_inventory: '%ls' FNV mismatch (have %s, stored %s) -- corrupt/tampered",
                file.c_str(), want, fnvHex.c_str());
        return false;
    }
    PlayerInventory decoded;
    if (!coop::inventory_wire::Deserialize(blob, decoded)) {
        UE_LOGW("player_inventory: '%ls' inner profile payload is malformed/incompatible -- refusing",
                file.c_str());
        return false;
    }
    outBlob = std::move(blob);
    return true;
}

// Read peer GUID `guid`'s persisted inventory blob, FNV-verified, with a .bak fallback. Returns
// false (caller uses an EMPTY inventory) when neither the file nor its .bak is valid -- the
// fail-safe that guarantees a corrupt/missing file can NEVER leak another player's inventory or
// crash; the player just starts empty.
bool ReadBlobFile(const std::string& guid, std::vector<uint8_t>& outBlob) {
    const fs::path file = PlayerFilePath(guid);
    if (file.empty()) return false;
    if (ParseBlobFile(file, outBlob)) return true;
    const fs::path bak = fs::path(file).concat(L".bak");
    if (ParseBlobFile(bak, outBlob)) {
        UE_LOGW("player_inventory: recovered guid=%s inventory from .bak (primary corrupt)",
                guid.c_str());
        return true;
    }
    return false;
}

// Flush one host slot's pending blob to its <guid>.json (ignores the rate-limit -- used on
// disconnect/shutdown). Clears dirty.
void FlushSlotUnlocked(int slot) {
    if (slot < 0 || slot >= coop::net::kMaxPeers) return;
    HostEntry& e = g_hostBySlot[slot];
    if (!e.dirty || e.guid.empty()) return;
    // Use the nick SNAPSHOT captured in OnReliable (game thread). FlushSlot is reachable from the
    // window-thread shutdown flush (FlushAllToDisk), so it must touch NO game-thread-only state
    // (NicknameForSlot reads a GT-only side table -- a torn-wstring race on host exit otherwise).
    if (WriteBlobFile(PlayerFilePath(e.guid), e.blob, e.nick)) {
        e.dirty = false;
        e.lastWrite = Clock::now();
        UE_LOGI("player_inventory: flushed slot %d guid=%s (%zu-byte blob) to disk",
                slot, e.guid.c_str(), e.blob.size());
    }
}

// CLIENT: poll the live inventory, serialize, and stream to the host ON CHANGE (~1 Hz).
void ClientStreamTick(coop::net::Session* s) {
    const Clock::time_point now = Clock::now();
    if (now - g_lastPoll < kClientPoll) return;
    g_lastPoll = now;
    ue_wrap::inventory::PlayerInventory inv;
    if (!ue_wrap::inventory::ReadAll(inv)) return;  // saveSlot not up yet
    ue_wrap::inventory::LivePersonalStore live;
    if (!ue_wrap::inventory::ReadLivePersonalStore(live)) return;
    inv.inventory = std::move(live.records);  // actual carried objects, not stale save projection
    (void)ue_wrap::vitals::Read(ue_wrap::vitals::Field::Health, &inv.vitals.health);
    (void)ue_wrap::vitals::Read(ue_wrap::vitals::Field::MaxHealth, &inv.vitals.maxHealth);
    (void)ue_wrap::vitals::Read(ue_wrap::vitals::Field::Food, &inv.vitals.food);
    (void)ue_wrap::vitals::Read(ue_wrap::vitals::Field::Sleep, &inv.vitals.stamina);
    SanitizeVitals(inv.vitals);
    const std::vector<uint8_t> blob = coop::inventory_wire::Serialize(inv);
    const uint64_t hash = coop::blob_chunks::Fnv64(blob);
    if (hash == g_lastSentHash) return;  // unchanged -> don't re-send
    if (coop::blob_chunks::SendBlob(s, coop::net::ReliableKind::PlayerInventoryBlob, ++g_sendSeq, blob)) {
        g_lastSentHash = hash;
        if (now - g_lastStreamLog >= std::chrono::seconds(60)) {
            g_lastStreamLog = now;
            UE_LOGI("player-state[client]: checkpoint stream alive (%zu bytes, %zu carried items)",
                    blob.size(), inv.inventory.size());
        }
    }  // else: channel busy -> retry next poll under a fresh seq (the blob unchanged)
}

// HOST: sweep stale half-assemblies + flush rate-limited dirty blobs + push each newly-connected
// joiner its per-player apply blob ONCE (the pre-world connect edge -- see g_applySentToSlot).
void HostPersistTick(coop::net::Session* s) {
    const Clock::time_point now = Clock::now();
    if (now - g_lastSweep < std::chrono::seconds(1)) return;
    g_lastSweep = now;
    std::lock_guard<std::recursive_mutex> lk(g_hostMutex);
    g_assembler.Sweep(now, kAsmTtl);
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) {
        HostEntry& e = g_hostBySlot[slot];
        if (e.dirty && now - e.lastWrite >= kWriteRate) FlushSlotUnlocked(slot);
        // Connect-edge apply push: once a slot is connected AND its GUID has arrived (carried in the
        // Join), send it its persisted per-player inventory ONCE, latching on a successful enqueue.
        // SendInventoryToSlot returns false on a channel-busy refusal (or a not-yet-arrived GUID), so
        // an un-latched failure simply retries on the next 1 Hz tick until it goes out -- the original
        // "latch even on refusal" bug ("no inventory blob") is fixed by latching on TRUE only. GNS
        // reliable delivery guarantees the single enqueued blob reaches the joiner during its pre-world
        // wait, where the (now PRE-WORLD-installed) receiver buffers it for OnSaveObjectReady. No spray
        // is needed: the earlier "never arrived" symptom was the receiver's g_session being null during
        // the wait (the world-gated Install bug, now fixed at StartCoopSession), not lossy delivery.
        if (!g_applySentToSlot[slot] && s->IsSlotConnected(slot) &&
            !coop::player_handshake::GuidForSlot(slot).empty()) {
            if (SendInventoryToSlot(slot)) g_applySentToSlot[slot] = true;
        }
    }
}

// CLIENT: the engine's pre-materialize hook (registered in Install). Fires on the game thread
// with the freshly loaded/created save object, BEFORE the native loadObjects() builds the world
// from it -- the one window to substitute this client's per-player inventory. Self-gates: only a
// CLIENT, only with a pending blob (the join boot waits for it). On a miss it SKIPS (leaves the
// loaded inventory) rather than wiping -- never destroys data it can't replace.
bool OnSaveObjectReady(void* saveSlotObject) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Client) return true;  // only a joining client applies
    if (!g_hasPendingApply.load(std::memory_order_acquire)) {
        UE_LOGE("player_inventory: SaveObjectReady but no personal profile arrived -- refusing "
                "the world load rather than retaining host-derived player state");
        return false;
    }
    const auto& v = g_pendingApply.vitals;
    const bool vitalsApplied =
        ue_wrap::vitals::WriteToSaveObject(saveSlotObject, ue_wrap::vitals::Field::MaxHealth, v.maxHealth) &&
        ue_wrap::vitals::WriteToSaveObject(saveSlotObject, ue_wrap::vitals::Field::Health, v.health) &&
        ue_wrap::vitals::WriteToSaveObject(saveSlotObject, ue_wrap::vitals::Field::Food, v.food) &&
        ue_wrap::vitals::WriteToSaveObject(saveSlotObject, ue_wrap::vitals::Field::Sleep, v.stamina);
    if (!vitalsApplied) {
        UE_LOGE("player_inventory[client]: vital-field apply FAILED on %p -- refusing world load",
                saveSlotObject);
        return false;
    }
    if (ue_wrap::inventory::ApplyToSaveObject(saveSlotObject, g_pendingApply)) {
        UE_LOGI("player_inventory[client]: applied per-player inventory to save object %p "
                "(inventory=%zu equip=%zu hold=%zu vitals=%s) -- RETIRES the v56 host-save inheritance",
                saveSlotObject, g_pendingApply.inventory.size(),
                g_pendingApply.equipment.size(), g_pendingApply.hold.size(),
                vitalsApplied ? "applied" : "FAILED");
        return true;
    } else {
        UE_LOGE("player_inventory[client]: ApplyToSaveObject FAILED on %p -- the client will "
                "refuse the world load (no host-inventory fallback)", saveSlotObject);
        return false;
    }
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    // Inc 4: arm the engine's pre-materialize apply point (no-op until a client join with the
    // apply gate on + a pending blob). Idempotent re-arm across Stop()/Start() is harmless.
    ue_wrap::engine::SetSaveObjectReadyHook(&OnSaveObjectReady);
}

void Tick() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (s && s->connected()) {
        if (s->role() == coop::net::Role::Client) ClientStreamTick(s);
        else                                      HostPersistTick(s);
    }

    // INCREMENT 2 read-verify self-test (ini inventory_selftest=1). One-shot; dev diagnostic.
    static const bool s_selftest = ::coop::config::ResolveFlag(::coop::config_registry::rows::inventory_selftest);
    if (!s_selftest) return;
    static bool s_done = false;
    if (s_done) return;
    static int s_ticks = 0;
    if (++s_ticks < 600) return;  // ~5s settle (world up + saveSlot resolvable)
    ue_wrap::inventory::PlayerInventory inv;
    if (!ue_wrap::inventory::ReadAll(inv)) return;  // saveSlot not up yet -> retry next tick
    s_done = true;
    UE_LOGI("inventory[selftest]: read local saveSlot -- inventory=%zu equipment=%zu hold=%zu",
            inv.inventory.size(), inv.equipment.size(), inv.hold.size());
    {   // DIAG: name what the LOCAL player actually has post-spawn (find flashlight/glasses/compass).
        auto narrow = [](const std::wstring& w) { return std::string(w.begin(), w.end()); };
        for (size_t i = 0; i < inv.inventory.size(); ++i)
            UE_LOGI("  selftest inv[%zu]: className='%s' key='%s'", i,
                    narrow(inv.inventory[i].className).c_str(), narrow(inv.inventory[i].key).c_str());
        for (size_t i = 0; i < inv.equipment.size(); ++i)
            UE_LOGI("  selftest eq[%zu]: propName='%s' className='%s'", i,
                    narrow(inv.equipment[i].propName).c_str(), narrow(inv.equipment[i].data.className).c_str());
        for (size_t i = 0; i < inv.hold.size(); ++i)
            UE_LOGI("  selftest hold[%zu]: propName='%s' className='%s'", i,
                    narrow(inv.hold[i].propName).c_str(), narrow(inv.hold[i].data.className).c_str());
    }
    const std::vector<uint8_t> blob1 = coop::inventory_wire::Serialize(inv);
    ue_wrap::inventory::PlayerInventory inv2;
    const bool de = coop::inventory_wire::Deserialize(blob1, inv2);
    std::vector<uint8_t> blob2;
    if (de) blob2 = coop::inventory_wire::Serialize(inv2);
    const bool roundtrip = de && blob1 == blob2;
    UE_LOGI("inventory[selftest]: serialize=%zu bytes, deserialize=%d, ROUND-TRIP %s "
            "(inv2: inventory=%zu equip=%zu hold=%zu)",
            blob1.size(), de ? 1 : 0, roundtrip ? "OK" : "MISMATCH",
            inv2.inventory.size(), inv2.equipment.size(), inv2.hold.size());

    // RULE-1 proper-fix PROBE (ini starterkit_test=1, dev-only): equip the 3 SP starters via the
    // game's OWN AddEquipment (begin_equipment::GiveFromClass) + re-read, confirming the canonical
    // equip path adds the items BEFORE it's wired to the first-join edge. One-shot (rides this
    // one-shot selftest). Removed once first-join uses it.
    if (::coop::config::ResolveFlag(::coop::config_registry::rows::starterkit_test)) {
        for (const wchar_t* c : {L"prop_equipment_flashlight_C", L"prop_equipment_glasses_C",
                                 L"prop_equipment_compass_C"})
            ue_wrap::begin_equipment::GiveFromClass(c);
        ue_wrap::inventory::PlayerInventory after;
        if (ue_wrap::inventory::ReadAll(after))
            UE_LOGI("starterkit[probe]: after AddEquipment x3 -- inventory=%zu equip=%zu hold=%zu "
                    "(was %zu/%zu/%zu)", after.inventory.size(), after.equipment.size(),
                    after.hold.size(), inv.inventory.size(), inv.equipment.size(), inv.hold.size());
    }
}

void OnReliable(const coop::net::BlobChunkPayload& p, uint8_t senderPeerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s) return;

    // CLIENT direction (Inc 4): the host (slot 0) pushed this client its per-player apply blob.
    if (s->role() == coop::net::Role::Client) {
        if (senderPeerSlot != 0) return;  // only the host pushes the apply blob
        std::vector<uint8_t> blob;
        if (!g_clientAssembler.OnChunk(p, senderPeerSlot, blob)) return;  // not complete yet
        ue_wrap::inventory::PlayerInventory inv;
        if (!coop::inventory_wire::Deserialize(blob, inv)) {
            UE_LOGW("player_inventory[client]: host apply blob (%zu bytes) failed to deserialize "
                    "-- ignoring (will keep waiting / fall back)", blob.size());
            return;
        }
        g_pendingApply = std::move(inv);
        SanitizeVitals(g_pendingApply.vitals);
        g_hasPendingApply.store(true, std::memory_order_release);
        UE_LOGI("player_inventory[client]: received per-player apply blob from host (%zu bytes, "
                "inventory=%zu equip=%zu hold=%zu)", blob.size(), g_pendingApply.inventory.size(),
                g_pendingApply.equipment.size(), g_pendingApply.hold.size());
        return;
    }

    // HOST direction (Inc 3): a CLIENT slot streamed its live inventory to persist.
    if (s->role() != coop::net::Role::Host) return;
    if (senderPeerSlot < 1 || senderPeerSlot >= coop::net::kMaxPeers) return;  // a CLIENT slot
    if (!s->IsSlotReady(senderPeerSlot)) return;
    std::lock_guard<std::recursive_mutex> lk(g_hostMutex);
    std::vector<uint8_t> blob;
    if (!g_assembler.OnChunk(p, senderPeerSlot, blob)) return;  // not complete yet
    const std::string& guid = coop::player_handshake::GuidForSlot(senderPeerSlot);
    if (guid.empty()) {
        UE_LOGW("player_inventory: inventory blob from slot %u but no GUID -- dropping",
                senderPeerSlot);
        return;
    }
    PlayerInventory proposed;
    if (!coop::inventory_wire::Deserialize(blob, proposed)) {
        UE_LOGW("player-state: rejected malformed inventory/profile blob from slot %u", senderPeerSlot);
        return;
    }
    SanitizeVitals(proposed.vitals);
    HostEntry& e = g_hostBySlot[senderPeerSlot];
    if (!e.loaded || e.guid != guid) {
        UE_LOGW("player-state: rejected update from slot %u before authoritative profile load", senderPeerSlot);
        return;
    }
    bool ownershipChanged = false;
    if (!ValidateClientInventory(e, proposed, ownershipChanged)) {
        UE_LOGW("player-state: refused unapproved inventory addition/duplicate from slot %u guid=%.8s",
                senderPeerSlot, guid.c_str());
        return;
    }
    e.state = std::move(proposed);
    e.blob = coop::inventory_wire::Serialize(e.state);
    const uint64_t hash = coop::blob_chunks::Fnv64(e.blob);
    if (e.hash == hash && !e.guid.empty()) return;
    e.guid = guid;
    e.hash = hash;
    e.nick = NickForJson(coop::player_handshake::NicknameForSlot(senderPeerSlot));  // GT snapshot
    e.dirty = true;
    // Write now if the rate-limit window has passed; else HostPersistTick flushes it.
    if (ownershipChanged) {
        UE_LOGI("player-state: checkpoint reason=inventory-change slot=%u id=%.8s items=%zu",
                senderPeerSlot, guid.c_str(), e.state.inventory.size());
        FlushSlotUnlocked(senderPeerSlot);
    } else if (Clock::now() - e.lastWrite >= kWriteRate) {
        FlushSlotUnlocked(senderPeerSlot);
    }
}

bool SendInventoryToSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return false;  // host pushes; clients receive
    if (peerSlot < 1 || peerSlot >= coop::net::kMaxPeers) return false;
    std::lock_guard<std::recursive_mutex> lk(g_hostMutex);
    const std::string& guid = coop::player_handshake::GuidForSlot(peerSlot);
    if (guid.empty()) {
        UE_LOGI("player_inventory: slot %d has no GUID yet -- not sending an apply blob this edge",
                peerSlot);
        return false;  // not sent -> caller does not latch; retries when the GUID lands
    }
    std::vector<uint8_t> blob;
    if (!ReadBlobFile(guid, blob)) {
        // Never derive a remote profile from the host's live records: doing so
        // cloned both ownership and persistent item keys. New remote players
        // start with an explicit empty inventory and healthy survival defaults.
        blob = coop::inventory_wire::Serialize(PlayerInventory{});
        UE_LOGI("player-state: new profile slot=%d id=%.8s -- EMPTY inventory + default vitals",
                peerSlot, guid.c_str());
    }
    PlayerInventory state;
    if (!coop::inventory_wire::Deserialize(blob, state)) {
        UE_LOGW("player-state: profile for slot=%d id=%.8s failed payload decode -- safe EMPTY recovery",
                peerSlot, guid.c_str());
        state = PlayerInventory{};
        blob = coop::inventory_wire::Serialize(state);
    }
    SanitizeVitals(state.vitals);
    blob = coop::inventory_wire::Serialize(state);
    HostEntry& entry = g_hostBySlot[peerSlot];
    entry.guid = guid;
    entry.nick = NickForJson(coop::player_handshake::NicknameForSlot(peerSlot));
    entry.state = state;
    entry.blob = blob;
    entry.hash = coop::blob_chunks::Fnv64(blob);
    entry.loaded = true;
    if (coop::blob_chunks::SendBlobToSlot(s, peerSlot, coop::net::ReliableKind::PlayerInventoryBlob,
                                          ++g_hostSendSeq[peerSlot], blob)) {
        UE_LOGI("player-state: restored slot=%d id=%.8s inventory=%zu equip=%zu hold=%zu (%zu bytes)",
                peerSlot, guid.c_str(), state.inventory.size(), state.equipment.size(),
                state.hold.size(), blob.size());
        return true;
    }
    UE_LOGW("player_inventory: send to slot %d refused (channel busy) -- will RETRY next tick "
            "(NOT latched as sent)", peerSlot);
    return false;
}

bool HasPendingApply() { return g_hasPendingApply.load(std::memory_order_acquire); }

void OnDisconnectForSlot(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;
    std::lock_guard<std::recursive_mutex> lk(g_hostMutex);
    FlushSlotUnlocked(peerSlot);         // last authoritative state to disk
    if (peerSlot >= 0 && peerSlot < coop::net::kMaxPeers) {
        g_assembler.ClearSlot(static_cast<uint8_t>(peerSlot));
        g_hostBySlot[peerSlot] = HostEntry{};
        g_applySentToSlot[peerSlot] = false;  // a rejoin re-pushes the apply blob
    }
}

void OnDisconnect() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (s && s->role() == coop::net::Role::Host) {
        std::lock_guard<std::recursive_mutex> lk(g_hostMutex);
        for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) {
            FlushSlotUnlocked(slot);
            g_hostBySlot[slot] = HostEntry{};
            g_applySentToSlot[slot] = false;
        }
    }
    // Client: reset the send-dedup so a reconnect re-streams, and drop any pending apply blob +
    // its assembler so a rejoin waits for a FRESH host push (never applies a stale inventory to
    // a new world).
    g_lastSentHash = 0;
    g_lastPoll = Clock::time_point{};
    g_lastStreamLog = Clock::time_point{};
    g_hasPendingApply.store(false, std::memory_order_release);
    g_pendingApply = ue_wrap::inventory::PlayerInventory{};
    g_assembler.Clear();
    g_clientAssembler.Clear();
}

void FlushAllToDisk() {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;
    std::lock_guard<std::recursive_mutex> lk(g_hostMutex);
    for (int slot = 1; slot < coop::net::kMaxPeers; ++slot) FlushSlotUnlocked(slot);
}

void EnsurePlayerFile(int peerSlot) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host) return;  // the host owns the per-player files
    const std::string& guid = coop::player_handshake::GuidForSlot(peerSlot);
    if (guid.empty()) {
        UE_LOGI("player_inventory: slot %d has no GUID yet (Join not landed / pre-v73 peer) -- "
                "no file this edge", peerSlot);
        return;
    }
    const fs::path file = PlayerFilePath(guid);
    if (file.empty()) {
        UE_LOGW("player_inventory: cannot build path for slot %d guid=%s "
                "(SaveGamesDir / HostSlot empty) -- skipping", peerSlot, guid.c_str());
        return;
    }
    std::error_code ec;
    if (fs::exists(file, ec)) {
        UE_LOGI("player_inventory: slot %d guid=%s -- file already present ('%ls')",
                peerSlot, guid.c_str(), file.c_str());
        return;
    }
    // First join for this GUID on this save -> write an EMPTY-inventory file in the real
    // magic+FNV format (NOT a raw placeholder), so the on-disk format is uniform. The client's
    // inventory stream overwrites it ~1 s later if it has items.
    const std::vector<uint8_t> empty = coop::inventory_wire::Serialize(ue_wrap::inventory::PlayerInventory{});
    const std::string nick = NickForJson(coop::player_handshake::NicknameForSlot(peerSlot));
    if (WriteBlobFile(file, empty, nick))
        UE_LOGI("player_inventory: created empty inventory file for slot %d guid=%s ('%ls')",
                peerSlot, guid.c_str(), file.c_str());
}

bool AuthorizeWorldPickup(int peerSlot, void* actor, const std::wstring& key) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host || peerSlot < 1 ||
        peerSlot >= coop::net::kMaxPeers || !s->IsSlotReady(peerSlot) || !actor) return false;
    const auto auth = coop::element::IntentTarget::ForClientIntent(
        *s, static_cast<uint8_t>(peerSlot), kInventoryReachUU).Authorize(actor);
    if (!auth) {
        UE_LOGW("player-state: pickup refused slot=%d reason=%s", peerSlot,
                coop::element::OutcomeName(auth.outcome));
        return false;
    }
    std::lock_guard<std::recursive_mutex> lk(g_hostMutex);
    HostEntry& e = g_hostBySlot[peerSlot];
    if (!e.loaded || e.guid != coop::player_handshake::GuidForSlot(peerSlot)) return false;
    while (!e.pendingPickups.empty() && e.pendingPickups.front().deadline < Clock::now())
        e.pendingPickups.pop_front();
    if (e.pendingPickups.size() >= kMaxPendingPickups) {
        UE_LOGW("player-state: pickup permit cap %zu reached for slot=%d -- refusing",
                kMaxPendingPickups, peerSlot);
        return false;
    }
    const std::wstring actualKey = ue_wrap::prop::GetInteractableKeyString(actor);
    e.pendingPickups.push_back(HostEntry::PendingPickup{
        (actualKey.empty() || actualKey == L"None") ? key : actualKey,
        ue_wrap::reflection::ClassNameOf(actor), Clock::now() + kPickupTtl});
    UE_LOGI("player-state: authorized world->inventory pickup slot=%d id=%.8s key='%ls' cls='%ls'",
            peerSlot, e.guid.c_str(), e.pendingPickups.back().key.c_str(),
            e.pendingPickups.back().className.c_str());
    return true;
}

bool CommitInventoryDrop(int peerSlot, const std::wstring& key,
                         const std::wstring& className) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (!s || s->role() != coop::net::Role::Host || peerSlot < 1 ||
        peerSlot >= coop::net::kMaxPeers || !s->IsSlotReady(peerSlot)) return false;
    std::lock_guard<std::recursive_mutex> lk(g_hostMutex);
    HostEntry& e = g_hostBySlot[peerSlot];
    if (!e.loaded || e.guid != coop::player_handshake::GuidForSlot(peerSlot)) return false;

    PlayerInventory next = e.state;
    size_t matches = 0;
    auto isMatch = [&](const SaveRecord& r) {
        const bool yes = r.key == key && r.className == className;
        if (yes) ++matches;
        return yes;
    };
    next.inventory.erase(std::remove_if(next.inventory.begin(), next.inventory.end(), isMatch),
                         next.inventory.end());
    for (auto& row : next.equipment)
        if (isMatch(row.data)) row = ue_wrap::inventory::EquipRecord{};
    for (auto& row : next.hold)
        if (isMatch(row.data)) row = ue_wrap::inventory::EquipRecord{};
    if (matches == 0) {
        // A legitimate pickup can be dropped again before the client's 1 Hz
        // profile stream reports the intermediate inventory row. In that case
        // ownership is represented by the host-issued, exact-class pickup
        // permit. Consume exactly one matching in-flight transition; multiple
        // keyless same-class permits are ambiguous and fail closed.
        const auto now = Clock::now();
        while (!e.pendingPickups.empty() && e.pendingPickups.front().deadline < now)
            e.pendingPickups.pop_front();
        auto pending = e.pendingPickups.end();
        size_t pendingMatches = 0;
        for (auto it = e.pendingPickups.begin(); it != e.pendingPickups.end(); ++it) {
            if (it->className != className) continue;
            if (!it->key.empty() && it->key != L"None" && it->key != key) continue;
            pending = it;
            ++pendingMatches;
        }
        if (pendingMatches == 1) {
            // The durable profile already excludes this in-flight item. Flush
            // that exclusion before authorizing the replacement world actor.
            if (!WriteBlobFile(PlayerFilePath(e.guid), e.blob, e.nick,
                               /*mirrorNewToBackup=*/true)) {
                UE_LOGE("player-state: in-flight inventory->world checkpoint failed "
                        "slot=%d key='%ls' -- no world spawn", peerSlot, key.c_str());
                return false;
            }
            e.pendingPickups.erase(pending);
            e.dirty = false;
            e.lastWrite = now;
            UE_LOGI("player-state: checkpoint reason=in-flight-inventory-drop slot=%d "
                    "id=%.8s items=%zu", peerSlot, e.guid.c_str(), e.state.inventory.size());
            return true;
        }
    }
    if (matches != 1) {
        UE_LOGW("player-state: inventory->world drop refused slot=%d key='%ls' cls='%ls' matches=%zu",
                peerSlot, key.c_str(), className.c_str(), matches);
        return false;
    }
    std::vector<uint8_t> nextBlob = coop::inventory_wire::Serialize(next);
    if (!WriteBlobFile(PlayerFilePath(e.guid), nextBlob, e.nick,
                       /*mirrorNewToBackup=*/true)) {
        UE_LOGE("player-state: inventory->world drop checkpoint failed slot=%d key='%ls' -- no world spawn",
                peerSlot, key.c_str());
        return false;
    }
    e.state = std::move(next);
    e.blob = std::move(nextBlob);
    e.hash = coop::blob_chunks::Fnv64(e.blob);
    e.dirty = false;
    e.lastWrite = Clock::now();
    UE_LOGI("player-state: checkpoint reason=inventory-drop slot=%d id=%.8s items=%zu",
            peerSlot, e.guid.c_str(), e.state.inventory.size());
    return true;
}

}  // namespace coop::player_inventory_sync
