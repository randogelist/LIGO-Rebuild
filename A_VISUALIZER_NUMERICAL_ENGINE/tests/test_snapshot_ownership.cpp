#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
int main(){
  std::ifstream f("../renderer/main.cpp");
  std::ostringstream ss; ss<<f.rdbuf(); const std::string s=ss.str();
  const bool noBad = s.find("s.m1")==std::string::npos && s.find("s.m2")==std::string::npos;
  const bool good = s.find("bin.input.m1*s.x1")!=std::string::npos && s.find("bin.input.m2*s.x2")!=std::string::npos;
  std::cout << "SNAPSHOT_MASS_OWNERSHIP=" << ((noBad&&good)?"PASS":"FAIL") << "\n";
  std::cout << "RUN=" << ((noBad&&good)?"PASS":"FAIL") << "\n";
  return (noBad&&good)?0:1;
}
