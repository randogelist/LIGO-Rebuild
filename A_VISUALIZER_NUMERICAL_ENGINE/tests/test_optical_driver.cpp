#include <cmath>
#include <iostream>
#include "../renderer/binary_driver.h"
using namespace bkqrgr_driver;
int main(){
    BinaryInput b{}; State st{}; initialize(b,st);
    const auto a=snapshot(b,st);
    const double rA=effective_individual_optical_radius(b.m1,b.m2,a.r,a.mergerBlend,0.94);
    advance(b,st,900.0); const auto mid=snapshot(b,st);
    const double rMid=effective_individual_optical_radius(b.m1,b.m2,mid.r,mid.mergerBlend,0.94);
    advance(b,st,400.0); const auto merger=snapshot(b,st);
    const double rMerger=effective_individual_optical_radius(b.m1,b.m2,merger.r,merger.mergerBlend,0.94);
    const double rRem=effective_remnant_optical_radius(merger.systemMass,0.94);

    State one{}; initialize(b,one); advance(b,one,1.0); const auto c=snapshot(b,one);
    const double com0=norm(b.m1*a.x1+b.m2*a.x2);
    const double com1=norm(b.m1*c.x1+b.m2*c.x2);
    const double comv=norm(b.m1*c.v1+b.m2*c.v2);
    const bool radiusOk=rMid>rA && rMerger<rMid && rRem>0.0;
    const bool ok=com0<1e-12&&com1<1e-12&&comv<1e-12&&c.r<a.r&&std::abs(c.phase-a.phase)>0&&c.gwPower>0&&radiusOk;
    std::cout<<"COM0="<<com0<<" COM1="<<com1<<" COMV="<<comv<<" R0="<<a.r<<" R1="<<c.r<<" DPHI="<<c.phase-a.phase<<"\n"
             <<"OBJECT_RADIUS_INITIAL="<<rA<<"\n"
             <<"OBJECT_RADIUS_LATE_INSPIRAL="<<rMid<<"\n"
             <<"OBJECT_RADIUS_MERGER="<<rMerger<<"\n"
             <<"REMNANT_RADIUS="<<rRem<<"\n"
             <<"TIME_EVOLVING_OBJECT_RADIUS="<<(radiusOk?"PASS":"FAIL")<<"\n"
             <<"OPTICAL_DRIVER_TIME_EVOLUTION="<<(ok?"PASS":"FAIL")<<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";
    return ok?0:1;
}
