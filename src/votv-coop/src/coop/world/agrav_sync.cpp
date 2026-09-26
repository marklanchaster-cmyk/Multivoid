// Host-result synchronization for trigger_agrav_C. We never replay runTrigger on a client: it
// gathers and shuffles targets, breaks a random generator, and can award reputation. Instead the
// host captures the concrete PrimitiveComponent gravity/velocity outputs authored by the active
// controller and sends exact prop identities plus state. EventAuthority owns BEGIN/END; this lane
// owns the long-lived floating-object result and the safe craft/audio/light presentation subset.

#include "coop/world/agrav_sync.h"

#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/props/prop_element_tracker.h"
#include "coop/props/remote_prop.h"
#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/component_calls.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/ufunction_hook.h"
#include "ue_wrap/engine/engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <unordered_map>
#include <unordered_set>

namespace coop::agrav_sync { namespace {
namespace R = ue_wrap::reflection;
namespace CC = ue_wrap::component_calls;

std::atomic<coop::net::Session*> g_session{nullptr};
void* g_gravityFn = nullptr;
void* g_angularFn = nullptr;
void* g_getOwnerFn = nullptr;
void* g_isGravityFn = nullptr;
bool g_hooksInstalled = false;

struct HostInstance { void* controller=nullptr; int32_t index=-1; uint32_t sequence=0; };
struct TargetState { coop::net::AgravStatePayload payload{}; };
std::unordered_map<uint64_t, HostInstance> g_hostInstances;
std::unordered_map<void*, uint64_t> g_instanceByController;
std::unordered_map<uint64_t, std::unordered_map<std::string, TargetState>> g_hostTargets;
std::unordered_set<uint64_t> g_clientInstances;
std::unordered_map<uint64_t, std::unordered_set<std::string>> g_clientTargets;
std::unordered_map<uint64_t, std::unordered_map<std::string, coop::net::AgravStatePayload>> g_pending;
std::unordered_map<uint64_t, std::unordered_map<std::string, uint32_t>> g_clientSequence;
std::chrono::steady_clock::time_point g_nextPendingRetry{};

std::string WireString(const coop::net::WireKey& k) {
    const size_t n=std::min<size_t>(31,k.len); return std::string(k.data,k.data+n);
}
void PutWire(coop::net::WireKey& k, const std::wstring& w) {
    std::memset(&k,0,sizeof(k));
    const size_t n=std::min<size_t>(31,w.size()); k.len=static_cast<uint8_t>(n);
    for(size_t i=0;i<n;++i) k.data[i]=w[i]>=0&&w[i]<128?static_cast<char>(w[i]):'?';
}
std::wstring Widen(const coop::net::WireKey& k) {
    std::wstring w; const size_t n=std::min<size_t>(31,k.len); w.reserve(n);
    for(size_t i=0;i<n;++i) w.push_back(static_cast<unsigned char>(k.data[i]));
    return w;
}
bool Finite(const coop::net::AgravStatePayload& p) {
    return std::isfinite(p.linX)&&std::isfinite(p.linY)&&std::isfinite(p.linZ)&&
           std::isfinite(p.angX)&&std::isfinite(p.angY)&&std::isfinite(p.angZ) &&
           std::fabs(p.linX)<=100000.f&&std::fabs(p.linY)<=100000.f&&std::fabs(p.linZ)<=100000.f&&
           std::fabs(p.angX)<=100000.f&&std::fabs(p.angY)<=100000.f&&std::fabs(p.angZ)<=100000.f;
}
void* OwnerOf(void* component) {
    if(!component||!R::IsLive(component))return nullptr;
    if(!g_getOwnerFn)g_getOwnerFn=R::FindFunction(R::ClassOf(component),L"GetOwner");
    if(!g_getOwnerFn)return nullptr;
    ue_wrap::ParamFrame f(g_getOwnerFn);
    return f.valid()&&ue_wrap::Call(component,f)?f.Get<void*>(L"ReturnValue"):nullptr;
}
bool GravityEnabled(void* component, bool* out) {
    if(!component||!out)return false;
    if(!g_isGravityFn)g_isGravityFn=R::FindFunction(R::ClassOf(component),L"IsGravityEnabled");
    if(!g_isGravityFn)return false;
    ue_wrap::ParamFrame f(g_isGravityFn);
    if(!f.valid()||!ue_wrap::Call(component,f))return false;
    *out=f.Get<bool>(L"ReturnValue");return true;
}
void SetGravity(void* component,bool enabled) {
    if(!component)return;
    void* fn=R::FindFunction(R::ClassOf(component),L"SetEnableGravity");
    if(!fn)return;
    ue_wrap::ParamFrame f(fn); if(f.valid()){f.Set<bool>(L"bGravityEnabled",enabled);ue_wrap::Call(component,f);}
}
void AttachLightToLocalCamera(void* light) {
    if(!light||!R::IsLive(light))return;
    void* camera=R::FindObjectByClass(L"PlayerCameraManager");if(!camera)return;
    void* getRoot=R::FindFunction(R::ClassOf(camera),L"K2_GetRootComponent");if(!getRoot)return;
    ue_wrap::ParamFrame rootFrame(getRoot);if(!rootFrame.valid()||!ue_wrap::Call(camera,rootFrame))return;
    void* root=rootFrame.Get<void*>(L"ReturnValue");if(!root)return;
    void* attach=R::FindFunction(R::ClassOf(light),L"K2_AttachToComponent");if(!attach)return;
    ue_wrap::ParamFrame f(attach);if(!f.valid())return;
    f.Set<void*>(L"Parent",root);f.Set<uint8_t>(L"LocationRule",0);f.Set<uint8_t>(L"RotationRule",0);
    f.Set<uint8_t>(L"ScaleRule",0);f.Set<bool>(L"bWeldSimulatedBodies",true);ue_wrap::Call(light,f);
}
void* ResolveTarget(const coop::net::AgravStatePayload& p) {
    const std::wstring key=Widen(p.key);
    if(!key.empty())if(void* a=coop::prop_element_tracker::ResolveLiveActorByKey(key))return a;
    if(p.elementId&&p.elementId!=coop::element::kInvalidId)
        if(auto* e=coop::element::Registry::Get().Get(p.elementId))return e->GetActor();
    return nullptr;
}
void* PresentationController() {
    // trigger_eventer_C owns an authored `event_agrav` object reference and calls that exact
    // object's runTrigger. The scheduler gate suppresses the call, not construction of the placed
    // eventer or its referenced controller. Resolve through that reference instead of touching
    // every trigger_agrav_C in the object array.
    void* found=nullptr;
    for(void* eventer:R::FindObjectsByClass(L"trigger_eventer_C")){
        if(!eventer||!R::IsLive(eventer)||R::NameStartsWith(R::NameOf(eventer),L"Default__"))continue;
        void* cls=R::ClassOf(eventer);
        const int32_t off=R::FindPropertyOffset(cls,L"event_agrav");
        if(off<0)continue;
        void* candidate=*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(eventer)+off);
        if(!candidate||!R::IsLive(candidate)||R::ClassNameOf(candidate)!=L"trigger_agrav_C")continue;
        if(found&&found!=candidate){
            static bool warned=false;
            if(!warned){warned=true;UE_LOGW("agrav_sync: multiple distinct trigger_eventer.event_agrav "
                                            "references -- presentation declined (not instance-addressable)");}
            return nullptr;
        }
        found=candidate;
    }
    return found;
}
void ApplyPresentation(bool on) {
    void* c=PresentationController();
    if(!c){UE_LOGW("agrav_sync: authored trigger_eventer.event_agrav controller unavailable -- "
                   "presentation not applied");return;}
    // ReceiveBeginPlay creates the dynamic materials on the authored StaticMesh. runTrigger does
    // not create the craft, hover cue, PointLight, or PointLight1; it only drives/attaches them.
    if(void* fn=R::FindFunction(R::ClassOf(c),L"cloak")){ue_wrap::ParamFrame f(fn);if(f.valid()){f.Set<bool>(L"forward",on);ue_wrap::Call(c,f);}}
    void* cls=R::ClassOf(c);
    const int32_t audioOff=R::FindPropertyOffset(cls,L"arirHover_Cue");
    const int32_t lightOff=R::FindPropertyOffset(cls,L"PointLight");
    if(audioOff>=0)CC::SetActive(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(c)+audioOff),on);
    if(lightOff>=0){void* light=*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(c)+lightOff);if(on)AttachLightToLocalCamera(light);CC::SetVisibility(light,on);}
}
void ReconcileCloakAge(uint16_t elapsedSec) {
    if(!elapsedSec)return;
    void* c=PresentationController();if(!c)return;
    const int32_t off=R::FindPropertyOffset(R::ClassOf(c),L"b");if(off<0)return;
    void* timeline=*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(c)+off);if(!timeline)return;
    void* fn=R::FindFunction(R::ClassOf(timeline),L"SetNewTime");if(!fn)return;
    ue_wrap::ParamFrame f(fn);if(f.valid()){f.Set<float>(L"NewTime",static_cast<float>(elapsedSec));ue_wrap::Call(timeline,f);}
}
void Send(const coop::net::AgravStatePayload& p,int slot=-1) {
    auto*s=g_session.load(std::memory_order_acquire);if(!s||!s->connected()||s->role()!=coop::net::Role::Host)return;
    if(slot>=0)s->SendReliableToSlot(slot,coop::net::ReliableKind::AgravState,&p,sizeof(p));
    else s->SendReliable(coop::net::ReliableKind::AgravState,&p,sizeof(p));
}
void Capture(void* component,void* source) {
    auto*s=g_session.load(std::memory_order_acquire);if(!s||!s->connected()||s->role()!=coop::net::Role::Host)return;
    auto itId=g_instanceByController.find(source);if(itId==g_instanceByController.end())return;
    auto itInst=g_hostInstances.find(itId->second);if(itInst==g_hostInstances.end())return;
    if(!R::IsLiveByIndex(source,itInst->second.index))return;
    void* actor=OwnerOf(component);if(!actor||!R::IsLive(actor))return;
    std::wstring key=ue_wrap::prop::GetInteractableKeyString(actor);
    if(key.empty()||key==L"None")return; // agrav gathers prop_C; never ship an unnameable target.
    bool gravity=true;if(!GravityEnabled(component,&gravity))return;
    ue_wrap::FVector lin{},ang{};ue_wrap::engine::GetActorRootPhysicsVelocity(actor,lin,ang);
    coop::net::AgravStatePayload p{};p.instanceId=itId->second;p.sequence=++itInst->second.sequence;
    p.elementId=static_cast<uint32_t>(coop::element::Registry::Get().EidForActor(actor));PutWire(p.key,key);
    p.linX=lin.X;p.linY=lin.Y;p.linZ=lin.Z;p.angX=ang.X;p.angY=ang.Y;p.angZ=ang.Z;p.active=gravity?0:1;
    if(!Finite(p))return;
    const std::string sk=WireString(p.key);if(p.active)g_hostTargets[p.instanceId][sk]=TargetState{p};
    else g_hostTargets[p.instanceId].erase(sk);
    Send(p);
    UE_LOGI("agrav_sync: HOST state instance=%llu seq=%u key=%s active=%u eid=%u",
            static_cast<unsigned long long>(p.instanceId),p.sequence,sk.c_str(),p.active,p.elementId);
}
void OnPhysicsPost(void* component,void* source,void*){Capture(component,source);}
void ResolveHooks() {
    if(g_hooksInstalled)return;
    void* pc=R::FindClass(L"PrimitiveComponent");if(!pc)return;
    if(!g_gravityFn)g_gravityFn=R::FindFunction(pc,L"SetEnableGravity");
    if(!g_angularFn)g_angularFn=R::FindFunction(pc,L"SetPhysicsAngularVelocityInDegrees");
    if(!g_gravityFn||!g_angularFn)return;
    const bool a=ue_wrap::ufunction_hook::InstallPostHook(g_gravityFn,&OnPhysicsPost);
    const bool b=ue_wrap::ufunction_hook::InstallPostHook(g_angularFn,&OnPhysicsPost);
    g_hooksInstalled=a&&b;if(g_hooksInstalled)UE_LOGI("agrav_sync: host result capture hooks installed");
}
bool ApplyClientState(const coop::net::AgravStatePayload&p) {
    void*a=ResolveTarget(p);if(!a||!R::IsLive(a))return false;
    void*mesh=ue_wrap::prop::GetStaticMesh(a);if(!mesh)return false;SetGravity(mesh,p.active==0);
    coop::remote_prop::DriveSetLinearVelocity(mesh,p.linX,p.linY,p.linZ);
    coop::remote_prop::DriveSetAngularVelocity(mesh,p.angX,p.angY,p.angZ);
    std::string key=WireString(p.key);if(p.active)g_clientTargets[p.instanceId].insert(key);else g_clientTargets[p.instanceId].erase(key);
    UE_LOGI("agrav_sync: CLIENT applied instance=%llu seq=%u key=%s active=%u",static_cast<unsigned long long>(p.instanceId),p.sequence,key.c_str(),p.active);
    return true;
}
} // namespace

void Install(coop::net::Session*s){g_session.store(s,std::memory_order_release);}
void Tick(){ResolveHooks();if(g_pending.empty()||std::chrono::steady_clock::now()<g_nextPendingRetry)return;g_nextPendingRetry=std::chrono::steady_clock::now()+std::chrono::milliseconds(250);for(auto i=g_pending.begin();i!=g_pending.end();){if(!g_clientInstances.count(i->first)){i=g_pending.erase(i);continue;}for(auto p=i->second.begin();p!=i->second.end();){const auto sit=g_clientSequence[i->first].find(p->first);if(sit==g_clientSequence[i->first].end()||sit->second!=p->second.sequence){p=i->second.erase(p);continue;}p=ApplyClientState(p->second)?i->second.erase(p):std::next(p);}if(i->second.empty())i=g_pending.erase(i);else ++i;}}
void HostBegin(uint64_t id,void* c){if(!id||!c)return;ResolveHooks();g_hostInstances[id]={c,R::InternalIndexOf(c),0};g_instanceByController[c]=id;g_hostTargets[id];UE_LOGI("agrav_sync: HOST BEGIN instance=%llu controller=%p",static_cast<unsigned long long>(id),c);}
void HostEnd(uint64_t id,void* c){g_instanceByController.erase(c);g_hostInstances.erase(id);g_hostTargets.erase(id);UE_LOGI("agrav_sync: HOST END instance=%llu",static_cast<unsigned long long>(id));}
void ClientBegin(uint64_t id,uint16_t elapsed,bool snap){if(!id)return;g_clientInstances.insert(id);ApplyPresentation(true);if(snap)ReconcileCloakAge(elapsed);UE_LOGI("agrav_sync: CLIENT %s BEGIN instance=%llu elapsed=%us presentation reconciled",snap?"SNAPSHOT":"authoritative",static_cast<unsigned long long>(id),elapsed);}
void ClientEnd(uint64_t id){
    auto it=g_clientTargets.find(id);if(it!=g_clientTargets.end()){for(const auto&k:it->second){coop::net::AgravStatePayload p{};p.key.len=static_cast<uint8_t>(std::min<size_t>(31,k.size()));std::memcpy(p.key.data,k.data(),p.key.len);if(void*a=ResolveTarget(p))SetGravity(ue_wrap::prop::GetStaticMesh(a),true);}g_clientTargets.erase(it);}
    g_pending.erase(id);g_clientSequence.erase(id);g_clientInstances.erase(id);if(g_clientInstances.empty())ApplyPresentation(false);UE_LOGI("agrav_sync: CLIENT authoritative END instance=%llu cleanup",static_cast<unsigned long long>(id));
}
void SendJoinSnapshotForSlot(int slot){size_t n=0;for(auto&[id,targets]:g_hostTargets){auto hi=g_hostInstances.find(id);if(hi==g_hostInstances.end())continue;for(auto&[key,state]:targets){(void)key;void* actor=ResolveTarget(state.payload);if(!actor||!R::IsLive(actor))continue;ue_wrap::FVector lin{},ang{};ue_wrap::engine::GetActorRootPhysicsVelocity(actor,lin,ang);auto p=state.payload;p.sequence=++hi->second.sequence;p.elementId=static_cast<uint32_t>(coop::element::Registry::Get().EidForActor(actor));p.linX=lin.X;p.linY=lin.Y;p.linZ=lin.Z;p.angX=ang.X;p.angY=ang.Y;p.angZ=ang.Z;p.active=1;if(!Finite(p))continue;state.payload=p;Send(p,slot);++n;}}UE_LOGI("agrav_sync: HOST join snapshot slot=%d activeTargets=%zu (current velocities; transform remains PropPose-owned)",slot,n);}
void OnReliable(const coop::net::AgravStatePayload&p){
    auto*s=g_session.load(std::memory_order_acquire);if(!s||s->role()==coop::net::Role::Host||!p.instanceId||!p.sequence||p.active>1||!Finite(p))return;
    if(!g_clientInstances.count(p.instanceId)){UE_LOGI("agrav_sync: CLIENT ignored state for inactive instance=%llu seq=%u",static_cast<unsigned long long>(p.instanceId),p.sequence);return;}
    const std::string key=WireString(p.key);uint32_t& known=g_clientSequence[p.instanceId][key];if(p.sequence<=known){UE_LOGI("agrav_sync: CLIENT ignored stale state instance=%llu seq=%u known=%u key=%s",static_cast<unsigned long long>(p.instanceId),p.sequence,known,key.c_str());return;}known=p.sequence;
    if(ApplyClientState(p)){auto ii=g_pending.find(p.instanceId);if(ii!=g_pending.end()){auto pi=ii->second.find(key);if(pi!=ii->second.end()&&pi->second.sequence<=p.sequence)ii->second.erase(pi);if(ii->second.empty())g_pending.erase(ii);}}
    else{g_pending[p.instanceId][key]=p;UE_LOGI("agrav_sync: CLIENT parked unresolved state instance=%llu seq=%u key=%s eid=%u",static_cast<unsigned long long>(p.instanceId),p.sequence,key.c_str(),p.elementId);}
}
void OnDisconnect(){while(!g_clientInstances.empty())ClientEnd(*g_clientInstances.begin());g_pending.clear();g_clientSequence.clear();g_clientTargets.clear();g_hostTargets.clear();g_hostInstances.clear();g_instanceByController.clear();g_session.store(nullptr,std::memory_order_release);}
} // namespace coop::agrav_sync
