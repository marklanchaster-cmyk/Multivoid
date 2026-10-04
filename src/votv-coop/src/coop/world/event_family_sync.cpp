#include "coop/world/event_family_sync.h"

#include "coop/element/object_scan_hub.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/world/event_active_sync.h"
#include "coop/world/event_output_sync.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/engine/engine.h"
#include "ue_wrap/engine/world_identity.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace coop::event_family_sync { namespace {
namespace R=ue_wrap::reflection; namespace SG=ue_wrap::script_gate;

struct Ref { void* actor=nullptr; int32_t idx=-1; };
struct BadSunState { uint8_t version=1,flags=0,reserved[2]{}; float dryness=0,intensity=1,color[4]{1,1,1,1}; };
static_assert(sizeof(BadSunState)==28);
struct Family { void* controller=nullptr; int32_t idx=-1; std::string cls; uint64_t output=0; long long nextUpdateMs=0; };
struct ClientFamily { std::string cls; uint8_t state[32]{}; uint8_t len=0; bool applied=false; uint32_t worldGen=0; };

std::atomic<coop::net::Session*> g_session{nullptr};
void *g_gmCls=nullptr,*g_lakeCls=nullptr,*g_dncCls=nullptr,*g_skyCls=nullptr;
void *g_lakeSetActive=nullptr,*g_setDryness=nullptr,*g_gsCdo=nullptr,*g_playSound=nullptr,*g_playShake=nullptr;
int32_t g_lakeActive=-1,g_lakeAlways=-1,g_setActiveParam=-1,g_gmDnc=-1,g_gmDry=-1,g_dncSky=-1,g_skyIntensity=-1,g_skyColor=-1;
std::vector<Ref> g_scan,g_index; uint32_t g_indexGen=0; bool g_registered=false,g_watch=false;
std::unordered_map<uint64_t,Family> g_families;
std::unordered_map<uint64_t,ClientFamily> g_clientFamilies;
bool g_applyingLake=false;
long long NowMs(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}

template<class T>T Read(void*o,int32_t off,T fallback={}){return o&&off>=0?*reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(o)+off):fallback;}
template<class T>void Write(void*o,int32_t off,const T&v){if(o&&off>=0)*reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(o)+off)=v;}

bool Resolve(){
 if(!g_gmCls)g_gmCls=R::FindClass(L"mainGamemode_C");if(!g_lakeCls)g_lakeCls=R::FindClass(L"lakeglow_C");if(!g_dncCls)g_dncCls=R::FindClass(L"daynightCycle_C");
 if(!g_gmCls||!g_lakeCls||!g_dncCls)return false;
 if(g_lakeActive<0)g_lakeActive=R::FindPropertyOffset(g_lakeCls,L"isActive");if(g_lakeAlways<0)g_lakeAlways=R::FindPropertyOffset(g_lakeCls,L"alpwaysActive");
 if(g_gmDnc<0)g_gmDnc=R::FindPropertyOffset(g_gmCls,L"daynightCycle");if(g_gmDry<0)g_gmDry=R::FindPropertyOffset(g_gmCls,L"dryAlpha");if(g_dncSky<0)g_dncSky=R::FindPropertyOffset(g_dncCls,L"skysphere");
 if(!g_lakeSetActive)g_lakeSetActive=R::FindFunction(g_lakeCls,L"setActive");if(g_lakeSetActive&&g_setActiveParam<0)g_setActiveParam=R::FindParamOffset(g_lakeSetActive,L"activate");
 if(!g_setDryness)g_setDryness=R::FindFunction(g_gmCls,L"setDryness");
 return g_lakeActive>=0&&g_lakeAlways>=0&&g_gmDnc>=0&&g_gmDry>=0&&g_dncSky>=0&&g_lakeSetActive&&g_setActiveParam>=0;
}
bool IsInstance(void*o){void*c=R::ClassOf(o);return c==g_gmCls||c==g_lakeCls||c==g_dncCls;}
void PassBegin(void*,bool full){if(full)g_scan.clear();}
void Match(void*,void*o){if(!R::NameStartsWith(R::NameOf(o),L"Default__"))g_scan.push_back({o,R::InternalIndexOf(o)});}
size_t PassComplete(void*,bool full,uint32_t gen){if(full)g_index=g_scan;else for(auto&r:g_scan)g_index.push_back(r);g_index.erase(std::remove_if(g_index.begin(),g_index.end(),[](const Ref&r){return !R::IsLiveByIndex(r.actor,r.idx);}),g_index.end());g_scan.clear();g_indexGen=gen;return g_index.size();}
void RegisterHub(){if(g_registered)return;g_registered=true;coop::element::scan_hub::Register({"event_family_state",nullptr,&Resolve,&IsInstance,&PassBegin,&Match,&PassComplete,15});}
void* Indexed(void*cls){if(g_indexGen!=ue_wrap::world_identity::Generation())return nullptr;for(const Ref&r:g_index)if(R::IsLiveByIndex(r.actor,r.idx)&&R::ClassOf(r.actor)==cls)return r.actor;return nullptr;}

bool ApplyLake(bool active){void*lake=Indexed(g_lakeCls);if(!lake)return false;ue_wrap::ParamFrame p(g_lakeSetActive);if(!p.valid())return false;p.Set<bool>(L"activate",active);g_applyingLake=true;const bool ok=ue_wrap::Call(lake,p);g_applyingLake=false;return ok;}
SG::Verdict LakePre(const SG::Call&c){auto*s=g_session.load();if(c.fromOurCode||g_applyingLake||!s||!s->connected()||c.object==nullptr)return SG::Verdict::Run;if(s->role()==coop::net::Role::Client)return SG::Verdict::Cancel;return SG::Verdict::Run;}
void LakePost(const SG::Call&c){auto*s=g_session.load();if(c.fromOurCode||g_applyingLake||!s||s->role()!=coop::net::Role::Host||!c.object||!c.locals)return;const bool active=*(c.locals+g_setActiveParam)!=0;if(!active){if(coop::event_active_sync::HostInstanceForController(c.object))coop::event_active_sync::HostEndExternal(c.object);return;}bool created=false;uint64_t id=coop::event_active_sync::HostBeginExternal(c.object,"lakeglow_C","lakeMonsert",&created);if(!id)return;auto it=g_families.find(id);if(it==g_families.end()){Family f{c.object,R::InternalIndexOf(c.object),"lakeglow_C",0,0};uint8_t state=1;f.output=coop::event_output_sync::HostBeginState(id,"lakeglow_C",&state,1);g_families[id]=f;}else{uint8_t state=1;coop::event_output_sync::HostUpdateState(id,it->second.output,&state,1);}}

bool ValidBadSun(const BadSunState&st){if(st.version!=1||!std::isfinite(st.dryness)||!std::isfinite(st.intensity))return false;for(float c:st.color)if(!std::isfinite(c))return false;return true;}
bool CaptureBadSun(BadSunState&st){void*gm=Indexed(g_gmCls);if(!gm)return false;void*dnc=Read<void*>(gm,g_gmDnc,nullptr);void*sky=Read<void*>(dnc,g_dncSky,nullptr);if(!sky)return false;if(!g_skyCls||R::ClassOf(sky)!=g_skyCls){g_skyCls=R::ClassOf(sky);g_skyIntensity=R::FindPropertyOffset(g_skyCls,L"sunIntensityMult");g_skyColor=R::FindPropertyOffset(g_skyCls,L"suncolor");}if(g_skyIntensity<0||g_skyColor<0)return false;st.dryness=std::clamp(Read<float>(gm,g_gmDry,0.f),0.f,1.f);st.intensity=Read<float>(sky,g_skyIntensity,1.f);std::memcpy(st.color,reinterpret_cast<uint8_t*>(sky)+g_skyColor,sizeof(st.color));return ValidBadSun(st);}
bool ApplyBadSun(const BadSunState&st){if(!ValidBadSun(st))return false;void*gm=Indexed(g_gmCls);if(!gm)return false;void*dnc=Read<void*>(gm,g_gmDnc,nullptr);void*sky=Read<void*>(dnc,g_dncSky,nullptr);if(!sky)return false;if(!g_skyCls||R::ClassOf(sky)!=g_skyCls){g_skyCls=R::ClassOf(sky);g_skyIntensity=R::FindPropertyOffset(g_skyCls,L"sunIntensityMult");g_skyColor=R::FindPropertyOffset(g_skyCls,L"suncolor");}if(g_skyIntensity<0||g_skyColor<0)return false;Write<float>(gm,g_gmDry,std::clamp(st.dryness,0.f,1.f));Write<float>(sky,g_skyIntensity,st.intensity);std::memcpy(reinterpret_cast<uint8_t*>(sky)+g_skyColor,st.color,sizeof(st.color));if(g_setDryness){ue_wrap::ParamFrame p(g_setDryness);if(p.valid()){p.Set<float>(L"Add",0.f);ue_wrap::Call(gm,p);}}return true;}

bool ApplyClientNow(const ClientFamily&f){if(f.cls=="lakeglow_C"&&f.len==1)return ApplyLake(f.state[0]!=0);if(f.cls=="badSun_C"&&f.len==sizeof(BadSunState)){BadSunState st{};std::memcpy(&st,f.state,sizeof(st));return ApplyBadSun(st);}return false;}

void ResolveCue(){if(!g_gsCdo){void*c=R::FindClass(L"GameplayStatics");if(c){g_gsCdo=R::FindClassDefaultObject(L"GameplayStatics");g_playSound=R::FindFunction(c,L"PlaySound2D");g_playShake=R::FindFunction(c,L"PlayWorldCameraShake");}}}
void PlayCue(){ResolveCue();void*ctx=coop::players::Registry::Get().Local();if(!ctx||!g_gsCdo)return;for(const auto&n: {L"piramid_step_far-01",L"chamberAppear"}){void*snd=R::FindObject(n,L"SoundWave");if(!snd)snd=R::FindObject(n,L"SoundCue");if(!snd||!g_playSound)continue;ue_wrap::ParamFrame p(g_playSound);if(!p.valid())continue;p.Set<void*>(L"WorldContextObject",ctx);p.Set<void*>(L"Sound",snd);p.Set<float>(L"VolumeMultiplier",n[0]==L'p'?10.f:1.f);p.Set<float>(L"PitchMultiplier",n[0]==L'p'?0.f:0.5f);ue_wrap::Call(g_gsCdo,p);}void*shake=R::FindClass(L"piramidPingShake_C");if(shake&&g_playShake){ue_wrap::ParamFrame p(g_playShake);if(p.valid()){auto loc=ue_wrap::engine::GetActorLocation(ctx);p.Set<void*>(L"WorldContextObject",ctx);p.Set<void*>(L"Shake",shake);p.SetRaw(L"Epicenter",&loc,sizeof(loc));p.Set<float>(L"InnerRadius",10000.f);p.Set<float>(L"OuterRadius",10000.f);p.Set<float>(L"Falloff",1.f);p.Set<bool>(L"bOrientShakeTowardsEpicenter",false);ue_wrap::Call(g_gsCdo,p);}}}
} // namespace

void Install(coop::net::Session*s){g_session.store(s);RegisterHub();Resolve();if(!g_watch&&g_lakeSetActive){SG::SetEnabled(true);g_watch=SG::Watch(g_lakeSetActive,650111,&LakePre,&LakePost);}}
void HostBegin(uint64_t id,void*c,const std::string&cls){if(!id)return;if(cls=="event_fleshRain_C"||cls=="event_fossilBoarWar_C")coop::event_output_sync::HostEmitTransient(id,cls.c_str(),1);if(cls=="badSun_C")g_families[id]=Family{c,R::InternalIndexOf(c),cls,0,0};}
void HostNativeActive(uint64_t id,void*,const std::string&cls){if(id&&cls=="event_fossilBoarWar_C")coop::event_output_sync::HostEmitTransient(id,cls.c_str(),1);}
void HostEnd(uint64_t id,void*,const std::string&cls){if(cls=="badSun_C"){BadSunState reset{};reset.dryness=Read<float>(Indexed(g_gmCls),g_gmDry,0.f);ApplyBadSun(reset);}g_families.erase(id);}
void Tick(){RegisterHub();if(!g_watch&&g_lakeSetActive){SG::SetEnabled(true);g_watch=SG::Watch(g_lakeSetActive,650111,&LakePre,&LakePost);}auto*s=g_session.load();if(!s)return;if(s->role()==coop::net::Role::Client){const uint32_t gen=ue_wrap::world_identity::Generation();for(auto&[id,f]:g_clientFamilies){(void)id;if(!f.applied||f.worldGen!=gen){f.applied=ApplyClientNow(f);f.worldGen=gen;}}return;}const long long now=NowMs();for(auto&[id,f]:g_families){if(f.cls!="badSun_C"||now<f.nextUpdateMs)continue;f.nextUpdateMs=now+250;BadSunState st{};if(!CaptureBadSun(st))continue;if(!f.output)f.output=coop::event_output_sync::HostBeginState(id,"badSun_C",&st,sizeof(st));else coop::event_output_sync::HostUpdateState(id,f.output,&st,sizeof(st));}}
void ClientApplyState(uint64_t id,const std::string&cls,const uint8_t*state,uint8_t len,bool){if(!state||!len||len>32)return;ClientFamily f{};f.cls=cls;f.len=len;std::memcpy(f.state,state,len);f.applied=ApplyClientNow(f);f.worldGen=ue_wrap::world_identity::Generation();g_clientFamilies[id]=f;}
void ClientEndState(uint64_t id,const std::string&cls){if(cls=="lakeglow_C")ApplyLake(false);else if(cls=="badSun_C"){BadSunState st{};st.dryness=Read<float>(Indexed(g_gmCls),g_gmDry,0.f);ApplyBadSun(st);}g_clientFamilies.erase(id);}
void ClientApplyCue(const std::string&cls,uint8_t cue){if(cue==1&&(cls=="event_fleshRain_C"||cls=="event_fossilBoarWar_C"))PlayCue();}
void OnDisconnect(){auto clients=g_clientFamilies;for(const auto&[id,f]:clients){if(f.cls=="lakeglow_C")ApplyLake(Read<bool>(Indexed(g_lakeCls),g_lakeAlways,false));else ClientEndState(id,f.cls);}g_clientFamilies.clear();g_families.clear();g_session.store(nullptr);}
} // namespace coop::event_family_sync
