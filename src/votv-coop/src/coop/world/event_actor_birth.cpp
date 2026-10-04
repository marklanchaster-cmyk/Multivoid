#include "coop/world/event_actor_birth.h"

#include "coop/net/protocol.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace coop::event_actor_birth {
namespace {
namespace R = ue_wrap::reflection;

constexpr wchar_t kNb5Leaf[] = L"NewBlueprint5_C";
constexpr wchar_t kNb5Package[] = L"/Game/objects/NewBlueprint5";
constexpr wchar_t kNb5Key[] = L"objects/NewBlueprint5_C";
constexpr wchar_t kCorpseLeaf[] = L"screamingCorpse_C";
constexpr wchar_t kCorpsePackage[] = L"/Game/objects/misc/screamingCorpse";
constexpr wchar_t kCorpseKey[] = L"objects/misc/screamingCorpse_C";
constexpr wchar_t kCorpseToken[] = L"xLPJ2BPoJq5oULJJmJw%5AC^vb$eE@%A";

enum : uint8_t { kVersion = 1, kNb5 = 1, kCorpse = 2 };
struct BirthV1 { uint8_t version, kind, flags, token; float remaining; };
static_assert(sizeof(BirthV1) == 8);

bool PackageIs(void* cls, const wchar_t* package) {
    void* outer = cls ? R::OuterOf(cls) : nullptr;
    return outer && R::NameEquals(R::NameOf(outer), package);
}

bool IsExact(void* cls, const wchar_t* leaf, const wchar_t* package) {
    return cls && R::NameEquals(R::NameOf(cls), leaf) && PackageIs(cls, package);
}

void* FindExact(const wchar_t* leaf, const wchar_t* package) {
    const int32_t n = R::NumObjects();
    for (int32_t i = 0; i < n; ++i) {
        void* obj = R::ObjectAt(i);
        if (obj && R::NameEquals(R::NameOf(obj), leaf) && PackageIs(obj, package))
            return obj;
    }
    return nullptr;
}

bool ReadBool(void* actor, const wchar_t* name, bool& out) {
    int32_t off = -1; uint8_t mask = 0;
    if (!actor || !R::FindBoolProperty(R::ClassOf(actor), name, off, mask) || off < 0 || !mask)
        return false;
    out = (reinterpret_cast<const uint8_t*>(actor)[off] & mask) != 0;
    return true;
}

bool WriteBool(void* actor, const wchar_t* name, bool value) {
    int32_t off = -1; uint8_t mask = 0;
    if (!actor || !R::FindBoolProperty(R::ClassOf(actor), name, off, mask) || off < 0 || !mask)
        return false;
    uint8_t& b = reinterpret_cast<uint8_t*>(actor)[off];
    b = value ? static_cast<uint8_t>(b | mask) : static_cast<uint8_t>(b & ~mask);
    return true;
}

bool ReadFloat(void* actor, const wchar_t* name, float& out) {
    const int32_t off = actor ? R::FindPropertyOffset(R::ClassOf(actor), name) : -1;
    if (off < 0) return false;
    out = *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(actor) + off);
    return std::isfinite(out);
}

bool WriteFloat(void* actor, const wchar_t* name, float value) {
    const int32_t off = actor ? R::FindPropertyOffset(R::ClassOf(actor), name) : -1;
    if (off < 0 || !std::isfinite(value)) return false;
    *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(actor) + off) = value;
    return true;
}

bool ReadLifeSpan(void* actor, float& out) {
    static void* fn = nullptr;
    if (!fn) if (void* actorCls = R::FindClass(L"Actor")) fn = R::FindFunction(actorCls, L"GetLifeSpan");
    if (!actor || !fn) return false;
    ue_wrap::ParamFrame f(fn);
    if (!f.valid() || !ue_wrap::Call(actor, f)) return false;
    out = f.Get<float>(L"ReturnValue");
    return std::isfinite(out) && out > 0.f && out <= 5.5f;
}

bool WriteLifeSpan(void* actor, float value) {
    static void* fn = nullptr;
    if (!fn) if (void* actorCls = R::FindClass(L"Actor")) fn = R::FindFunction(actorCls, L"SetLifeSpan");
    if (!actor || !fn || !std::isfinite(value) || value <= 0.f || value > 5.5f) return false;
    ue_wrap::ParamFrame f(fn);
    return f.valid() && f.Set<float>(L"InLifespan", value) && ue_wrap::Call(actor, f);
}

bool Decode(const coop::net::WorldActorSpawnPayload& p, uint8_t kind, BirthV1& out) {
    if (p.birthLen != sizeof(out)) return false;
    std::memcpy(&out, p.birth, sizeof(out));
    return out.version == kVersion && out.kind == kind && std::isfinite(out.remaining);
}
}  // namespace

bool RequiresBirth(void* cls) {
    return IsExact(cls, kNb5Leaf, kNb5Package) || IsExact(cls, kCorpseLeaf, kCorpsePackage);
}

std::wstring WireClassKey(void* cls) {
    if (IsExact(cls, kNb5Leaf, kNb5Package)) return kNb5Key;
    if (IsExact(cls, kCorpseLeaf, kCorpsePackage)) return kCorpseKey;
    return {};
}

bool IsWireClassKey(const std::wstring& key) { return key == kNb5Key || key == kCorpseKey; }

void* ResolveWireClass(const std::wstring& key) {
    if (key == kNb5Key) return FindExact(kNb5Leaf, kNb5Package);
    if (key == kCorpseKey) return FindExact(kCorpseLeaf, kCorpsePackage);
    return nullptr;
}

bool Capture(void* actor, coop::net::WorldActorSpawnPayload& p) {
    void* cls = actor ? R::ClassOf(actor) : nullptr;
    BirthV1 b{kVersion, 0, 0, 0, 0.f};
    if (IsExact(cls, kNb5Leaf, kNb5Package)) {
        bool enabled = false;
        if (!ReadBool(actor, L"NewVar_0", enabled) || !ReadFloat(actor, L"ime", b.remaining) ||
            b.remaining < -1.f || b.remaining > 3600.f) return false;
        b.kind = kNb5; b.flags = enabled ? 1 : 0;
    } else if (IsExact(cls, kCorpseLeaf, kCorpsePackage)) {
        const int32_t stringOff = R::FindPropertyOffset(cls, L"NewVar_0");
        if (stringOff < 0 || ue_wrap::field_io::ReadFStringAt(actor, stringOff) != kCorpseToken ||
            !ReadLifeSpan(actor, b.remaining)) return false;
        b.kind = kCorpse; b.token = 1;
    } else {
        return false;
    }
    std::memcpy(p.birth, &b, sizeof(b));
    p.birthLen = sizeof(b);
    return true;
}

bool ApplyBeforeFinish(void* actor, const std::wstring& key,
                       const coop::net::WorldActorSpawnPayload& p) {
    BirthV1 b{};
    if (key == kNb5Key) {
        if (!Decode(p, kNb5, b) || b.flags > 1 || b.remaining < -1.f || b.remaining > 3600.f)
            return false;
        return WriteBool(actor, L"NewVar_0", (b.flags & 1) != 0) &&
               WriteFloat(actor, L"ime", b.remaining);
    }
    if (key == kCorpseKey) {
        if (!Decode(p, kCorpse, b) || b.token != 1 || b.remaining <= 0.f || b.remaining > 5.5f)
            return false;
        const int32_t off = R::FindPropertyOffset(R::ClassOf(actor), L"NewVar_0");
        return off >= 0 && ue_wrap::field_io::WriteFStringField(actor, off, kCorpseToken);
    }
    return true;
}

bool ApplyAfterFinish(void* actor, const std::wstring& key,
                      const coop::net::WorldActorSpawnPayload& p) {
    if (key != kCorpseKey) return true;
    BirthV1 b{};
    return Decode(p, kCorpse, b) && b.token == 1 && WriteLifeSpan(actor, b.remaining);
}

bool UsesLocalPresentationTick(const std::string& key) {
    return key == "objects/NewBlueprint5_C" || key == "objects/misc/screamingCorpse_C";
}

}  // namespace coop::event_actor_birth
