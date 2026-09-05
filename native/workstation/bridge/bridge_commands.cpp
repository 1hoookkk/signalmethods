#include "bridge_values.hpp"
#include <trench/core/body_from_audio.hpp>

namespace mb {
void putMorph(const ws::Morph& m,std::vector<mxArray*>& out) {
    auto* g=mxCreateLogicalMatrix(6,1);
    for(int i=0;i<6;++i) mxGetLogicals(g)[i]=m.guarded[i];
    out={put(m.words),g,mxCreateLogicalScalar(m.outside)};
}
std::vector<trench::core::audio::Resonance> read(const mxArray* x,const mxArray* fs,const mxArray* mode) {
    auto d=doubles(x); std::vector<float> block(d.begin(),d.end());
    double rate=scalar(fs); require(rate>=8000 && rate<=384000,"Invalid sample rate");
    auto m=str(mode); require(m=="speech" || m=="bells","Unknown reader");
    if(block.size()<256) return {};
    return m=="speech"?trench::core::audio::speech_poles(block,rate,6):trench::core::audio::resonances_from_audio(block,rate,6);
}
mxArray* readFrame(const std::vector<trench::core::audio::Resonance>& input) {
    if(input.empty()) return matrix(0,0);
    auto r=input; std::sort(r.begin(),r.end(),[](auto a,auto b){return a.hz<b.hz;}); ws::Words w{};
    for(size_t i=0;i<6;++i) {
        trench::core::SectionGeometry g;
        g.pole=trench::core::ConjugatePair{20000,0}; g.zero=trench::core::ConjugatePair{20000,0};
        if(i<r.size()) g.pole=trench::core::ConjugatePair{std::clamp(r[i].hz,20.0,20000.0),
            std::clamp(std::exp(-3.141592653589793*std::max(10.0,r[i].bw_hz)/44100),0.0,0.9995)};
        w[i]=trench::core::words_from_geometry(g,44100);
    }
    ws::unityDc(w); return put(w);
}
bool bodyCommand(const std::string& cmd,int nr,Args p,std::vector<mxArray*>& out) {
    if(cmd=="bodyLerp" || cmd=="bodyRepresentable") {
        count(nr,cmd=="bodyLerp"?4:2); require(mxIsUint8(p[1]) && mxGetNumberOfElements(p[1])==240,"Expected 240 uint8 bytes");
        auto b=trench::core::PackedBody::from_legacy_bytes({static_cast<const std::uint8_t*>(mxGetData(p[1])),240});
        if(cmd=="bodyRepresentable") out.push_back(mxCreateLogicalScalar(b.is_legacy_representable()));
        else { auto cw=b.interpolate_words(float(scalar(p[2])),float(scalar(p[3])),0); ws::Words w{};
            std::copy_n(cw.begin(),6,w.begin()); out.push_back(put(w)); }
        return true;
    }
    if(cmd!="bodyCorners" && cmd!="bodyBytes" && cmd!="writeBody") return false;
    bool write=cmd=="writeBody"; count(nr,write?5:4); int arg=write?2:1;
    auto cw=corners(p[arg]); std::array<bool,6> rows;
    require(mxGetNumberOfElements(p[arg+1])==6,"Expected six row switches");
    require(mxIsLogical(p[arg+1]) || mxIsDouble(p[arg+1]),"Expected row switches");
    for(int i=0;i<6;++i) rows[i]=mxIsLogical(p[arg+1])?mxGetLogicals(p[arg+1])[i]:mxGetPr(p[arg+1])[i]!=0;
    cw=bodyCorners(cw,rows,scalar(p[arg+2])!=0);
    if(write) {
        juce::File file(str(p[1])); require(file.getParentDirectory().createDirectory().wasOk(),"Cannot create body folder");
        auto bytes=bodyBytes(cw);
        require(file.replaceWithData(bytes.data(),bytes.size()),"Cannot write body"); out.push_back(mxCreateLogicalScalar(true));
    } else if(cmd=="bodyBytes") {
        auto bytes=bodyBytes(cw); auto* a=mxCreateNumericMatrix(240,1,mxUINT8_CLASS,mxREAL);
        std::copy(bytes.begin(),bytes.end(),static_cast<std::uint8_t*>(mxGetData(a))); out.push_back(a);
    } else {
        mwSize dims[]={6,5,4}; auto* a=mxCreateNumericArray(3,dims,mxUINT16_CLASS,mxREAL);
        auto* d=static_cast<std::uint16_t*>(mxGetData(a));
        for(int i=0;i<4;++i) { auto w=cw[i];
            for(int s=0;s<6;++s) for(int k=0;k<5;++k) d[i*30+k*6+s]=w[s][k]; } out.push_back(a);
    } return true;
}
bool dataCommand(const std::string& cmd,int nr,Args p,std::vector<mxArray*>& out) {
    if(bodyCommand(cmd,nr,p,out)) return true;
    if(cmd=="pairMorph") { count(nr,4); putMorph(ws::pairMorph(words(p[1]),words(p[2]),scalar(p[3])),out); }
    else if(cmd=="wheelMorph") { count(nr,4); putMorph(ws::wheelMorph(corners(p[1]),scalar(p[2]),scalar(p[3])),out); }
    else if(cmd=="unityDc") { count(nr,2); auto w=words(p[1]); ws::unityDc(w); out.push_back(put(w)); }
    else if(cmd=="curveHz") { count(nr,1); out.push_back(put(trench::core::logarithmic_frequency_grid(20,20000,160))); }
    else if(cmd=="responseDb" || cmd=="sectionDb") {
        bool section=cmd=="sectionDb"; count(nr,section?4:3); auto w=words(p[1]); auto hz=doubles(p[section?3:2]);
        int row=section?int(scalar(p[2]))-1:0; require(row>=0 && row<6,"Row outside 1..6");
        auto cascade=ws::cascadeOf(w); for(auto& h:hz) h=section?ws::sectionDb(w,row,h):ws::responseDb(cascade,h);
        out.push_back(put(hz));
    } else if(cmd=="geometry") {
        count(nr,2); auto g=ws::geometryOf(words(p[1])); auto* a=matrix(6,6); auto* d=mxGetPr(a);
        for(int i=0;i<6;++i) { d[i]=g[i].pole; d[i+6]=g[i].pHz; d[i+12]=g[i].pR;
            d[i+18]=g[i].zero; d[i+24]=g[i].zHz; d[i+30]=g[i].zR; } out.push_back(a);
    } else if(cmd=="excessOf") {
        count(nr,2); auto e=ws::excessOf(words(p[1])); auto* a=matrix(e.size(),2);
        for(size_t i=0;i<e.size();++i) { mxGetPr(a)[i]=e[i].hz; mxGetPr(a)[i+e.size()]=e[i].above; } out.push_back(a);
    } else if(cmd=="readFrame" || cmd=="readResonances") {
        count(nr,4); auto r=read(p[1],p[2],p[3]);
        if(cmd=="readFrame") out.push_back(readFrame(r));
        else { auto* a=matrix(r.size(),3); for(size_t i=0;i<r.size();++i) {
            mxGetPr(a)[i]=r[i].hz; mxGetPr(a)[i+r.size()]=r[i].bw_hz; mxGetPr(a)[i+2*r.size()]=r[i].gain_db; } out.push_back(a); }
    } else return false;
    return true;
}
}
