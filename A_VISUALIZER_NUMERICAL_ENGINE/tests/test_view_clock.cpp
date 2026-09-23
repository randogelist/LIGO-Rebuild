#include "../renderer/binary_driver.h"
#include <cmath>
#include <iomanip>
#include <iostream>
using namespace bkqrgr_driver;
int main(){
    BinaryInput in{}; State st{}; initialize(in,st);
    const double target=1.0;
    const double rate0=phase_normalized_time_rate(in,st,target);
    const double hz0=apparent_orbit_hz(in,st,rate0);
    advance(in,st,40.0);
    const double rate1=phase_normalized_time_rate(in,st,target);
    const double hz1=apparent_orbit_hz(in,st,rate1);
    const auto s1=snapshot(in,st);

    // Continue all the way into the remnant.  The normalized clock uses the
    // characteristic merger/ringdown phase rate once there is no binary orbit.
    advance(in,st,4000.0);
    const auto sr=snapshot(in,st);
    const double rateR=phase_normalized_time_rate(in,st,target);
    const double hzR=apparent_orbit_hz(in,st,rateR);

    const bool ok=std::abs(hz0-target)<1e-12 && std::abs(hz1-target)<1e-12 &&
                  std::abs(hzR-target)<1e-12 && rate1<rate0 &&
                  s1.stage==STAGE_INSPIRAL && sr.stage>=STAGE_RINGDOWN;
    std::cout<<std::setprecision(14)
             <<"VIEW_TARGET_HZ="<<target
             <<"\nINITIAL_SIM_RATE_MPS="<<rate0
             <<"\nINITIAL_VIEW_HZ="<<hz0
             <<"\nLATER_SIM_RATE_MPS="<<rate1
             <<"\nLATER_VIEW_HZ="<<hz1
             <<"\nREMNANT_CHARACTERISTIC_VIEW_HZ="<<hzR
             <<"\nPHASE_NORMALIZED_ONE_HZ="<<(ok?"PASS":"FAIL")
             <<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";
    return ok?0:1;
}
