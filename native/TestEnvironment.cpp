#include "EnvironmentPolicy.h"
#include "ConditionPolicy.h"
#include <stdio.h>
#include <stdint.h>
using namespace EnvironmentPolicy;
static unsigned assertions=0;
#define CHECK(x) do{assertions++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
inline bool near(double a,double b){return fabs(a-b)<.000000001;}
int main(){
 Rates r;Weather dry{},rain{true,false},storm{true,true};
 CHECK(near(rate(false,false,rain,{},1,false,r),.05));CHECK(near(rate(false,false,storm,{},1,false,r),.10));
 CHECK(near(rate(true,false,storm,rain,.5,true,r),2));CHECK(near(rate(true,true,storm,{},1,false,r),2));
 CHECK(rate(false,true,storm,rain,.5,true,r)==0);CHECK(rate(false,false,dry,{},1,false,r)==0);
 CHECK(near(rate(false,false,rain,storm,.5,true,r),.075));CHECK(near(rate(false,false,rain,{},0,false,r),.05));
 CHECK(loss(.05,0)==0);CHECK(loss(.05,60)==0);CHECK(loss(.05,-1)==0);CHECK(loss(.05,300)==0); // no skipped-time catchup
 double rain20=0,storm10=0,swim30=0;
 for(unsigned i=0;i<1200;i++)rain20+=loss(.05,1);
 for(unsigned i=0;i<600;i++)storm10+=loss(.10,1);
 for(unsigned i=0;i<30;i++)swim30+=loss(2,1);
 CHECK(near(rain20,.01));CHECK(near(storm10,.01));CHECK(near(swim30,.01));
 for(int i=0;i<10000;i++){
  double p=i/9999.;double value=rate(false,false,rain,storm,p,true,r);
  CHECK(value>=.05&&value<=.10);CHECK(near(value,.1-.05*p));
  CHECK(rate(false,true,rain,storm,p,true,r)==0);CHECK(rate(true,false,rain,storm,p,true,r)==2);
 }
 Clock c;CHECK(c.advance(1000,true)==0);CHECK(c.advance(1500,true)==0);CHECK(c.advance(2000,true)==1);
 CHECK(c.advance(2500,false)==0);CHECK(c.advance(100000,false)==0);CHECK(c.advance(101000,true)==0);CHECK(c.advance(102000,true)==1);
 CHECK(c.advance(200000,true)==0);CHECK(c.advance(201000,true)==1); // load/stall gap discarded
 CHECK(c.advance(10,true)==0);CHECK(c.advance(1010,true)==1); // backwards clock safe
 c.reset();CHECK(c.advance(1000,true)==0);CHECK(c.advance(2000,true)==1);
 Rates disabled{0,0,0};CHECK(rate(true,false,storm,rain,.5,true,disabled)==0);
 CHECK(weatherRate(storm,r)==r.storm);CHECK(weatherRate(storm,r)!=r.storm+r.rain);
 double health=.1;
 for(unsigned i=0;i<1200;i++){double old=health;health=fmax(0.,health-loss(2,1));CHECK(health>=0&&health<=old);}
 CHECK(health==0&&Policy::tier((float)health)==Policy::Ruined);
 printf("PASS %u environmental assertions: rates, weather blending, swimming priority, interior protection, pause/focus/reset clocks, no catchup and no condition restoration. Pure policy test; no game process.\n",assertions);
 return 0;
}
