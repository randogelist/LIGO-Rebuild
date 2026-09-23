#include <cmath>
#include <iomanip>
#include <iostream>
#include "../renderer/binary_driver.h"
using namespace bkqrgr_driver;
int main(){
    BinaryInput a{}; const auto d=derive(a);
    const double e_r=std::abs(d.r0-12.0);
    const double e_w=std::abs(d.omega0-1.0/std::pow(12.0,1.5));
    const bool nrFit=d.finalMassFit>0.0 && d.finalMassFit<d.initialADM && std::abs(d.finalSpinFit)<0.998;
    const bool transition=d.xTransition>d.x0 && d.rTransition<d.r0;

    BinaryInput b{}; b.m1=.7;b.m2=.3;b.spin1z=.03;b.spin2z=-.01;b.jTotal=.7650907965843948;
    const auto q=derive(b);
    const double jerr=std::abs(realized_total_J(b)-b.jTotal);
    State st{}; initialize(b,st); advance(b,st,2.0); const auto s=snapshot(b,st);
    const double ledger=std::abs((s.systemMass+s.energyRadiated)-q.initialADM);
    const bool evolved=s.r<q.r0 && std::abs(s.phase-b.phase0)>0.0 && s.energyRadiated>0.0;

    // Direct initial-separation input contract: choosing r0/M determines the
    // compatible circular 3PN orbital angular momentum.
    BinaryInput c{};
    const double targetRoverM=18.0;
    const double Mc=c.m1+c.m2, etac=(c.m1*c.m2)/(Mc*Mc);
    const double xc=1.0/targetRoverM;
    c.jTotal=c.spin1z+c.spin2z+pn_orbital_J(Mc,etac,xc);
    const auto dc=derive(c);
    const double separationInputError=std::abs(dc.r0/dc.M-targetRoverM);

    bool ok=e_r<1e-12&&e_w<1e-12&&jerr<1e-12&&ledger<1e-11&&nrFit&&transition&&evolved&&separationInputError<1e-11;
    std::cout<<std::setprecision(14)
             <<"DEFAULT_R0="<<d.r0<<"\nDEFAULT_OMEGA0="<<d.omega0
             <<"\nPN_TRANSITION_R="<<d.rTransition
             <<"\nINITIAL_ADM="<<d.initialADM
             <<"\nFINAL_MASS_FIT="<<d.finalMassFit
             <<"\nFINAL_SPIN_FIT="<<d.finalSpinFit
             <<"\nARBITRARY_R0="<<q.r0
             <<"\nJTOT_REALIZATION_ERROR="<<jerr
             <<"\nENERGY_LEDGER_ERROR="<<ledger
             <<"\nDIRECT_INITIAL_SEPARATION_ERROR="<<separationInputError
             <<"\nDIRECT_INITIAL_SEPARATION_INPUT="<<(separationInputError<1e-11?"PASS":"FAIL")
             <<"\nM1_M2_JTOT_PN_NR_DERIVATION="<<(ok?"PASS":"FAIL")
             <<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";
    return ok?0:1;
}
