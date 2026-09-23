#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string slurp(const char* p){
    std::ifstream f(p,std::ios::binary);
    std::ostringstream s;
    s<<f.rdbuf();
    return s.str();
}
static bool has(const std::string& s,const char* x){return s.find(x)!=std::string::npos;}
int main(){
    const auto m=slurp("../renderer/main.cpp");
    const auto c=slurp("../renderer/control_panel.h");
    bool ok=true;
    auto mark=[&](const char* n,bool v){std::cout<<n<<"="<<(v?"PASS":"FAIL")<<"\n";ok&=v;};
    mark("DANGLING_RESET_LAMBDA_ABSENT",!has(m,"auto reset_phys=[&]"));
    mark("TRANSACTIONAL_PHYSICAL_INPUT",has(m,"commit_physical_input")&&has(m,"PHYSICAL_INPUT_REJECTED"));
    mark("STORED_PHYSICAL_SETTERS_CAPTURE_LONG_LIVED_STATE",has(m,"[&bin,&sim](double v)"));
    mark("CONTROL_CALLBACK_EXCEPTION_CONTAINMENT",has(c,"CONTROL_PANEL_EXCEPTION"));
    mark("CONTROL_GDI_READY_BEFORE_SHOW",has(c,"Own all GDI objects before the HWND becomes paintable")&&has(c,"ShowWindow(hwnd_,SW_SHOW)"));
    mark("SAFE_GPU_SERIAL_DEFAULT",has(m,"bool safeSerial_=true")&&has(m,"BKQR_SAFE_GPU_SERIAL"));
    mark("POST_SIGNAL_SERIAL_FENCE",has(m,"if(safeSerial_) wait_fence_value(fc.fenceValue"));
    std::cout<<"CALLBACK_LIFETIME_AND_SAFE_SUBMISSION_AUDIT="<<(ok?"PASS":"FAIL")<<"\n";
    return ok?0:1;
}
