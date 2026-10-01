#include "audio/audio.hpp"
#include <cassert>
#include <cstdint>
#include <vector>
int main(){
 wormhole::Audio a; assert(a.init());
 std::vector<std::uint8_t> w(48,0); auto p16=[&](std::size_t p,std::uint16_t v){w[p]=v&255;w[p+1]=v>>8;}; auto p32=[&](std::size_t p,std::uint32_t v){w[p]=v&255;w[p+1]=(v>>8)&255;w[p+2]=(v>>16)&255;w[p+3]=v>>24;};
 w[0]='R';w[1]='I';w[2]='F';w[3]='F';p32(4,40);w[8]='W';w[9]='A';w[10]='V';w[11]='E';w[12]='f';w[13]='m';w[14]='t';w[15]=' ';p32(16,16);p16(20,1);p16(22,1);p32(24,24000);p32(28,24000);p16(32,1);p16(34,8);w[36]='d';w[37]='a';w[38]='t';w[39]='a';p32(40,4);w[44]=128;w[45]=160;w[46]=96;w[47]=128;
 assert(a.playWav(w)); a.update(); a.shutdown(); return 0;
}