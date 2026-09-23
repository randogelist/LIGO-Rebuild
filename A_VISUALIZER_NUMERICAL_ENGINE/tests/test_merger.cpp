#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include "../renderer/binary_driver.h"
using namespace bkqrgr_driver;
int main(){
    BinaryInput in{}; State st{}; initialize(in,st); const auto d=derive(in);
    int steps=0;
    while(st.transitionTime<0.0 && steps++<10000) advance(in,st,0.5);
    State atTransition=st; atTransition.t=st.transitionTime;
    const auto a=snapshot(in,atTransition);
    const bool transitioned=st.transitionTime>=0.0 && a.stage==STAGE_MERGER && a.mergerBlend<1e-12;
    const double mA=a.systemMass,eA=a.energyRadiated,jA=a.systemJ;
    advance(in,st,30.0*d.qnmTau220); const auto z=snapshot(in,st);
    const double ledger=std::abs((z.systemMass+z.energyRadiated)-d.initialADM);
    const double jledger=std::abs((z.systemJ+z.angularMomentumRadiated)-in.jTotal);
    const double mfit=std::abs(z.systemMass-d.finalMassFit);
    const double targetJ=d.finalJFit;
    const double jfit=std::abs(z.systemJ-targetJ);
    const bool monotone=z.systemMass<=mA+1e-12 && z.energyRadiated>=eA-1e-12 &&
                        d.orientation*z.systemJ<=d.orientation*jA+1e-12;
    const bool remnant=z.stage==STAGE_REMNANT && z.mergerBlend>0.999999 && z.r<1e-6*d.M;
    const bool ok=transitioned&&monotone&&remnant&&ledger<1e-10&&jledger<1e-10&&mfit<1e-8&&jfit<1e-8;
    std::cout<<std::setprecision(14)
             <<"TRANSITION_STAGE="<<stage_name(a.stage)
             <<"\nFINAL_STAGE="<<stage_name(z.stage)
             <<"\nMERGER_BLEND="<<z.mergerBlend
             <<"\nFINAL_SEPARATION="<<z.r
             <<"\nFINAL_SYSTEM_MASS="<<z.systemMass
             <<"\nNR_FINAL_MASS_FIT="<<d.finalMassFit
             <<"\nFINAL_SYSTEM_J="<<z.systemJ
             <<"\nNR_FINAL_J_FIT="<<targetJ
             <<"\nENERGY_LEDGER_ERROR="<<ledger
             <<"\nANGULAR_MOMENTUM_LEDGER_ERROR="<<jledger
             <<"\nNR_CALIBRATED_MERGER_RINGDOWN="<<(ok?"PASS":"FAIL")
             <<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";
    return ok?0:1;
}
