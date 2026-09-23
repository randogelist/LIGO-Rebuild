#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include "../renderer/binary_driver.h"
using namespace bkqrgr_driver;
int main(){
    BinaryInput in{}; State st{}; initialize(in,st); const auto d=derive(in);
    double prevR=d.r0, prevE=-1.0; double maxLedger=0.0,maxPn=0.0,maxPnJ=0.0; int samples=0;
    bool monotone=true, positive=true;
    while(st.transitionTime<0.0 && samples<10000){
        advance(in,st,0.5); const auto s=snapshot(in,st);
        maxLedger=std::max(maxLedger,std::abs((s.systemMass+s.energyRadiated)-d.initialADM));
        maxPn=std::max(maxPn,std::abs(s.pnEnergyBalanceResidual));
        maxPnJ=std::max(maxPnJ,std::abs(s.pnAngularMomentumBalanceResidual));
        monotone &= (s.r<=prevR+1e-11) && (s.energyRadiated+1e-13>=prevE);
        positive &= s.gwPower>=0.0 && (s.d.orientation*s.gwJFlux)>=-1e-13;
        prevR=s.r; prevE=s.energyRadiated; ++samples;
    }
    const auto tr=snapshot(in,st);
    const double jLedger=std::abs((tr.systemJ+tr.angularMomentumRadiated)-in.jTotal);
    const bool reached=st.transitionTime>=0.0 && tr.r<=d.rTransition*(1.0+1e-8);
    const bool ok=reached&&monotone&&positive&&maxLedger<2e-10&&maxPn<2e-7&&maxPnJ<2e-7&&jLedger<2e-10;
    std::cout<<std::setprecision(14)
             <<"INSPIRAL_SAMPLES="<<samples
             <<"\nPN_TRANSITION_TIME="<<st.transitionTime
             <<"\nPN_TRANSITION_R="<<tr.r
             <<"\nMAX_ENERGY_LEDGER_ERROR="<<maxLedger
             <<"\nMAX_PN_BALANCE_RESIDUAL="<<maxPn
             <<"\nMAX_PN_J_BALANCE_RESIDUAL="<<maxPnJ
             <<"\nANGULAR_MOMENTUM_LEDGER_ERROR="<<jLedger
             <<"\nMONOTONE_GW_ENERGY_LOSS="<<(monotone?"PASS":"FAIL")
             <<"\n3P5PN_ENERGY_BALANCE="<<(ok?"PASS":"FAIL")
             <<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";
    return ok?0:1;
}
