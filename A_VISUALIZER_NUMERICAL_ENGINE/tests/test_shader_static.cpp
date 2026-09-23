#include <fstream>
#include <iostream>
#include <sstream>

int main(){std::ifstream f("../renderer/gr_binary_cs.hlsl");std::ostringstream o;o<<f.rdbuf();auto s=o.str();auto has=[&](const char*q){bool b=s.find(q)!=std::string::npos;std::cout<<q<<"="<<(b?"PASS":"FAIL")<<"\n";return b;};bool ok=has("0x00016000u")&&has("shade_detector_sample")&&has("trace_to_source_plane")&&has("RENDER_MODE_POINT_DETECTOR")&&has("RENDER_MODE_CYLINDER_DETECTOR")&&has("FieldSample field_sample")&&has("binary_retarded")&&has("Claim boundary")&&has("GW power")&&has("tan(0.5*Camera0.w)")&&has("detectorScale")&&has("source_sphere_profile")&&has("Reserved0.z")&&has("Reserved0.w")&&has("hitObj1")&&has("hitObj2")&&has("effective_individual_optical_radius")&&has("length(f.x1-f.x2)");std::cout<<"PN_NR_OPTICAL_SHADER_STATIC_AUDIT="<<(ok?"PASS":"FAIL")<<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";return ok?0:1;}
