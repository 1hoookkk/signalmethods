#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "audio_engine.hpp"
#include <algorithm>
#include <cmath>

namespace ta {
void callback(ma_device* d,void* out,const void* in,ma_uint32 n) {
    static_cast<Engine*>(d->pUserData)->process(static_cast<float*>(out),static_cast<const float*>(in),n);
}
bool Engine::start(bool live) {
    if(open && duplex==live) return true;
    stop(); duplex=live; error.clear();
    auto c=ma_device_config_init(live?ma_device_type_duplex:ma_device_type_playback);
    c.playback.format=ma_format_f32; c.playback.channels=2;
    c.capture.format=ma_format_f32; c.capture.channels=1;
    c.sampleRate=0; c.dataCallback=callback; c.pUserData=this;
    auto result=ma_device_init(nullptr,&c,&device);
    if(result!=MA_SUCCESS) { error=ma_result_description(result); return false; }
    open=true; rate=device.sampleRate; inputSize=size_t(rate*10);
    input=std::make_unique<std::atomic<float>[]>(inputSize);
    inputWritten=0; outputWritten=0;
    for(auto& v:outputRing) v.store(0);
    runner.set_sample_rate(rate); runner.reset(); runner.set_ring_leveller(true); runner.set_pole_distortion(0);
    trench::core::Cascade identity{}; for(auto& row:identity) row=trench::core::section_words_to_biquad(trench::core::kIdentitySection);
    runner.set_immediate(identity); consumed=0;
    result=ma_device_start(&device);
    if(result!=MA_SUCCESS) { error=ma_result_description(result); stop(); return false; } return true;
}
void Engine::stop() {
    playing=false; if(open) { ma_device_uninit(&device); open=false; } peak=0;
}
void Engine::publish(const std::array<std::uint16_t,30>& w) {
    auto generation=published.load()+1; auto& slot=slots[generation%2];
    slot.sequence.store(generation*2-1,std::memory_order_seq_cst);
    for(size_t i=0;i<30;++i) slot.words[i].store(w[i],std::memory_order_seq_cst);
    slot.sequence.store(generation*2,std::memory_order_seq_cst); published.store(generation,std::memory_order_release);
}
void Engine::consume() {
    auto generation=published.load(std::memory_order_acquire); if(generation==0 || generation==consumed) return;
    auto& slot=slots[generation%2]; if(slot.sequence.load()!=generation*2) return;
    trench::core::CornerWords w; w.fill(trench::core::kIdentitySection);
    for(size_t s=0;s<6;++s) for(size_t k=0;k<5;++k) w[s][k]=slot.words[s*5+k].load();
    if(slot.sequence.load()!=generation*2) return;
    runner.set_glide(trench::core::native::rewarp_cascade(w,44100,rate),256); consumed=generation;
}
float Engine::next(float x) {
    int src=source.load(std::memory_order_relaxed);
    if(src==3) return x;
    if(src==1 || src==4) {
        random^=random<<13; random^=random>>17; random^=random<<5; double white=double(random)/4294967295.0*2-1;
        if(src==4) return float(white*0.4);
        pink0=0.99765*pink0+white*0.0990460; pink1=0.96300*pink1+white*0.2965164; pink2=0.57000*pink2+white*1.0526913;
        return float(0.18*(pink0+pink1+pink2+white*0.1848));
    }
    if(src==0) { phase+=110/rate; phase-=std::floor(phase); return float((2*phase-1)*0.4); }
    auto* c=clip.load(std::memory_order_acquire); if(!c || c->samples.size()<2) return 0;
    double a=std::clamp(in.load()*c->rate,0.0,double(c->samples.size()-1));
    double b=std::clamp(out.load()*c->rate,a+1,double(c->samples.size()));
    if(position<a || position>=b) position=a;
    size_t i=size_t(position),j=i+1; if(double(j)>=b) j=size_t(a);
    float value=float(c->samples[i]+(c->samples[j]-c->samples[i])*(position-i));
    position+=c->rate/rate; if(position>=b) position=a+std::fmod(position-a,b-a);
    head.store(position/c->rate,std::memory_order_relaxed); return value;
}
void Engine::process(float* output,const float* inputSamples,size_t count) {
    consume(); std::array<float,1024> block{}; float highest=0;
    auto iw=inputWritten.load(),ow=outputWritten.load();
    for(size_t offset=0;offset<count;offset+=block.size()) {
        size_t n=std::min(block.size(),count-offset);
        bool play=playing.load(),filter=wet.load();
        for(size_t i=0;i<n;++i) {
            float x=inputSamples?inputSamples[offset+i]:0;
            if(inputSamples && inputSize) input[(iw++)%inputSize].store(x,std::memory_order_relaxed);
            block[i]=play?next(x):0;
        }
        if(play && filter) runner.process(std::span<float>(block.data(),n));
        for(size_t i=0;i<n;++i) {
            float y=std::isfinite(block[i])?block[i]*0.5f:0;
            highest=std::max(highest,std::abs(y)); outputRing[(ow++)%16384].store(y,std::memory_order_relaxed);
            output[2*(offset+i)]=y; output[2*(offset+i)+1]=y;
        }
    }
    inputWritten.store(iw,std::memory_order_release); outputWritten.store(ow,std::memory_order_release); peak=highest;
}
}
