// coop/dev/transformer_probe.cpp -- see coop/dev/transformer_probe.h.

#include "coop/dev/transformer_probe.h"

#include "coop/config/config.h"
#include "coop/element/portable_identity.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/vm_dispatch.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace coop::dev::transformer_probe {
namespace {

namespace R  = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace VM = ue_wrap::vm_dispatch;

bool Enabled() {
    static const bool on = coop::config::ResolveFlag(
        ::coop::config_registry::rows::transformer_probe);
    return on;
}

struct BoolField { int32_t off = -1; uint8_t mask = 0; };
struct GeneratorFields {
    BoolField broken;
    int32_t cycle = -1;
    int32_t panel = -1;
    int32_t power = -1;
    int32_t trigger = -1;
} g_gen;
struct PanelFields {
    BoolField sineComplete, switchesComplete, rotatorsComplete;
    int32_t sineOffset = -1, sineFrequency = -1, sineAmplitude = -1;
    int32_t targetSineOffset = -1, targetSineFrequency = -1, targetSineAmplitude = -1;
    int32_t switchesTarget = -1, switchesStates = -1, rotatorsStates = -1;
    int32_t transformer = -1;
} g_panel;

void* g_genCls = nullptr;
void* g_panelCls = nullptr;
void* g_powerCls = nullptr;
bool  g_installed = false;
bool  g_installFailed = false;
bool  g_connected = false;
bool  g_isHost = false;

template <typename T>
T ReadAt(void* obj, int32_t off, T fallback = T{}) {
    if (!obj || off < 0) return fallback;
    T out{};
    std::memcpy(&out, reinterpret_cast<const uint8_t*>(obj) + off, sizeof(T));
    return out;
}

int ReadBool(void* obj, const BoolField& f) {
    if (!obj || f.off < 0 || f.mask == 0) return -1;
    return (ReadAt<uint8_t>(obj, f.off) & f.mask) ? 1 : 0;
}

struct RawArray { void* data; int32_t num; int32_t max; };

std::string FormatByteArray(void* obj, int32_t off, bool binary) {
    if (!obj || off < 0) return "?";
    const RawArray a = ReadAt<RawArray>(obj, off);
    if (a.num < 0 || a.max < a.num || a.num > 64 || (a.num && !a.data)) return "<invalid>";
    std::string out;
    out.reserve(static_cast<size_t>(a.num) * 3 + 16);
    out.push_back('[');
    const auto* bytes = static_cast<const uint8_t*>(a.data);
    for (int32_t i = 0; i < a.num; ++i) {
        if (i) out.push_back(',');
        const unsigned v = bytes[i];
        if (binary) out.push_back(v ? '1' : '0');
        else {
            if (v >= 10) out.push_back(static_cast<char>('0' + (v / 10) % 10));
            out.push_back(static_cast<char>('0' + v % 10));
        }
    }
    out += "]n=" + std::to_string(a.num);
    return out;
}

struct Snapshot {
    void* gen = nullptr;
    void* panel = nullptr;
    void* power = nullptr;
    void* trigger = nullptr;
    int broken = -1, cycle = -1;
    int sineComplete = -1, switchesComplete = -1, rotatorsComplete = -1;
    int sineOffset = 0, sineFrequency = 0, sineAmplitude = 0;
    int targetSineOffset = 0, targetSineFrequency = 0, targetSineAmplitude = 0;
    int switchesTarget = -1;
    std::string switches, rotators;
};

bool Capture(void* gen, Snapshot& s) {
    if (!gen || !R::IsLive(gen) || R::ClassOf(gen) != g_genCls) return false;
    s.gen = gen;
    s.broken = ReadBool(gen, g_gen.broken);
    s.cycle = ReadAt<int32_t>(gen, g_gen.cycle, -1);
    s.panel = ReadAt<void*>(gen, g_gen.panel);
    s.power = ReadAt<void*>(gen, g_gen.power);
    s.trigger = ReadAt<void*>(gen, g_gen.trigger);
    if (!s.panel || !R::IsLive(s.panel) || R::ClassOf(s.panel) != g_panelCls) {
        s.panel = nullptr;
        s.switches = s.rotators = "?";
        return true;
    }
    s.sineComplete = ReadBool(s.panel, g_panel.sineComplete);
    s.switchesComplete = ReadBool(s.panel, g_panel.switchesComplete);
    s.rotatorsComplete = ReadBool(s.panel, g_panel.rotatorsComplete);
    s.sineOffset = ReadAt<int32_t>(s.panel, g_panel.sineOffset);
    s.sineFrequency = ReadAt<int32_t>(s.panel, g_panel.sineFrequency);
    s.sineAmplitude = ReadAt<int32_t>(s.panel, g_panel.sineAmplitude);
    s.targetSineOffset = ReadAt<int32_t>(s.panel, g_panel.targetSineOffset);
    s.targetSineFrequency = ReadAt<int32_t>(s.panel, g_panel.targetSineFrequency);
    s.targetSineAmplitude = ReadAt<int32_t>(s.panel, g_panel.targetSineAmplitude);
    s.switchesTarget = static_cast<int>(ReadAt<uint8_t>(s.panel, g_panel.switchesTarget));
    s.switches = FormatByteArray(s.panel, g_panel.switchesStates, true);
    s.rotators = FormatByteArray(s.panel, g_panel.rotatorsStates, false);
    return true;
}

std::string Fingerprint(const Snapshot& s) {
    return std::to_string(s.broken) + ":" + std::to_string(s.cycle) + ":" +
        std::to_string(reinterpret_cast<uintptr_t>(s.panel)) + ":" +
        std::to_string(s.sineComplete) + std::to_string(s.switchesComplete) +
        std::to_string(s.rotatorsComplete) + ":" + std::to_string(s.sineOffset) + ":" +
        std::to_string(s.sineFrequency) + ":" + std::to_string(s.sineAmplitude) + ":" +
        std::to_string(s.targetSineOffset) + ":" + std::to_string(s.targetSineFrequency) + ":" +
        std::to_string(s.targetSineAmplitude) + ":" + std::to_string(s.switchesTarget) + ":" +
        s.switches + ":" + s.rotators + ":" +
        std::to_string(reinterpret_cast<uintptr_t>(s.trigger));
}

void Emit(const char* phase, const char* function, int action, void* gen) {
    Snapshot s;
    if (!Capture(gen, s)) return;
    const std::wstring key = coop::element::PortableWireKey(gen);
    const std::wstring ident = coop::element::PortableIdentity(gen);
    const bool triggerPopulated = s.trigger != nullptr;
    const bool triggerLive = s.trigger && R::IsLive(s.trigger);
    const std::wstring triggerName = triggerLive ? R::ToString(R::NameOf(s.trigger)) : L"-";
    const std::wstring triggerClass = triggerLive ? R::ClassNameOf(s.trigger) : L"-";
    UE_LOGI("[transformer_probe] SNAP role=%s phase=%s fn=%s action=%d key='%ls' ident='%ls' "
            "gen=%p broken=%d cycle=%d panel=%p complete=S%d/W%d/R%d "
            "sine=O:%d/%d,F:%d/%d,A:%d/%d switches=%s target=0x%02X rotators=%s "
            "power=%p trigger=%p trigger_populated=%d trigger_live=%d trigger_name='%ls' trigger_class='%ls'",
            g_isHost ? "HOST" : "CLIENT", phase, function, action, key.c_str(), ident.c_str(),
            s.gen, s.broken, s.cycle, s.panel,
            s.sineComplete, s.switchesComplete, s.rotatorsComplete,
            s.sineOffset, s.targetSineOffset, s.sineFrequency, s.targetSineFrequency,
            s.sineAmplitude, s.targetSineAmplitude, s.switches.c_str(),
            static_cast<unsigned>(s.switchesTarget & 0xff), s.rotators.c_str(), s.power,
            s.trigger, triggerPopulated ? 1 : 0, triggerLive ? 1 : 0,
            triggerName.c_str(), triggerClass.c_str());
}

enum VerbId {
    Damage = 1, Break, ActionOptionIndex, Update, Upd, FullFix,
    RandomizeTargets, RandomizeValues, Solar, SendPower
};
struct Watched {
    const wchar_t* clsName;
    const wchar_t* fnName;
    const char* label;
    VerbId id;
    void* cls = nullptr;
    void* fn = nullptr;
};
std::array<Watched, 10> g_watch{{
    {L"generator_C", L"damage",             "generator.damage", Damage},
    {L"generator_C", L"break",              "generator.break", Break},
    {L"generator_C", L"actionOptionIndex",  "generator.actionOptionIndex", ActionOptionIndex},
    {L"generator_C", L"update",             "generator.update", Update},
    {L"generator_C", L"upd",                "generator.upd", Upd},
    {L"generator_C", L"fullFix",            "generator.fullFix", FullFix},
    {L"transformerMGPanel_C", L"randomizeTargets", "panel.randomizeTargets", RandomizeTargets},
    {L"transformerMGPanel_C", L"randomizeValues",  "panel.randomizeValues", RandomizeValues},
    {L"powerControl_C", L"solar",            "power.solar", Solar},
    {L"powerControl_C", L"sendPower",        "power.sendPower", SendPower},
}};

struct GenRef { void* ptr; int32_t index; };
std::vector<GenRef> g_generators;
std::unordered_map<void*, std::string> g_last;
std::chrono::steady_clock::time_point g_nextScan{};
std::chrono::steady_clock::time_point g_nextRefresh{};

void RefreshGenerators() {
    std::vector<GenRef> fresh;
    for (void* p : R::FindObjectsByClass(L"generator_C")) {
        if (!p || !R::IsLive(p)) continue;
        const std::wstring name = R::ToString(R::NameOf(p));
        if (name.rfind(L"Default__", 0) == 0) continue;
        fresh.push_back({p, R::InternalIndexOf(p)});
    }
    g_generators = std::move(fresh);
}

void* GeneratorFor(void* self, VerbId id) {
    if (!self) return nullptr;
    if (id <= FullFix && R::IsLive(self) && R::ClassOf(self) == g_genCls) return self;
    if ((id == RandomizeTargets || id == RandomizeValues) && R::IsLive(self) &&
        R::ClassOf(self) == g_panelCls) {
        void* direct = ReadAt<void*>(self, g_panel.transformer);
        if (direct && R::IsLive(direct) && R::ClassOf(direct) == g_genCls) return direct;
    }
    for (const GenRef& r : g_generators) {
        if (!R::IsLiveByIndex(r.ptr, r.index)) continue;
        if ((id == RandomizeTargets || id == RandomizeValues) &&
            ReadAt<void*>(r.ptr, g_gen.panel) == self) return r.ptr;
        if ((id == Solar || id == SendPower) && ReadAt<void*>(r.ptr, g_gen.power) == self)
            return r.ptr;
    }
    return nullptr;
}

const Watched* FindWatch(void* fn) {
    for (const Watched& w : g_watch) if (w.fn == fn) return &w;
    return nullptr;
}

int ReadAction(void* function, void* params) {
    if (!function || !params) return -1;
    const int32_t frameSize = R::FunctionFrameSize(function);
    for (const auto& p : R::FunctionParams(function)) {
        if ((p.name != L"action" && p.name != L"Action") || p.offset < 0 || p.size <= 0 ||
            p.offset + p.size > frameSize) continue;
        int32_t value = 0;
        const int32_t n = p.size < static_cast<int32_t>(sizeof(value)) ? p.size : sizeof(value);
        std::memcpy(&value, static_cast<const uint8_t*>(params) + p.offset, static_cast<size_t>(n));
        return value;
    }
    return -1;
}

void OnPre(void* self, void* function, void* params) {
    if (!Enabled() || !g_connected || GT::IsDefinitelyOffGameThread()) return;
    const Watched* w = FindWatch(function);
    if (!w || !self || !R::IsLive(self) || R::ClassOf(self) != w->cls) return;
    void* gen = GeneratorFor(self, w->id);
    if (gen) Emit("PE-ENTER", w->label, ReadAction(function, params), gen);
}

void OnPost(void* self, void* function, void* params) {
    if (!Enabled() || !g_connected || GT::IsDefinitelyOffGameThread()) return;
    const Watched* w = FindWatch(function);
    if (!w || !self || !R::IsLive(self) || R::ClassOf(self) != w->cls) return;
    void* gen = GeneratorFor(self, w->id);
    if (gen) Emit("PE-EXIT", w->label, ReadAction(function, params), gen);
}

void OnVmEntry(const VM::Bracket& b) {
    if (!Enabled() || !g_connected || !b.ctx || !R::IsLive(b.ctx)) return;
    for (const Watched& w : g_watch) {
        if (w.id != b.verbId || R::ClassOf(b.ctx) != w.cls) continue;
        void* gen = GeneratorFor(b.ctx, w.id);
        if (gen) Emit("VM-ENTER", w.label, -1, gen);
        return;
    }
}

bool ResolveFields() {
    if (!R::FindBoolProperty(g_genCls, L"isBroken", g_gen.broken.off, g_gen.broken.mask))
        return false;
    g_gen.cycle = R::FindPropertyOffset(g_genCls, L"cycle");
    g_gen.panel = R::FindPropertyOffset(g_genCls, L"panelObj");
    g_gen.power = R::FindPropertyOffset(g_genCls, L"powerControl");
    g_gen.trigger = R::FindPropertyOffset(g_genCls, L"triggerWhenCompleted");
    if (!R::FindBoolProperty(g_panelCls, L"isSineComplete", g_panel.sineComplete.off,
                             g_panel.sineComplete.mask) ||
        !R::FindBoolProperty(g_panelCls, L"isSwitchesComplete", g_panel.switchesComplete.off,
                             g_panel.switchesComplete.mask) ||
        !R::FindBoolProperty(g_panelCls, L"isRotatorsComplete", g_panel.rotatorsComplete.off,
                             g_panel.rotatorsComplete.mask)) return false;
    g_panel.sineOffset = R::FindPropertyOffset(g_panelCls, L"sine_offset");
    g_panel.sineFrequency = R::FindPropertyOffset(g_panelCls, L"sine_frequency");
    g_panel.sineAmplitude = R::FindPropertyOffset(g_panelCls, L"sine_amplitude");
    g_panel.targetSineOffset = R::FindPropertyOffset(g_panelCls, L"targetSine_offset");
    g_panel.targetSineFrequency = R::FindPropertyOffset(g_panelCls, L"targetSine_frequency");
    g_panel.targetSineAmplitude = R::FindPropertyOffset(g_panelCls, L"targetSine_amplitude");
    g_panel.switchesTarget = R::FindPropertyOffset(g_panelCls, L"switches_target");
    g_panel.switchesStates = R::FindPropertyOffset(g_panelCls, L"switches_states");
    g_panel.rotatorsStates = R::FindPropertyOffset(g_panelCls, L"rotators_states");
    g_panel.transformer = R::FindPropertyOffset(g_panelCls, L"transformer");
    return g_gen.cycle >= 0 && g_gen.panel >= 0 && g_gen.power >= 0 && g_gen.trigger >= 0 &&
        g_panel.sineOffset >= 0 && g_panel.sineFrequency >= 0 && g_panel.sineAmplitude >= 0 &&
        g_panel.targetSineOffset >= 0 && g_panel.targetSineFrequency >= 0 &&
        g_panel.targetSineAmplitude >= 0 && g_panel.switchesTarget >= 0 &&
        g_panel.switchesStates >= 0 && g_panel.rotatorsStates >= 0 &&
        g_panel.transformer >= 0;
}

void Install() {
    if (g_installed || g_installFailed || !Enabled() || !g_connected) return;
    g_genCls = R::FindClass(L"generator_C");
    g_panelCls = R::FindClass(L"transformerMGPanel_C");
    g_powerCls = R::FindClass(L"powerControl_C");
    if (!g_genCls || !g_panelCls || !g_powerCls) return;
    if (!ResolveFields()) {
        UE_LOGE("[transformer_probe] required reflected field missing; probe not installed");
        g_installFailed = true;
        return;
    }

    int pePairs = 0, vmEntries = 0;
    for (Watched& w : g_watch) {
        w.cls = R::FindClass(w.clsName);
        w.fn = w.cls ? R::FindFunction(w.cls, w.fnName) : nullptr;
        if (!w.fn) {
            UE_LOGW("[transformer_probe] function missing: %ls::%ls", w.clsName, w.fnName);
            continue;
        }
        const bool pre = GT::RegisterPreObserver(w.fn, &OnPre);
        const bool post = GT::RegisterPostObserver(w.fn, &OnPost);
        if (pre && post) ++pePairs;
        else {
            GT::UnregisterObservers(w.fn, &OnPre);
            GT::UnregisterObservers(w.fn, &OnPost);
            UE_LOGW("[transformer_probe] PE observer pair failed for %s", w.label);
        }
        if (VM::RegisterVirtualVerb(w.fnName, static_cast<int>(w.id), &OnVmEntry)) ++vmEntries;
    }
    g_installed = true;
    RefreshGenerators();
    UE_LOGI("[transformer_probe] INSTALLED read-only targeted probe role=%s PE-pairs=%d/10 "
            "VM-entries=%d/10 generators=%zu; phases: PE-ENTER/PE-EXIT, VM-ENTER, EDGE",
            g_isHost ? "HOST" : "CLIENT", pePairs, vmEntries, g_generators.size());
}

void PollEdges() {
    const auto now = std::chrono::steady_clock::now();
    if (now < g_nextScan) return;
    g_nextScan = now + std::chrono::milliseconds(100);
    // GUObjectArray enumeration is deliberately low-rate.  The 100 ms edge pass
    // walks only this small cached set and validates each cached slot without
    // dereferencing a possibly-purged pointer.
    if (now >= g_nextRefresh) {
        RefreshGenerators();
        g_nextRefresh = now + std::chrono::seconds(2);
    }
    std::unordered_map<void*, std::string> next;
    for (const GenRef& r : g_generators) {
        Snapshot s;
        if (!Capture(r.ptr, s)) continue;
        std::string fp = Fingerprint(s);
        const auto old = g_last.find(r.ptr);
        if (old == g_last.end()) Emit("BASELINE", "state", -1, r.ptr);
        else if (old->second != fp) Emit("EDGE", "state-change", -1, r.ptr);
        next.emplace(r.ptr, std::move(fp));
    }
    g_last = std::move(next);
}

}  // namespace

void Tick(bool connected, bool isHost) {
    if (!Enabled()) return;
    g_connected = connected;
    g_isHost = isHost;
    if (!connected) {
        g_generators.clear();
        g_last.clear();
        g_nextScan = {};
        g_nextRefresh = {};
        return;
    }
    Install();
    if (!g_installed) return;
    VM::TickResolvePending();
    PollEdges();
}

}  // namespace coop::dev::transformer_probe
