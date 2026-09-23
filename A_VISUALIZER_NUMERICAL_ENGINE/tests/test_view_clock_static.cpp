#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
static std::string read(const char*p){std::ifstream f(p,std::ios::binary);std::ostringstream s;s<<f.rdbuf();return s.str();}
static bool mark(bool v,const char*n){std::cout<<n<<"="<<(v?"PASS":"FAIL")<<"\n";return v;}
int main(){auto m=read("../renderer/main.cpp"),b=read("../renderer/binary_driver.h");bool ok=true;
ok&=mark(m.find("Visual orbital rate")!=std::string::npos,"VIEW_HZ_SLIDER");
ok&=mark(m.find("BKQR_VIEW_ORBIT_HZ")!=std::string::npos,"VIEW_HZ_ENV");
ok&=mark(m.find("BKQR_PHASE_NORMALIZED")!=std::string::npos,"PHASE_CLOCK_ENV");
ok&=mark(m.find("phase_normalized_time_rate")!=std::string::npos,"PHASE_NORMALIZED_CLOCK_USED");
ok&=mark(m.find("VIEW_ORBIT_HZ_ACTUAL")!=std::string::npos,"VIEW_HZ_DIAGNOSTIC");
ok&=mark(b.find("2.0*PI*hz")!=std::string::npos,"TWO_PI_HZ_NORMALIZATION");
ok&=mark(b.find("STAGE_MERGER")!=std::string::npos && b.find("qnmOmega220")!=std::string::npos,"MERGER_RINGDOWN_CLOCK_CONTINUATION");
ok&=mark(b.find("FLOOR_ORBIT_HOLD")==std::string::npos,"NO_ARTIFICIAL_FLOOR_ORBIT_HOLD");
std::cout<<"VIEW_CLOCK_STATIC_AUDIT="<<(ok?"PASS":"FAIL")<<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";return ok?0:1;}
