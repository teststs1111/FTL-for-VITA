#include "audio/audio.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#ifdef __vita__
#include <psp2/audioout.h>
#endif
namespace wormhole {
namespace {
std::uint16_t u16(const std::vector<std::uint8_t>& b,std::size_t p){return std::uint16_t(b[p]|(std::uint16_t(b[p+1])<<8));}
std::uint32_t u32(const std::vector<std::uint8_t>& b,std::size_t p){return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
bool tag(const std::vector<std::uint8_t>& b,std::size_t p,const char* t){return p+4<=b.size()&&b[p]==t[0]&&b[p+1]==t[1]&&b[p+2]==t[2]&&b[p+3]==t[3];}
}
Audio::~Audio(){shutdown();}
bool Audio::init(){
 if(initialized_) return true;
#ifdef __vita__
 port_=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,bufferFrames_,SCE_AUDIO_OUT_SAMPLE_RATE_48000,SCE_AUDIO_OUT_MODE_STEREO);
 if(port_<0){port_=-1;return false;}
 int volume=SCE_AUDIO_VOLUME_0DB;
 sceAudioOutSetVolume(port_,SCE_AUDIO_VOLUME_FLAG_L_CH|SCE_AUDIO_VOLUME_FLAG_R_CH,&volume);
 outputBuffer_.assign(std::size_t(bufferFrames_)*2,0);
#endif
 initialized_=true; return true;
}
void Audio::shutdown(){voices_.clear();
#ifdef __vita__
 if(port_>=0)sceAudioOutReleasePort(port_); port_=-1; outputBuffer_.clear();
#endif
 initialized_=false;
}
bool Audio::playPcm16Stereo(const std::vector<std::int16_t>& s,float v){
 if(!initialized_||s.empty()||(s.size()&1u)||voices_.size()>=16)return false;
 Voice x; x.samples=s; x.volume=std::clamp(v,0.f,1.f); voices_.push_back(std::move(x)); return true;
}
bool Audio::playWav(const std::vector<std::uint8_t>& b,float v){
 if(!initialized_||b.size()<44||!tag(b,0,"RIFF")||!tag(b,8,"WAVE"))return false;
 std::uint16_t ch=0,bits=0; std::uint32_t rate=0; std::size_t off=0,size=0,p=12;
 while(p+8<=b.size()){
  const auto n=u32(b,p+4), q=p+8; if(q>b.size()||n>b.size()-q)return false;
  if(tag(b,p,"fmt ")){if(n<16)return false; if(u16(b,q)!=1)return false; ch=u16(b,q+2);rate=u32(b,q+4);bits=u16(b,q+14);if((ch!=1&&ch!=2)||(bits!=8&&bits!=16)||!rate)return false;}
  else if(tag(b,p,"data")){off=q;size=n;break;}
  p=q+n+(n&1u);
 }
 if(!off||!size)return false;
 const std::size_t bps=bits/8, frames=size/(bps*ch); if(!frames)return false;
 std::vector<std::int16_t> pcm(frames*2);
 for(std::size_t i=0;i<frames;++i){
  auto sample=[&](std::size_t c){const std::size_t x=off+(i*ch+c)*bps;return bits==16?std::int16_t(std::uint16_t(b[x])|(std::uint16_t(b[x+1])<<8)):std::int16_t((int(b[x])-128)*257);};
  pcm[i*2]=sample(0);pcm[i*2+1]=ch==2?sample(1):pcm[i*2];
 }
 if(rate!=48000){
  const std::size_t out=std::max<std::size_t>(1,std::size_t((std::uint64_t(frames)*48000u)/rate));
  std::vector<std::int16_t> r(out*2);
  for(std::size_t i=0;i<out;++i){const auto src=std::min(frames-1,std::size_t((std::uint64_t(i)*rate)/48000u));r[i*2]=pcm[src*2];r[i*2+1]=pcm[src*2+1];}
  pcm.swap(r);
 }
 return playPcm16Stereo(pcm,v);
}
void Audio::update(){
 if(!initialized_)return;
#ifdef __vita__
 if(port_<0||outputBuffer_.empty()||sceAudioOutGetRestSample(port_)>bufferFrames_)return;
 std::fill(outputBuffer_.begin(),outputBuffer_.end(),0);
 for(auto it=voices_.begin();it!=voices_.end();){
  auto& v=*it; const std::size_t avail=v.samples.size()/2-v.frame,count=std::min<std::size_t>(bufferFrames_,avail);
  for(std::size_t i=0;i<count;++i){const auto s=(v.frame+i)*2,d=i*2;
   const int l=int(outputBuffer_[d])+int(std::lround(v.samples[s]*v.volume));
   const int r=int(outputBuffer_[d+1])+int(std::lround(v.samples[s+1]*v.volume));
   outputBuffer_[d]=std::int16_t(std::clamp(l,int(std::numeric_limits<std::int16_t>::min()),int(std::numeric_limits<std::int16_t>::max())));
   outputBuffer_[d+1]=std::int16_t(std::clamp(r,int(std::numeric_limits<std::int16_t>::min()),int(std::numeric_limits<std::int16_t>::max())));
  }
  v.frame+=count;if(v.frame>=v.samples.size()/2)it=voices_.erase(it);else ++it;
 }
 sceAudioOutOutput(port_,outputBuffer_.data());
#endif
}
}
