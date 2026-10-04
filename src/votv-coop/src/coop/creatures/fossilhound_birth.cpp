#include "coop/creatures/fossilhound_birth.h"
#include "coop/net/protocol.h"
#include "ue_wrap/core/reflection.h"
#include <cmath>

namespace coop::fossilhound_birth { namespace { namespace R=ue_wrap::reflection;
int32_t GibOffset(void*actor){static void*cls=nullptr;static int32_t off=-1;if(!actor)return -1;void*c=R::ClassOf(actor);if(!c||R::ToString(R::NameOf(c))!=L"fossilhound_C")return -1;if(c!=cls){cls=c;off=R::FindPropertyOffset(c,L"gibLifespan");}return off;}
}
void Capture(void*actor,coop::net::EntitySpawnPayload&p){const int32_t off=GibOffset(actor);if(off<0)return;const float v=*reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(actor)+off);if(std::isfinite(v)&&v>=0.f&&v<=86400.f){p.fossilhoundGibLifespan=v;p.hasFossilhoundGibLifespan=1;}}
void ApplyBeforeFinish(void*actor,const coop::net::EntitySpawnPayload&p){if(!p.hasFossilhoundGibLifespan||!std::isfinite(p.fossilhoundGibLifespan)||p.fossilhoundGibLifespan<0.f||p.fossilhoundGibLifespan>86400.f)return;const int32_t off=GibOffset(actor);if(off>=0)*reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(actor)+off)=p.fossilhoundGibLifespan;}
}
