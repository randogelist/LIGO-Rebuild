#include "../renderer/command_protocol.h"
#include <iostream>
int main(){bool ok=sizeof(bkqrgrbin::BinaryFramePacket)==256&&alignof(bkqrgrbin::BinaryFramePacket)==256&&bkqrgrbin::GR_BINARY_PROTOCOL_VERSION==0x00016000u;std::cout<<"FRAME_PACKET_BYTES="<<sizeof(bkqrgrbin::BinaryFramePacket)<<"\nFRAME_PACKET_ALIGN="<<alignof(bkqrgrbin::BinaryFramePacket)<<"\nPROTOCOL=0x"<<std::hex<<bkqrgrbin::GR_BINARY_PROTOCOL_VERSION<<std::dec<<"\nDRIVER_PACKET_ABI="<<(ok?"PASS":"FAIL")<<"\nRUN="<<(ok?"PASS":"FAIL")<<"\n";return ok?0:1;}
