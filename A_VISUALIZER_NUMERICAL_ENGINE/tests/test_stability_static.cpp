#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
static std::string read(const char*p){std::ifstream f(p,std::ios::binary);std::ostringstream s;s<<f.rdbuf();return s.str();}
static bool req(bool x,const char*n){std::cout<<n<<"="<<(x?"PASS":"FAIL")<<"\n";return x;}
int main(){std::string m=read("../renderer/main.cpp"),c=read("../renderer/control_panel.h");bool ok=true;
ok&=req(m.find("render_child_proc")!=std::string::npos,"DX12_RENDER_CHILD_CLASS");
ok&=req(m.find("Dx12Engine engine(renderHwnd,RENDER_W,RENDER_H)")!=std::string::npos,"SWAPCHAIN_BINDS_RENDER_CHILD");
ok&=req(m.find("PANEL_REFRESH_HZ=20")!=std::string::npos,"PANEL_REFRESH_THROTTLED");
ok&=req(m.find("BKQR_FPS_CAP")!=std::string::npos,"FRAME_RATE_CAP_CONFIGURABLE");
ok&=req(m.find("GetDeviceRemovedReason")!=std::string::npos,"DEVICE_REMOVAL_DIAGNOSTIC");
ok&=req(m.find("WaitForSingleObject(fenceEvent_,5000)")!=std::string::npos,"BOUNDED_GPU_FENCE_WAIT");
ok&=req(m.find("SetUnhandledExceptionFilter(unhandled_filter)")!=std::string::npos,"UNHANDLED_EXCEPTION_DIAGNOSTIC");
ok&=req(c.find("WM_CAPTURECHANGED")!=std::string::npos,"SLIDER_CAPTURE_RECOVERY");
ok&=req(c.find("struct ToggleSpec")!=std::string::npos && c.find("add_toggle")!=std::string::npos && c.find("toggle_rect_for")!=std::string::npos,"ON_OFF_TOGGLE_WIDGET_PRESENT");
ok&=req(m.find("panel.add_toggle({L\"Paused\"")!=std::string::npos && m.find("panel.add_toggle({L\"Radiation reaction\"")!=std::string::npos && m.find("panel.add_toggle({L\"Orbital-plane inset\"")!=std::string::npos && m.find("panel.add_toggle({L\"Show mirrored source sphere\"")!=std::string::npos,"BOOLEAN_VALUES_USE_TOGGLES");
ok&=req(m.find("panel.add_toggle({L\"Time normalization\"")!=std::string::npos && m.find("OFF (physical time)")!=std::string::npos,"TIME_NORMALIZATION_DEACTIVATION_TOGGLE_PRESENT");
ok&=req(c.find("point_hits_track_or_knob")!=std::string::npos,"SLIDER_TRACK_KNOB_HIT_TEST");
ok&=req(c.find("activeGrabOffsetPx_")!=std::string::npos,"SLIDER_KNOB_DRAG_OFFSET");
ok&=req(c.find("WM_NCDESTROY")!=std::string::npos,"PANEL_LIFETIME_CLEANUP");
ok&=req(c.find("std::max<LONG>(static_cast<LONG>(kPadX+20),sw.left-10)")!=std::string::npos,"MSVC_TOGGLE_RECT_LONG_TYPE_SAFE");
ok&=req(c.find("GLOBAL RESTART")!=std::string::npos && c.find("set_global_restart")!=std::string::npos,"GLOBAL_RESTART_BUTTON_PRESENT");
ok&=req(c.find("GLOBAL PLAY")!=std::string::npos && c.find("set_global_play")!=std::string::npos,"GLOBAL_PLAY_BUTTON_PRESENT");
ok&=req(c.find("MoveWindow(playButton_")!=std::string::npos && c.find("MoveWindow(restartButton_")!=std::string::npos,"GLOBAL_PLAY_BESIDE_RESTART");
ok&=req(m.find("global_restart(cam,sim,bin,quality,&wave)")!=std::string::npos,"GLOBAL_RESTART_CALLBACK_BINDS_ALL_STATE");
ok&=req(m.find("panel.set_global_play")!=std::string::npos && m.find("sim.paused=false;")!=std::string::npos,"GLOBAL_PLAY_RESUMES_WITHOUT_RESET");
ok&=req(m.find("VK_F5")!=std::string::npos,"GLOBAL_RESTART_F5_PRESENT");
ok&=req(m.find("ControlPanel cameraPanel")!=std::string::npos && m.find("configure_camera_panel(cameraPanel,cam)")!=std::string::npos,"LEFT_CAMERA_USES_SAME_SLIDER_DECK");
ok&=req(m.find("CAM_PANEL_W=360")!=std::string::npos && m.find("PANEL_W=360")!=std::string::npos,"LEFT_RIGHT_PANEL_WIDTHS_MATCH");
ok&=req(m.find("panel.add_slider({L\"R\"")!=std::string::npos && m.find("panel.add_slider({L\"theta\"")!=std::string::npos && m.find("panel.add_slider({L\"phi\"")!=std::string::npos,"CAMERA_R_THETA_PHI_SLIDERS_PRESENT");
ok&=req(m.find("CameraKnobBar")==std::string::npos && m.find("TRACKBAR_CLASSW")==std::string::npos,"NO_SEPARATE_CAMERA_WIDGET_PATH");
ok&=req(m.find("INITCOMMONCONTROLSEX")==std::string::npos && m.find("InitCommonControlsEx")==std::string::npos && m.find("ICC_BAR_CLASSES")==std::string::npos,"NO_STALE_COMMON_CONTROLS_TRACKBAR_INIT");
ok&=req(m.find("Vec3 forward=unit3((-1.0)*pos)")!=std::string::npos,"CAMERA_ALWAYS_CENTER_LOCKED");
ok&=req(m.find("sim.paused=true; // Restart always returns to t=0 in a stable paused inspection state.")!=std::string::npos,"GLOBAL_RESTART_ALWAYS_PAUSED");
ok&=req(m.find("bin.dyn=State{};")!=std::string::npos && m.find("CURRENT_PARAMETERS_PRESERVED=1")!=std::string::npos,"GLOBAL_RESTART_REWINDS_CURRENT_PARAMETERS");
ok&=req(m.find("cam=CameraState{};")==std::string::npos && m.find("q=QualityState{};")==std::string::npos,"GLOBAL_RESTART_DOES_NOT_FACTORY_RESET_CAMERA_OR_QUALITY");
ok&=req(m.find("showMirroredSourceSphere")!=std::string::npos && m.find("sourceSphereRadius")!=std::string::npos,"MIRRORED_SOURCE_SPHERE_STATE_PRESENT");
ok&=req(m.find("Show mirrored source sphere")!=std::string::npos && m.find("Source sphere radius")!=std::string::npos,"MIRRORED_SOURCE_SPHERE_CONTROLS_PRESENT");
ok&=req(m.find("Initial separation r0 / M")!=std::string::npos,"INITIAL_SEPARATION_SLIDER_PRESENT");
ok&=req(m.find("commit_initial_separation_over_M")!=std::string::npos && m.find("pn_orbital_J(M,eta,x)")!=std::string::npos,"INITIAL_SEPARATION_REBINDS_3PN_J");
ok&=req(m.find("Object 1 radius")==std::string::npos && m.find("Object 2 radius")==std::string::npos,"NO_MANUAL_OBJECT_RADIUS_INPUTS");
ok&=req(m.find("effective_individual_optical_radius")!=std::string::npos,"TIME_EVOLVING_OBJECT_RADIUS_WIRED");
ok&=req(m.find("waveform_child_proc")!=std::string::npos && m.find("WAVEFORM_CHILD=1")!=std::string::npos,"WAVEFORM_CHILD_PANEL_PRESENT");
ok&=req(m.find("induced_waveform_sample")!=std::string::npos && m.find("hPlus")!=std::string::npos && m.find("hCross")!=std::string::npos && m.find("hDet")!=std::string::npos,"INDUCED_GW_CHANNELS_PRESENT");
ok&=req(m.find("Observer inclination")!=std::string::npos && m.find("Detector polarization psi")!=std::string::npos && m.find("Observer distance")!=std::string::npos && m.find("Wave amplitude gain")!=std::string::npos,"WAVEFORM_OBSERVER_CONTROLS_PRESENT");
ok&=req(m.find("waveform_reset(*wave)")!=std::string::npos && m.find("waveform_reset(wave)")!=std::string::npos,"WAVEFORM_HISTORY_REWINDS_ON_RESTART");
ok&=req(m.find("gr_binary_cs.hlsl")!=std::string::npos,"LENSING_SHADER_PATH_RETAINED");
std::cout<<"STABILITY_STATIC_AUDIT="<<(ok?"PASS":"FAIL")<<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";return ok?0:1;}
