#include "coop/world/event_output_sync.h"

#include "coop/creatures/wisp_attack_sync.h"
#include "coop/element/intent_authority.h"
#include "coop/element/registry.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "coop/player/players_registry.h"
#include "coop/player/remote_player.h"
#include "coop/world/black_fog_sync.h"
#include "ue_wrap/actors/wisp.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::event_output_sync { namespace {
namespace R = ue_wrap::reflection;
namespace GT = ue_wrap::game_thread;

constexpr uint8_t kBegin=0,kUpdate=1,kEnd=2,kSnapshot=3;
constexpr uint8_t kActorMirror=static_cast<uint8_t>(OutputType::ActorMirror);
constexpr uint8_t kEnvironment=static_cast<uint8_t>(OutputType::EnvironmentState);
constexpr uint8_t kInteractive=1;
constexpr uint8_t kTouch=1;
constexpr uint8_t kAccepted=0,kNoOutput=1,kNotInteractive=2,kOutOfReach=3,kUnsupported=4;
constexpr uint8_t kResultRequester=0;
constexpr size_t kMaxOutputs=256,kMaxPending=16,kMaxRecentResultsPerSlot=32;
constexpr long long kPendingMs=5000;
constexpr long long kResolveRetryMs=100,kResolveDeadlineMs=30000;
constexpr long long kIntentWindowMs=1000,kIntentWarnMs=5000;
constexpr long long kDecisionLogMs=2000;
constexpr uint32_t kMaxIntentsPerWindow=8;
constexpr float kWispContactUU=200.0f;

struct Key { uint64_t instance=0,output=0; bool operator==(const Key&o)const{return instance==o.instance&&output==o.output;} };
struct KeyHash { size_t operator()(const Key&k)const{return std::hash<uint64_t>{}(k.instance)^(std::hash<uint64_t>{}(k.output)<<1);} };
struct HostOutput { coop::net::EventOutputStatePayload wire{}; void* actor=nullptr; int32_t actorIdx=-1; long long createdMs=0; };
struct ClientOutput { coop::net::EventOutputStatePayload wire{}; bool bound=false; bool contactLatched=false; bool resolveExpiredLogged=false; long long nextResolveMs=0,resolveDeadlineMs=0; };
struct Pending { Key key{}; long long deadlineMs=0; };
struct Recent { uint32_t requestId=0; coop::net::EventOutputResultPayload result{}; };
struct IntentRate { uint32_t generation=0,count=0,dropped=0; long long windowStartMs=0,nextWarnMs=0,nextDecisionLogMs=0; };

std::atomic<coop::net::Session*> g_session{nullptr};
std::unordered_map<Key,HostOutput,KeyHash> g_host;
std::unordered_map<Key,ClientOutput,KeyHash> g_client;
std::unordered_map<Key,uint32_t,KeyHash> g_tombstones;
std::deque<Key> g_tombstoneOrder;
std::unordered_map<uint32_t,Pending> g_pending;
std::vector<Recent> g_recent[coop::net::kMaxPeers];
uint32_t g_recentGeneration[coop::net::kMaxPeers]{};
IntentRate g_intentRate[coop::net::kMaxPeers]{};
uint64_t g_nextOutputId=1;
uint32_t g_nextRequestId=1;

long long NowMs(){using namespace std::chrono;return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();}
std::string Bound(const char*p,size_t n){size_t z=0;while(z<n&&p[z])++z;return std::string(p,p+z);}
void CopyClass(char*dst,size_t n,const char*src){if(!src||!n)return;std::strncpy(dst,src,n-1);dst[n-1]='\0';}
void RememberTombstone(const Key&k,uint32_t sequence){auto it=g_tombstones.find(k);if(it!=g_tombstones.end()){it->second=std::max(it->second,sequence);return;}if(g_tombstones.size()>=kMaxOutputs){g_tombstones.erase(g_tombstoneOrder.front());g_tombstoneOrder.pop_front();}g_tombstones.emplace(k,sequence);g_tombstoneOrder.push_back(k);}

void SendState(HostOutput&o,uint8_t op,int slot=-1){
 auto*s=g_session.load(std::memory_order_acquire);if(!s||s->role()!=coop::net::Role::Host)return;
 if(op==kSnapshot){const long long sec=(NowMs()-o.createdMs)/1000;o.wire.elapsedSec=static_cast<uint16_t>(std::clamp(sec,0LL,65535LL));}
 o.wire.op=op;o.wire.sequence++;
 if(slot>=0)s->SendReliableToSlot(slot,coop::net::ReliableKind::EventOutputState,&o.wire,sizeof(o.wire));
 else s->SendReliable(coop::net::ReliableKind::EventOutputState,&o.wire,sizeof(o.wire));
}

void ApplyClient(ClientOutput&o,bool snapshot){
 const auto&p=o.wire;const std::string cls=Bound(p.className,sizeof(p.className));
 if(p.outputType==kEnvironment){
 if(cls!="blackFog_C"||p.stateLen!=1){UE_LOGW("event_output: unknown environment adapter class=%s len=%u -- ignored",cls.c_str(),p.stateLen);return;}
  if(snapshot||!o.bound)coop::black_fog_sync::ClientBegin(p.eventInstanceId,p.elapsedSec,p.state[0],snapshot);
  else coop::black_fog_sync::ClientPhase(p.eventInstanceId,p.state[0]);
  o.bound=true;return;
 }
 if(p.outputType!=kActorMirror||p.backingElementId==0)return;
 const bool wasBound=o.bound;o.bound=false;
 auto*e=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(p.backingElementId));
 void*a=e?e->GetActor():nullptr;
 if(!e||e->GetType()!=coop::element::ElementType::Npc||!a||!R::IsLiveByIndex(a,e->GetInternalIdx())||e->GetTypeName()!=cls)return;
 if(!wasBound)UE_LOGI("event_output: CLIENT BIND instance=%llu output=%llu backingNpcEid=%u class=%s",
  static_cast<unsigned long long>(p.eventInstanceId),static_cast<unsigned long long>(p.outputId),p.backingElementId,cls.c_str());
 o.bound=true;
 o.resolveExpiredLogged=false;
}

void SendResult(uint8_t requester,const coop::net::EventOutputIntentPayload&i,uint8_t code){
 auto*s=g_session.load(std::memory_order_acquire);if(!s||s->role()!=coop::net::Role::Host)return;
 coop::net::EventOutputResultPayload r{};r.eventInstanceId=i.eventInstanceId;r.outputId=i.outputId;
 r.requestId=i.requestId;r.action=i.action;r.result=code;r.targetPolicy=kResultRequester;r.targetSlot=requester;
 s->SendReliableToSlot(requester,coop::net::ReliableKind::EventOutputResult,&r,sizeof(r));
 if(requester<coop::net::kMaxPeers){auto&v=g_recent[requester];v.push_back({i.requestId,r});if(v.size()>kMaxRecentResultsPerSlot)v.erase(v.begin());}
 auto&rate=g_intentRate[requester];const long long now=NowMs();
 if(now>=rate.nextDecisionLogMs){
  UE_LOGI("event_output: HOST RESULT slot=%u request=%u instance=%llu output=%llu result=%u target=requester",
          static_cast<unsigned>(requester),i.requestId,
          static_cast<unsigned long long>(i.eventInstanceId),
          static_cast<unsigned long long>(i.outputId),static_cast<unsigned>(code));
  rate.nextDecisionLogMs=now+kDecisionLogMs;
 }
}

bool AdmitIntent(uint8_t sender,uint32_t generation,long long now){
 auto&r=g_intentRate[sender];
 if(r.generation!=generation){r=IntentRate{};r.generation=generation;r.windowStartMs=now;}
 else if(now-r.windowStartMs>=kIntentWindowMs){
  r.count=0;r.dropped=0;r.windowStartMs=now;
 }
 if(r.count<kMaxIntentsPerWindow){++r.count;return true;}
 ++r.dropped;
 if(now>=r.nextWarnMs){
  UE_LOGW("event_output: HOST INTENT rate-limited slot=%u generation=%u dropped=%u",
          static_cast<unsigned>(sender),generation,r.dropped);
  r.nextWarnMs=now+kIntentWarnMs;r.dropped=0;
 }
 return false;
}

void RequestTouch(const Key&key,ClientOutput&o){
 auto*s=g_session.load(std::memory_order_acquire);if(!s||s->role()!=coop::net::Role::Client||g_pending.size()>=kMaxPending)return;
 uint32_t id=g_nextRequestId++;if(!id)id=g_nextRequestId++;
 coop::net::EventOutputIntentPayload p{};p.eventInstanceId=key.instance;p.outputId=key.output;p.requestId=id;p.action=kTouch;
 if(s->SendReliable(coop::net::ReliableKind::EventOutputIntent,&p,sizeof(p))){g_pending.emplace(id,Pending{key,NowMs()+kPendingMs});o.contactLatched=true;
  UE_LOGI("event_output: CLIENT INTENT request=%u instance=%llu output=%llu action=touch",id,static_cast<unsigned long long>(key.instance),static_cast<unsigned long long>(key.output));}
}
} // namespace

void Install(coop::net::Session*s){g_session.store(s,std::memory_order_release);}
uint64_t HostBeginActor(uint64_t instance,void*actor,uint32_t eid,const char*cls,bool interactive){
 auto*s=g_session.load(std::memory_order_acquire);if(!GT::IsGameThread()||!s||s->role()!=coop::net::Role::Host||!instance||!actor||!eid)return 0;
 auto*backing=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(eid));
 if(!backing){UE_LOGW("event_output: HOST BEGIN actor refused -- backing eid=%u unresolved (instance=%llu class=%s)",eid,static_cast<unsigned long long>(instance),cls?cls:"");return 0;}
 if(backing->GetType()!=coop::element::ElementType::Npc){UE_LOGW("event_output: HOST BEGIN actor refused -- backing eid=%u is not NPC type (instance=%llu class=%s)",eid,static_cast<unsigned long long>(instance),cls?cls:"");return 0;}
 if(backing->GetActor()!=actor){UE_LOGW("event_output: HOST BEGIN actor refused -- backing eid=%u actor mismatch (instance=%llu class=%s)",eid,static_cast<unsigned long long>(instance),cls?cls:"");return 0;}
 if(backing->GetTypeName()!=(cls?cls:"")){UE_LOGW("event_output: HOST BEGIN actor refused -- backing eid=%u class mismatch requested=%s actual=%s (instance=%llu)",eid,cls?cls:"",backing->GetTypeName().c_str(),static_cast<unsigned long long>(instance));return 0;}
 for(const auto&[k,o]:g_host)if(k.instance==instance&&o.actor==actor&&R::IsLiveByIndex(actor,o.actorIdx))return k.output;
 if(g_host.size()>=kMaxOutputs){UE_LOGW("event_output: HOST BEGIN actor refused -- registry capacity %zu reached (instance=%llu eid=%u class=%s)",kMaxOutputs,static_cast<unsigned long long>(instance),eid,cls?cls:"");return 0;}
 uint64_t id=g_nextOutputId++;if(!id)id=g_nextOutputId++;Key k{instance,id};HostOutput o{};o.actor=actor;o.actorIdx=R::InternalIndexOf(actor);o.createdMs=NowMs();
 o.wire.eventInstanceId=instance;o.wire.outputId=id;o.wire.backingElementId=eid;o.wire.outputType=kActorMirror;o.wire.flags=interactive?kInteractive:0;CopyClass(o.wire.className,sizeof(o.wire.className),cls);
 auto[it,ok]=g_host.emplace(k,std::move(o));if(!ok)return 0;SendState(it->second,kBegin);
 UE_LOGI("event_output: HOST BEGIN actor instance=%llu output=%llu backingNpcEid=%u class=%s interactive=%u",static_cast<unsigned long long>(instance),static_cast<unsigned long long>(id),eid,cls?cls:"",interactive?1u:0u);return id;
}
uint64_t HostBeginEnvironment(uint64_t instance,const char*cls,uint8_t state){
 auto*s=g_session.load(std::memory_order_acquire);if(!GT::IsGameThread()||!s||s->role()!=coop::net::Role::Host||!instance)return 0;
 if(g_host.size()>=kMaxOutputs){UE_LOGW("event_output: HOST BEGIN environment refused -- registry capacity %zu reached (instance=%llu class=%s)",kMaxOutputs,static_cast<unsigned long long>(instance),cls?cls:"");return 0;}
 uint64_t id=g_nextOutputId++;if(!id)id=g_nextOutputId++;Key k{instance,id};HostOutput o{};o.createdMs=NowMs();o.wire.eventInstanceId=instance;o.wire.outputId=id;o.wire.outputType=kEnvironment;o.wire.stateLen=1;o.wire.state[0]=state;CopyClass(o.wire.className,sizeof(o.wire.className),cls);
 auto[it,ok]=g_host.emplace(k,std::move(o));if(!ok)return 0;SendState(it->second,kBegin);UE_LOGI("event_output: HOST BEGIN environment instance=%llu output=%llu class=%s state=%u",static_cast<unsigned long long>(instance),static_cast<unsigned long long>(id),cls?cls:"",static_cast<unsigned>(state));return id;
}
void HostUpdateEnvironment(uint64_t instance,uint64_t output,uint8_t state){auto it=g_host.find({instance,output});if(it==g_host.end()||it->second.wire.outputType!=kEnvironment||it->second.wire.state[0]==state)return;it->second.wire.state[0]=state;SendState(it->second,kUpdate);UE_LOGI("event_output: HOST UPDATE instance=%llu output=%llu state=%u",static_cast<unsigned long long>(instance),static_cast<unsigned long long>(output),static_cast<unsigned>(state));}
void HostEndInstance(uint64_t instance){for(auto it=g_host.begin();it!=g_host.end();){if(it->first.instance!=instance){++it;continue;}SendState(it->second,kEnd);UE_LOGI("event_output: HOST END instance=%llu output=%llu",static_cast<unsigned long long>(it->first.instance),static_cast<unsigned long long>(it->first.output));it=g_host.erase(it);}}
void SendJoinSnapshotForSlot(int slot){size_t n=0;for(auto&[k,o]:g_host){(void)k;SendState(o,kSnapshot,slot);++n;}UE_LOGI("event_output: HOST SNAPSHOT slot=%d outputs=%zu",slot,n);}
void ClientEndInstance(uint64_t instance){for(auto it=g_client.begin();it!=g_client.end();){if(it->first.instance!=instance){++it;continue;}if(it->second.wire.outputType==kEnvironment)coop::black_fog_sync::ClientEnd(instance);RememberTombstone(it->first,it->second.wire.sequence);it=g_client.erase(it);}for(auto it=g_pending.begin();it!=g_pending.end();)it=(it->second.key.instance==instance)?g_pending.erase(it):std::next(it);}
void OnReliable(const coop::net::EventOutputStatePayload&p){if(!GT::IsGameThread()||!p.eventInstanceId||!p.outputId||p.op>kSnapshot||p.stateLen>sizeof(p.state))return;Key k{p.eventInstanceId,p.outputId};auto it=g_client.find(k);
 if(p.outputType!=kActorMirror&&p.outputType!=kEnvironment){UE_LOGW("event_output: CLIENT UNKNOWN type=%u instance=%llu output=%llu -- ignored",static_cast<unsigned>(p.outputType),static_cast<unsigned long long>(p.eventInstanceId),static_cast<unsigned long long>(p.outputId));return;}
 auto dead=g_tombstones.find(k);if(dead!=g_tombstones.end()){UE_LOGI("event_output: CLIENT TOMBSTONE-DROP instance=%llu output=%llu seq=%u endSeq=%u",static_cast<unsigned long long>(p.eventInstanceId),static_cast<unsigned long long>(p.outputId),p.sequence,dead->second);return;}
 if(p.op==kEnd){if(it!=g_client.end()&&p.sequence<=it->second.wire.sequence){UE_LOGI("event_output: CLIENT STALE-END-DROP instance=%llu output=%llu seq=%u current=%u",static_cast<unsigned long long>(p.eventInstanceId),static_cast<unsigned long long>(p.outputId),p.sequence,it->second.wire.sequence);return;}if(it!=g_client.end()){if(it->second.wire.outputType==kEnvironment)coop::black_fog_sync::ClientEnd(p.eventInstanceId);g_client.erase(it);}RememberTombstone(k,p.sequence);return;}
 if(it!=g_client.end()&&p.sequence<=it->second.wire.sequence){UE_LOGI("event_output: CLIENT STALE-DROP instance=%llu output=%llu seq=%u current=%u",static_cast<unsigned long long>(p.eventInstanceId),static_cast<unsigned long long>(p.outputId),p.sequence,it->second.wire.sequence);return;}
 if(it==g_client.end()&&p.op==kUpdate){UE_LOGW("event_output: CLIENT UPDATE-BEFORE-BEGIN instance=%llu output=%llu seq=%u -- dropped",static_cast<unsigned long long>(p.eventInstanceId),static_cast<unsigned long long>(p.outputId),p.sequence);return;}
 if(it==g_client.end()&&g_client.size()>=kMaxOutputs){UE_LOGW("event_output: client registry full -- dropping output");return;}
 auto&out=g_client[k];out.wire=p;if(p.outputType==kActorMirror){out.nextResolveMs=0;out.resolveDeadlineMs=NowMs()+kResolveDeadlineMs;out.resolveExpiredLogged=false;}ApplyClient(out,p.op==kSnapshot);UE_LOGI("event_output: CLIENT %s instance=%llu output=%llu type=%u class=%s seq=%u",p.op==kSnapshot?"JIP-RESTORE":p.op==kBegin?"BEGIN":"UPDATE",static_cast<unsigned long long>(p.eventInstanceId),static_cast<unsigned long long>(p.outputId),static_cast<unsigned>(p.outputType),Bound(p.className,sizeof(p.className)).c_str(),p.sequence);
}
void OnIntent(const coop::net::EventOutputIntentPayload&i,uint8_t sender,uint32_t senderGeneration){if(!GT::IsGameThread()||sender==0||sender>=coop::net::kMaxPeers)return;
 auto*s=g_session.load(std::memory_order_acquire);if(!s||s->role()!=coop::net::Role::Host||!s->IsSlotReady(sender))return;
 const uint32_t liveGeneration=s->peerGenerationForSlot(sender);if(!senderGeneration||senderGeneration!=liveGeneration)return;
 if(!AdmitIntent(sender,senderGeneration,NowMs()))return;
 if(!i.eventInstanceId||!i.outputId||!i.requestId)return;
 if(g_recentGeneration[sender]!=senderGeneration){g_recent[sender].clear();g_recentGeneration[sender]=senderGeneration;}
 for(const auto&r:g_recent[sender])if(r.requestId==i.requestId){s->SendReliableToSlot(sender,coop::net::ReliableKind::EventOutputResult,&r.result,sizeof(r.result));return;}
 auto it=g_host.find({i.eventInstanceId,i.outputId});if(it==g_host.end()){SendResult(sender,i,kNoOutput);return;}auto&o=it->second;
 if(o.wire.outputType==kActorMirror&&!R::IsLiveByIndex(o.actor,o.actorIdx)){SendResult(sender,i,kNoOutput);return;}
 if(!(o.wire.flags&kInteractive)){SendResult(sender,i,kNotInteractive);return;}
 const auto auth=coop::element::IntentTarget::ForClientIntent(*g_session.load(std::memory_order_acquire),sender,kWispContactUU).Authorize(o.actor);
 if(!auth){SendResult(sender,i,kOutOfReach);return;}
 const std::string cls=Bound(o.wire.className,sizeof(o.wire.className));
 if(i.action!=kTouch||cls!="killerwisp_C"||!ue_wrap::wisp::CanReach(o.actor,auth.actor)){SendResult(sender,i,kUnsupported);return;}
 // Close the validation/work gap as far as the existing atomics permit. The
 // game-thread world teardown cannot interleave with this handler, while a
 // net-thread departure/replacement changes readiness/generation here.
 if(!s->IsSlotReady(sender)||s->peerGenerationForSlot(sender)!=senderGeneration)return;
 const bool accepted=coop::wisp_attack_sync::HandleClientTouchIntent(o.actor,sender);
 SendResult(sender,i,accepted?kAccepted:kOutOfReach);
}
void OnResult(const coop::net::EventOutputResultPayload&r){if(!GT::IsGameThread()||!r.requestId||r.targetPolicy!=kResultRequester||r.targetSlot!=coop::players::Registry::Get().LocalPeerId())return;auto it=g_pending.find(r.requestId);if(it==g_pending.end()||it->second.key!=Key{r.eventInstanceId,r.outputId})return;if(NowMs()>it->second.deadlineMs){g_pending.erase(it);return;}UE_LOGI("event_output: CLIENT RESULT request=%u instance=%llu output=%llu result=%u target=requester",r.requestId,static_cast<unsigned long long>(r.eventInstanceId),static_cast<unsigned long long>(r.outputId),r.result);g_pending.erase(it);}
void Tick(){if(!GT::IsGameThread())return;auto*s=g_session.load(std::memory_order_acquire);if(!s)return;const long long now=NowMs();for(auto it=g_pending.begin();it!=g_pending.end();)it=(now>it->second.deadlineMs)?g_pending.erase(it):std::next(it);
 if(s->role()==coop::net::Role::Host){std::vector<uint64_t>dead;for(const auto&[k,o]:g_host)if(o.wire.outputType==kActorMirror&&!R::IsLiveByIndex(o.actor,o.actorIdx))dead.push_back(k.instance);for(uint64_t id:dead)HostEndInstance(id);return;}
 void*local=coop::players::Registry::Get().Local();if(local&&!R::IsLive(local))local=nullptr;
 for(auto&[k,o]:g_client){if(o.wire.outputType==kActorMirror&&now>=o.nextResolveMs&&now<=o.resolveDeadlineMs){ApplyClient(o,false);o.nextResolveMs=now+kResolveRetryMs;}if(o.wire.outputType==kActorMirror&&!o.bound&&now>o.resolveDeadlineMs&&!o.resolveExpiredLogged){UE_LOGW("event_output: CLIENT BIND-TIMEOUT instance=%llu output=%llu backingNpcEid=%u",static_cast<unsigned long long>(k.instance),static_cast<unsigned long long>(k.output),o.wire.backingElementId);o.resolveExpiredLogged=true;}if(!local||!o.bound||!(o.wire.flags&kInteractive))continue;auto*e=coop::element::Registry::Get().Get(static_cast<coop::element::ElementId>(o.wire.backingElementId));void*a=e?e->GetActor():nullptr;if(!a||!ue_wrap::wisp::IsKillerWisp(a))continue;const float d=ue_wrap::wisp::DistanceTo(a,local);if(d>250.f){o.contactLatched=false;continue;}if(!o.contactLatched&&d<=kWispContactUU&&ue_wrap::wisp::CanReach(a,local))RequestTouch(k,o);}
}
void OnPeerLeft(uint8_t slot){if(slot>=coop::net::kMaxPeers)return;g_recent[slot].clear();g_recentGeneration[slot]=0;g_intentRate[slot]=IntentRate{};}
void OnDisconnect(){g_host.clear();g_client.clear();g_tombstones.clear();g_tombstoneOrder.clear();g_pending.clear();for(uint8_t slot=0;slot<coop::net::kMaxPeers;++slot)OnPeerLeft(slot);g_nextOutputId=1;g_nextRequestId=1;g_session.store(nullptr,std::memory_order_release);}
} // namespace coop::event_output_sync
