#include "coop/interactables/generator_break_sync.h"
#include "coop/element/portable_identity.h"
#include "coop/net/protocol.h"
#include "coop/net/session.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/game_thread.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/script_gate.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace coop::generator_break_sync { namespace {
namespace R=ue_wrap::reflection; namespace GT=ue_wrap::game_thread; namespace SG=ue_wrap::script_gate;
std::atomic<coop::net::Session*> g_session{nullptr};
struct BoolField{int32_t off=-1;uint8_t mask=0;};
struct Fields{
 void* genCls=nullptr;void* panelCls=nullptr;void* breakFn=nullptr;void* updateFn=nullptr;
 void* setRotatorsFn=nullptr;void* setKnobsFn=nullptr;void* setSwitchesFn=nullptr;
 int32_t panel=-1,cycle=-1,sineOffset=-1,sineFrequency=-1,sineAmplitude=-1;
 int32_t targetSineOffset=-1,targetSineFrequency=-1,targetSineAmplitude=-1;
 int32_t switchesTarget=-1,switchesStates=-1,rotatorStates=-1,rotatorGrid=-1;
 BoolField broken,sineComplete,switchesComplete,rotatorsComplete;
}g;
bool g_ready=false,g_failed=false,g_watch=false;int g_attempts=0;std::chrono::steady_clock::time_point g_nextResolve{};
enum:int{kBreakWatch=650054};
template<class T>struct RawArray{T*Data;int32_t Num;int32_t Max;};
struct RotatorCell{uint8_t top,right,bottom,left;};static_assert(sizeof(RotatorCell)==4);
struct Pending{coop::net::GeneratorBreakStatePayload p{};uint64_t expiresMs=0;};
std::deque<Pending>g_pending;constexpr size_t kMaxPending=32;constexpr uint64_t kPendingTtlMs=30000;
std::unordered_map<std::string,uint32_t>g_hostRevision,g_clientRevision;
std::unordered_set<void*>g_breakArmed;

uint64_t NowMs(){using namespace std::chrono;return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());}
std::string Narrow(const std::wstring&w){std::string s;s.reserve(w.size());for(wchar_t c:w)s.push_back(c>=0&&c<128?static_cast<char>(c):'?');return s;}
std::string Identity(void*a){return Narrow(coop::element::PortableWireKey(a));}
void PutKey(coop::net::WireKey&k,const std::string&s){std::memset(&k,0,sizeof(k));size_t n=std::min<size_t>(31,s.size());k.len=static_cast<uint8_t>(n);if(n)std::memcpy(k.data,s.data(),n);}
std::string Key(const coop::net::WireKey&k){size_t n=std::min<size_t>(31,k.len);return std::string(k.data,k.data+n);}
bool ReadBool(void*o,const BoolField&f){return o&&f.off>=0&&f.mask&&(*(reinterpret_cast<uint8_t*>(o)+f.off)&f.mask)!=0;}
void WriteBool(void*o,const BoolField&f,bool v){if(!o||f.off<0||!f.mask)return;auto&b=*(reinterpret_cast<uint8_t*>(o)+f.off);b=v?static_cast<uint8_t>(b|f.mask):static_cast<uint8_t>(b&~f.mask);}
template<class T>T&At(void*o,int32_t off){return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(o)+off);}
bool BoolProp(void*c,const wchar_t*n,BoolField&f){return R::FindBoolProperty(c,n,f.off,f.mask)&&f.off>=0&&f.mask;}
bool Prop(void*c,const wchar_t*n,int32_t&off){off=R::FindPropertyOffset(c,n);return off>=0;}
void Missing(bool ok,const char*name){if(!ok)UE_LOGW("generator_break: missing reflected field/function %s",name);}

void Resolve(){
 if((g_ready&&g_watch)||g_failed||std::chrono::steady_clock::now()<g_nextResolve)return;
 g_nextResolve=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 if(!g.genCls)g.genCls=R::FindClass(L"generator_C");
 if(!g.panelCls)g.panelCls=R::FindClass(L"transformerMGPanel_C");
 if(!g.genCls||!g.panelCls)return;
 bool ok=true;
 if(g.panel<0){bool x=Prop(g.genCls,L"panelObj",g.panel);Missing(x,"generator_C::panelObj");ok&=x;}
 if(g.cycle<0){bool x=Prop(g.genCls,L"cycle",g.cycle);Missing(x,"generator_C::cycle");ok&=x;}
 if(g.broken.off<0){bool x=BoolProp(g.genCls,L"isBroken",g.broken);Missing(x,"generator_C::isBroken(bool)");ok&=x;}
#define GP(member,name) if(g.member<0){bool x=Prop(g.panelCls,L##name,g.member);Missing(x,"transformerMGPanel_C::" name);ok&=x;}
 GP(sineOffset,"sine_offset") GP(sineFrequency,"sine_frequency") GP(sineAmplitude,"sine_amplitude")
 GP(targetSineOffset,"targetSine_offset") GP(targetSineFrequency,"targetSine_frequency") GP(targetSineAmplitude,"targetSine_amplitude")
 GP(switchesTarget,"switches_target") GP(switchesStates,"switches_states") GP(rotatorStates,"rotators_states") GP(rotatorGrid,"rotators_colorGrid")
#undef GP
 if(g.sineComplete.off<0){bool x=BoolProp(g.panelCls,L"isSineComplete",g.sineComplete);Missing(x,"transformerMGPanel_C::isSineComplete(bool)");ok&=x;}
 if(g.switchesComplete.off<0){bool x=BoolProp(g.panelCls,L"isSwitchesComplete",g.switchesComplete);Missing(x,"transformerMGPanel_C::isSwitchesComplete(bool)");ok&=x;}
 if(g.rotatorsComplete.off<0){bool x=BoolProp(g.panelCls,L"isRotatorsComplete",g.rotatorsComplete);Missing(x,"transformerMGPanel_C::isRotatorsComplete(bool)");ok&=x;}
 if(!g.breakFn)g.breakFn=R::FindFunction(g.genCls,L"break");
 if(!g.updateFn)g.updateFn=R::FindFunction(g.genCls,L"update");
 if(!g.setRotatorsFn)g.setRotatorsFn=R::FindFunction(g.panelCls,L"setRotators");
 if(!g.setKnobsFn)g.setKnobsFn=R::FindFunction(g.panelCls,L"setKnobs");
 if(!g.setSwitchesFn)g.setSwitchesFn=R::FindFunction(g.panelCls,L"setSwitches");
 Missing(g.breakFn,"generator_C::break");Missing(g.updateFn,"generator_C::update");Missing(g.setRotatorsFn,"transformerMGPanel_C::setRotators");Missing(g.setKnobsFn,"transformerMGPanel_C::setKnobs");Missing(g.setSwitchesFn,"transformerMGPanel_C::setSwitches");
 ok&=g.breakFn&&g.updateFn&&g.setRotatorsFn&&g.setKnobsFn&&g.setSwitchesFn;
 if(ok){g_ready=true;SG::SetEnabled(true);g_watch=SG::Watch(g.breakFn,kBreakWatch,[](const SG::Call&call){
   auto*s=g_session.load(std::memory_order_acquire);if(!call.fromOurCode&&s&&s->connected()&&s->role()==coop::net::Role::Host&&call.function==g.breakFn&&call.object&&R::ClassOf(call.object)==g.genCls&&!ReadBool(call.object,g.broken))g_breakArmed.insert(call.object);return SG::Verdict::Run;
  },[](const SG::Call&call){
   if(!g_breakArmed.erase(call.object))return;
   auto*s=g_session.load(std::memory_order_acquire);if(call.fromOurCode||!s||!s->connected()||s->role()!=coop::net::Role::Host||call.function!=g.breakFn||!call.object||R::ClassOf(call.object)!=g.genCls||!ReadBool(call.object,g.broken))return;
   coop::net::GeneratorBreakStatePayload p{};std::string key=Identity(call.object);if(key.empty())return;void*panel=At<void*>(call.object,g.panel);if(!panel||!R::IsLive(panel)||R::ClassOf(panel)!=g.panelCls)return;
   auto&sw=At<RawArray<uint8_t>>(panel,g.switchesStates);auto&rs=At<RawArray<uint8_t>>(panel,g.rotatorStates);auto&grid=At<RawArray<RotatorCell>>(panel,g.rotatorGrid);
   if(sw.Num!=8||rs.Num!=9||grid.Num!=9||!sw.Data||!rs.Data||!grid.Data){UE_LOGW("generator_break: HOST capture refused unexpected schema key=%s switches=%d rotators=%d grid=%d",key.c_str(),sw.Num,rs.Num,grid.Num);return;}
   PutKey(p.key,key);p.revision=++g_hostRevision[key];p.cycle=At<int32_t>(call.object,g.cycle);p.isBroken=ReadBool(call.object,g.broken)?1:0;
   p.sineOffset=At<int32_t>(panel,g.sineOffset);p.sineFrequency=At<int32_t>(panel,g.sineFrequency);p.sineAmplitude=At<int32_t>(panel,g.sineAmplitude);
   p.targetSineOffset=At<int32_t>(panel,g.targetSineOffset);p.targetSineFrequency=At<int32_t>(panel,g.targetSineFrequency);p.targetSineAmplitude=At<int32_t>(panel,g.targetSineAmplitude);
   p.switchesTarget=At<uint8_t>(panel,g.switchesTarget);p.completionBits=(ReadBool(panel,g.sineComplete)?1:0)|(ReadBool(panel,g.switchesComplete)?2:0)|(ReadBool(panel,g.rotatorsComplete)?4:0);
   for(int i=0;i<8;++i)if(sw.Data[i])p.switchesLiveBits|=static_cast<uint8_t>(1u<<i);
   std::memcpy(p.rotatorStates,rs.Data,9);std::memcpy(p.rotatorGrid,grid.Data,36);
   UE_LOGI("generator_break: HOST capture key=%s rev=%u cycle=%d puzzle=sine(%d,%d,%d)->(%d,%d,%d) switches=%02X/%02X",key.c_str(),p.revision,p.cycle,p.sineOffset,p.sineFrequency,p.sineAmplitude,p.targetSineOffset,p.targetSineFrequency,p.targetSineAmplitude,p.switchesLiveBits,p.switchesTarget);
   if(!s->SendReliable(coop::net::ReliableKind::GeneratorBreakState,&p,sizeof(p)))
    UE_LOGW("generator_break: HOST send failed key=%s rev=%u",key.c_str(),p.revision);
  });UE_LOGI("generator_break: exact generator_C::break POST watch %s",g_watch?"armed":"FAILED");}
 else if(++g_attempts>=5)g_failed=true;
}

bool Capture(void*gen,coop::net::GeneratorBreakStatePayload&p,bool bump){
 if(!g_ready||!gen||R::ClassOf(gen)!=g.genCls||!ReadBool(gen,g.broken))return false;
 std::string key=Identity(gen);if(key.empty())return false;
 void*panel=At<void*>(gen,g.panel);if(!panel||!R::IsLive(panel)||R::ClassOf(panel)!=g.panelCls)return false;
 auto&sw=At<RawArray<uint8_t>>(panel,g.switchesStates);auto&rs=At<RawArray<uint8_t>>(panel,g.rotatorStates);auto&grid=At<RawArray<RotatorCell>>(panel,g.rotatorGrid);if(sw.Num!=8||rs.Num!=9||grid.Num!=9||!sw.Data||!rs.Data||!grid.Data)return false;
 PutKey(p.key,key);uint32_t&rev=g_hostRevision[key];if(bump||!rev)++rev;p.revision=rev;p.cycle=At<int32_t>(gen,g.cycle);p.isBroken=1;
 p.sineOffset=At<int32_t>(panel,g.sineOffset);p.sineFrequency=At<int32_t>(panel,g.sineFrequency);p.sineAmplitude=At<int32_t>(panel,g.sineAmplitude);p.targetSineOffset=At<int32_t>(panel,g.targetSineOffset);p.targetSineFrequency=At<int32_t>(panel,g.targetSineFrequency);p.targetSineAmplitude=At<int32_t>(panel,g.targetSineAmplitude);p.switchesTarget=At<uint8_t>(panel,g.switchesTarget);p.completionBits=(ReadBool(panel,g.sineComplete)?1:0)|(ReadBool(panel,g.switchesComplete)?2:0)|(ReadBool(panel,g.rotatorsComplete)?4:0);for(int i=0;i<8;++i)if(sw.Data[i])p.switchesLiveBits|=static_cast<uint8_t>(1u<<i);std::memcpy(p.rotatorStates,rs.Data,9);std::memcpy(p.rotatorGrid,grid.Data,36);return true;
}
void*Find(const std::string&key){for(void*o:R::FindObjectsByClass(L"generator_C"))if(o&&R::IsLive(o)&&R::ClassOf(o)==g.genCls&&Identity(o)==key)return o;return nullptr;}
bool Call0(void*o,void*f){ue_wrap::ParamFrame p(f);return p.valid()&&ue_wrap::Call(o,p);}
bool Apply(const coop::net::GeneratorBreakStatePayload&p){
 std::string key=Key(p.key);void*gen=Find(key);if(!gen)return false;void*panel=At<void*>(gen,g.panel);if(!panel||!R::IsLive(panel)||R::ClassOf(panel)!=g.panelCls)return false;
 auto&sw=At<RawArray<uint8_t>>(panel,g.switchesStates);auto&rs=At<RawArray<uint8_t>>(panel,g.rotatorStates);auto&grid=At<RawArray<RotatorCell>>(panel,g.rotatorGrid);if(sw.Num!=8||rs.Num!=9||grid.Num!=9||!sw.Data||!rs.Data||!grid.Data){UE_LOGW("generator_break: CLIENT apply refused unexpected schema key=%s",key.c_str());return true;}
 // Static break audit proves this verb only sets broken/cycle, plays the shutdown cue, updates
 // power, and randomizes this panel. We immediately replace every randomized field below.
 bool callsOk=Call0(gen,g.breakFn);WriteBool(gen,g.broken,p.isBroken!=0);At<int32_t>(gen,g.cycle)=p.cycle;
 At<int32_t>(panel,g.sineOffset)=p.sineOffset;At<int32_t>(panel,g.sineFrequency)=p.sineFrequency;At<int32_t>(panel,g.sineAmplitude)=p.sineAmplitude;At<int32_t>(panel,g.targetSineOffset)=p.targetSineOffset;At<int32_t>(panel,g.targetSineFrequency)=p.targetSineFrequency;At<int32_t>(panel,g.targetSineAmplitude)=p.targetSineAmplitude;At<uint8_t>(panel,g.switchesTarget)=p.switchesTarget;
 for(int i=0;i<8;++i)sw.Data[i]=(p.switchesLiveBits&(1u<<i))?1:0;
 std::memcpy(rs.Data,p.rotatorStates,9);std::memcpy(grid.Data,p.rotatorGrid,36);
 callsOk=Call0(panel,g.setRotatorsFn)&&callsOk;callsOk=Call0(panel,g.setKnobsFn)&&callsOk;callsOk=Call0(panel,g.setSwitchesFn)&&callsOk;
 WriteBool(panel,g.sineComplete,(p.completionBits&1)!=0);WriteBool(panel,g.switchesComplete,(p.completionBits&2)!=0);WriteBool(panel,g.rotatorsComplete,(p.completionBits&4)!=0);callsOk=Call0(gen,g.updateFn)&&callsOk;
 uint8_t liveBits=0;for(int i=0;i<8;++i)if(sw.Data[i])liveBits|=static_cast<uint8_t>(1u<<i);
 uint8_t complete=(ReadBool(panel,g.sineComplete)?1:0)|(ReadBool(panel,g.switchesComplete)?2:0)|(ReadBool(panel,g.rotatorsComplete)?4:0);
 bool verified=callsOk&&ReadBool(gen,g.broken)==(p.isBroken!=0)&&At<int32_t>(gen,g.cycle)==p.cycle&&
  At<int32_t>(panel,g.sineOffset)==p.sineOffset&&At<int32_t>(panel,g.sineFrequency)==p.sineFrequency&&At<int32_t>(panel,g.sineAmplitude)==p.sineAmplitude&&
  At<int32_t>(panel,g.targetSineOffset)==p.targetSineOffset&&At<int32_t>(panel,g.targetSineFrequency)==p.targetSineFrequency&&At<int32_t>(panel,g.targetSineAmplitude)==p.targetSineAmplitude&&
  At<uint8_t>(panel,g.switchesTarget)==p.switchesTarget&&liveBits==p.switchesLiveBits&&complete==p.completionBits&&
  std::memcmp(rs.Data,p.rotatorStates,9)==0&&std::memcmp(grid.Data,p.rotatorGrid,36)==0;
 UE_LOGI("generator_break: CLIENT apply key=%s rev=%u cycle=%d verified=%d",key.c_str(),p.revision,p.cycle,verified?1:0);return true;
}
} // namespace

void Install(coop::net::Session*s){g_session.store(s,std::memory_order_release);SG::SetEnabled(true);}
void Tick(){if(!GT::IsGameThread())return;Resolve();uint64_t now=NowMs();for(auto it=g_pending.begin();it!=g_pending.end();){if(now>it->expiresMs){UE_LOGW("generator_break: pending apply expired key=%s",Key(it->p.key).c_str());it=g_pending.erase(it);}else if(Apply(it->p))it=g_pending.erase(it);else++it;}}
void OnReliable(const coop::net::GeneratorBreakStatePayload&p,uint8_t senderSlot){if(!GT::IsGameThread())return;auto*s=g_session.load(std::memory_order_acquire);if(!s||s->role()==coop::net::Role::Host||senderSlot!=0){UE_LOGW("generator_break: refused non-host/host-loop packet sender=%u",senderSlot);return;}Resolve();std::string key=Key(p.key);if(key.empty()||p.isBroken!=1||p.revision==0)return;uint32_t&last=g_clientRevision[key];if(p.revision<=last){UE_LOGI("generator_break: stale key=%s rev=%u last=%u ignored",key.c_str(),p.revision,last);return;}last=p.revision;for(auto it=g_pending.begin();it!=g_pending.end();)it=Key(it->p.key)==key?g_pending.erase(it):++it;if(Apply(p))return;if(g_pending.size()>=kMaxPending)g_pending.pop_front();g_pending.push_back(Pending{p,NowMs()+kPendingTtlMs});UE_LOGI("generator_break: queued key=%s rev=%u until world object resolves",key.c_str(),p.revision);}
void SendJoinSnapshotForSlot(int slot){auto*s=g_session.load(std::memory_order_acquire);if(!s||s->role()!=coop::net::Role::Host||!s->connected())return;Resolve();if(!g_ready)return;size_t n=0;for(void*o:R::FindObjectsByClass(L"generator_C")){coop::net::GeneratorBreakStatePayload p{};if(Capture(o,p,false)&&s->SendReliableToSlot(slot,coop::net::ReliableKind::GeneratorBreakState,&p,sizeof(p)))++n;}UE_LOGI("generator_break: HOST join snapshot slot=%d broken=%zu",slot,n);}
void OnDisconnect(){g_pending.clear();g_hostRevision.clear();g_clientRevision.clear();g_breakArmed.clear();g_session.store(nullptr,std::memory_order_release);}
} // namespace coop::generator_break_sync
