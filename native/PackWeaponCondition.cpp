// Pack-owned, source-available adapter for the EXACT CSF binary supplied with
// this pack. No author DLL is edited. Published PDB layouts and entry bytes are
// verified offline; runtime/version/hash guards precede all hooks.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <stddef.h>
#include "MinHook.h"
#include "ConditionPolicy.h"
#include "Bindings.h"
#include "EnvironmentPolicy.h"
#include "EnvironmentBindings.h"
using U32=uint32_t;
struct F4SEInterface{U32 f4seVersion,runtimeVersion,editorVersion,isEditor;void*(*QueryInterface)(U32);U32(*GetPluginHandle)();U32(*GetReleaseIndex)();};
struct PluginInfo{U32 version;const char* name;U32 pluginVersion;};
struct Messaging{struct Message{const char* sender;U32 type,dataLen;void* data;};U32 version;bool(*RegisterListener)(U32,const char*,void(*)(Message*));bool(*Dispatch)(U32,U32,void*,U32,const char*);void*(*GetEventDispatcher)(U32);};
struct Array{void* data;U32 capacity,pad,count,pad2;};
struct Weapon{const char* plugin;U32 local;int family;float weight;int repairStyle;U32 runtime;void* form;};
struct Context{const char* plugin;U32 local;int context;U32 runtime;void* form;};
#include "WeaponCatalog.h"
#include "Contexts.h"
#include "RadstormCatalog.h"
template<class T>T read(const void* p,size_t o){T x;memcpy(&x,(const uint8_t*)p+o,sizeof x);return x;}
template<class T>void write(void* p,size_t o,T x){memcpy((uint8_t*)p+o,&x,sizeof x);}
static uintptr_t game,csf;
static FILE* logfile;
static bool bound=false,dataReady=false,sessionLive=false,packEnabled=true;
static float *tierGlobal,*readyGlobal;
static void* accuracyPerk;
static wchar_t iniPath[32768];
static uint32_t seed=0x5bda1234;
static bool environmentReady=false;
static float *rainLossGlobal,*stormLossGlobal,*swimLossGlobal;
static EnvironmentPolicy::Clock environmentClock;
static void log(const char* s){if(logfile){fprintf(logfile,"%s\n",s);fflush(logfile);}}
static void* player(){return csf?((void*(*)())(csf+R_GetPlayer))():nullptr;}
static void* form(U32 id){return id?((void*(*)(U32))(game+0x152c90))(id):nullptr;}
static U32 resolve(const char* name,U32 local){
 auto data=read<void*>((void*)(game+0x58cf080),0);if(!data)return 0;
 const size_t lists[]={0xfc0,0xfd8};for(size_t off:lists){
  auto a=read<Array>(data,off);if(a.count>4096||a.count>a.capacity||(a.count&&!a.data))return 0;
  for(U32 i=0;i<a.count;i++){
   void* mod=read<void*>(a.data,i*8);if(!mod)continue;
   const char* n=(const char*)mod+0x70;if(!memchr(n,0,260)||_stricmp(n,name))continue;
   auto idx=read<uint8_t>(mod,0x370);if(idx==0xff)return 0;
   if(read<U32>(mod,0x334)&0x200){auto light=read<uint16_t>(mod,0x372);return light<4096&&local<=0xfff?(0xfe000000u|(U32(light)<<12)|local):0;}
   return local<=0xffffff?(U32(idx)<<24)|local:0;
  }
 }
 return 0;
}
static Weapon* metadata(void* object){
 if(!object||read<uint8_t>(object,0x1a)!=0x2b||!dataReady)return nullptr;
 U32 id=read<U32>(object,0x14);size_t lo=0,hi=sizeof(weapons)/sizeof(*weapons);
 while(lo<hi){size_t m=(lo+hi)/2;if(weapons[m].runtime<id)lo=m+1;else hi=m;}
 return lo<sizeof(weapons)/sizeof(*weapons)&&weapons[lo].runtime==id?&weapons[lo]:nullptr;
}
static auto getHealth=(void*(*)(void*))nullptr;
static auto setHealth=(void(*)(void*,float))nullptr;
static auto getEquipped=(float(*)())nullptr;
static auto csfNew=(void*(*)(size_t))nullptr;
static void* healthOf(void* stack){auto e=stack?read<void*>(stack,0x18):nullptr;return e?getHealth(e):nullptr;}
static float condition(void* stack){auto h=healthOf(stack);if(!h||read<float>(h,0x18)>=.9f)return -1;float c=Policy::clamp(read<float>(h,0x18)*10,0,1);return c<=.000001f?0:c;}
// A positive engine sentinel preserves NPC usability of a traded ruined gun.
// Player firing/equipping is blocked by the native gates, not by NPC engine health.
static void setCondition(void* stack,float c){auto e=stack?read<void*>(stack,0x18):nullptr;if(!e)return;float v=c<=.000001f?.00000001f:Policy::clamp(c,0,1)*.1f;setHealth(e,v);if(auto h=getHealth(e))write(h,0x18,v);}
static void* findStack(void* actor,void* obj,U32 index,bool equipped=false){
 auto list=actor?read<void*>(actor,0xf8):nullptr;if(!list)return nullptr;
 auto a=read<Array>(list,0x58);if(a.count>16384||a.count>a.capacity)return nullptr;
 for(U32 i=0;i<a.count;i++){
  auto item=(uint8_t*)a.data+i*16;auto object=read<void*>(item,0);
  if(obj&&object!=obj)continue;if(!metadata(object))continue;
  U32 j=0;for(auto s=read<void*>(item,8);s&&j<16384;s=read<void*>(s,0x10),j++){
   if(equipped){if(read<uint16_t>(s,0x24)&7)return s;}
   else if(index==j||index==0xffffffff)return s;
  }
 }
 return nullptr;
}
static bool equippedRuined(){auto s=findStack(player(),nullptr,0,true);return s&&condition(s)==0;}
static unsigned rank(void* p,const U32* ids,unsigned count){
 auto fn=(uint8_t(*)(void*,void*))(game+G_PerkRank);
 for(unsigned i=count;i>0;i--){auto perk=form(ids[i-1]);if(perk&&fn(p,perk)>0)return i;}
 return 0;
}
static float repairCap(void* obj,void*){
 auto w=metadata(obj);if(!w||!dataReady||!packEnabled)return 0;
 const U32 gun[]={0x4a0da,0x4a0db,0x4a0dc,0x16578e},science[]={0x264d9,0x264da,0x264db,0x16578f},blacksmith[]={0x4b253,0x4b26a,0x264d8};
 auto p=player();if(!p)return 0;
 auto f=(Policy::Family)w->family;
 unsigned r=f==Policy::Melee?rank(p,blacksmith,3):f==Policy::Energy?rank(p,science,4):rank(p,gun,4);
 return Policy::cap(f,r);
}
static void policy(){
 if(!csf||!bound)return;
 auto s=(uint8_t*)(csf+R_MCM);bool active=dataReady&&packEnabled;
 // CSF's General enable controls the widget only. These are the real gates.
 const size_t yes[]={M_enable,M_randomLootDurability,M_enableWeaponCondition,M_enableWorkbenchMaterialRepair,M_enableEnergyFault,M_showItemCardCND};for(auto off:yes)s[off]=active;
 const size_t no[]={M_enableArmorCondition,M_enableOverCondition,M_dialogueFullDurability,M_enableWeaponOverheat,M_enablePipboyJuryRepair,M_enableRepairKits,M_weaponConditionAffectsDamage,M_weaponConditionAffectsValue,M_armorConditionAffectsResistance,M_armorConditionAffectsValue};for(auto off:no)s[off]=0;
 write(s,M_widgetBackend,0);write(s,M_usePrismaUI,uint8_t(0));write(s,M_unjamWidgetBackend,1);write(s,M_heatWidgetBackend,1);
 write(s,M_jamThreshold,.90f);write(s,M_maxJamChance,.035f);write(s,M_energyFaultDuration,2.f);
 write(s,M_weaponWearMultiplier,1.f);write(s,M_workbenchRepairCap,1.f);write(s,M_juryRepairCap,0.f);
 write(s,M_iJammingPhase,0);write(s,M_penaltyMultMid,1.f);write(s,M_penaltyMultLow,1.f);write(s,M_linearMinMult,1.f);
}
static uint16_t(*originalFlags)(void*,bool);
static uint16_t flags(void* obj,bool applySwitch){
 if(obj){auto t=read<uint8_t>(obj,0x1a);if(t==0x2b)return metadata(obj)&&packEnabled?0x101:0;if(t==0x1d)return 0;}
 return originalFlags(obj,applySwitch);
}
static void(*originalSettings)(void*);
static void settings(void* self){originalSettings(self);policy();}
// All strings use the exact 32-byte MSVC SSO layout. Allocate vector storage
// through CSF's own operator new; its ordinary vector destructor owns the result.
struct MSString{char text[16];size_t length,capacity;};
struct Material{void* component;U32 count,pad;MSString name;};
struct Materials{Material* first;Material* last;Material* end;};
static_assert(sizeof(MSString)==32&&sizeof(Material)==48&&offsetof(Material,name)==16&&sizeof(Materials)==24);
static const U32 componentIds[]={0x1fabd,0x3d294,0x1fabc,0x1fab4,0x1faa5,0x1fa9b,0x1fa9c,0x1fa9f,0x1fac2,0x1faae,0x1223c7};
static const char* componentNames[]={"Steel","Screws","Springs","Oil","Adhesive","Circuitry","Copper","Crystal","Wood","Leather","Cloth"};
static void* components[11];
static Materials* costs(Materials* out,void* obj,void* stack,float delta){
 out->first=out->last=out->end=nullptr;auto w=metadata(obj);if(!w||!stack||!isfinite(delta)||delta<=.001f)return out;
 float c=condition(stack),cap=repairCap(obj,nullptr);if(c<0||c>=cap-.001f)return out;
 // Always recompute the delta for this exact instance, never trust a stale UI value.
 delta=cap-c;bool ruined=c<=0;
 float scale=Policy::clamp(w->weight/8.f,.65f,3.f);
 float budgets[11]={12,3,3,2,3,0,0,0,0,0,0};
 if(w->family==Policy::Energy){budgets[5]=3;budgets[6]=4;budgets[7]=2;}
 if(w->family==Policy::Melee&&w->repairStyle!=3){
  float m[]={8,2,0,0,3,0,0,0,0,0,0};
  if(w->repairStyle==1){m[0]=3;m[1]=0;m[8]=8;}
  if(w->repairStyle==2){m[0]=m[1]=0;m[9]=4;m[10]=6;}
  memcpy(budgets,m,sizeof m);
 }
 unsigned counts[11]={},n=0;
 for(unsigned i=0;i<11;i++)if(budgets[i]>0){counts[i]=Policy::cost(budgets[i]*scale,delta,ruined);if(counts[i])n++;}
 if(!n)return out;auto data=(Material*)csfNew(n*sizeof(Material));if(!data)return out;
 memset(data,0,n*sizeof(Material));unsigned j=0;
 for(unsigned i=0;i<11;i++)if(counts[i]){auto& m=data[j++];m.component=components[i];m.count=counts[i];strcpy(m.name.text,componentNames[i]);m.name.length=strlen(componentNames[i]);m.name.capacity=15;}
 out->first=data;out->last=out->end=data+n;return out;
}
static float random01(){unsigned x;do{x=seed;unsigned next=x^x<<13;next^=next>>17;next^=next<<5;if(InterlockedCompareExchange((volatile LONG*)&seed,(LONG)next,(LONG)x)==(LONG)x)return (next&0xffffff)/16777216.f;}while(true);}
static bool roll(float c,int mechanism){return dataReady&&packEnabled&&random01()<Policy::chance(c,mechanism);}
static thread_local int lootContext=-1;
struct Pending{U32 id;int context;ULONGLONG until;};
static Pending pending[64];static SRWLOCK pendingLock=SRWLOCK_INIT;
static int sourceContext(void* ref){
 if(!ref)return 0;if(ref==player())return 0;
 if(read<uint8_t>(ref,0x1a)==0x41){
  auto vf=read<void**>(ref,0);auto isFaction=(bool(*)(void*,void*))vf[0x10a];
  for(auto& f:factions)if(f.form&&isFaction(ref,f.form))return f.context;
  return 1;
 }
 auto cell=read<void*>(ref,0xb8);
 for(auto& s:sealedCells)if(cell==s.form)return 5;
 auto owner=((void*(*)(void*))(game+G_Owner))(ref);
 for(auto& f:factions)if(owner&&owner==f.form)return f.context;
 auto location=((void*(*)(void*))(game+G_Location))(ref);
 for(auto& l:locations)if(location==l.form)return l.context;
 return owner?1:4;
}
static int pendingContext(U32 id,int fallback){
 int out=fallback;ULONGLONG now=GetTickCount64();AcquireSRWLockShared(&pendingLock);
 for(auto& p:pending)if(p.id==id&&p.until>now)out=p.context;
 ReleaseSRWLockShared(&pendingLock);return out;
}
static void(*originalWake)(void*,void*,bool);
static void wake(void* obj,void* stack,bool loot){
 if(!metadata(obj)||!stack)return;
 bool fresh=condition(stack)<0;if(!fresh)return;originalWake(obj,stack,loot);
 if(healthOf(stack)){
  int context=lootContext<0?pendingContext(read<U32>(obj,0x14),loot?4:0):lootContext;
  auto range=Policy::initial(context);setCondition(stack,range.lo+(range.hi-range.lo)*random01());
 }
}
static void(*originalInventory)(void*);
static void inventory(void* ref){int prior=lootContext;lootContext=sourceContext(ref);originalInventory(ref);lootContext=prior;}
static void(*originalWorld)(void*,void*);
static void world(void* ref,void* obj){
 // World references have a separate ExtraDataList; the framework initializes it
 // without visiting WakeUpAndRandomizeSingleStack.
 if(!metadata(obj))return;auto extra=ref?read<void*>(ref,0x100):nullptr;auto h=extra?getHealth(extra):nullptr;
 bool fresh=!h||read<float>(h,0x18)>=.9f;originalWorld(ref,obj);
 extra=ref?read<void*>(ref,0x100):nullptr;h=extra?getHealth(extra):nullptr;
 if(fresh&&h){auto r=Policy::initial(sourceContext(ref));float v=(r.lo+(r.hi-r.lo)*random01())*.1f;setHealth(extra,v);if(auto hh=getHealth(extra))write(hh,0x18,v);}
}
static int(*originalPickup)(void*,const void*,void*);
static int pickup(void* self,const void* event,void* source){
 if(event&&read<U32>(event,4)==0x14&&read<int>(event,12)>0){
  U32 id=read<U32>(event,8);if(metadata(form(id))){
   int c=sourceContext(form(read<U32>(event,0)));ULONGLONG now=GetTickCount64();AcquireSRWLockExclusive(&pendingLock);
   unsigned i=0;for(unsigned j=0;j<64;j++)if(pending[j].id==id||pending[j].until<now){i=j;break;}
   pending[i]={id,c,now+5000};ReleaseSRWLockExclusive(&pendingLock);
  }
 }
 return originalPickup(self,event,source);
}
struct Burst{void* stack;ULONGLONG last;unsigned consecutive;};static Burst burst;
static float(*originalWear)(void*,void*,float,float);
static float wear(void* obj,void* stack,float damage,float maximum){
 auto w=metadata(obj);if(!w)return 1;
 // CSF calls this only for the player, but keep that invariant explicit.
 auto p=player();bool owned=false;auto list=p?read<void*>(p,0xf8):nullptr;
 if(list){auto a=read<Array>(list,0x58);if(a.count<=16384)for(U32 i=0;i<a.count&&!owned;i++)for(auto s=read<void*>((uint8_t*)a.data+i*16,8);s;s=read<void*>(s,0x10))if(s==stack){owned=true;break;}}
 if(!owned)return condition(stack);
 ULONGLONG now=GetTickCount64();burst.consecutive=burst.stack==stack&&now-burst.last<160?burst.consecutive+1:1;burst.stack=stack;burst.last=now;
 float out=originalWear(obj,stack,Policy::wear(damage,burst.consecutive>=8,(Policy::Family)w->family),maximum);
 if(out<=0)setCondition(stack,0);return out;
}
static void(*originalDepleted)(void*,void*,const char*,float);
static void depleted(void* actor,void* stack,const char* text,float health){
 originalDepleted(actor,stack,actor==player()?"Ruined - rebuild at a weapons workbench":text,actor==player()?.00000001f:health);
}
static bool(*originalEquip)(void*,void*,const void*,U32,U32,const void*,bool,bool,bool,bool,bool);
static bool equip(void* mgr,void* actor,const void* instance,U32 index,U32 count,const void* slot,bool q,bool force,bool sound,bool now,bool locked){
 auto obj=instance?read<void*>(instance,0):nullptr;
 if(dataReady&&packEnabled&&actor==player()&&metadata(obj)){
  if(auto s=findStack(actor,obj,index)){if(condition(s)==0)return false;}
 }
 return originalEquip(mgr,actor,instance,index,count,slot,q,force,sound,now,locked);
}
static void(*originalQueue)(void*,void*,void*,U32,void*);
static void queueFire(void* queue,void* weapon,void* actor,U32 index,void* ammo){
 if(dataReady&&packEnabled&&actor==player()&&metadata(weapon)){
  auto s=findStack(actor,weapon,0,true);if(!s)s=findStack(actor,weapon,0);
  if(s&&condition(s)==0)return;
 }
 originalQueue(queue,weapon,actor,index,ammo);
}
static void(*originalFire)(const void*,void*,U32,void*,void*);
static void fire(const void* instance,void* actor,U32 slot,void* ammo,void* extra){
 auto obj=instance?read<void*>(instance,0):nullptr;
 if(dataReady&&packEnabled&&actor==player()&&metadata(obj)){
  auto stack=findStack(actor,obj,0,true);if(!stack)stack=findStack(actor,obj,0);
  if(stack&&condition(stack)==0)return;
  if(((bool(*)())(csf+R_IsJammed))()||((bool(*)())(csf+R_IsEnergyFault))())return;
 }
 originalFire(instance,actor,slot,ammo,extra);
}
static void updateTier(){
 if(!readyGlobal||!tierGlobal)return;
 if(!dataReady||!sessionLive||!packEnabled){*readyGlobal=0;*tierGlobal=0;return;}
 auto s=findStack(player(),nullptr,0,true);int t=0;
 if(s){auto c=condition(s);if(c>=0)t=Policy::tier(c);}
 *tierGlobal=float(t);*readyGlobal=1;
 static unsigned frames=0;
 if(++frames%60==1&&accuracyPerk&&player()){
  if(((uint8_t(*)(void*,void*))(game+G_PerkRank))(player(),accuracyPerk)==0)((void(*)(void*,void*,uint8_t))(game+G_AddPerk))(player(),accuracyPerk,0);
 }
}
static void(*originalInput)(void*,const void*);
static bool conditionTrue(void* actor,uint16_t function){
 // Exact 0x38-byte TESConditionItem from the supplied release PDB/header.
 // Comparison==1, subject=self, no parameters, no linked condition chain.
 alignas(8) uint8_t item[0x38]={};write(item,8,1.f);write(item,0x18,function);
 return ((bool(*)(void*,void*,void*))(game+G_EnvConditionIsTrue))(item,actor,nullptr);
}
static EnvironmentPolicy::Weather describeWeather(void* weather){
 EnvironmentPolicy::Weather out;
 if(!weather||read<uint8_t>(weather,0x1a)!=0x39)return out;
 out.rain=(read<uint8_t>(weather,0xa77)&4)!=0;
 auto id=read<U32>(weather,0x14);
 for(auto& r:radstormWeather)if(r.runtime==id){out.storm=true;break;}
 return out;
}
static float* environmentGlobal(U32 id,const char* expected){
 auto f=form(resolve("PackWeaponCondition.esp",id));if(!f||read<uint8_t>(f,0x1a)!=0xb)return nullptr;
 auto ed=read<const char*>(f,0x20);return ed&&!strcmp(ed,expected)?(float*)((uint8_t*)f+0x30):nullptr;
}
static void initializeEnvironment(){
 environmentReady=false;environmentClock.reset();
 if(memcmp((void*)(game+G_EnvSkySingleton),E_EnvSkySingleton,16)||memcmp((void*)(game+G_EnvConditionIsTrue),E_EnvConditionIsTrue,16)){log("Weather extension inactive: environment function binding conflict. Base condition system retained.");return;}
 rainLossGlobal=environmentGlobal(0x803,"PackWeaponConditionRainLoss");stormLossGlobal=environmentGlobal(0x804,"PackWeaponConditionStormLoss");swimLossGlobal=environmentGlobal(0x805,"PackWeaponConditionSwimLoss");
 if(!rainLossGlobal||!stormLossGlobal||!swimLossGlobal){log("Weather extension inactive: required exposure globals missing.");return;}
 for(auto& r:radstormWeather){r.runtime=resolve(r.plugin,r.local);r.form=form(r.runtime);if(!r.form||read<uint8_t>(r.form,0x1a)!=0x39){log("Weather extension inactive: audited storm identity missing.");return;}}
 environmentReady=true;log("Weather extension ready: condition loss only, equipped weapon only; rain/radstorm/swimming; real active time; no additional faults.");
}
static void updateEnvironment(){
 ULONGLONG now=GetTickCount64();
 auto p=player();auto ui=csf?((void*(*)())(csf+R_EnvUI))():nullptr;
 DWORD foreground=0;auto window=GetForegroundWindow();if(window)GetWindowThreadProcessId(window,&foreground);
 bool active=environmentReady&&dataReady&&sessionLive&&packEnabled&&p&&read<void*>(p,0xb8)&&ui&&read<U32>(ui,0x1e0)==0&&read<U32>(ui,0x1f0)==0&&foreground==GetCurrentProcessId();
 double seconds=environmentClock.advance(now,active);if(seconds<=0)return;
 auto stack=findStack(p,nullptr,0,true);if(!stack)return;
 float before=condition(stack);if(before<=0)return; // no reroll or unknown-item initialization here
 if(conditionTrue(p,46)){environmentClock.reset();return;} // dead player
 bool swimming=conditionTrue(p,185),interior=conditionTrue(p,300);
 void* sky=((void*(*)())(game+G_EnvSkySingleton))();
 auto current=sky?read<void*>(sky,0x48):nullptr;auto outgoing=sky?read<void*>(sky,0x50):nullptr;
 double pct=sky?read<float>(sky,0x360):1.;
 EnvironmentPolicy::Rates rates{*rainLossGlobal,*stormLossGlobal,*swimLossGlobal};
 double rate=EnvironmentPolicy::rate(swimming,interior,describeWeather(current),describeWeather(outgoing),pct,outgoing!=nullptr,rates);
 double loss=EnvironmentPolicy::loss(rate,seconds);if(loss<=0)return;
 float after=(float)((double)before-loss);if(after<0)after=0;setCondition(stack,after);
 if(after<=0&&before>0)depleted(p,stack,"Ruined - rebuild at a weapons workbench",.00000001f);
 float points=((float(*)())(csf+R_EnvPoints))();
 ((void(*)(float,float,bool))(csf+R_EnvHUD))(after,points,false);
}
static void input(void* self,const void* head){
 updateEnvironment();updateTier();if(dataReady&&sessionLive&&packEnabled&&equippedRuined()){
  unsigned guard=0;for(auto e=head;e&&guard++<4096;e=read<void*>(e,0x18))if(read<int>(e,0x10)==0){
   auto name=read<const char*>(e,0x28);if(name&&(!strcmp(name,"PrimaryAttack")||!strcmp(name,"VATS")))write((void*)e,0x38,0.f);
  }
 }
 originalInput(self,head);
}
// Exact IIF 1.0.6/CSF 1.1.1 card ABI. The callback copies strings synchronously.
struct Card{const char* text;const char* label;bool highlightLabel;const char* value;float difference;bool hideDifference,invertDiffColor;int displayType;bool hasBackground;U32 backgroundColor;const char* sortValueFrom;float fillPct,shieldPct;U32 fillColor;float thresholdPct,thresholdPct2;U32 thresholdColor;bool showBar,showValue;const char* valueText;const char* valueAlign;U32 valueColor;bool valueStandard,valueBad,valueGood;const char* icon1;bool icon1IsText;const char* val1;bool val1Bad,val1Good,val1Standard;const char* align1;const char* icon2;bool icon2IsText;const char* val2;bool val2Bad,val2Good,val2Standard;const char* align2;};
struct Update{void* itemForm;void* inventoryItem;void* instanceData;void* gfxArray;void* movie;U32 stackIndex,itemHandleID;void* context;void(*addCard)(void*,const Card*);void(*modifyCard)(void*,const char*,double);};
static_assert(sizeof(Card)==200&&offsetof(Card,valueText)==96&&offsetof(Card,align2)==192&&sizeof(Update)==72&&offsetof(Update,addCard)==56);
static thread_local void(*cardCallback)(void*,const Card*);
static thread_local bool cardWeapon;
static void tierCard(void* ctx,const Card* c){
 if(cardCallback&&c){if(cardWeapon&&c->label&&!strcmp(c->label,"$condition")){
  Card out=*c;float pct=Policy::clamp(c->fillPct,0,1);char text[64];snprintf(text,sizeof text,"%s - %.0f%%",Policy::name(Policy::tier(pct)),pct*100);
  out.showValue=true;out.valueText=text;out.thresholdPct=.65f;out.thresholdPct2=.35f;cardCallback(ctx,&out);
 }else cardCallback(ctx,c);}
}
static void(*originalCard)(Update*);
static void card(Update* msg){
 if(!msg||!msg->addCard){originalCard(msg);return;}
 auto obj=msg->itemForm?msg->itemForm:msg->inventoryItem?read<void*>(msg->inventoryItem,0):nullptr;
 auto prev=cardCallback;bool prior=cardWeapon;Update copy=*msg;cardCallback=msg->addCard;cardWeapon=metadata(obj)!=nullptr;copy.addCard=tierCard;
 originalCard(&copy);cardCallback=prev;cardWeapon=prior;
}
static bool hashMatches(HMODULE module){
 wchar_t file[32768];if(!GetModuleFileNameW(module,file,32768))return false;FILE* f=_wfopen(file,L"rb");if(!f)return false;
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
 if(ok)ok=BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)>=0;
 uint8_t buf[65536],digest[32];size_t n;
 while(ok&&(n=fread(buf,1,sizeof buf,f))>0)ok=BCryptHashData(h,buf,(ULONG)n,0)>=0;
 if(ferror(f))ok=false;fclose(f);if(ok)ok=BCryptFinishHash(h,digest,32,0)>=0&&!memcmp(digest,CSF_SHA,32);
 if(h)BCryptDestroyHash(h);if(alg)BCryptCloseAlgorithmProvider(alg,0);return ok;
}
struct Hook{uintptr_t offset;const unsigned char* expected;void* replacement;void** original;bool engine;};
static bool bind(){
 auto module=GetModuleHandleW(L"ConditionSystemFramework.dll");if(!module||!hashMatches(module)){log("Inactive: exact CSF 1.1.1 binary not present; on-disk gameplay gates remain OFF.");return false;}
 csf=(uintptr_t)module;
 Hook hooks[]={
 {R_Flags,B_Flags,(void*)flags,(void**)&originalFlags,false},{R_Cap,B_Cap,(void*)repairCap,nullptr,false},
 {R_Cost,B_Cost,(void*)costs,nullptr,false},{R_Roll,B_Roll,(void*)roll,nullptr,false},
 {R_Wake,B_Wake,(void*)wake,(void**)&originalWake,false},{R_Inventory,B_Inventory,(void*)inventory,(void**)&originalInventory,false},
 {R_World,B_World,(void*)world,(void**)&originalWorld,false},{R_Pickup,B_Pickup,(void*)pickup,(void**)&originalPickup,false},
 {R_Input,B_Input,(void*)input,(void**)&originalInput,false},{R_Equip,B_Equip,(void*)equip,(void**)&originalEquip,false},
 {R_Settings,B_Settings,(void*)settings,(void**)&originalSettings,false},{R_Wear,B_Wear,(void*)wear,(void**)&originalWear,false},
 {R_Card,B_Card,(void*)card,(void**)&originalCard,false},{R_Depleted,B_Depleted,(void*)depleted,(void**)&originalDepleted,false},
 {G_QueueFire,E_QueueFire,(void*)queueFire,(void**)&originalQueue,true},{G_Fire,E_Fire,(void*)fire,(void**)&originalFire,true}};
 for(auto& h:hooks)if(memcmp((void*)((h.engine?game:csf)+h.offset),h.expected,16)){log("Inactive: native function conflict; nothing enabled.");return false;}
 if(memcmp((void*)(game+G_PerkRank),E_PerkRank,16)||memcmp((void*)(game+G_AddPerk),E_AddPerk,16)||memcmp((void*)(game+G_Location),E_Location,16)||memcmp((void*)(game+G_Owner),E_Owner,16)){log("Inactive: engine binding mismatch.");return false;}
 if(MH_Initialize()!=MH_OK){log("Inactive: hook initialization failed.");return false;}
 unsigned created=0;
 for(auto& h:hooks){if(MH_CreateHook((void*)((h.engine?game:csf)+h.offset),h.replacement,h.original)!=MH_OK)break;created++;}
 if(created!=sizeof(hooks)/sizeof(*hooks)){for(unsigned i=0;i<created;i++)MH_RemoveHook((void*)((hooks[i].engine?game:csf)+hooks[i].offset));log("Inactive: hook preparation failed, all prepared hooks removed.");return false;}
 getHealth=(decltype(getHealth))(csf+R_GetHealth);setHealth=(decltype(setHealth))(csf+R_SetHealth);getEquipped=(decltype(getEquipped))(csf+R_GetEquipped);csfNew=(decltype(csfNew))(csf+R_New);
 if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK){MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();log("Inactive: hook commit failed; all hooks removed.");return false;}
 bound=true;policy();log("Exact-runtime guards passed; 16 scoped hooks committed. Waiting for all catalog forms.");return true;
}
static int compareWeapons(const void* a,const void* b){U32 x=((const Weapon*)a)->runtime,y=((const Weapon*)b)->runtime;return x<y?-1:x>y?1:0;}
static bool initialize(){
 dataReady=false;sessionLive=false;packEnabled=GetPrivateProfileIntW(L"General",L"Enabled",1,iniPath)!=0;
 for(auto& w:weapons){w.runtime=resolve(w.plugin,w.local);w.form=form(w.runtime);if(!w.form||read<uint8_t>(w.form,0x1a)!=0x2b){log("Inactive: weapon catalog resolution failed.");policy();return false;}}
 qsort(weapons,sizeof(weapons)/sizeof(*weapons),sizeof(Weapon),compareWeapons);
 for(auto& c:factions){c.runtime=resolve(c.plugin,c.local);c.form=form(c.runtime);}
 for(auto& c:locations){c.runtime=resolve(c.plugin,c.local);c.form=form(c.runtime);}
 for(auto& c:sealedCells){c.runtime=resolve(c.plugin,c.local);c.form=form(c.runtime);}
 for(unsigned i=0;i<11;i++){components[i]=form(componentIds[i]);if(!components[i]||read<uint8_t>(components[i],0x1a)!=0x08){log("Inactive: repair component identity mismatch.");policy();return false;}}
 auto t=form(resolve("PackWeaponCondition.esp",0x800)),r=form(resolve("PackWeaponCondition.esp",0x801));accuracyPerk=form(resolve("PackWeaponCondition.esp",0x802));
 if(!t||!r||!accuracyPerk||read<uint8_t>(t,0x1a)!=0xb||read<uint8_t>(r,0x1a)!=0xb||read<uint8_t>(accuracyPerk,0x1a)!=0x5f){log("Inactive: owned plugin/accuracy perk missing or wrong type.");policy();return false;}
 tierGlobal=(float*)((uint8_t*)t+0x30);readyGlobal=(float*)((uint8_t*)r+0x30);*tierGlobal=0;*readyGlobal=0;
 dataReady=true;policy();initializeEnvironment();log("Ready: complete weapon catalog, workbench components, tiers and player-only policy bound. Runtime tests are still required.");return true;
}
static void onMessage(Messaging::Message* m){
 if(m->type==0&&!bound)bind();
 if(m->type==10&&bound&&m->data)initialize();
 if(m->type==2||m->type==8){sessionLive=false;burst={};environmentClock.reset();AcquireSRWLockExclusive(&pendingLock);memset(pending,0,sizeof pending);ReleaseSRWLockExclusive(&pendingLock);if(readyGlobal)*readyGlobal=0;}
 if(m->type==3||m->type==8){sessionLive=m->type==8||m->data!=nullptr;policy();updateTier();}
}
extern "C" __declspec(dllexport) bool F4SEPlugin_Query(const F4SEInterface* f,PluginInfo* info){info->version=1;info->name="PackWeaponCondition";info->pluginVersion=2;return !f->isEditor&&f->runtimeVersion==0x010a0a30;}
extern "C" __declspec(dllexport) bool F4SEPlugin_Load(const F4SEInterface* f){
 if(f->isEditor||f->runtimeVersion!=0x010a0a30)return false;game=(uintptr_t)GetModuleHandleW(nullptr);
 wchar_t path[32768];DWORD len=GetModuleFileNameW(nullptr,path,32768);if(!len||len>=32700)return false;auto slash=wcsrchr(path,L'\\');if(!slash)return false;
 wcscpy(slash+1,L"Data\\F4SE\\Plugins\\PackWeaponCondition.log");logfile=_wfopen(path,L"wb");wcscpy(iniPath,path);auto end=wcsrchr(iniPath,L'.');wcscpy(end,L".ini");
 seed^=GetTickCount();auto messages=(Messaging*)f->QueryInterface(1);if(!messages||!messages->RegisterListener(f->GetPluginHandle(),"F4SE",onMessage))return false;
 log("PackWeaponCondition 0.2.0 TEST loaded. Gameplay remains held OFF until guarded bindings and catalog validation complete.");return true;
}
