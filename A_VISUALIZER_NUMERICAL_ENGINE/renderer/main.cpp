#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

#include "command_protocol.h"
#include "binary_driver.h"
#include "control_panel.h"

using Microsoft::WRL::ComPtr;
using namespace bkqrgrbin;
using namespace bkqrgr_driver;
using namespace bkqrgr_ui;

namespace {
constexpr UINT FRAME_COUNT=3;
constexpr float PI_F=3.14159265358979323846f;

void throw_hr(HRESULT hr,const char* what){
    if(FAILED(hr)){
        std::ostringstream os; os<<what<<" failed: HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr);
        throw std::runtime_error(os.str());
    }
}
float clampf(float x,float lo,float hi){ return std::max(lo,std::min(hi,x)); }
float wrap_pi_f(float x){ while(x>PI_F)x-=2.0f*PI_F; while(x<-PI_F)x+=2.0f*PI_F; return x; }

std::string narrow(const wchar_t* ws){
    if(!ws) return {};
    int n=WideCharToMultiByte(CP_UTF8,0,ws,-1,nullptr,0,nullptr,nullptr);
    std::string s(n?static_cast<size_t>(n):0,'\0');
    if(n>1){ WideCharToMultiByte(CP_UTF8,0,ws,-1,s.data(),n,nullptr,nullptr); s.resize(static_cast<size_t>(n-1)); }
    return s;
}

volatile const char* g_runtime_stage = "STARTUP";
volatile std::uint64_t g_runtime_frame = 0;
LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* ep){
    const unsigned long code=(ep&&ep->ExceptionRecord)?ep->ExceptionRecord->ExceptionCode:0ul;
    const void* address=(ep&&ep->ExceptionRecord)?ep->ExceptionRecord->ExceptionAddress:nullptr;
    std::fprintf(stderr,"UNHANDLED_EXCEPTION_CODE=0x%08lX ADDRESS=%p STAGE=%s FRAME=%llu\n",
                 code,address,g_runtime_stage?g_runtime_stage:"UNKNOWN",
                 static_cast<unsigned long long>(g_runtime_frame));
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

LRESULT CALLBACK render_child_proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT ps{};BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}
    }
    return DefWindowProcW(h,m,w,l);
}

LRESULT CALLBACK wnd_proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
        case WM_CLOSE: DestroyWindow(h); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_KEYDOWN: if(w==VK_ESCAPE){DestroyWindow(h);return 0;} break;
        case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(h,m,w,l);
}

Vec3 cross3(Vec3 a,Vec3 b){ return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec3 unit3(Vec3 a){ double n=norm(a); return (n>1e-12)?((1.0/n)*a):Vec3{0,0,0}; }

struct SimulationState{
    double time=0.0;
    float physicalRate=1.0f;          // M / wall-second when phase normalization is disabled
    float viewOrbitHz=1.0f;           // target apparent orbital rotations / wall-second
    bool phaseNormalized=true;        // default: keep current apparent orbital rotation near viewOrbitHz
    bool paused=false;
};
struct CameraState{
    float r=24.0f;
    float theta=68.0f*PI_F/180.0f;
    float phi=0.0f;
    float exposure=2.4f;
    float fovYDeg=55.0f;
    std::uint32_t flags=0u;
};

struct QualityState{
    int pixelStride=2;
    int baseSPP=1;
    int criticalExtraSPP=2;
    int maxSteps=5000;
    float integratorQuality=1.0f;
    float maxStep=0.045f;
    float minStep=0.0015f;
    float criticalGrad=1.8f;
};
struct BinaryVisualState{
    BinaryInput input{};
    State dyn{};
    int renderMode=3; // 0 sky, 1 optical field, 2 capture IDs, 3 mirrored point-source detector, 4 mirrored cylindrical-beam detector
    float captureU=0.94f;
    float maxRayTravel=150.0f;
    float skyScale=70.0f;
    float fieldGain=1.0f;
    bool planeOverlay=true;
    float planeOverlayScale=1.0f;
    float detectorDistance=26.0f;
    float sourceDistance=26.0f;
    float detectorHalfHeight=10.0f;
    float pointSigma=0.35f;
    float beamRadius=1.5f;
    float beamSoftness=0.35f;
    bool showMirroredSourceSphere=true;
    float sourceSphereRadius=1.0f;
};

struct WaveformSample{
    double tObs=0.0;
    double hPlus=0.0;
    double hCross=0.0;
    double hDet=0.0;
    int stage=STAGE_INSPIRAL;
};

struct WaveformState{
    bool showPlus=true;
    bool showCross=true;
    bool showDetector=true;
    float inclinationDeg=35.0f;
    float polarizationDeg=20.0f;
    float observerDistance=120.0f;
    float amplitudeGain=10.0f;
    std::deque<WaveformSample> history{};
    size_t maxSamples=1400;
    double lastSimTime=-1.0;
};

WaveformSample induced_waveform_sample(const Snapshot& snap,const WaveformState& wf){
    WaveformSample out{};
    out.tObs=snap.d.M>0.0?snap.phase:0.0;
    out.stage=snap.stage;
    const double inc=wf.inclinationDeg*PI/180.0;
    const double ci=std::cos(inc);
    const double absOm=std::abs(snap.omega);
    const double x=std::pow(std::max(1e-12,snap.d.M*absOm),2.0/3.0);
    double amp=(4.0*snap.d.eta*x/std::max(1e-9,(double)wf.observerDistance))*std::max(0.01,(double)wf.amplitudeGain);
    if(snap.stage!=STAGE_INSPIRAL){
        const double tau=std::max(1e-9,snap.d.qnmTau220);
        const double decay=std::exp(-std::max(0.0,snap.timeSinceTransition)/tau);
        amp*= (1.0+1.35*snap.mergerBlend)*decay;
    }
    const double ph=2.0*snap.phase;
    out.hPlus = amp*0.5*(1.0+ci*ci)*std::cos(ph);
    out.hCross = amp*(ci)*std::sin(ph);
    const double psi=wf.polarizationDeg*PI/180.0;
    const double Fp=std::cos(2.0*psi);
    const double Fx=std::sin(2.0*psi);
    out.hDet=Fp*out.hPlus+Fx*out.hCross;
    return out;
}

void waveform_reset(WaveformState& wf){ wf.history.clear(); wf.lastSimTime=-1.0; }

void waveform_push(WaveformState& wf,double simTime,const Snapshot& snap){
    if(wf.lastSimTime>=0.0 && simTime+1e-9<wf.lastSimTime) waveform_reset(wf);
    if(!wf.history.empty() && std::abs(simTime-wf.lastSimTime)<=1e-9) return;
    WaveformSample samp=induced_waveform_sample(snap,wf);
    samp.tObs=simTime;
    wf.history.push_back(samp);
    while(wf.history.size()>wf.maxSamples) wf.history.pop_front();
    wf.lastSimTime=simTime;
}

void draw_waveform_panel(HDC hdc,const RECT& rc,const WaveformState& wf){
    HBRUSH bg=CreateSolidBrush(RGB(8,8,14)); FillRect(hdc,&rc,bg); DeleteObject(bg);
    SetBkMode(hdc,TRANSPARENT); SetTextColor(hdc,RGB(230,230,240));
    RECT inner=rc; inner.left+=44; inner.right-=10; inner.top+=22; inner.bottom-=22;
    HPEN gridPen=CreatePen(PS_SOLID,1,RGB(50,55,70));
    HPEN axisPen=CreatePen(PS_SOLID,1,RGB(130,140,160));
    HPEN plusPen=CreatePen(PS_SOLID,1,RGB(120,170,255));
    HPEN crossPen=CreatePen(PS_SOLID,1,RGB(255,120,170));
    HPEN detPen=CreatePen(PS_SOLID,2,RGB(255,215,90));
    const int w=std::max(1,static_cast<int>(inner.right-inner.left));
    const int h=std::max(1,static_cast<int>(inner.bottom-inner.top));
    for(int i=0;i<=4;++i){ int y=inner.top + (h*i)/4; SelectObject(hdc,gridPen); MoveToEx(hdc,inner.left,y,nullptr); LineTo(hdc,inner.right,y); }
    for(int i=0;i<=6;++i){ int x=inner.left + (w*i)/6; SelectObject(hdc,gridPen); MoveToEx(hdc,x,inner.top,nullptr); LineTo(hdc,x,inner.bottom); }
    SelectObject(hdc,axisPen); Rectangle(hdc,inner.left,inner.top,inner.right,inner.bottom);
    const wchar_t* waveTitle=L"Induced wave signal at distant observer (retarded-time view, constant delay removed)"; TextOutW(hdc,10,4,waveTitle,lstrlenW(waveTitle));
    if(wf.history.size()>=2){
        double amax=1e-9;
        for(const auto& s:wf.history){ if(wf.showPlus) amax=std::max(amax,std::abs(s.hPlus)); if(wf.showCross) amax=std::max(amax,std::abs(s.hCross)); if(wf.showDetector) amax=std::max(amax,std::abs(s.hDet)); }
        amax*=1.15;
        auto mapx=[&](size_t i){ return inner.left + int((double)i*double(w-1)/double(std::max<size_t>(1,wf.history.size()-1))); };
        auto mapy=[&](double v){ return inner.top + h/2 - int((v/amax)*(0.46*h)); };
        std::vector<POINT> pts; pts.reserve(wf.history.size());
        auto drawSeries=[&](auto getter, HPEN pen){ pts.clear(); for(size_t i=0;i<wf.history.size();++i){ pts.push_back({mapx(i), mapy(getter(wf.history[i]))}); } SelectObject(hdc,pen); if(pts.size()>=2) Polyline(hdc,pts.data(),(int)pts.size()); };
        if(wf.showPlus) drawSeries([](const WaveformSample& s){return s.hPlus;}, plusPen);
        if(wf.showCross) drawSeries([](const WaveformSample& s){return s.hCross;}, crossPen);
        if(wf.showDetector) drawSeries([](const WaveformSample& s){return s.hDet;}, detPen);
        const int y0=mapy(0.0); SelectObject(hdc,axisPen); MoveToEx(hdc,inner.left,y0,nullptr); LineTo(hdc,inner.right,y0);
        std::wostringstream os; os<<std::fixed<<std::setprecision(3)<<L"incl="<<wf.inclinationDeg<<L" deg  psi="<<wf.polarizationDeg<<L" deg  R="<<wf.observerDistance<<L" M  gain="<<wf.amplitudeGain<<L"  scale=±"<<amax;
        const std::wstring info=os.str(); TextOutW(hdc,10,rc.bottom-18,info.c_str(),(int)info.size());
        const wchar_t* legend=L"yellow=h_obs  blue=h+  pink=hx"; TextOutW(hdc,inner.right-176,4,legend,lstrlenW(legend));
    }else{
        const wchar_t* emptyMsg=L"Waveform history will accumulate as the binary evolves."; TextOutW(hdc,10,rc.top+28,emptyMsg,lstrlenW(emptyMsg));
    }
    DeleteObject(gridPen); DeleteObject(axisPen); DeleteObject(plusPen); DeleteObject(crossPen); DeleteObject(detPen);
}

LRESULT CALLBACK waveform_child_proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{ PAINTSTRUCT ps{}; BeginPaint(h,&ps); RECT rc{}; GetClientRect(h,&rc); auto* wf=reinterpret_cast<WaveformState*>(GetWindowLongPtrW(h,GWLP_USERDATA)); if(wf) draw_waveform_panel(ps.hdc,rc,*wf); EndPaint(h,&ps); return 0; }
    }
    return DefWindowProcW(h,m,w,l);
}

void global_restart(CameraState& cam,SimulationState& sim,BinaryVisualState& bin,QualityState& q,WaveformState* wave=nullptr){
    // Rewind the current experiment without changing any user-selected parameters.
    // Camera, physical inputs, optical/source controls, quality controls, and
    // view-clock settings are intentionally preserved.
    (void)cam;
    (void)q;
    bin.dyn=State{};
    if(wave) waveform_reset(*wave);
    sim.time=0.0;
    sim.paused=true; // Restart always returns to t=0 in a stable paused inspection state.
}

std::wstring render_mode_name(double v){
    const int m=std::clamp((int)std::lround(v),0,4);
    switch(m){
        case 0: return L"Sky";
        case 1: return L"Optical field";
        case 2: return L"Capture IDs";
        case 3: return L"Detector: mirrored point source";
        default: return L"Detector: mirrored cylindrical beam";
    }
}

bool commit_physical_input(BinaryVisualState& bin,SimulationState& sim,const BinaryInput& candidate) noexcept {
    try {
        (void)derive(candidate);
        bin.input=candidate;
        bin.dyn=State{};
        sim.time=0.0;
        return true;
    } catch(const std::exception& e) {
        std::cerr << "PHYSICAL_INPUT_REJECTED=" << e.what() << "\n";
        return false;
    } catch(...) {
        std::cerr << "PHYSICAL_INPUT_REJECTED=UNKNOWN\n";
        return false;
    }
}

double initial_separation_over_M(const BinaryVisualState& bin) noexcept {
    try {
        const auto d=derive(bin.input);
        return d.r0/std::max(1e-12,d.M);
    } catch(...) {
        return 12.0;
    }
}

bool commit_initial_separation_over_M(BinaryVisualState& bin,SimulationState& sim,double rOverM) noexcept {
    try {
        if(!(rOverM>0.0)) throw std::runtime_error("initial separation r0/M must be positive");
        BinaryInput c=bin.input;
        const double M=c.m1+c.m2;
        const double mu=c.m1*c.m2/M;
        const double eta=mu/M;
        const double x=1.0/rOverM;
        const double xMeco=pn_meco_x(M,eta);
        const double xTransition=std::min(0.92*xMeco,1.0/c.minSeparationFactor);
        if(x>xTransition*(1.0+1e-12))
            throw std::runtime_error("requested initial separation is inside the supported 3PN circular branch; increase r0/M");
        const double oldL=c.jTotal-c.spin1z-c.spin2z;
        const int orientation=(oldL<0.0)?-1:1;
        const double Lmag=pn_orbital_J(M,eta,x);
        c.jTotal=c.spin1z+c.spin2z+orientation*Lmag;
        return commit_physical_input(bin,sim,c);
    } catch(const std::exception& e) {
        std::cerr << "INITIAL_SEPARATION_REJECTED=" << e.what() << "\n";
        return false;
    } catch(...) {
        std::cerr << "INITIAL_SEPARATION_REJECTED=UNKNOWN\n";
        return false;
    }
}

void configure_camera_panel(ControlPanel& panel,CameraState& cam){
    panel.add_section(L"CENTER-LOCKED CAMERA");
    panel.add_slider({L"R",5.0,100.0,0,24.0,SliderScale::Logarithmic,[&]{return(double)cam.r;},[&](double v){cam.r=(float)v;},[](double v){return format_fixed(v,2,L" M");}});
    panel.add_slider({L"theta",5.0,175.0,0.5,68.0,SliderScale::Linear,[&]{return(double)cam.theta*180.0/PI;},[&](double v){cam.theta=(float)(v*PI/180.0);},[](double v){return format_fixed(v,1,L" deg");}});
    panel.add_slider({L"phi",-180.0,180.0,0.5,0.0,SliderScale::Linear,[&]{return(double)cam.phi*180.0/PI;},[&](double v){cam.phi=wrap_pi_f((float)(v*PI/180.0));},[](double v){return format_fixed(v,1,L" deg");}});
}

void configure_control_panel(ControlPanel& panel,CameraState& cam,SimulationState& sim,BinaryVisualState& bin,QualityState& q,WaveformState& wave){
    panel.add_section(L"SIMULATION / VIEW CLOCK");
    panel.add_toggle({L"Paused",false,[&]{return sim.paused;},[&](bool v){sim.paused=v;},L"PAUSED",L"RUNNING"});
    panel.add_toggle({L"Time normalization",true,[&]{return sim.phaseNormalized;},[&](bool v){sim.phaseNormalized=v;},L"ON (phase-normalized)",L"OFF (physical time)"});
    panel.add_slider({L"Visual orbital rate",0.05,5.0,0,1.0,SliderScale::Logarithmic,[&]{return(double)sim.viewOrbitHz;},[&](double v){sim.viewOrbitHz=(float)v;},[](double v){return format_fixed(v,2,L" Hz");}});
    panel.add_slider({L"Physical time rate",0.02,200.0,0,1.0,SliderScale::Logarithmic,[&]{return(double)sim.physicalRate;},[&](double v){sim.physicalRate=(float)v;},[](double v){return format_fixed(v,2,L" M/s");}});
    panel.add_slider({L"Render mode",0,4,1,3,SliderScale::Linear,[&]{return(double)bin.renderMode;},[&](double v){bin.renderMode=std::clamp((int)std::lround(v),0,4); if(bin.renderMode==0) cam.flags=FLAG_SKY; else if(bin.renderMode==1) cam.flags=FLAG_FIELD_DEBUG; else if(bin.renderMode==2) cam.flags=FLAG_CAPTURE_DEBUG; else cam.flags=0u;},[](double v){return render_mode_name(v);}});

    panel.add_section(L"VIEW / DISPLAY");
    panel.add_slider({L"Field of view",25.0,100.0,0.5,55.0,SliderScale::Linear,[&]{return(double)cam.fovYDeg;},[&](double v){cam.fovYDeg=(float)v;},[](double v){return format_fixed(v,1,L" deg");}});
    panel.add_slider({L"Exposure",0.1,12.0,0,2.4,SliderScale::Logarithmic,[&]{return(double)cam.exposure;},[&](double v){cam.exposure=(float)v;},[](double v){return format_fixed(v,2);}});

    panel.add_section(L"PHYSICAL INITIAL CONDITIONS");
    panel.add_slider({L"Mass M1",0.05,5.0,0.01,0.5,SliderScale::Logarithmic,[&]{return bin.input.m1;},[&bin,&sim](double v){auto c=bin.input;c.m1=v;commit_physical_input(bin,sim,c);},[](double v){return format_fixed(v,3,L" M");}});
    panel.add_slider({L"Mass M2",0.05,5.0,0.01,0.5,SliderScale::Logarithmic,[&]{return bin.input.m2;},[&bin,&sim](double v){auto c=bin.input;c.m2=v;commit_physical_input(bin,sim,c);},[](double v){return format_fixed(v,3,L" M");}});
    panel.add_slider({L"Initial separation r0 / M",6.0,60.0,0.05,12.0,SliderScale::Logarithmic,[&]{return initial_separation_over_M(bin);},[&bin,&sim](double v){commit_initial_separation_over_M(bin,sim,v);},[](double v){return format_fixed(v,2,L" M apart");}});
    panel.add_slider({L"Total angular momentum J",-5.0,5.0,0.002,0.9944154929906234,SliderScale::Linear,[&]{return bin.input.jTotal;},[&bin,&sim](double v){auto c=bin.input;c.jTotal=v;commit_physical_input(bin,sim,c);},[](double v){return format_fixed(v,4,L" M^2");}});
    panel.add_slider({L"Spin S1z (Kerr-bounded)",-2.0,2.0,0.002,0.0,SliderScale::Linear,[&]{return bin.input.spin1z;},[&bin,&sim](double v){auto c=bin.input;c.spin1z=v;commit_physical_input(bin,sim,c);},[](double v){return format_fixed(v,4,L" M^2");}});
    panel.add_slider({L"Spin S2z (Kerr-bounded)",-2.0,2.0,0.002,0.0,SliderScale::Linear,[&]{return bin.input.spin2z;},[&bin,&sim](double v){auto c=bin.input;c.spin2z=v;commit_physical_input(bin,sim,c);},[](double v){return format_fixed(v,4,L" M^2");}});
    panel.add_slider({L"Initial orbital phase",-180.0,180.0,0.5,0.0,SliderScale::Linear,[&]{return bin.input.phase0*180.0/PI;},[&bin,&sim](double v){auto c=bin.input;c.phase0=std::remainder(v*PI/180.0,2.0*PI);commit_physical_input(bin,sim,c);},[](double v){return format_fixed(v,1,L" deg");}});
    panel.add_toggle({L"Radiation reaction",true,[&]{return bin.input.radiationReaction;},[&bin,&sim](bool v){auto c=bin.input;c.radiationReaction=v;commit_physical_input(bin,sim,c);},L"ON",L"OFF"});
    panel.add_slider({L"PN safety r / M",1.8,6.0,0.02,2.6,SliderScale::Linear,[&]{return bin.input.minSeparationFactor;},[&bin,&sim](double v){auto c=bin.input;c.minSeparationFactor=v;commit_physical_input(bin,sim,c);},[](double v){return format_fixed(v,2);}});

    panel.add_section(L"OPTICAL FIELD / PLANE");
    panel.add_slider({L"Capture threshold u",0.60,0.995,0.001,0.94,SliderScale::Linear,[&]{return(double)bin.captureU;},[&](double v){bin.captureU=(float)v;},[](double v){return format_fixed(v,3);}});
    panel.add_slider({L"Max ray travel",30.0,400.0,1.0,150.0,SliderScale::Logarithmic,[&]{return(double)bin.maxRayTravel;},[&](double v){bin.maxRayTravel=(float)v;},[](double v){return format_fixed(v,0,L" M");}});
    panel.add_slider({L"Sky scale",10.0,200.0,1.0,70.0,SliderScale::Logarithmic,[&]{return(double)bin.skyScale;},[&](double v){bin.skyScale=(float)v;},[](double v){return format_fixed(v,0);}});
    panel.add_slider({L"Field gain",0.10,5.0,0,1.0,SliderScale::Logarithmic,[&]{return(double)bin.fieldGain;},[&](double v){bin.fieldGain=(float)v;},[](double v){return format_fixed(v,2);}});
    panel.add_toggle({L"Orbital-plane inset",true,[&]{return bin.planeOverlay;},[&](bool v){bin.planeOverlay=v;},L"ON",L"OFF"});
    panel.add_slider({L"Plane inset scale",0.5,2.0,0.05,1.0,SliderScale::Linear,[&]{return(double)bin.planeOverlayScale;},[&](double v){bin.planeOverlayScale=(float)v;},[](double v){return format_fixed(v,2);}});

    panel.add_section(L"SOURCE / DETECTOR MODES");
    panel.add_slider({L"Detector half-height",1.0,30.0,0,10.0,SliderScale::Logarithmic,[&]{return(double)bin.detectorHalfHeight;},[&](double v){bin.detectorHalfHeight=(float)v;},[](double v){return format_fixed(v,2,L" M");}});
    panel.add_slider({L"Point-source size",0.02,2.0,0,0.35,SliderScale::Logarithmic,[&]{return(double)bin.pointSigma;},[&](double v){bin.pointSigma=(float)v;},[](double v){return format_fixed(v,3,L" M");}});
    panel.add_slider({L"Beam radius",0.05,8.0,0,1.5,SliderScale::Logarithmic,[&]{return(double)bin.beamRadius;},[&](double v){bin.beamRadius=(float)v;},[](double v){return format_fixed(v,3,L" M");}});
    panel.add_slider({L"Beam softness",0.01,2.0,0,0.35,SliderScale::Logarithmic,[&]{return(double)bin.beamSoftness;},[&](double v){bin.beamSoftness=(float)v;},[](double v){return format_fixed(v,3,L" M");}});
    panel.add_toggle({L"Show mirrored source sphere",true,[&]{return bin.showMirroredSourceSphere;},[&](bool v){bin.showMirroredSourceSphere=v;},L"ON",L"OFF"});
    panel.add_slider({L"Source sphere radius",0.05,6.0,0,1.0,SliderScale::Logarithmic,[&]{return(double)bin.sourceSphereRadius;},[&](double v){bin.sourceSphereRadius=(float)v;},[](double v){return format_fixed(v,3,L" M");}});

    panel.add_section(L"INDUCED GW SIGNAL");
    panel.add_slider({L"Observer inclination",0.0,180.0,0.5,35.0,SliderScale::Linear,[&]{return(double)wave.inclinationDeg;},[&](double v){wave.inclinationDeg=(float)v;},[](double v){return format_fixed(v,1,L" deg");}});
    panel.add_slider({L"Detector polarization psi",0.0,180.0,0.5,20.0,SliderScale::Linear,[&]{return(double)wave.polarizationDeg;},[&](double v){wave.polarizationDeg=(float)v;},[](double v){return format_fixed(v,1,L" deg");}});
    panel.add_slider({L"Observer distance",20.0,500.0,0,120.0,SliderScale::Logarithmic,[&]{return(double)wave.observerDistance;},[&](double v){wave.observerDistance=(float)v;},[](double v){return format_fixed(v,1,L" M");}});
    panel.add_slider({L"Wave amplitude gain",1.0,100.0,0,10.0,SliderScale::Logarithmic,[&]{return(double)wave.amplitudeGain;},[&](double v){wave.amplitudeGain=(float)v;},[](double v){return format_fixed(v,2);}});

    panel.add_section(L"RAY QUALITY");
    panel.add_slider({L"Pixel stride",1,4,1,2,SliderScale::Linear,[&]{return(double)q.pixelStride;},[&](double v){q.pixelStride=std::clamp((int)std::lround(v),1,4);},[](double v){return format_fixed(std::round(v),0);}});
    panel.add_slider({L"Base samples / pixel",1,8,1,1,SliderScale::Linear,[&]{return(double)q.baseSPP;},[&](double v){q.baseSPP=std::clamp((int)std::lround(v),1,8);},[](double v){return format_fixed(std::round(v),0);}});
    panel.add_slider({L"Critical extra SPP",0,12,1,2,SliderScale::Linear,[&]{return(double)q.criticalExtraSPP;},[&](double v){q.criticalExtraSPP=std::clamp((int)std::lround(v),0,12);},[](double v){return format_fixed(std::round(v),0);}});
    panel.add_slider({L"Max integration steps",128,12000,128,5000,SliderScale::Logarithmic,[&]{return(double)q.maxSteps;},[&](double v){q.maxSteps=std::clamp((int)std::lround(v),128,12000);},[](double v){return format_fixed(std::round(v),0);}});
    panel.add_slider({L"Integrator quality",0.5,4.0,0.05,1.0,SliderScale::Linear,[&]{return(double)q.integratorQuality;},[&](double v){q.integratorQuality=(float)v;},[](double v){return format_fixed(v,2);}});
    panel.add_slider({L"Maximum ray step",0.002,0.20,0,0.045,SliderScale::Logarithmic,[&]{return(double)q.maxStep;},[&](double v){q.maxStep=(float)std::max(v,(double)q.minStep);},[](double v){return format_fixed(v,4);}});
    panel.add_slider({L"Minimum ray step",0.0002,0.05,0,0.0015,SliderScale::Logarithmic,[&]{return(double)q.minStep;},[&](double v){q.minStep=(float)std::min(v,(double)q.maxStep);},[](double v){return format_fixed(v,5);}});
    panel.add_slider({L"Critical gradient",0.05,8.0,0,1.8,SliderScale::Logarithmic,[&]{return(double)q.criticalGrad;},[&](double v){q.criticalGrad=(float)v;},[](double v){return format_fixed(v,2);}});
}

struct CameraFrame{ Vec3 pos,right,up,forward; };
CameraFrame camera_frame(const CameraState& c){
    const double st=std::sin(c.theta), ct=std::cos(c.theta), cp=std::cos(c.phi), sp=std::sin(c.phi);
    Vec3 pos{c.r*st*cp,c.r*st*sp,c.r*ct};
    Vec3 forward=unit3((-1.0)*pos);
    Vec3 worldUp{0,0,1};
    Vec3 right=unit3(cross3(forward,worldUp));
    if(norm(right)<1e-8) right={1,0,0};
    Vec3 up=unit3(cross3(right,forward));
    return {pos,right,up,forward};
}

double effective_sim_rate_mps(const SimulationState& sim,const BinaryVisualState& bin){
    if(!sim.phaseNormalized) return std::max(0.0,(double)sim.physicalRate);
    return phase_normalized_time_rate(bin.input,bin.dyn,std::max(0.0,(double)sim.viewOrbitHz));
}

void update_states(CameraState& cam,SimulationState& sim,BinaryVisualState& bin,QualityState& q,WaveformState& wave,float realDt,bool keys){
    if(!keys) return;
    if(GetAsyncKeyState(VK_SPACE)&1) sim.paused=!sim.paused;
    if(GetAsyncKeyState(VK_OEM_MINUS)&0x8000){
        if(sim.phaseNormalized) sim.viewOrbitHz=std::max(0.05f,sim.viewOrbitHz*(1.0f-1.2f*realDt));
        else sim.physicalRate=std::max(0.02f,sim.physicalRate*(1.0f-1.2f*realDt));
    }
    if(GetAsyncKeyState(VK_OEM_PLUS)&0x8000){
        if(sim.phaseNormalized) sim.viewOrbitHz=std::min(5.0f,sim.viewOrbitHz*(1.0f+1.2f*realDt));
        else sim.physicalRate=std::min(200.0f,sim.physicalRate*(1.0f+1.2f*realDt));
    }
    if((GetAsyncKeyState(VK_F5)&1) || (GetAsyncKeyState('T')&1)){global_restart(cam,sim,bin,q,&wave);}
    if(GetAsyncKeyState('W')&0x8000) cam.r=std::max(5.0f,cam.r-10.0f*realDt);
    if(GetAsyncKeyState('S')&0x8000) cam.r=std::min(100.0f,cam.r+10.0f*realDt);
    if(GetAsyncKeyState('A')&0x8000) cam.phi=wrap_pi_f(cam.phi+0.75f*realDt);
    if(GetAsyncKeyState('D')&0x8000) cam.phi=wrap_pi_f(cam.phi-0.75f*realDt);
    if(GetAsyncKeyState('Q')&0x8000) cam.theta=clampf(cam.theta-0.55f*realDt,0.12f,PI_F-0.12f);
    if(GetAsyncKeyState('E')&0x8000) cam.theta=clampf(cam.theta+0.55f*realDt,0.12f,PI_F-0.12f);
    if(GetAsyncKeyState('C')&1){bin.input.radiationReaction=!bin.input.radiationReaction;bin.dyn=State{};sim.time=0;}
    if(GetAsyncKeyState('R')&1){bin=BinaryVisualState{};sim.time=0.0; waveform_reset(wave);}
    if(GetAsyncKeyState('1')&1){bin.renderMode=0;cam.flags=FLAG_SKY;}
    if(GetAsyncKeyState('2')&1){bin.renderMode=1;cam.flags=FLAG_FIELD_DEBUG;}
    if(GetAsyncKeyState('3')&1){bin.renderMode=2;cam.flags=FLAG_CAPTURE_DEBUG;}
    if(GetAsyncKeyState('4')&1){bin.renderMode=3;cam.flags=0u;}
    if(GetAsyncKeyState('5')&1){bin.renderMode=4;cam.flags=0u;}
}

BinaryFramePacket make_packet(const CameraState& cam,const SimulationState& sim,const BinaryVisualState& bin,const QualityState& q,UINT W,UINT H,std::uint32_t frameId,float simDt,float effectiveRateMps){
    BinaryFramePacket p{}; auto cf=camera_frame(cam); auto s=snapshot(bin.input,bin.dyn);
    const std::uint32_t flags=(bin.renderMode==0)?FLAG_SKY:((bin.renderMode==1)?FLAG_FIELD_DEBUG:((bin.renderMode==2)?FLAG_CAPTURE_DEBUG:0u));
    p.header0[0]=GR_BINARY_MAGIC;p.header0[1]=GR_BINARY_PROTOCOL_VERSION;p.header0[2]=flags;p.header0[3]=(std::uint32_t)std::clamp(bin.renderMode,0,4);
    p.header1[0]=W;p.header1[1]=H;p.header1[2]=(std::uint32_t)std::clamp(q.maxSteps,128,12000);p.header1[3]=frameId;
    p.camera0[0]=(float)cf.pos.x;p.camera0[1]=(float)cf.pos.y;p.camera0[2]=(float)cf.pos.z;p.camera0[3]=cam.fovYDeg*PI_F/180.0f;
    p.camRight[0]=(float)cf.right.x;p.camRight[1]=(float)cf.right.y;p.camRight[2]=(float)cf.right.z;
    p.camUp[0]=(float)cf.up.x;p.camUp[1]=(float)cf.up.y;p.camUp[2]=(float)cf.up.z;
    p.camForward[0]=(float)cf.forward.x;p.camForward[1]=(float)cf.forward.y;p.camForward[2]=(float)cf.forward.z;
    p.render0[0]=cam.exposure;p.render0[1]=(float)sim.time;p.render0[2]=effectiveRateMps;p.render0[3]=simDt;
    p.binary0[0]=(float)bin.input.m1;p.binary0[1]=(float)bin.input.m2;p.binary0[2]=(float)s.r;p.binary0[3]=(float)s.phase;
    p.binary1[0]=(float)s.d.eta;p.binary1[1]=(float)s.omega;p.binary1[2]=(float)s.rdot;p.binary1[3]=bin.input.radiationReaction?1.0f:0.0f;
    const float rObj1=(float)effective_individual_optical_radius(bin.input.m1,bin.input.m2,s.r,s.mergerBlend,bin.captureU);
    const float rObj2=(float)effective_individual_optical_radius(bin.input.m2,bin.input.m1,s.r,s.mergerBlend,bin.captureU);
    p.binary2[0]=rObj1;p.binary2[1]=bin.captureU;p.binary2[2]=bin.maxRayTravel;p.binary2[3]=(bin.renderMode<3)?bin.skyScale:bin.detectorHalfHeight;
    p.quality0[0]=clampf(q.integratorQuality,0.5f,4.0f);p.quality0[1]=(float)std::clamp(q.baseSPP,1,8);p.quality0[2]=(float)std::clamp(q.criticalExtraSPP,0,12);p.quality0[3]=(float)std::clamp(q.pixelStride,1,4);
    p.quality1[0]=clampf(q.maxStep,0.002f,0.20f);p.quality1[1]=clampf(q.minStep,0.0002f,p.quality1[0]);p.quality1[2]=std::max(0.05f,q.criticalGrad);p.quality1[3]=rObj2;
    p.visual0[0]=(float)s.mergerBlend;p.visual0[1]=(float)s.gwPower;p.visual0[2]=(bin.renderMode==3)?bin.pointSigma:((bin.renderMode==4)?bin.beamRadius:bin.fieldGain);p.visual0[3]=(float)s.timeSinceTransition;
    p.visual1[0]=(bin.renderMode<3)?(bin.planeOverlay?1.0f:0.0f):cam.r;p.visual1[1]=(bin.renderMode<3)?bin.planeOverlayScale:cam.r;p.visual1[2]=(float)s.systemMass;p.visual1[3]=(float)s.remnantSpin;
    p.reserved0[0]=(bin.renderMode==4)?bin.beamRadius:(bin.renderMode==3?bin.pointSigma:(float)s.d.r0);p.reserved0[1]=(bin.renderMode==4)?bin.beamSoftness:(float)s.energyRadiated;p.reserved0[2]=bin.showMirroredSourceSphere?1.0f:0.0f;p.reserved0[3]=bin.sourceSphereRadius;
    p.reserved1[0]=(float)s.d.qnmOmega220;p.reserved1[1]=(float)s.d.qnmTau220;p.reserved1[2]=(float)s.d.rTransition;p.reserved1[3]=(float)(s.d.orientation*std::pow(s.d.xTransition,1.5)/s.d.M);
    return p;
}

struct FrameContext {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12Resource> upload;
    ComPtr<ID3D12Resource> gpuCB;
    std::uint8_t* mapped=nullptr;
    UINT64 fenceValue=0;
    bool gpuCBInCopyDest=true;
};

class Dx12Engine {
public:
    Dx12Engine(HWND hwnd, UINT surfaceWidth, UINT surfaceHeight) : hwnd_(hwnd), surfaceW_(surfaceWidth), surfaceH_(surfaceHeight) { init(); }
    ~Dx12Engine() {
        try { wait_idle(); } catch(...) {}
        for(auto& f:frames_) if(f.upload && f.mapped) f.upload->Unmap(0,nullptr);
        if(fenceEvent_) CloseHandle(fenceEvent_);
    }

    void render(const BinaryFramePacket& packet) {
        const UINT fi=swap_->GetCurrentBackBufferIndex();
        FrameContext& fc=frames_[fi];
        wait_frame(fc);
        std::memcpy(fc.mapped,&packet,sizeof(packet));

        throw_hr(fc.allocator->Reset(),"allocator Reset");
        throw_hr(cmd_->Reset(fc.allocator.Get(),pso_.Get()),"command list Reset");

        if(!fc.gpuCBInCopyDest) {
            auto b=transition(fc.gpuCB.Get(),D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,D3D12_RESOURCE_STATE_COPY_DEST);
            cmd_->ResourceBarrier(1,&b); fc.gpuCBInCopyDest=true;
        }
        cmd_->CopyBufferRegion(fc.gpuCB.Get(),0,fc.upload.Get(),0,sizeof(BinaryFramePacket));
        {
            auto b=transition(fc.gpuCB.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            cmd_->ResourceBarrier(1,&b); fc.gpuCBInCopyDest=false;
        }

        ID3D12DescriptorHeap* heaps[]={uavHeap_.Get()};
        cmd_->SetDescriptorHeaps(1,heaps);
        cmd_->SetComputeRootSignature(rootSig_.Get());
        cmd_->SetComputeRootConstantBufferView(0,fc.gpuCB->GetGPUVirtualAddress());
        cmd_->SetComputeRootDescriptorTable(1,uavHeap_->GetGPUDescriptorHandleForHeapStart());
        const UINT pixelStride=std::max(1u,std::min(4u,static_cast<UINT>(std::lround(packet.quality0[3]))));
        const UINT logicalW=std::max(1u,packet.header1[0]);
        const UINT logicalH=std::max(1u,packet.header1[1]);
        const UINT renderW=(logicalW+pixelStride-1)/pixelStride;
        const UINT renderH=(logicalH+pixelStride-1)/pixelStride;
        cmd_->Dispatch((renderW+7)/8,(renderH+7)/8,1);

        D3D12_RESOURCE_BARRIER uav{};
        uav.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uav.UAV.pResource=output_.Get();
        cmd_->ResourceBarrier(1,&uav);

        auto bo=transition(output_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
        auto bb=transition(back_[fi].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_RESOURCE_BARRIER bs[2]={bo,bb};
        cmd_->ResourceBarrier(2,bs);
        cmd_->CopyResource(back_[fi].Get(),output_.Get());
        bo=transition(output_.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bb=transition(back_[fi].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);
        bs[0]=bo; bs[1]=bb; cmd_->ResourceBarrier(2,bs);

        throw_hr(cmd_->Close(),"command list Close");
        ID3D12CommandList* lists[]={cmd_.Get()};
        queue_->ExecuteCommandLists(1,lists);
        HRESULT phr=swap_->Present(1,0);
        if(FAILED(phr)){
            HRESULT reason=device_->GetDeviceRemovedReason();
            std::cerr<<"DXGI_PRESENT_FAIL=0x"<<std::hex<<static_cast<unsigned long>(phr)
                     <<" DEVICE_REASON=0x"<<static_cast<unsigned long>(reason)<<std::dec<<"\n";
            throw_hr(phr,"Present");
        }
        fc.fenceValue=++fenceSerial_;
        HRESULT shr=queue_->Signal(fence_.Get(),fc.fenceValue);
        if(FAILED(shr)){
            HRESULT reason=device_->GetDeviceRemovedReason();
            std::cerr<<"DX12_SIGNAL_FAIL=0x"<<std::hex<<static_cast<unsigned long>(shr)
                     <<" DEVICE_REASON=0x"<<static_cast<unsigned long>(reason)<<std::dec<<"\n";
            throw_hr(shr,"Signal");
        }
        // Stability-first default: keep at most one submitted frame in flight.
        // This sacrifices some peak throughput but eliminates allocator/resource overlap as a crash source.
        if(safeSerial_) wait_fence_value(fc.fenceValue,"serial frame wait");
    }

    std::string adapter_name() const { return adapterName_; }
    void set_safe_serial(bool v) { safeSerial_=v; }

private:
    static D3D12_RESOURCE_BARRIER transition(ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER b{}; b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource=r; b.Transition.StateBefore=before; b.Transition.StateAfter=after;
        b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; return b;
    }
    static D3D12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE t) {
        D3D12_HEAP_PROPERTIES p{}; p.Type=t; p.CPUPageProperty=D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        p.MemoryPoolPreference=D3D12_MEMORY_POOL_UNKNOWN; p.CreationNodeMask=1; p.VisibleNodeMask=1; return p;
    }
    static D3D12_RESOURCE_DESC buffer_desc(UINT64 n) {
        D3D12_RESOURCE_DESC d{}; d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; d.Width=n; d.Height=1;
        d.DepthOrArraySize=1; d.MipLevels=1; d.Format=DXGI_FORMAT_UNKNOWN; d.SampleDesc.Count=1;
        d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR; return d;
    }

    void init() {
#if defined(_DEBUG)
        ComPtr<ID3D12Debug> dbg; if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) dbg->EnableDebugLayer();
#endif
        throw_hr(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory_)),"CreateDXGIFactory2");
        std::cout << "DXGI_FACTORY=PASS\n";
        pick_adapter();
        std::cout << "ADAPTER_SELECTION=PASS GPU_ADAPTER=" << adapterName_ << "\n";
        throw_hr(D3D12CreateDevice(adapter_.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device_)),"D3D12CreateDevice");
        std::cout << "D3D12_DEVICE=PASS\n";

        D3D12_COMMAND_QUEUE_DESC q{}; q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
        throw_hr(device_->CreateCommandQueue(&q,IID_PPV_ARGS(&queue_)),"CreateCommandQueue");

        DXGI_SWAP_CHAIN_DESC1 sd{}; sd.Width=surfaceW_; sd.Height=surfaceH_; sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count=1; sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount=FRAME_COUNT;
        sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD; sd.Scaling=DXGI_SCALING_STRETCH; sd.AlphaMode=DXGI_ALPHA_MODE_UNSPECIFIED;
        ComPtr<IDXGISwapChain1> sc1;
        throw_hr(factory_->CreateSwapChainForHwnd(queue_.Get(),hwnd_,&sd,nullptr,nullptr,&sc1),"CreateSwapChainForHwnd");
        throw_hr(factory_->MakeWindowAssociation(hwnd_,DXGI_MWA_NO_ALT_ENTER),"MakeWindowAssociation");
        throw_hr(sc1.As(&swap_),"swapchain As");
        for(UINT i=0;i<FRAME_COUNT;++i) throw_hr(swap_->GetBuffer(i,IID_PPV_ARGS(&back_[i])),"GetBuffer");

        create_output();
        create_pipeline();
        create_frames();
        throw_hr(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,frames_[0].allocator.Get(),pso_.Get(),IID_PPV_ARGS(&cmd_)),"CreateCommandList");
        throw_hr(cmd_->Close(),"initial command list Close");
        throw_hr(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence_)),"CreateFence");
        fenceEvent_=CreateEventW(nullptr,FALSE,FALSE,nullptr); if(!fenceEvent_) throw std::runtime_error("CreateEvent failed");
    }

    void pick_adapter() {
        SIZE_T best=0;
        for(UINT i=0;;++i) {
            ComPtr<IDXGIAdapter1> a;
            HRESULT hr=factory_->EnumAdapters1(i,&a); if(hr==DXGI_ERROR_NOT_FOUND) break; throw_hr(hr,"EnumAdapters1");
            DXGI_ADAPTER_DESC1 d{}; a->GetDesc1(&d);
            if(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            ComPtr<ID3D12Device> testDevice;
            if(FAILED(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&testDevice)))) continue;
            if(d.DedicatedVideoMemory>=best) { best=d.DedicatedVideoMemory; adapter_=a; adapterName_=narrow(d.Description); }
        }
        if(!adapter_) throw std::runtime_error("No D3D12 feature-level 12_0 hardware adapter found");
    }

    void create_output() {
        D3D12_FEATURE_DATA_FORMAT_SUPPORT fs{}; fs.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        throw_hr(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&fs,sizeof(fs)),"CheckFeatureSupport format");
        if((fs.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)==0) throw std::runtime_error("R8G8B8A8_UNORM typed UAV store unsupported by selected adapter");
        D3D12_RESOURCE_DESC td{}; td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width=surfaceW_; td.Height=surfaceH_;
        td.DepthOrArraySize=1; td.MipLevels=1; td.Format=DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count=1;
        td.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN; td.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        auto hp=heap_props(D3D12_HEAP_TYPE_DEFAULT);
        throw_hr(device_->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&td,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output_)),"Create output texture");

        D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.NumDescriptors=1; hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        throw_hr(device_->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&uavHeap_)),"CreateDescriptorHeap");
        D3D12_UNORDERED_ACCESS_VIEW_DESC ud{}; ud.Format=DXGI_FORMAT_R8G8B8A8_UNORM; ud.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D; ud.Texture2D.MipSlice=0;
        device_->CreateUnorderedAccessView(output_.Get(),nullptr,&ud,uavHeap_->GetCPUDescriptorHandleForHeapStart());
    }

    void create_pipeline() {
        D3D12_DESCRIPTOR_RANGE range{}; range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV; range.NumDescriptors=1; range.BaseShaderRegister=0; range.RegisterSpace=0; range.OffsetInDescriptorsFromTableStart=0;
        D3D12_ROOT_PARAMETER rp[2]{};
        rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV; rp[0].Descriptor.ShaderRegister=0; rp[0].Descriptor.RegisterSpace=0; rp[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        rp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[1].DescriptorTable.NumDescriptorRanges=1; rp[1].DescriptorTable.pDescriptorRanges=&range; rp[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rs{}; rs.NumParameters=2; rs.pParameters=rp; rs.Flags=D3D12_ROOT_SIGNATURE_FLAG_NONE;
        ComPtr<ID3DBlob> sig,err;
        HRESULT hr=D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err);
        if(FAILED(hr)) { if(err) std::cerr << "ROOT_SIGNATURE_ERROR=" << static_cast<const char*>(err->GetBufferPointer()) << "\n"; throw_hr(hr,"D3D12SerializeRootSignature"); }
        throw_hr(device_->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&rootSig_)),"CreateRootSignature");

        UINT flags=D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        flags|=D3DCOMPILE_DEBUG|D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags|=D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
        ComPtr<ID3DBlob> cs;
        err.Reset();
        hr=D3DCompileFromFile(L"gr_binary_cs.hlsl",nullptr,D3D_COMPILE_STANDARD_FILE_INCLUDE,"main","cs_5_1",flags,0,&cs,&err);
        if(FAILED(hr)) { if(err) std::cerr << "HLSL_ERROR=" << static_cast<const char*>(err->GetBufferPointer()) << "\n"; throw_hr(hr,"D3DCompileFromFile"); }
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd{}; pd.pRootSignature=rootSig_.Get(); pd.CS={cs->GetBufferPointer(),cs->GetBufferSize()};
        std::cout << "HLSL_COMPILE=PASS PROFILE=cs_5_1\n";
        throw_hr(device_->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pso_)),"CreateComputePipelineState");
        std::cout << "PIPELINE_CREATE=PASS\n";
    }

    void create_frames() {
        for(auto& f:frames_) {
            throw_hr(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&f.allocator)),"CreateCommandAllocator");
            auto up=heap_props(D3D12_HEAP_TYPE_UPLOAD); auto bd=buffer_desc(sizeof(BinaryFramePacket));
            throw_hr(device_->CreateCommittedResource(&up,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&f.upload)),"Create upload packet");
            void* p=nullptr; throw_hr(f.upload->Map(0,nullptr,&p),"Map upload"); f.mapped=static_cast<std::uint8_t*>(p);
            auto def=heap_props(D3D12_HEAP_TYPE_DEFAULT);
            throw_hr(device_->CreateCommittedResource(&def,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&f.gpuCB)),"Create GPU packet");
            f.gpuCBInCopyDest=true;
        }
    }

    void wait_fence_value(UINT64 value,const char* label) {
        if(!value || fence_->GetCompletedValue()>=value) return;
        throw_hr(fence_->SetEventOnCompletion(value,fenceEvent_),"SetEventOnCompletion");
        DWORD wr=WaitForSingleObject(fenceEvent_,5000);
        if(wr!=WAIT_OBJECT_0){
            HRESULT reason=device_?device_->GetDeviceRemovedReason():E_FAIL;
            std::ostringstream os;os<<label<<" timeout/device state HRESULT=0x"<<std::hex<<static_cast<unsigned long>(reason);
            throw std::runtime_error(os.str());
        }
    }
    void wait_frame(FrameContext& f) { wait_fence_value(f.fenceValue,"frame fence"); }
    void wait_idle() {
        if(!queue_ || !fence_) return;
        UINT64 v=++fenceSerial_; throw_hr(queue_->Signal(fence_.Get(),v),"idle Signal");
        wait_fence_value(v,"idle fence");
    }

    HWND hwnd_{}; UINT surfaceW_{},surfaceH_{};
    ComPtr<IDXGIFactory6> factory_; ComPtr<IDXGIAdapter1> adapter_; ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_; ComPtr<IDXGISwapChain3> swap_; std::array<ComPtr<ID3D12Resource>,FRAME_COUNT> back_;
    ComPtr<ID3D12Resource> output_; ComPtr<ID3D12DescriptorHeap> uavHeap_; ComPtr<ID3D12RootSignature> rootSig_; ComPtr<ID3D12PipelineState> pso_;
    std::array<FrameContext,FRAME_COUNT> frames_; ComPtr<ID3D12GraphicsCommandList> cmd_; ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_{}; UINT64 fenceSerial_=0; bool safeSerial_=true; std::string adapterName_;
};


double env_double(const char* name,double fallback){char buf[128]{};DWORD n=GetEnvironmentVariableA(name,buf,(DWORD)sizeof(buf));if(n==0||n>=sizeof(buf))return fallback;try{return std::stod(buf);}catch(...){return fallback;}}

void print_controls(){
    std::cout << "CONTROLS=GLOBAL_PLAY_BUTTON_RESUMES_CURRENT_STATE; GLOBAL_RESTART_BUTTON_AND_F5_RESET_ALL_STATE; ALL_LIVE_INPUTS_AVAILABLE_AS_GRABBABLE_ON_SCREEN_SLIDERS; left bar = center-locked camera (R, theta, phi); right bar = physics/optics; mouse wheel scrolls panel; double-click slider resets default; ESC quits\n";
}

} // namespace

int main(){
    // Keep recoverable runtime failures in the console instead of invoking
    // legacy Windows critical-error UI. This does not bypass SmartScreen or
    // other OS trust policy; those remain controlled by Windows.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(unhandled_filter);
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);
    std::cout << "PROCESS_START=PASS\n";
    try{
        constexpr UINT CAM_PANEL_W=360,RENDER_W=960,RENDER_H=540,WAVE_H=180,H=RENDER_H+WAVE_H,PANEL_W=360,CLIENT_W=CAM_PANEL_W+RENDER_W+PANEL_W;
        HINSTANCE inst=GetModuleHandleW(nullptr);
        WNDCLASSW wc{};wc.lpfnWndProc=wnd_proc;wc.hInstance=inst;wc.lpszClassName=L"BKQR_GR_OPTICAL_STABLE_FRAME_V1500";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Register frame class failed");
        WNDCLASSW rwc{};rwc.lpfnWndProc=render_child_proc;rwc.hInstance=inst;rwc.lpszClassName=L"BKQR_GR_OPTICAL_STABLE_RENDER_CHILD_V1600";rwc.hCursor=LoadCursor(nullptr,IDC_ARROW);rwc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
        if(!RegisterClassW(&rwc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Register render child class failed");
        WNDCLASSW wwc{};wwc.lpfnWndProc=waveform_child_proc;wwc.hInstance=inst;wwc.lpszClassName=L"BKQR_GR_OPTICAL_WAVEFORM_CHILD_V1600";wwc.hCursor=LoadCursor(nullptr,IDC_ARROW);wwc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
        if(!RegisterClassW(&wwc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)throw std::runtime_error("Register waveform child class failed");
        const DWORD frameStyle=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPCHILDREN;
        RECT rc{0,0,(LONG)CLIENT_W,(LONG)H};AdjustWindowRect(&rc,frameStyle,FALSE);
        HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"BKQR GR Optical Dynamics v1.6.6.2 - PN/NR Inspiral-Merger + Observer Wave Signal",frameStyle,
                                  CW_USEDEFAULT,CW_USEDEFAULT,rc.right-rc.left,rc.bottom-rc.top,nullptr,nullptr,inst,nullptr);
        if(!hwnd)throw std::runtime_error("Create frame window failed");
        HWND renderHwnd=CreateWindowExW(0,rwc.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,(int)CAM_PANEL_W,0,(int)RENDER_W,(int)RENDER_H,hwnd,nullptr,inst,nullptr);
        HWND waveHwnd=CreateWindowExW(0,L"BKQR_GR_OPTICAL_WAVEFORM_CHILD_V1600",L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,(int)CAM_PANEL_W,(int)RENDER_H,(int)RENDER_W,(int)WAVE_H,hwnd,nullptr,inst,nullptr);
        if(!renderHwnd)throw std::runtime_error("Create render child failed");
        if(!waveHwnd)throw std::runtime_error("Create waveform child failed");
        std::cout<<"WINDOW_CREATE=PASS TOPLEVEL=1 DX12_RENDER_CHILD=1 WAVEFORM_CHILD=1\n";

        CameraState cam{}; SimulationState sim{}; BinaryVisualState bin{}; QualityState quality{}; WaveformState wave{};
        bin.input.m1=env_double("BKQR_M1",bin.input.m1);
        bin.input.m2=env_double("BKQR_M2",bin.input.m2);
        bin.input.jTotal=env_double("BKQR_JTOT",bin.input.jTotal);
        bin.input.spin1z=env_double("BKQR_S1Z",bin.input.spin1z);
        bin.input.spin2z=env_double("BKQR_S2Z",bin.input.spin2z);
        sim.physicalRate=(float)env_double("BKQR_TIME_RATE",sim.physicalRate);
        sim.viewOrbitHz=(float)std::clamp(env_double("BKQR_VIEW_ORBIT_HZ",sim.viewOrbitHz),0.05,5.0);
        sim.phaseNormalized=env_double("BKQR_PHASE_NORMALIZED",1.0)>=0.5;
        try{auto d0=derive(bin.input);const double rate0=phase_normalized_time_rate(bin.input,bin.dyn,sim.viewOrbitHz);std::cout<<"INPUT_M1="<<bin.input.m1<<" INPUT_M2="<<bin.input.m2<<" INPUT_JTOT="<<bin.input.jTotal<<" DERIVED_R0="<<d0.r0<<" DERIVED_OMEGA0="<<d0.omega0<<" TCOAL_LO="<<d0.tCoal<<" PN_SWITCH_R="<<d0.rTransition<<" MFINAL_FIT="<<d0.finalMassFit<<" AFINAL_FIT="<<d0.finalSpinFit<<" QNM_TAU="<<d0.qnmTau220<<" VIEW_ORBIT_HZ_TARGET="<<sim.viewOrbitHz<<" INITIAL_NORMALIZED_RATE_MPS="<<rate0<<"\n";}catch(const std::exception&e){throw std::runtime_error(std::string("invalid physical input: ")+e.what());}
        ControlPanel cameraPanel(hwnd,0,0,(int)CAM_PANEL_W,(int)H,L"CAMERA BAR",L"Same slider deck as right: R, theta, phi  •  always center-locked");
        configure_camera_panel(cameraPanel,cam);
        ControlPanel panel(hwnd,(int)(CAM_PANEL_W+RENDER_W),0,(int)PANEL_W,(int)H,L"BINARY CONTROL DECK",L"PLAY resumes  •  RESTART rewinds current parameters paused  •  wheel scrolls");
        panel.set_global_restart([&]{
            global_restart(cam,sim,bin,quality,&wave);
            std::cout << "GLOBAL_RESTART=PASS SOURCE=BUTTON CURRENT_PARAMETERS_PRESERVED=1 PAUSED=1\n";
        });
        panel.set_global_play([&]{
            sim.paused=false;
            std::cout << "GLOBAL_PLAY=PASS SOURCE=BUTTON PAUSED=0\n";
        });
        configure_control_panel(panel,cam,sim,bin,quality,wave);
        SetWindowLongPtrW(waveHwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(&wave));
        std::cout << "SLIDER_PANEL_CREATE=PASS GLOBAL_PLAY_BUTTON=PASS GLOBAL_RESTART_BUTTON=PASS\n";

        g_runtime_stage="DX12_INIT";
        Dx12Engine engine(renderHwnd,RENDER_W,RENDER_H);
        const bool safeGpuSerial=env_double("BKQR_SAFE_GPU_SERIAL",1.0)>=0.5;
        engine.set_safe_serial(safeGpuSerial);
        std::cout << "DX12_SURFACE_ISOLATION=PASS SURFACE=960x540 CONTROLS=SIBLING_CHILD WAVEFORM_PANEL=960x180\n";
        std::cout << "RENDERER_START=PASS\n";
        std::cout << "STATUS=BKQR_GR_OPTICAL_DYNAMICS_DX12_V1_6_6_2\n";
        std::cout << "GPU_ADAPTER=" << engine.adapter_name() << "\n";
        std::cout << "DYNAMICS=3PN_BINDING_PLUS_3P5PN_GW_FLUX_BALANCE_WITH_NR_CALIBRATED_REMNANT\n";
        std::cout << "RAY_TIME=BINARY_STATE_EVALUATED_AT_RETARDED_OPTICAL_TRAVEL_TIME\n";
        std::cout << "GEOMETRY=TWO_PUNCTURE_OPTICAL_ANSATZ_SMOOTHLY_TRANSITIONED_TO_SINGLE_REMNANT_OPTICS\n";
        std::cout << "ONE_HOLE_LIMIT=EXACT_STATIC_SCHWARZSCHILD_ISOTROPIC_OPTICS_OUTSIDE_HORIZON\n";
        std::cout << "CLAIM_BOUNDARY=PN_PLUS_NR_CALIBRATED_REDUCED_MODEL_NOT_A_BSSN_Z4C_EVOLUTION\n";
        std::cout << "WAVE_SIGNAL=PANEL_SHOWS_hPLUS_hCROSS_AND_hOBS_FOR_A_DISTANT_OBSERVER_CONSTANT_DELAY_REMOVED\n";
        print_controls();
        const double fpsCap=std::clamp(env_double("BKQR_FPS_CAP",90.0),30.0,300.0);
        std::cout<<"STABILITY_MODE=PASS FPS_CAP="<<fpsCap<<" PANEL_REFRESH_HZ=20 GPU_FENCE_TIMEOUT_MS=5000 SAFE_GPU_SERIAL="<<(safeGpuSerial?1:0)<<"\n";
        std::cout<<"VIEW_CLOCK="<<(sim.phaseNormalized?"PHASE_NORMALIZED":"PHYSICAL_M_PER_S")<<" TARGET_ORBIT_HZ="<<sim.viewOrbitHz<<" PHYSICAL_RATE_FALLBACK_MPS="<<sim.physicalRate<<"\n";

        MSG msg{}; bool running=true; std::uint32_t frameId=0;
        auto prev=std::chrono::steady_clock::now(),fpsT=prev,panelT=prev; int fpsFrames=0;
        while(running){
            while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){ if(msg.message==WM_QUIT){running=false;break;} TranslateMessage(&msg); DispatchMessageW(&msg); }
            if(!running) break;
            auto frameWallStart=std::chrono::steady_clock::now();
            auto now=frameWallStart;
            float dt=std::chrono::duration<float>(now-prev).count();prev=now;dt=std::min(dt,0.05f);
            if(IsIconic(hwnd)){g_runtime_stage="MINIMIZED";std::this_thread::sleep_for(std::chrono::milliseconds(20));continue;}
            bool keys=(GetForegroundWindow()==hwnd||GetParent(GetFocus())==hwnd);
            g_runtime_stage="UPDATE_STATE";
            update_states(cam,sim,bin,quality,wave,dt,keys);
            const float effectiveRateMps=(float)effective_sim_rate_mps(sim,bin);
            float simDt=sim.paused?0.0f:dt*effectiveRateMps;
            if(simDt>0.0f){advance(bin.input,bin.dyn,simDt);sim.time+=simDt;}
            const auto snapNow=snapshot(bin.input,bin.dyn);
            waveform_push(wave,sim.time,snapNow);
            g_runtime_stage="BUILD_PACKET";
            auto packet=make_packet(cam,sim,bin,quality,RENDER_W,RENDER_H,frameId,simDt,effectiveRateMps);
            g_runtime_frame=frameId;++frameId;
            g_runtime_stage="GPU_RENDER";
            engine.render(packet);
            g_runtime_stage="PANEL_REFRESH";
            if(std::chrono::duration<double>(now-panelT).count()>=0.05){cameraPanel.refresh();panel.refresh();InvalidateRect(waveHwnd,nullptr,FALSE);panelT=now;}
            ++fpsFrames;
            float fpsDt=std::chrono::duration<float>(now-fpsT).count();
            if(fpsDt>=1.0f){
                float fps=fpsFrames/fpsDt; fpsFrames=0; fpsT=now;
                auto s=snapNow;
                Vec3 com=bin.input.m1*s.x1+bin.input.m2*s.x2;
                Vec3 comv=bin.input.m1*s.v1+bin.input.m2*s.v2;
                std::wostringstream title;
                const double viewHzActual=std::abs(s.omega)*effectiveRateMps/(2.0*PI);
                const double rObj1Now=effective_individual_optical_radius(bin.input.m1,bin.input.m2,s.r,s.mergerBlend,bin.captureU);
                const double rObj2Now=effective_individual_optical_radius(bin.input.m2,bin.input.m1,s.r,s.mergerBlend,bin.captureU);
                const double rRemNow=effective_remnant_optical_radius(s.systemMass,bin.captureU);
                title<<L"BKQR Optical Dynamics v1.6.6.2 | "<<std::fixed<<std::setprecision(1)<<fps<<L" FPS | view="<<std::setprecision(2)<<viewHzActual<<L" Hz | "<<stage_name(s.stage)<<L" | r="<<std::setprecision(3)<<s.r<<L" | R1="<<rObj1Now<<L" R2="<<rObj2Now<<L" | Msys="<<s.systemMass;
                SetWindowTextW(hwnd,title.str().c_str());
                std::cout<<std::setprecision(10)
                    <<"FRAME_FPS="<<fps
                    <<" VIEW_CLOCK="<<(sim.phaseNormalized?"PHASE_NORMALIZED":"PHYSICAL_M_PER_S")
                    <<" VIEW_ORBIT_HZ_TARGET="<<sim.viewOrbitHz
                    <<" VIEW_ORBIT_HZ_ACTUAL="<<viewHzActual
                    <<" SIM_RATE_MPS="<<effectiveRateMps
                    <<" SIM_TIME="<<sim.time
                    <<" SEPARATION="<<s.r
                    <<" PHASE="<<s.phase
                    <<" OMEGA="<<s.omega
                    <<" D_DOT="<<s.rdot
                    <<" M1="<<bin.input.m1<<" M2="<<bin.input.m2
                    <<" JTOT="<<bin.input.jTotal<<" LORB="<<s.d.Lorb
                    <<" R0="<<s.d.r0<<" TCOAL_LO="<<s.d.tCoal
                    <<" F_ORB="<<(s.omega/(2.0*PI))<<" F_GW="<<(s.omega/PI)
                    <<" ETA="<<s.d.eta
                    <<" STAGE="<<stage_name(s.stage)
                    <<" GW_POWER="<<s.gwPower
                    <<" GW_J_FLUX="<<s.gwJFlux
                    <<" E_RAD="<<s.energyRadiated
                    <<" J_RAD="<<s.angularMomentumRadiated
                    <<" SYSTEM_MASS="<<s.systemMass
                    <<" SYSTEM_J="<<s.systemJ
                    <<" REMNANT_SPIN="<<s.remnantSpin
                    <<" MERGER_BLEND="<<s.mergerBlend
                    <<" OBJECT1_RADIUS_T="<<rObj1Now
                    <<" OBJECT2_RADIUS_T="<<rObj2Now
                    <<" REMNANT_RADIUS_T="<<rRemNow
                    <<" PN_ENERGY_BALANCE_RESIDUAL="<<s.pnEnergyBalanceResidual
                    <<" PN_J_BALANCE_RESIDUAL="<<s.pnAngularMomentumBalanceResidual
                    <<" COM_RESIDUAL="<<norm(com)
                    <<" COM_VEL_RESIDUAL="<<norm(comv)
                    <<" RR="<<(bin.input.radiationReaction?1:0)
                    <<" PIXEL_STRIDE="<<quality.pixelStride
                    <<" BASE_SPP="<<quality.baseSPP
                    <<"\n";
            }
            g_runtime_stage="FRAME_PACING";
            const double targetFrame=1.0/fpsCap;
            const double used=std::chrono::duration<double>(std::chrono::steady_clock::now()-frameWallStart).count();
            if(used<targetFrame)std::this_thread::sleep_for(std::chrono::duration<double>(targetFrame-used));
        }
        g_runtime_stage="SHUTDOWN";
        std::cout << "RUN=PASS\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr << "ERROR="<<e.what()<<"\nRUN=FAIL\n";
        return 1;
    }
}
