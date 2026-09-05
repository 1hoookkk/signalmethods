#include "mex.h"
#include "audio_engine.hpp"
#include <stdexcept>
#include <cmath>

namespace {
std::unique_ptr<ta::Engine> engine;
void need(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
std::string text(const mxArray* a) {
    need(mxIsChar(a),"Expected text"); auto* p=mxArrayToUTF8String(a); need(p,"Invalid text");
    std::string s(p); mxFree(p); return s;
}
double number(const mxArray* a) {
    need((mxIsNumeric(a)||mxIsLogical(a))&&!mxIsComplex(a)&&mxGetNumberOfElements(a)==1,"Expected scalar");
    double x=mxGetScalar(a); need(std::isfinite(x),"Expected finite number"); return x;
}
void cleanup() { if(engine) { engine->stop(); engine.reset(); } }
mxArray* state() {
    const char* keys[]={"device","rateHz","error","playing","wet","source","peak","playhead","inputWritten","open"};
    auto* a=mxCreateStructMatrix(1,1,10,keys); auto& e=*engine;
    auto set=[&](const char* k,mxArray* v){mxSetField(a,0,k,v);};
    set("device",mxCreateString(e.open?e.device.playback.name:"")); set("rateHz",mxCreateDoubleScalar(e.rate));
    set("error",mxCreateString(e.error.c_str())); set("playing",mxCreateLogicalScalar(e.playing));
    set("wet",mxCreateLogicalScalar(e.wet)); const char* names[]={"saw","noise","sample","input","white"};
    set("source",mxCreateString(names[e.source.load()])); set("peak",mxCreateDoubleScalar(e.peak));
    set("playhead",mxCreateDoubleScalar(e.head)); set("inputWritten",mxCreateDoubleScalar(double(e.inputWritten.load())));
    set("open",mxCreateLogicalScalar(e.open)); return a;
}
mxArray* ring(bool live) {
    auto& e=*engine; size_t n=live?e.inputSize:16384;
    auto written=live?e.inputWritten.load(std::memory_order_acquire):e.outputWritten.load(std::memory_order_acquire);
    auto* a=mxCreateDoubleMatrix(n,1,mxREAL); auto* p=mxGetPr(a);
    for(size_t i=0;i<n;++i) if(written+i>=n) {
        size_t j=size_t((written+i-n)%n); p[i]=live?e.input[j].load():e.outputRing[j].load();
    } return a;
}
void setClip(const mxArray* a,const mxArray* rate) {
    need(mxIsDouble(a)&&!mxIsComplex(a)&&!mxIsSparse(a),"Expected mono doubles");
    auto c=std::make_unique<ta::Clip>(); c->rate=number(rate); need(c->rate>=8000 && c->rate<=384000,"Invalid rate");
    auto* p=mxGetPr(a); size_t n=mxGetNumberOfElements(a); need(n>1,"Clip too short");
    c->samples.reserve(n); for(size_t i=0;i<n;++i) { need(std::isfinite(p[i]),"Invalid sample"); c->samples.push_back(float(p[i])); }
    engine->in=0; engine->out=n/c->rate; auto* ptr=c.get(); engine->clips.push_back(std::move(c)); engine->clip.store(ptr,std::memory_order_release);
}
mxArray* command(const std::string& cmd,int nr,const mxArray** p) {
    auto& e=*engine;
    auto count=[&](int n){need(nr==n,"Wrong number of arguments");};
    if(cmd=="start") { count(1); e.start(e.source==3); return state(); }
    if(cmd=="stop") { count(1); e.stop(); return nullptr; }
    if(cmd=="state") { count(1); return state(); }
    if(cmd=="snapshot" || cmd=="inputRing") { count(1); return ring(cmd=="inputRing"); }
    if(cmd=="peak" || cmd=="playhead" || cmd=="inputWritten") {
        count(1); return mxCreateDoubleScalar(cmd=="peak"?double(e.peak):cmd=="playhead"?double(e.head):double(e.inputWritten.load()));
    }
    if(cmd=="words") {
        count(2); need(mxIsUint16(p[1]) && mxGetM(p[1])==6 && mxGetN(p[1])==5,"Expected uint16 6 x 5 words");
        auto* v=static_cast<const std::uint16_t*>(mxGetData(p[1])); std::array<std::uint16_t,30> w{};
        for(int s=0;s<6;++s) for(int k=0;k<5;++k) w[s*5+k]=v[k*6+s]; e.publish(w);
    } else if(cmd=="playing" || cmd=="wet") { count(2); bool v=number(p[1])!=0; if(cmd=="playing") e.playing=v&&e.open; else e.wet=v; }
    else if(cmd=="source") {
        count(2); auto name=text(p[1]); const std::array<std::string,5> names={"saw","noise","sample","input","white"};
        auto it=std::find(names.begin(),names.end(),name); need(it!=names.end(),"Unknown source"); int src=int(it-names.begin());
        bool resume=e.playing; e.source=src; if(e.open && ((src==3)!=e.duplex)) { e.start(src==3); e.playing=resume&&e.open; }
    } else if(cmd=="clip") { count(3); setClip(p[1],p[2]); }
    else if(cmd=="region") { count(3); double a=number(p[1]),b=number(p[2]); need(a>=0 && b>a,"Invalid loop region"); e.in=a; e.out=b; }
    else throw std::runtime_error("Unknown command");
    return nullptr;
}
}
void mexFunction(int nl,mxArray** lhs,int nr,const mxArray** rhs) {
    std::string cmd="command";
    try {
        need(nr>0,"Command required"); cmd=text(rhs[0]); need(nl<=1,"At most one output");
        if(cmd=="unload") { need(nr==1,"Wrong number of arguments"); cleanup(); if(mexIsLocked()) mexUnlock(); return; }
        if(!engine) { engine=std::make_unique<ta::Engine>(); mexLock(); mexAtExit(cleanup); }
        auto* result=command(cmd,nr,rhs);
        if(nl) lhs[0]=result?result:mxCreateDoubleMatrix(0,0,mxREAL); else if(result) mxDestroyArray(result);
    } catch(const std::exception& e) { mexErrMsgIdAndTxt(("trench:audio:"+cmd).c_str(),"%s",e.what()); }
}
