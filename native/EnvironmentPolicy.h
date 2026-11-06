#pragma once
#include <stdint.h>
#include <math.h>
namespace EnvironmentPolicy {
struct Rates{double rain=.05,storm=.10,swim=2.0;}; // percentage points per active real minute
struct Weather{bool rain=false,storm=false;};
inline double sane(double x,double cap){return !isfinite(x)||x<0?0:x>cap?cap:x;}
inline double weatherRate(Weather w,Rates r){return w.storm?sane(r.storm,10):w.rain?sane(r.rain,5):0;}
inline double rate(bool swimming,bool interior,Weather current,Weather outgoing,double currentPct,bool hasOutgoing,Rates r){
 if(swimming)return sane(r.swim,20);
 if(interior)return 0;
 double p=!isfinite(currentPct)?1:currentPct<0?0:currentPct>1?1:currentPct;
 if(!hasOutgoing)p=1;
 return weatherRate(current,r)*p+weatherRate(outgoing,r)*(1-p);
}
inline double loss(double percentagePointsPerMinute,double seconds){
 return !isfinite(seconds)||seconds<=0||seconds>2?0:sane(percentagePointsPerMinute,20)*seconds/6000.0;
}
struct Clock{
 uint64_t last=0;double accumulated=0;bool active=false;
 void reset(){last=0;accumulated=0;active=false;}
 double advance(uint64_t now,bool playing){
  if(!playing||!active){last=now;accumulated=0;active=playing;return 0;}
  uint64_t delta=now>=last?now-last:3000;last=now;
  if(delta>2000){accumulated=0;return 0;}
  accumulated+=delta/1000.;
  if(accumulated<1)return 0;
  double seconds=accumulated;accumulated=0;return seconds<=2?seconds:0;
 }
};
}
