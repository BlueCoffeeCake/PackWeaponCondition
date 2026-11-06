#pragma once
#include <stdint.h>
#include <math.h>
namespace Policy {
enum Tier { Pristine, Worn, Damaged, BadlyDamaged, Ruined };
enum Family { Ballistic, Energy, Melee, Heavy };
inline float clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
inline Tier tier(float c){return !isfinite(c)||c<=.000001f?Ruined:c<.35f?BadlyDamaged:c<.65f?Damaged:c<.90f?Worn:Pristine;}
inline const char* name(Tier t){const char* n[]={"Pristine","Worn","Damaged","Badly Damaged","Ruined"};return n[t];}
inline float chance(float c,int mechanism){
 if(mechanism==3||mechanism==4)return 0;
 const float p[]={0,.001f,.008f,.035f,1};
 return p[tier(c)]*(mechanism==2?.75f:1.f);
}
inline float cap(Family f,int rank){
 if(f==Melee){const float a[]={.75f,.85f,.95f,1};return a[rank<0?0:rank>3?3:rank];}
 const float a[]={.75f,.80f,.85f,.95f,1};return a[rank<0?0:rank>4?4:rank];
}
inline unsigned cost(float budget,float delta,bool ruined){
 if(!isfinite(delta)||delta<=.001f)return 0;
 return (unsigned)ceilf(budget*clamp(delta,0,1)*(ruined?2.75f:1.f));
}
inline float spread(Tier t){return t==Damaged?1.20f:t==BadlyDamaged?1.65f:1.f;}
inline float vats(Tier t){return t==Damaged?.90f:t==BadlyDamaged?.75f:1.f;}
inline float wear(float cost,bool sustained,Family f){return cost*((sustained&&f!=Melee)?1.35f:1.f);}
struct Range{float lo,hi;};
inline Range initial(int context){
 // Existing player gear/rewards, civilians/militia, professional units,
 // raiders, exposed/abandoned, explicitly curated sealed interiors.
 const Range a[]={{.80f,1.f},{.55f,.90f},{.80f,1.f},{.25f,.75f},{.12f,.60f},{.85f,1.f}};
 return a[context>=0&&context<6?context:4];
}
}
