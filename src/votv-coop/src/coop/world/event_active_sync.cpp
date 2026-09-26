// Host-authoritative mirror of mainGamemode_C.activeEvents_senders.
#include "coop/world/event_active_sync.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/voice/radio_state.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::event_active_sync { namespace {
namespace R = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;
namespace SG = ue_wrap::script_gate;
enum : uint8_t { kBegin=0, kEnd=1, kSnapshotBegin=2, kSnapshotItem=3, kSnapshotEnd=4 };
std::atomic<coop::net::Session*> g_session{nullptr};
void* g_gmCls=nullptr; int32_t g_offActiveEvents=-1, g_offSenders=-1;
void* g_libCls=nullptr; void* g_setEventFn=nullptr; bool g_setEventWatch=false;
std::chrono::steady_clock::time_point g_nextResolve{};
bool g_pollResolved=false; uint32_t g_pollResolveAttempts=0; uint32_t g_edgeResolveAttempts=0;
void* g_gm=nullptr; int32_t g_gmIdx=-1; void* g_polledGm=nullptr; int32_t g_polledGmIdx=-1;
bool g_primed=false; long long g_lastPollMs=0; constexpr long long kPollIntervalMs=250;
struct RawPtrArray { void** Data; int32_t Num; int32_t Max; };
struct ActiveEntry { int32_t objIdx=-1; std::string className,rowName; long long firstSeenMs=0; uint64_t instanceId=0; };
std::unordered_map<void*,ActiveEntry> g_active; uint64_t g_nextInstanceId=1; uint32_t g_setRevision=0;
struct ClientEntry { std::string className,rowName; uint16_t elapsedSec=0; };
std::unordered_map<uint64_t,ClientEntry> g_clientActive,g_stagedActive;
uint32_t g_clientRevision=0,g_stagedRevision=0; bool g_staging=false;

void HostPollTick();
void OnSetEventPost(const SG::Call&){
 auto*s=g_session.load(std::memory_order_acquire);
 if(!s||!s->connected()||s->role()!=coop::net::Role::Host)return;
 // setEvent has completed its exact activeEvents_senders add/remove. Diff now,
 // in the same call stack, so an ON->effect->OFF controller cannot live wholly
 // between two 250 ms reconciliation polls.
 HostPollTick();
}

struct ClassRow { const char* cls; const char* row; };
const ClassRow kClassRows[]={{"obelisk_C","obelisk"},{"piramid2_C","piramid"},{"trigger_solarBoom_C","solar"},
 {"trigger_vehtp_C","vehtp"},{"trigger_agrav_C","agrav"},{"trigger_bigmRoar_C","call0"},
 {"trigger_wispSwarm_C","wisps"},{"trigger_spawnFollowingArir_C","arirFollower"},{"trigger_arirEgg_C","arirEgg"},
 {"trigger_bedEvent_C","bedEvent"},{"tentacleBallsFollower_C","tentacleBalls"},{"soltomiaCleaning_C","soltoClean"},
 {"morningUfo_C","morningGay"},{"rozitBorg_C","borgRozital"},{"event_bottomHoleController_C","rozitalHole"},
 {"ventCrawler_C","ventCrawler"},{"kocker_C","ventKnocker"},{"grayEventController_C","graysforest"},
 {"arirBusterSpawner_C","arirBuster"},{"saltpile_C","salt"},{"superEgger_C","eggvasion"},
 {"boarInvasion_C","boarwar"},{"dreamer_dreambase_C","dreambase"},{"arirShip_C","arirShip"}};
const char* RowForClass(const std::string& c){for(const auto& e:kClassRows)if(c==e.cls)return e.row;return nullptr;}
long long NowMs(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}
std::string Narrow(const std::wstring&w){std::string s;s.reserve(w.size());for(wchar_t c:w)s.push_back(c>0&&c<128?static_cast<char>(c):'?');return s;}
std::string Bound(const char*p,size_t n){size_t z=0;while(z<n&&p[z])++z;return std::string(p,p+z);}

void ResolvePass(){
 if(g_pollResolved&&g_setEventWatch)return;
 if(std::chrono::steady_clock::now()<g_nextResolve)return;
 g_nextResolve=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 if(!g_gmCls)g_gmCls=R::FindClass(L"mainGamemode_C");
 if(g_gmCls&&!g_pollResolved){
  if(g_offActiveEvents<0)g_offActiveEvents=R::FindPropertyOffset(g_gmCls,L"activeEvents");
  if(g_offSenders<0)g_offSenders=R::FindPropertyOffset(g_gmCls,L"activeEvents_senders");
  if(g_offActiveEvents>=0&&g_offSenders>=0){g_pollResolved=true;UE_LOGI("event_active: polling fallback resolved activeEvents=0x%X senders=0x%X",g_offActiveEvents,g_offSenders);}
  else{++g_pollResolveAttempts;if(g_pollResolveAttempts==5||(g_pollResolveAttempts%30)==0)UE_LOGW("event_active: polling fields unresolved after %u attempts; retrying",g_pollResolveAttempts);}
 }
 if(!g_setEventWatch){
  if(!g_libCls)g_libCls=R::FindClass(L"lib_C");
  if(g_libCls&&!g_setEventFn)g_setEventFn=R::FindFunction(g_libCls,L"setEvent");
  if(g_setEventFn){SG::SetEnabled(true);g_setEventWatch=SG::Watch(g_setEventFn,650091,nullptr,&OnSetEventPost);}
  if(g_setEventWatch)UE_LOGI("event_active: lib_C::setEvent post-watch installed (short-lived event edge coverage)");
  else{++g_edgeResolveAttempts;if(g_edgeResolveAttempts==5||(g_edgeResolveAttempts%30)==0)UE_LOGW("event_active: lib_C::setEvent post-watch unresolved after %u attempts; 250 ms polling remains active, retrying",g_edgeResolveAttempts);}
 }
}
void* Gamemode(){
 if(!g_gm||!R::IsLiveByIndex(g_gm,g_gmIdx)){g_gm=nullptr;g_gmIdx=-1;for(void*o:R::FindObjectsByClass(L"mainGamemode_C"))
  if(o&&R::IsLive(o)&&!R::NameStartsWith(R::NameOf(o),L"Default__")){g_gm=o;g_gmIdx=R::InternalIndexOf(o);break;}}
 return g_gm;
}
int ReadRefcount(void*gm){return *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(gm)+g_offActiveEvents);}
void PublishRadio(size_t n){coop::radio_state::SetInterference(n==0?0.0f:n==1?0.65f:n==2?0.82f:1.0f);}
coop::net::EventAuthorityPayload MakePayload(uint8_t op,uint32_t rev,const ActiveEntry*e,long long now){
 coop::net::EventAuthorityPayload p{};p.op=op;p.setRevision=rev;if(e){p.instanceId=e->instanceId;long long sec=(now-e->firstSeenMs)/1000;
 p.elapsedSec=static_cast<uint16_t>(sec<0?0:sec>65535?65535:sec);std::strncpy(p.className,e->className.c_str(),sizeof(p.className)-1);
 std::strncpy(p.rowName,e->rowName.c_str(),sizeof(p.rowName)-1);}return p;
}
void Broadcast(uint8_t op,const ActiveEntry*e){auto*s=g_session.load(std::memory_order_acquire);if(!s||!s->connected()||s->role()!=coop::net::Role::Host)return;
 auto p=MakePayload(op,++g_setRevision,e,NowMs());s->SendReliable(coop::net::ReliableKind::EventAuthority,&p,sizeof(p));}

void HostPollTick(){
 if(g_offActiveEvents<0||g_offSenders<0)return;
 void*gm=Gamemode();if(!gm)return;
 if(gm!=g_polledGm||!R::IsLiveByIndex(g_polledGm,g_polledGmIdx)){for(const auto&[oldObj,e]:g_active){(void)oldObj;Broadcast(kEnd,&e);}g_active.clear();g_polledGm=gm;g_polledGmIdx=g_gmIdx;g_primed=false;}
 auto*arr=reinterpret_cast<RawPtrArray*>(reinterpret_cast<uint8_t*>(gm)+g_offSenders);if(arr->Num<0||arr->Num>4096)return;
 const long long now=NowMs();if(!g_primed){g_primed=true;UE_LOGI("event_active: host poll primed n=%d",ReadRefcount(gm));}
 for(int32_t i=0;i<arr->Num;++i){void*obj=arr->Data?arr->Data[i]:nullptr;if(!obj||g_active.count(obj)||!R::IsLive(obj))continue;
  ActiveEntry e;e.objIdx=R::InternalIndexOf(obj);e.className=Narrow(R::ClassNameOf(obj));if(const char*row=RowForClass(e.className))e.rowName=row;
  e.firstSeenMs=now;e.instanceId=g_nextInstanceId++;auto [it,ok]=g_active.emplace(obj,std::move(e));if(!ok)continue;
  UE_LOGI("random_event_auth: HOST BEGIN instance=%llu class=%s row=%s",static_cast<unsigned long long>(it->second.instanceId),it->second.className.c_str(),it->second.rowName.empty()?"<unmapped>":it->second.rowName.c_str());Broadcast(kBegin,&it->second);}
 std::vector<void*>ended;for(const auto&[obj,e]:g_active){bool present=false;if(arr->Data)for(int32_t i=0;i<arr->Num;++i)if(arr->Data[i]==obj){present=true;break;}
  if(present&&R::IsLiveByIndex(obj,e.objIdx))continue;
  UE_LOGI("random_event_auth: HOST END instance=%llu class=%s",static_cast<unsigned long long>(e.instanceId),e.className.c_str());Broadcast(kEnd,&e);ended.push_back(obj);}
 for(void*o:ended)g_active.erase(o);
 PublishRadio(g_active.size());
}
} // namespace

void Install(coop::net::Session*s){g_session.store(s,std::memory_order_release);}
void Tick(){if(!GT::IsGameThread())return;auto*s=g_session.load(std::memory_order_acquire);if(!s||!s->connected()||s->role()!=coop::net::Role::Host)return;
 long long now=NowMs();if(now-g_lastPollMs<kPollIntervalMs)return;g_lastPollMs=now;ResolvePass();HostPollTick();}
void SendJoinSnapshotForSlot(int slot){auto*s=g_session.load(std::memory_order_acquire);if(!s||!s->connected()||s->role()!=coop::net::Role::Host)return;
 ResolvePass();HostPollTick(); // Snapshot the live registry, not a potentially 250 ms-old poll.
 uint32_t rev=++g_setRevision;long long now=NowMs();auto p=MakePayload(kSnapshotBegin,rev,nullptr,now);s->SendReliableToSlot(slot,coop::net::ReliableKind::EventAuthority,&p,sizeof(p));
 for(const auto&[obj,e]:g_active){(void)obj;p=MakePayload(kSnapshotItem,rev,&e,now);s->SendReliableToSlot(slot,coop::net::ReliableKind::EventAuthority,&p,sizeof(p));}
 p=MakePayload(kSnapshotEnd,rev,nullptr,now);s->SendReliableToSlot(slot,coop::net::ReliableKind::EventAuthority,&p,sizeof(p));
 UE_LOGI("random_event_auth: HOST SNAPSHOT slot=%d revision=%u active=%zu",slot,rev,g_active.size());}
void OnReliable(const coop::net::EventAuthorityPayload&p){if(!GT::IsGameThread()||p.op>kSnapshotEnd)return;
 std::string cls=Bound(p.className,sizeof(p.className)),row=Bound(p.rowName,sizeof(p.rowName));ClientEntry e{cls,row,p.elapsedSec};
 if(p.op==kSnapshotBegin){if(p.setRevision<g_clientRevision)return;g_stagedActive.clear();g_stagedRevision=p.setRevision;g_staging=true;return;}
 if(p.op==kSnapshotItem){if(g_staging&&p.setRevision==g_stagedRevision&&p.instanceId)g_stagedActive[p.instanceId]=std::move(e);return;}
 if(p.op==kSnapshotEnd){if(!g_staging||p.setRevision!=g_stagedRevision||p.setRevision<g_clientRevision)return;g_clientActive.swap(g_stagedActive);g_stagedActive.clear();g_staging=false;g_clientRevision=p.setRevision;PublishRadio(g_clientActive.size());UE_LOGI("random_event_auth: CLIENT SNAPSHOT revision=%u active=%zu",p.setRevision,g_clientActive.size());return;}
 if(!p.instanceId||p.setRevision<=g_clientRevision)return;
 g_clientRevision=p.setRevision;
 if(p.op==kBegin){g_clientActive[p.instanceId]=std::move(e);UE_LOGI("random_event_auth: CLIENT authoritative BEGIN instance=%llu class=%s row=%s",static_cast<unsigned long long>(p.instanceId),cls.c_str(),row.empty()?"<unmapped>":row.c_str());UE_LOGW("random_event_auth: instance=%llu class=%s host-authoritative event active; output/presentation lane may be incomplete",static_cast<unsigned long long>(p.instanceId),cls.c_str());}
 else{g_clientActive.erase(p.instanceId);UE_LOGI("random_event_auth: CLIENT authoritative END instance=%llu",static_cast<unsigned long long>(p.instanceId));}PublishRadio(g_clientActive.size());}
void OnReliable(const coop::net::EventSnapshotPayload&){UE_LOGW("event_active: legacy EventSnapshot ignored under b65005 registry");}
void OnDisconnect(){coop::radio_state::SetInterference(0.0f);g_active.clear();g_clientActive.clear();g_stagedActive.clear();g_staging=false;g_clientRevision=g_stagedRevision=g_setRevision=0;g_nextInstanceId=1;g_polledGm=g_gm=nullptr;g_polledGmIdx=g_gmIdx=-1;g_primed=false;g_session.store(nullptr,std::memory_order_release);}
} // namespace coop::event_active_sync
