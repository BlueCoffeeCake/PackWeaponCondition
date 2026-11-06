// Standalone Windows harness. No game process, game launch, or save is used.
#include "PackWeaponCondition.cpp"
#include <assert.h>
static unsigned cases=0;
#define CHECK(x) do{++cases;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static void* fakePlayer;
static void* fakeOwner;
static void* fakeLocation;
static unsigned fakeRank;
static unsigned fires,equips,queueCalls;
static bool jammed,energyFault;
static U32 lookupIds[32];static uint8_t lookupForms[32][64];
static void* mockPlayer(){return fakePlayer;}
static void* mockLookup(U32 id){for(unsigned i=0;i<32;i++)if(lookupIds[i]==id)return lookupForms[i];return nullptr;}
static uint8_t mockRank(void*,void* perk){
 if(perk==accuracyPerk)return 1;
 unsigned i=unsigned(((uint8_t*)perk-(uint8_t*)lookupForms)/64);
 return i<11&&i%4<fakeRank?1:0;
}
static void* mockOwner(void*){return fakeOwner;}
static void* mockLocation(void*){return fakeLocation;}
static void* mockHealth(void* extra){return extra;}
static void mockSet(void* extra,float x){write(extra,0x18,x);}
static void mockWake(void*,void* stack,bool){if(auto h=healthOf(stack))write(h,0x18,.075f);}
static float mockWear(void*,void* stack,float damage,float maximum){float c=condition(stack);float n=Policy::clamp(c-damage/maximum,0,1);setCondition(stack,n);return n;}
static void mockFire(const void*,void*,U32,void*,void*){fires++;}
static bool mockEquip(void*,void*,const void*,U32,U32,const void*,bool,bool,bool,bool,bool){equips++;return true;}
static void mockQueue(void*,void*,void*,U32,void*){queueCalls++;}
static bool mockJam(){return jammed;}
static bool mockEnergy(){return energyFault;}
static void mockSettings(void*){}
static char captured[128];
static void capture(void*,const Card* c){strcpy(captured,c->valueText?c->valueText:"");}
static void mockCard(Update* msg){Card c={};c.label="$condition";c.fillPct=.2f;msg->addCard(nullptr,&c);}
static void jump(void* p,void* f){uint8_t b[]={0x48,0xb8,0,0,0,0,0,0,0,0,0xff,0xe0};memcpy(b+2,&f,8);memcpy(p,b,sizeof b);FlushInstructionCache(GetCurrentProcess(),p,sizeof b);}
int main(){
 CHECK(Policy::tier(1)==Policy::Pristine);CHECK(Policy::tier(.90f)==Policy::Pristine);CHECK(Policy::tier(.899f)==Policy::Worn);
 CHECK(Policy::tier(.65f)==Policy::Worn);CHECK(Policy::tier(.649f)==Policy::Damaged);CHECK(Policy::tier(.35f)==Policy::Damaged);CHECK(Policy::tier(.349f)==Policy::BadlyDamaged);CHECK(Policy::tier(0)==Policy::Ruined);
 CHECK(Policy::tier(.0000001f)==Policy::Ruined);CHECK(Policy::tier(-1)==Policy::Ruined);
 for(int family=0;family<4;family++)for(int rank=0;rank<=4;rank++){
  float c=Policy::cap((Policy::Family)family,rank);CHECK(c>=.75&&c<=1);CHECK(rank==0?c==.75f:true);
 }
 CHECK(Policy::cap(Policy::Energy,4)==1);CHECK(Policy::cap(Policy::Melee,3)==1);CHECK(Policy::chance(.91f,1)==0);
 CHECK(Policy::chance(.34f,1)>Policy::chance(.64f,1));CHECK(Policy::chance(.64f,1)>Policy::chance(.80f,1));
 CHECK(Policy::chance(.1f,3)==0);CHECK(Policy::chance(.1f,4)==0);CHECK(Policy::chance(.1f,2)>0);
 for(unsigned i=0;i<=10000;i++){
  float x=i/10000.f;CHECK(Policy::cost(12,x,true)>=Policy::cost(12,x,false));CHECK(Policy::cost(12,0,false)==0);
 }
 CHECK(Policy::wear(1,true,Policy::Ballistic)>1);CHECK(Policy::wear(1,true,Policy::Melee)==1);
 CHECK(Policy::initial(2).lo>Policy::initial(3).lo);CHECK(Policy::initial(5).lo>Policy::initial(4).hi);
 // Mock only the narrow external engine/framework interfaces; execute the actual
 // adapter's costs, equip/fire gates, persistence checks and item-card callback.
 csf=(uintptr_t)VirtualAlloc(nullptr,0x200000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
 game=(uintptr_t)VirtualAlloc(nullptr,0x1000000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);CHECK(csf&&game);
 jump((void*)(csf+R_GetPlayer),(void*)mockPlayer);jump((void*)(csf+R_IsJammed),(void*)mockJam);jump((void*)(csf+R_IsEnergyFault),(void*)mockEnergy);
 jump((void*)(game+G_PerkRank),(void*)mockRank);jump((void*)(game+G_Owner),(void*)mockOwner);jump((void*)(game+G_Location),(void*)mockLocation);jump((void*)(game+0x152c90),(void*)mockLookup);
 uint8_t actor[0x400]={},list[0x80]={},items[16]={},stack[0x28]={},extra[0x20]={},obj[0x40]={};
 fakePlayer=actor;write(actor,0xf8,(void*)list);write(list,0x58,(void*)items);write(list,0x60,U32(1));write(list,0x68,U32(1));write(items,0,(void*)obj);write(items,8,(void*)stack);write(stack,0x18,(void*)extra);write(stack,0x24,uint16_t(1));write(obj,0x1a,uint8_t(0x2b));
 for(unsigned i=0;i<sizeof(weapons)/sizeof(*weapons);i++)weapons[i].runtime=i+1;
 write(obj,0x14,U32(1));weapons[0].form=obj;weapons[0].family=Policy::Ballistic;weapons[0].weight=8;
 getHealth=mockHealth;setHealth=mockSet;csfNew=malloc;originalWake=mockWake;originalWear=mockWear;originalFire=mockFire;originalEquip=mockEquip;originalQueue=mockQueue;originalSettings=mockSettings;originalCard=mockCard;
 dataReady=bound=packEnabled=sessionLive=true;float tierValue=0,readyValue=0;tierGlobal=&tierValue;readyGlobal=&readyValue;accuracyPerk=(void*)0x1234;
 const U32 perks[]={0x4a0da,0x4a0db,0x4a0dc,0x16578e,0x264d9,0x264da,0x264db,0x16578f,0x4b253,0x4b26a,0x264d8};
 for(unsigned i=0;i<11;i++)lookupIds[i]=perks[i];for(unsigned i=0;i<11;i++)components[i]=lookupForms[11+i];
 setCondition(stack,.50f);fakeRank=0;CHECK(repairCap(obj,nullptr)==.75f);
 Materials v;costs(&v,obj,stack,.5f);CHECK(v.last-v.first==5);unsigned normalSteel=v.first->count;
 for(auto m=v.first;m!=v.last;m++){CHECK(m->name.capacity==15);CHECK(m->count>0);CHECK(m->name.length==strlen(m->name.text));}free(v.first);
 setCondition(stack,0);CHECK(read<float>(extra,0x18)>0);CHECK(condition(stack)==0);costs(&v,obj,stack,.75f);CHECK(v.first->count>normalSteel*2);free(v.first);
 void* instance[]={obj,nullptr};CHECK(!equip(nullptr,actor,instance,0,1,nullptr,false,false,false,false,false));CHECK(equips==0);
 fire(instance,actor,0,nullptr,nullptr);CHECK(fires==0);queueFire(nullptr,obj,actor,0,nullptr);CHECK(queueCalls==0);
 uint8_t npc[0x400]={};fire(instance,npc,0,nullptr,nullptr);CHECK(fires==1);CHECK(equip(nullptr,npc,instance,0,1,nullptr,false,false,false,false,false));CHECK(equips==1);
 setCondition(stack,.80f);fire(instance,actor,0,nullptr,nullptr);CHECK(fires==2);CHECK(equip(nullptr,actor,instance,0,1,nullptr,false,false,false,false,false));CHECK(equips==2);
 costs(&v,obj,stack,.2f);CHECK(v.first==nullptr);CHECK(condition(stack)>.79f); // no charge/lowering above cap
 jammed=true;fire(instance,actor,0,nullptr,nullptr);CHECK(fires==2);jammed=false;energyFault=true;fire(instance,actor,0,nullptr,nullptr);CHECK(fires==2);energyFault=false;
 fakeRank=4;CHECK(repairCap(obj,nullptr)==1);setCondition(stack,.50f);weapons[0].family=Policy::Energy;costs(&v,obj,stack,.5f);CHECK(v.last-v.first==8);free(v.first);
 weapons[0].family=Policy::Melee;fakeRank=3;CHECK(repairCap(obj,nullptr)==1);costs(&v,obj,stack,.5f);CHECK(v.last-v.first==3);free(v.first);weapons[0].family=Policy::Ballistic;
 // Existing condition is never rerolled by pickup/equip/inspection initialization.
 for(unsigned i=0;i<100;i++){setCondition(stack,.4321f);wake(obj,stack,true);CHECK(fabsf(condition(stack)-.4321f)<.00001f);}
 for(int c=0;c<6;c++){lootContext=c;setCondition(stack,1);write(extra,0x18,1.f);wake(obj,stack,true);auto r=Policy::initial(c);CHECK(condition(stack)>=r.lo&&condition(stack)<=r.hi);}lootContext=-1;
 // Same base form, separate stack: wearing one does not modify the other.
 uint8_t stack2[0x28]={},extra2[0x20]={};write(stack2,0x18,(void*)extra2);setCondition(stack,.7f);setCondition(stack2,.7f);burst={};wear(obj,stack,10,1000);CHECK(condition(stack)<.7f);CHECK(condition(stack2)>.699f);
 auto before=condition(stack);write(actor,0xf8,(void*)nullptr);wear(obj,stack,100,1000);CHECK(condition(stack)==before);write(actor,0xf8,(void*)list);
 setCondition(stack,.2f);updateTier();CHECK(tierValue==3&&readyValue==1);
 Update msg={};msg.itemForm=obj;msg.addCard=capture;card(&msg);CHECK(!strcmp(captured,"Badly Damaged - 20%"));
 settings(nullptr);CHECK(read<uint8_t>((void*)(csf+R_MCM),M_enableWeaponCondition)==1);CHECK(read<uint8_t>((void*)(csf+R_MCM),M_enableArmorCondition)==0);CHECK(read<uint8_t>((void*)(csf+R_MCM),M_weaponConditionAffectsDamage)==0);
 dataReady=false;policy();CHECK(read<uint8_t>((void*)(csf+R_MCM),M_enableWeaponCondition)==0);updateTier();CHECK(readyValue==0);
 PluginInfo info={};F4SEInterface api={};api.runtimeVersion=0x010a0a30;CHECK(F4SEPlugin_Query(&api,&info));api.runtimeVersion=0x010a0a40;CHECK(!F4SEPlugin_Query(&api,&info));api.runtimeVersion=0x010a0a30;api.isEditor=1;CHECK(!F4SEPlugin_Query(&api,&info));
 printf("PASS %u assertions: tier boundaries, caps, repair costs, per-stack state, player/NPC gates, queued firing, faults, card ABI, fail-closed policy. No engine execution.\n",cases);
 VirtualFree((void*)game,0,MEM_RELEASE);VirtualFree((void*)csf,0,MEM_RELEASE);return 0;
}
