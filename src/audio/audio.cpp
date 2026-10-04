#include "audio/audio.hpp"
#include "platform/runtime_diagnostics.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <array>
#include <cstring>
#include <cctype>
#include <cstdio>
#include <string>
#include <exception>
#ifdef __vita__
#include <psp2/audioout.h>
#include <vorbis/vorbisfile.h>
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
 RuntimeDiagnostics::checkpoint("audio_init_begin");
#ifdef __vita__
 port_=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,bufferFrames_,48000,SCE_AUDIO_OUT_MODE_STEREO);
 if(port_<0){ RuntimeDiagnostics::checkpoint("audio_init_failed", "sceAudioOutOpenPort=" + std::to_string(port_)); port_=-1;return false;}
 int volume=SCE_AUDIO_VOLUME_0DB;
 const int volumeResult = sceAudioOutSetVolume(port_,static_cast<SceAudioOutChannelFlag>(SCE_AUDIO_VOLUME_FLAG_L_CH|SCE_AUDIO_VOLUME_FLAG_R_CH),&volume);
 outputBuffer_.assign(std::size_t(bufferFrames_)*2,0);
 RuntimeDiagnostics::checkpoint("audio_port_opened", "port=" + std::to_string(port_) + " volume_result=" + std::to_string(volumeResult));
#endif
 initialized_=true;
 RuntimeDiagnostics::checkpoint("audio_ready");
 return true;
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

#ifdef __vita__
namespace {
struct OggMemory {
 const std::uint8_t* data{nullptr};
 std::size_t size{0};
 std::size_t pos{0};
};
size_t oggRead(void* ptr,size_t size,size_t nmemb,void* datasource){
 auto& m=*static_cast<OggMemory*>(datasource);
 const std::size_t want=size*nmemb;
 const std::size_t take=std::min(want,m.size-m.pos);
 if(take) std::memcpy(ptr,m.data+m.pos,take);
 m.pos+=take;
 return size ? take/size : 0;
}
int oggSeek(void* datasource,ogg_int64_t offset,int whence){
 auto& m=*static_cast<OggMemory*>(datasource);
 std::int64_t base=whence==SEEK_SET?0:(whence==SEEK_CUR?static_cast<std::int64_t>(m.pos):static_cast<std::int64_t>(m.size));
 const std::int64_t next=base+offset;
 if(next<0||static_cast<std::uint64_t>(next)>m.size)return -1;
 m.pos=static_cast<std::size_t>(next); return 0;
}
int oggClose(void*){return 0;}
long oggTell(void* datasource){return static_cast<long>(static_cast<OggMemory*>(datasource)->pos);}
}
#endif

bool Audio::playOgg(const std::vector<std::uint8_t>& bytes,float v,bool loop){
 if(!initialized_||bytes.empty()||voices_.size()>=16)return false;
 try {
#ifdef __vita__
 OggMemory memory{bytes.data(),bytes.size(),0};
 OggVorbis_File vf{};
 ov_callbacks callbacks{oggRead,oggSeek,oggClose,oggTell};
 if(ov_open_callbacks(&memory,&vf,nullptr,0,callbacks)<0)return false;
 vorbis_info* info=ov_info(&vf,-1);
 if(!info||info->channels<1||info->rate<=0){ov_clear(&vf);return false;}
 const int channels=info->channels;
 const int sourceRate=info->rate;
 const ogg_int64_t total=ov_pcm_total(&vf,-1);

 // Long looping music is the memory-sensitive case on real Vita hardware.
 // Decode music as mono at 24 kHz, then expand to the stereo mixer only once
 // at the end. This keeps the resident music buffer roughly one quarter of
 // the previous 48 kHz stereo representation while preserving playback.
 const bool compactMusic = loop && total > 0;
 if(compactMusic){
   const std::uint64_t sourceFrames=static_cast<std::uint64_t>(total);
   const std::uint64_t compactFrames=(sourceFrames*24000u)/static_cast<unsigned>(sourceRate);
   const std::uint64_t compactBytes=compactFrames*sizeof(std::int16_t);
   constexpr std::uint64_t maxResidentMusicBytes=24u*1024u*1024u;
   if(compactBytes>maxResidentMusicBytes){
     ov_clear(&vf);
     RuntimeDiagnostics::checkpoint("audio_ogg_deferred",
       "decoded_bytes=" + std::to_string(compactBytes) +
       " compressed_bytes=" + std::to_string(bytes.size()));
     return false;
   }

   std::vector<std::int16_t> mono;
   if(total>0 && total<static_cast<ogg_int64_t>(std::numeric_limits<std::size_t>::max()))
     mono.reserve(static_cast<std::size_t>(total));
   std::array<char,4096> buffer{};
   int bitstream=0;
   for(;;){
     const long got=ov_read(&vf,buffer.data(),static_cast<int>(buffer.size()),0,2,1,&bitstream);
     if(got==0)break;
     if(got<0){ov_clear(&vf);return false;}
     const std::size_t samples=static_cast<std::size_t>(got)/2;
     const auto* src=reinterpret_cast<const std::int16_t*>(buffer.data());
     const std::size_t frames=samples/static_cast<std::size_t>(channels);
     for(std::size_t i=0;i<frames;++i){
       std::int32_t sum=0;
       for(int c=0;c<channels;++c)
         sum += src[i*static_cast<std::size_t>(channels)+static_cast<std::size_t>(c)];
       mono.push_back(static_cast<std::int16_t>(sum/channels));
     }
   }
   ov_clear(&vf);
   if(mono.empty())return false;

   const std::size_t outFrames=std::max<std::size_t>(1,
     (static_cast<std::uint64_t>(mono.size())*24000u)/static_cast<unsigned>(sourceRate));
   std::vector<std::int16_t> compact(outFrames);
   for(std::size_t i=0;i<outFrames;++i){
     const std::size_t src=std::min(mono.size()-1,
       static_cast<std::size_t>((static_cast<std::uint64_t>(i)*static_cast<unsigned>(sourceRate))/24000u));
     compact[i]=mono[src];
   }
   std::vector<std::int16_t> pcm(compact.size()*2);
   for(std::size_t i=0;i<compact.size();++i){
     pcm[i*2]=compact[i];
     pcm[i*2+1]=compact[i];
   }
   Voice x; x.samples=std::move(pcm); x.volume=std::clamp(v,0.f,1.f); x.loop=true; x.music=true;
   if(loop)stopMusic();
   voices_.push_back(std::move(x));
   RuntimeDiagnostics::checkpoint("audio_ogg_compact_ready",
     "source_rate=" + std::to_string(sourceRate) +
     ",frames=" + std::to_string(mono.size()));
   return true;
 }

 std::vector<std::int16_t> pcm;
 if(total>0&&total<static_cast<ogg_int64_t>(std::numeric_limits<std::size_t>::max()/2))
     pcm.reserve(static_cast<std::size_t>(total)*2);
 std::array<char,4096> buffer{};
 int bitstream=0;
 for(;;){
   const long got=ov_read(&vf,buffer.data(),static_cast<int>(buffer.size()),0,2,1,&bitstream);
   if(got==0)break;
   if(got<0){ov_clear(&vf);return false;}
   const std::size_t samples=static_cast<std::size_t>(got)/2;
   const auto* src=reinterpret_cast<const std::int16_t*>(buffer.data());
   const std::size_t frames=samples/static_cast<std::size_t>(channels);
   for(std::size_t i=0;i<frames;++i){
      if(channels==1){pcm.push_back(src[i]);pcm.push_back(src[i]);}
      else {pcm.push_back(src[i*channels]);pcm.push_back(src[i*channels+1]);}
   }
 }
 ov_clear(&vf);
 if(pcm.empty())return false;
 if(sourceRate!=sampleRate_){
   const std::size_t inFrames=pcm.size()/2;
   const std::size_t outFrames=std::max<std::size_t>(1,(std::uint64_t(inFrames)*sampleRate_)/static_cast<unsigned>(sourceRate));
   std::vector<std::int16_t> resampled(outFrames*2);
   for(std::size_t i=0;i<outFrames;++i){
      const std::size_t src=std::min(inFrames-1,std::size_t((std::uint64_t(i)*sourceRate)/sampleRate_));
      resampled[i*2]=pcm[src*2]; resampled[i*2+1]=pcm[src*2+1];
   }
   pcm.swap(resampled);
 }
 if(loop)stopMusic();
 Voice x; x.samples=std::move(pcm); x.volume=std::clamp(v,0.f,1.f); x.loop=loop; x.music=loop;
 voices_.push_back(std::move(x)); return true;
#else
 (void)bytes;(void)v;(void)loop; return false;
#endif
 } catch (const std::bad_alloc&) {
  RuntimeDiagnostics::checkpoint("audio_ogg_alloc_failed", "bytes=" + std::to_string(bytes.size()));
  return false;
 } catch (const std::exception& e) {
  RuntimeDiagnostics::checkpoint("audio_ogg_exception", e.what());
  return false;
 } catch (...) {
  RuntimeDiagnostics::checkpoint("audio_ogg_exception", "unknown");
  return false;
 }
}
bool Audio::playAsset(const std::vector<std::uint8_t>& bytes,const std::string& name,float v,bool loop){
 const auto dot=name.find_last_of('.');
 if(dot==std::string::npos)return false;
 std::string ext=name.substr(dot+1);
 std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
 if(ext=="wav")return playWav(bytes,v);
 if(ext=="ogg")return playOgg(bytes,v,loop);
 return false;
}

void Audio::stopMusic(){
 voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[](const Voice& v){return v.music;}),voices_.end());
}

bool Audio::playWav(const std::vector<std::uint8_t>& b,float v){
 if(!initialized_||b.size()<44||!tag(b,0,"RIFF")||!tag(b,8,"WAVE"))return false;
 std::uint16_t ch=0,bits=0; std::uint32_t rate=0; std::size_t off=0,size=0,p=12;
 while(p+8<=b.size()){
  const std::uint32_t n=u32(b,p+4); const std::size_t q=p+8; if(q>b.size()||n>b.size()-q)return false;
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
   const int leftSample=v.samples[s];
   const int rightSample=v.samples[s+1];
   const int l=int(outputBuffer_[d])+int(std::lround(leftSample*v.volume));
   const int r=int(outputBuffer_[d+1])+int(std::lround(rightSample*v.volume));
   outputBuffer_[d]=std::int16_t(std::clamp(l,int(std::numeric_limits<std::int16_t>::min()),int(std::numeric_limits<std::int16_t>::max())));
   outputBuffer_[d+1]=std::int16_t(std::clamp(r,int(std::numeric_limits<std::int16_t>::min()),int(std::numeric_limits<std::int16_t>::max())));
  }
  v.frame+=count;
  if(v.frame>=v.samples.size()/2){
   if(v.loop) v.frame=0;
   else it=voices_.erase(it);
  } else ++it;
 }
 sceAudioOutOutput(port_,outputBuffer_.data());
#endif
}
}
