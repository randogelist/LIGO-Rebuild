#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace bkqrgr_driver {
constexpr double PI=3.141592653589793238462643383279502884;
constexpr double GAMMA_E=0.577215664901532860606512090082402431;
struct Vec3{double x{},y{},z{};};
inline Vec3 operator+(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec3 operator-(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 operator*(double s,Vec3 a){return {s*a.x,s*a.y,s*a.z};}
inline Vec3 operator*(Vec3 a,double s){return s*a;}
inline double dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline double norm(Vec3 a){return std::sqrt(dot(a,a));}
inline double sqr(double x){return x*x;}
inline double cube(double x){return x*x*x;}

// Dynamics stage values are intentionally stable because they are also exposed
// as diagnostics to the renderer/tests.
enum DynamicsStage : int { STAGE_INSPIRAL=0, STAGE_MERGER=1, STAGE_RINGDOWN=2, STAGE_REMNANT=3 };
inline const char* stage_name(int s){
    switch(s){case STAGE_INSPIRAL:return "INSPIRAL";case STAGE_MERGER:return "MERGER";case STAGE_RINGDOWN:return "RINGDOWN";default:return "REMNANT";}
}

struct BinaryInput{
    double m1=0.5;
    double m2=0.5;
    // Equal-mass default is the 3PN circular angular momentum at r0=12 M,
    // giving several visible orbits before the strong-field transition.
    double jTotal=0.9944154929906234;
    double spin1z=0.0;
    double spin2z=0.0;
    double phase0=0.0;
    bool radiationReaction=true;
    // Safety ceiling for how deep the PN inspiral may be used.  The actual
    // switch is normally earlier, at 0.92 of the 3PN minimum-energy orbit.
    double minSeparationFactor=2.6;
};

struct Derived{
    double M{},mu{},eta{},Lorb{},r0{},omega0{},fOrb0{},fGW0{},K{},tCoal{},tToFloor{};
    double x0{},xMeco{},xTransition{},rTransition{};
    double initialADM{},chi1L{},chi2L{};
    double eradFitFraction{},finalMassFit{},finalSpinFit{},finalJFit{};
    double qnmOmega220{},qnmQ220{},qnmTau220{};
    int orientation=1;
};

struct State{
    double t=0.0;
    double x=0.0;
    double phase=0.0;
    double eRad=0.0;
    double jRad=0.0; // signed z-angular momentum carried away
    double transitionTime=-1.0;
    double transitionX=0.0;
    double transitionR=0.0;
    double transitionPhase=0.0;
    double transitionOmega=0.0;
    double transitionERad=0.0;
    double transitionJRad=0.0;
    bool initialized=false;
};

struct Snapshot{
    Derived d{};
    double r{},phase{},omega{},rdot{},gwPower{},gwJFlux{};
    double energyRadiated{},angularMomentumRadiated{},systemMass{},systemJ{};
    double remnantMass{},remnantSpin{},mergerBlend{},timeSinceTransition{};
    double pnEnergyBalanceResidual{},pnAngularMomentumBalanceResidual{};
    int stage=STAGE_INSPIRAL;
    Vec3 x1{},x2{},v1{},v2{};
};

inline double pn_binding_energy(double M,double eta,double x){
    const double e1=-3.0/4.0-eta/12.0;
    const double e2=-27.0/8.0+19.0*eta/8.0-eta*eta/24.0;
    const double e3=-675.0/64.0+(34445.0/576.0-205.0*PI*PI/96.0)*eta
                    -155.0*eta*eta/96.0-35.0*eta*eta*eta/5184.0;
    return -0.5*M*eta*x*(1.0+e1*x+e2*x*x+e3*x*x*x);
}
inline double pn_binding_dEdx(double M,double eta,double x){
    const double e1=-3.0/4.0-eta/12.0;
    const double e2=-27.0/8.0+19.0*eta/8.0-eta*eta/24.0;
    const double e3=-675.0/64.0+(34445.0/576.0-205.0*PI*PI/96.0)*eta
                    -155.0*eta*eta/96.0-35.0*eta*eta*eta/5184.0;
    return -0.5*M*eta*(1.0+2.0*e1*x+3.0*e2*x*x+4.0*e3*x*x*x);
}
inline double pn_flux_3p5(double eta,double x){
    x=std::max(1e-10,x);
    const double e2=eta*eta, e3=e2*eta;
    const double sx=std::sqrt(x);
    const double x3h=x*sx, x2=x*x, x5h=x2*sx, x3=x2*x, x7h=x3*sx;
    double c=1.0;
    c+=(-1247.0/336.0-35.0*eta/12.0)*x;
    c+=4.0*PI*x3h;
    c+=(-44711.0/9072.0+9271.0*eta/504.0+65.0*e2/18.0)*x2;
    c+=(-8191.0/672.0-583.0*eta/24.0)*PI*x5h;
    c+=(6643739519.0/69854400.0+16.0*PI*PI/3.0-1712.0*GAMMA_E/105.0
        -856.0*std::log(16.0*x)/105.0
        +(-134543.0/7776.0+41.0*PI*PI/48.0)*eta
        -94403.0*e2/3024.0-775.0*e3/324.0)*x3;
    c+=(-16285.0/504.0+214745.0*eta/1728.0+193385.0*e2/3024.0)*PI*x7h;
    // Outside the controlled PN regime a truncated series can become badly
    // behaved.  The dynamics switches before that point; this guard prevents
    // a numerical sign flip if a user starts directly in the strong field.
    c=std::clamp(c,0.05,8.0);
    return (32.0/5.0)*eta*eta*std::pow(x,5.0)*c;
}

inline double pn_orbital_J(double M,double eta,double x){
    const double e1=-3.0/4.0-eta/12.0;
    const double e2=-27.0/8.0+19.0*eta/8.0-eta*eta/24.0;
    const double e3=-675.0/64.0+(34445.0/576.0-205.0*PI*PI/96.0)*eta
                    -155.0*eta*eta/96.0-35.0*eta*eta*eta/5184.0;
    const double corr=1.0-2.0*e1*x-e2*x*x-(4.0/5.0)*e3*x*x*x;
    return eta*M*M*corr/std::sqrt(std::max(1e-14,x));
}
inline double pn_x_from_orbital_J(double M,double eta,double Labs,double xMax){
    if(!(Labs>0.0)) throw std::runtime_error("orbital angular momentum must be positive in magnitude");
    const double jAtMax=pn_orbital_J(M,eta,xMax);
    if(Labs<jAtMax*(1.0-1e-12))
        throw std::runtime_error("|J-S1-S2| is below the supported 3PN quasicircular branch; increase |J| or use full numerical relativity");
    if(std::abs(Labs-jAtMax)<=1e-12*std::max(1.0,Labs)) return xMax;
    double lo=std::min(1e-5,0.01*xMax), hi=xMax;
    while(pn_orbital_J(M,eta,lo)<Labs && lo>1e-14) lo*=0.25;
    if(pn_orbital_J(M,eta,lo)<Labs)
        throw std::runtime_error("angular momentum is outside the numerically supported PN inversion range");
    // On the weak-field circular branch J(x) decreases monotonically toward
    // the minimum-energy orbit, so bisection is deterministic and stable.
    for(int i=0;i<100;++i){
        const double mid=0.5*(lo+hi);
        if(pn_orbital_J(M,eta,mid)>Labs) lo=mid; else hi=mid;
    }
    return 0.5*(lo+hi);
}
inline double pn_meco_x(double M,double eta){
    double lo=0.08, hi=0.36;
    double flo=pn_binding_dEdx(M,eta,lo), fhi=pn_binding_dEdx(M,eta,hi);
    if(flo*fhi>0.0) return 0.22;
    for(int i=0;i<90;++i){
        const double mid=0.5*(lo+hi), fm=pn_binding_dEdx(M,eta,mid);
        if(flo*fm<=0.0){hi=mid;fhi=fm;}else{lo=mid;flo=fm;}
    }
    return 0.5*(lo+hi);
}

inline double uib_total_spin(double eta,double chi1,double chi2){
    const double delta=std::sqrt(std::max(0.0,1.0-4.0*eta));
    const double m1=0.5*(1.0+delta),m2=0.5*(1.0-delta);
    return (m1*m1*chi1+m2*m2*chi2)/(m1*m1+m2*m2);
}
inline double uib_radiated_fraction(double eta,double chi1,double chi2){
    const double delta=std::sqrt(std::max(0.0,1.0-4.0*eta));
    const double e2=eta*eta,e3=e2*eta,e4=e3*eta;
    const double S=uib_total_spin(eta,chi1,chi2),S2=S*S,S3=S2*S;
    const double dc=chi1-chi2,dc2=dc*dc;
    const double noSpin=0.057190958417936644*eta+0.5609904135313374*e2-0.84667563764404*e3+3.145145224278187*e4;
    double eqSpin=noSpin*(1.0+(-0.13084389181783257-1.1387311580238488*eta+5.49074464410971*e2)*S
                       +(-0.17762802148331427+2.176667900182948*e2)*S2
                       +(-0.6320191645391563+4.952698546796005*eta-10.023747993978121*e2)*S3)
                 /(1.0+(-0.9919475346968611+0.367620218664352*eta+4.274567337924067*e2)*S);
    eqSpin-=noSpin;
    const double uneq=-0.09803730445895877*dc*delta*(1.0-3.2283713377939134*eta)*e2
                     +0.01118530335431078*dc2*e3
                     -0.01978238971523653*dc*delta*(1.0-4.91667749015812*eta)*eta*S;
    return std::clamp(noSpin+eqSpin+uneq,0.0,0.25);
}
inline double uib_final_spin(double eta,double chi1,double chi2){
    const double delta=std::sqrt(std::max(0.0,1.0-4.0*eta));
    const double m1=0.5*(1.0+delta),m2=0.5*(1.0-delta),m1s=m1*m1,m2s=m2*m2;
    const double e2=eta*eta,e3=e2*eta;
    const double S=uib_total_spin(eta,chi1,chi2),S2=S*S,S3=S2*S;
    const double dc=chi1-chi2,dc2=dc*dc;
    const double noSpin=(3.4641016151377544*eta+20.0830030082033*e2-12.333573402277912*e3)/(1.0+7.2388440419467335*eta);
    const double eqSpin=(m1s+m2s)*S
        +((-0.8561951310209386*eta-0.09939065676370885*e2+1.668810429851045*e3)*S
        +(0.5881660363307388*eta-2.149269067519131*e2+3.4768263932898678*e3)*S2
        +(0.142443244743048*eta-0.9598353840147513*e2+1.9595643107593743*e3)*S3)
        /(1.0+(-0.9142232693081653+2.3191363426522633*eta-9.710576749140989*e3)*S);
    const double uneq=0.3223660562764661*dc*delta*(1.0+9.332575956437443*eta)*e2
                     -0.059808322561702126*dc2*e3
                     +2.3170397514509933*dc*delta*(1.0-3.2624649875884852*eta)*e3*S;
    return std::clamp(noSpin+eqSpin+uneq,-0.998,0.998);
}

inline Derived derive(const BinaryInput& in){
    if(!(in.m1>0.0&&in.m2>0.0)) throw std::runtime_error("masses must be positive");
    if(!(in.minSeparationFactor>=1.8)) throw std::runtime_error("strong-field safety radius must be >= 1.8 M");
    if(std::abs(in.spin1z)>0.998*in.m1*in.m1+1e-12) throw std::runtime_error("|S1z| exceeds Kerr bound 0.998 m1^2");
    if(std::abs(in.spin2z)>0.998*in.m2*in.m2+1e-12) throw std::runtime_error("|S2z| exceeds Kerr bound 0.998 m2^2");
    Derived d{}; d.M=in.m1+in.m2; d.mu=in.m1*in.m2/d.M; d.eta=d.mu/d.M;
    d.Lorb=in.jTotal-in.spin1z-in.spin2z;
    if(std::abs(d.Lorb)<1e-9*d.M*d.M) throw std::runtime_error("orbital angular momentum too close to zero");
    d.orientation=d.Lorb>=0?1:-1; const double Labs=std::abs(d.Lorb);
    d.xMeco=pn_meco_x(d.M,d.eta);
    d.xTransition=std::min(0.92*d.xMeco,1.0/in.minSeparationFactor);
    d.x0=pn_x_from_orbital_J(d.M,d.eta,Labs,d.xTransition);
    d.r0=d.M/d.x0;
    d.omega0=d.orientation*std::pow(d.x0,1.5)/d.M;
    d.fOrb0=d.omega0/(2.0*PI); d.fGW0=d.omega0/PI;
    d.K=(64.0/5.0)*d.eta*d.M*d.M*d.M;
    d.tCoal=std::pow(d.r0,4.0)/(4.0*d.K);
    const double rf=std::max(1e-9,in.minSeparationFactor*d.M);
    d.tToFloor=std::max(0.0,(std::pow(d.r0,4.0)-std::pow(std::min(d.r0,rf),4.0))/(4.0*d.K));
    d.rTransition=d.M/d.xTransition;
    d.initialADM=d.M+pn_binding_energy(d.M,d.eta,d.x0);

    double chi1=d.orientation*in.spin1z/(in.m1*in.m1);
    double chi2=d.orientation*in.spin2z/(in.m2*in.m2);
    if(in.m2>in.m1) std::swap(chi1,chi2); // UIB convention: first spin belongs to heavier BH.
    d.chi1L=chi1; d.chi2L=chi2;
    d.eradFitFraction=uib_radiated_fraction(d.eta,chi1,chi2);
    d.finalMassFit=d.M*(1.0-d.eradFitFraction);
    // A simulation beginning at finite separation cannot end with more energy
    // than its finite-separation ADM budget.
    d.finalMassFit=std::min(d.finalMassFit,d.initialADM*(1.0-1e-10));
    const double afAlong=uib_final_spin(d.eta,chi1,chi2);
    d.finalSpinFit=d.orientation*afAlong;
    d.finalJFit=d.finalSpinFit*d.finalMassFit*d.finalMassFit;
    const double amag=std::clamp(std::abs(d.finalSpinFit),0.0,0.998);
    const double oneMinus=std::max(1e-8,1.0-amag);
    const double omegaDimless=1.5251-1.1568*std::pow(oneMinus,0.1292);
    d.qnmQ220=0.7000+1.4187*std::pow(oneMinus,-0.4990);
    d.qnmOmega220=omegaDimless/std::max(1e-12,d.finalMassFit);
    d.qnmTau220=2.0*d.qnmQ220/std::max(1e-12,d.qnmOmega220);
    return d;
}

inline void initialize(const BinaryInput& in,State& st){
    const Derived d=derive(in);
    st=State{}; st.initialized=true; st.x=d.x0; st.phase=in.phase0;
    if(in.radiationReaction && d.x0>=d.xTransition*(1.0-1e-12)){
        st.transitionTime=0.0; st.transitionX=d.x0; st.transitionR=d.r0;
        st.transitionPhase=in.phase0; st.transitionOmega=d.omega0;
        st.transitionERad=0.0; st.transitionJRad=0.0;
    }
}

struct Rates{double dx{},dphi{},dE{},dJ{};};
inline Rates inspiral_rates(const Derived& d,double x,bool rr){
    Rates q{}; q.dphi=d.orientation*std::pow(std::max(1e-12,x),1.5)/d.M;
    if(!rr) return q;
    const double flux=pn_flux_3p5(d.eta,x);
    const double dedx=pn_binding_dEdx(d.M,d.eta,x);
    if(!(dedx<-1e-14)) return q;
    q.dx=-flux/dedx;
    q.dE=flux;
    const double om=std::abs(q.dphi);
    q.dJ=d.orientation*flux/std::max(1e-12,om);
    return q;
}

inline void enter_transition(const BinaryInput& in,const Derived& d,State& st){
    st.x=std::max(st.x,d.xTransition);
    st.transitionTime=st.t;
    st.transitionX=st.x;
    st.transitionR=d.M/st.x;
    st.transitionPhase=st.phase;
    st.transitionOmega=d.orientation*std::pow(st.x,1.5)/d.M;
    st.transitionERad=st.eRad;
    st.transitionJRad=st.jRad;
    (void)in;
}

inline void advance(const BinaryInput& in,State& st,double dt){
    if(dt<=0.0) return;
    if(!st.initialized) initialize(in,st);
    const Derived d=derive(in);
    double remain=dt;
    int guard=0;
    while(remain>1e-12 && guard++<20000){
        if(!in.radiationReaction){
            const double om=d.orientation*std::pow(st.x,1.5)/d.M;
            st.phase=std::remainder(st.phase+om*remain,2.0*PI); st.t+=remain; remain=0.0; break;
        }
        if(st.transitionTime>=0.0){ st.t+=remain; remain=0.0; break; }
        if(st.x>=d.xTransition*(1.0-1e-10)){
            enter_transition(in,d,st); continue;
        }
        const Rates r0=inspiral_rates(d,st.x,true);
        if(!(r0.dx>0.0)){enter_transition(in,d,st);continue;}
        double h=std::min(remain,0.5*d.M);
        h=std::min(h,0.14/std::max(1e-12,std::abs(r0.dphi)));
        h=std::min(h,0.005*st.x/std::max(1e-14,r0.dx));
        const double toTr=(d.xTransition-st.x)/r0.dx;
        if(toTr<1e-8*d.M){st.x=d.xTransition;enter_transition(in,d,st);continue;}
        h=std::min(h,std::max(1e-8*d.M,0.90*toTr));
        h=std::min(h,remain);

        auto eval=[&](double x){return inspiral_rates(d,x,true);};
        const Rates k1=eval(st.x);
        const Rates k2=eval(st.x+0.5*h*k1.dx);
        const Rates k3=eval(st.x+0.5*h*k2.dx);
        const Rates k4=eval(st.x+h*k3.dx);
        const auto rk=[&](double a,double b,double c,double e){return h*(a+2*b+2*c+e)/6.0;};
        st.x+=rk(k1.dx,k2.dx,k3.dx,k4.dx);
        st.phase+=rk(k1.dphi,k2.dphi,k3.dphi,k4.dphi);
        st.eRad+=rk(k1.dE,k2.dE,k3.dE,k4.dE);
        st.jRad+=rk(k1.dJ,k2.dJ,k3.dJ,k4.dJ);
        st.phase=std::remainder(st.phase,2.0*PI);
        st.t+=h; remain-=h;
        if(st.x>=d.xTransition*(1.0-2e-9)){
            st.x=d.xTransition; enter_transition(in,d,st);
        }
    }
    if(guard>=20000 && remain>0.0){
        // Fail-safe against a pathological tiny-step loop: preserve monotone time
        // and hand the remainder to the strong-field continuation.
        if(st.transitionTime<0.0) enter_transition(in,d,st);
        st.t+=remain;
    }
}

inline State state_at_or_initialized(const BinaryInput& in,const State& src){
    if(src.initialized) return src;
    const double target=std::max(0.0,src.t);
    State q{}; initialize(in,q); if(target>0.0) advance(in,q,target); return q;
}

inline Snapshot snapshot(const BinaryInput& in,const State& src){
    Snapshot s{}; s.d=derive(in); State st=state_at_or_initialized(in,src);
    const double M=s.d.M;
    if(!in.radiationReaction || st.transitionTime<0.0){
        const double x=std::max(1e-12,st.x);
        s.r=M/x; s.phase=st.phase; s.omega=s.d.orientation*std::pow(x,1.5)/M;
        const Rates rr=inspiral_rates(s.d,x,in.radiationReaction);
        s.rdot=in.radiationReaction?(-M*rr.dx/(x*x)):0.0;
        s.gwPower=in.radiationReaction?rr.dE:0.0;
        s.gwJFlux=in.radiationReaction?rr.dJ:0.0;
        s.energyRadiated=st.eRad;
        s.angularMomentumRadiated=st.jRad;
        s.systemMass=s.d.initialADM-st.eRad;
        s.systemJ=in.jTotal-st.jRad;
        s.remnantMass=s.systemMass;
        s.remnantSpin=std::clamp(s.systemJ/std::max(1e-12,s.systemMass*s.systemMass),-0.998,0.998);
        s.mergerBlend=0.0; s.timeSinceTransition=0.0; s.stage=STAGE_INSPIRAL;
        const double exactLoss=pn_binding_energy(M,s.d.eta,s.d.x0)-pn_binding_energy(M,s.d.eta,x);
        s.pnEnergyBalanceResidual=st.eRad-exactLoss;
        const double exactJ=in.spin1z+in.spin2z+s.d.orientation*pn_orbital_J(M,s.d.eta,x);
        s.pnAngularMomentumBalanceResidual=s.systemJ-exactJ;
    }else{
        const double elapsed=std::max(0.0,st.t-st.transitionTime);
        const double tauE=std::max(4.0*M,s.d.qnmTau220);
        const double tauR=std::max(2.0*M,0.55*tauE);
        const double u=elapsed/tauR;
        const double eGeom=std::exp(-0.5*u*u);
        s.mergerBlend=std::clamp(1.0-eGeom,0.0,1.0);
        s.r=st.transitionR*eGeom;
        if(s.r<1e-7*M) s.r=0.0;
        s.rdot=(s.r>0.0)?(-s.r*elapsed/(tauR*tauR)):0.0;
        const double omTr=std::abs(st.transitionOmega);
        const double omPeak=0.5*s.d.qnmOmega220;
        const double gaussInt=tauR*std::sqrt(PI/2.0)*std::erf(elapsed/(std::sqrt(2.0)*tauR));
        const double dphi=omPeak*elapsed-(omPeak-omTr)*gaussInt;
        s.phase=std::remainder(st.transitionPhase+s.d.orientation*dphi,2.0*PI);
        s.omega=s.d.orientation*(omPeak-(omPeak-omTr)*eGeom);

        const double transitionMass=s.d.initialADM-st.transitionERad;
        const double residualE=std::max(0.0,transitionMass-s.d.finalMassFit);
        const double dec=std::exp(-elapsed/tauE);
        s.energyRadiated=st.transitionERad+residualE*(1.0-dec);
        s.gwPower=(residualE/tauE)*dec;
        s.systemMass=s.d.initialADM-s.energyRadiated;
        s.remnantMass=s.systemMass;

        const double Jtr=in.jTotal-st.transitionJRad;
        const double alongTr=s.d.orientation*Jtr;
        const double alongFit=s.d.orientation*s.d.finalJFit;
        const double alongFinal=(alongTr>=0.0)?std::clamp(alongFit,0.0,alongTr):alongTr;
        const double Jfinal=s.d.orientation*alongFinal;
        s.systemJ=Jfinal+(Jtr-Jfinal)*dec;
        s.angularMomentumRadiated=in.jTotal-s.systemJ;
        s.gwJFlux=(Jtr-Jfinal)*dec/tauE;
        s.remnantSpin=std::clamp(s.systemJ/std::max(1e-12,s.systemMass*s.systemMass),-0.998,0.998);
        s.timeSinceTransition=elapsed;
        s.pnEnergyBalanceResidual=st.transitionERad-
            (pn_binding_energy(M,s.d.eta,s.d.x0)-pn_binding_energy(M,s.d.eta,st.transitionX));
        const double exactJtr=in.spin1z+in.spin2z+s.d.orientation*pn_orbital_J(M,s.d.eta,st.transitionX);
        s.pnAngularMomentumBalanceResidual=(in.jTotal-st.transitionJRad)-exactJtr;
        if(s.mergerBlend<0.90) s.stage=STAGE_MERGER;
        else if(dec>1e-3) s.stage=STAGE_RINGDOWN;
        else s.stage=STAGE_REMNANT;
    }

    const double c=std::cos(s.phase),q=std::sin(s.phase); Vec3 er{c,q,0},ep{-q,c,0};
    Vec3 relv=s.rdot*er+(s.r*s.omega)*ep;
    const double f1=in.m2/M,f2=in.m1/M;
    s.x1=(f1*s.r)*er; s.x2=(-f2*s.r)*er; s.v1=f1*relv; s.v2=(-f2)*relv;
    return s;
}

inline double phase_normalized_time_rate(const BinaryInput& in,const State& st,double viewOrbitHz){
    const Snapshot s=snapshot(in,st);
    const double hz=std::max(0.0,viewOrbitHz),w=std::abs(s.omega);
    if(!(w>1e-15)) return 0.0;
    return (2.0*PI*hz)/w;
}

inline double effective_individual_optical_radius(double mSelf,double mOther,double separation,double mergerBlend,double captureU){
    const double b=std::clamp(mergerBlend,0.0,1.0);
    const double oneMinus=std::max(0.0,1.0-b);
    const double d=std::max(1e-9,separation);
    // In the current two-puncture optical ansatz, the local capture surface is
    // approximated by u = m_self/(2 r) + (1-blend) m_other/(2 d) = captureU.
    // The individual surface then fades continuously as the common remnant field takes over.
    const double companionU=oneMinus*mOther/(2.0*d);
    const double denom=std::max(0.05,captureU-companionU);
    const double localIso=mSelf/(2.0*denom);
    return localIso*std::sqrt(oneMinus);
}
inline double effective_remnant_optical_radius(double systemMass,double captureU){
    return std::max(0.0,systemMass)/(2.0*std::max(0.05,captureU));
}
inline double apparent_orbit_hz(const BinaryInput& in,const State& st,double simTimeRate){
    const Snapshot s=snapshot(in,st);
    return std::abs(s.omega)*std::max(0.0,simTimeRate)/(2.0*PI);
}
inline double realized_total_J(const BinaryInput& in){return derive(in).Lorb+in.spin1z+in.spin2z;}
}
