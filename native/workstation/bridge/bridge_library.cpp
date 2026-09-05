#include "bridge_values.hpp"
#include <trench/core/formants.hpp>

namespace mb {
mxArray* frames(const std::vector<ws::Frame>& f,const std::vector<juce::String>& sources) {
    const char* keys[]={"name","group","chord","words","capture","source","root","voicing","resonance"};
    auto* a=mxCreateStructMatrix(1,f.size(),9,keys);
    for(size_t i=0;i<f.size();++i) {
        field(a,i,"name",string(f[i].name)); field(a,i,"group",mxCreateString(ws::kGroupNames[f[i].group]));
        field(a,i,"chord",put(f[i].chord)); field(a,i,"words",put(f[i].words));
        field(a,i,"capture",mxCreateLogicalScalar(f[i].capture)); field(a,i,"source",string(sources[i]));
        auto s=ws::Stitch::shapeOf(f[i].chord);
        field(a,i,"root",mxCreateDoubleScalar(s.root)); field(a,i,"voicing",mxCreateDoubleScalar(s.voicing));
        field(a,i,"resonance",mxCreateDoubleScalar(s.width));
    } return a;
}
mxArray* loadFrames(const juce::File& root) {
    ws::Library lib; std::vector<juce::String> sources;
    auto json=root.getChildFile("native/python/workstation/frames_3d.json");
    if(!lib.loadJson(json)) require(lib.loadBodies(root.getChildFile("plugin/presets/p2k")),"No factory frames");
    std::erase_if(lib.frames,[](const ws::Frame& f){ return f.group==3 || f.group==6; });
    sources.resize(lib.frames.size(),"native/python/workstation/frames_3d.json");
    lib.addSchwa(); sources.resize(lib.frames.size(),"uniform tube");
    auto files=root.getChildFile("native/python/workstation/chords").findChildFiles(juce::File::findFiles,false,"*.json");
    files.sort();
    for(const auto& file:files) { lib.loadChords(file); sources.resize(lib.frames.size(),file.getRelativePathFrom(root)); }
    auto file=root.getChildFile("native/python/workstation/stitch.json");
    ws::Stitch stitch; require(stitch.loadJson(file),"Cannot load stitch.json");
    auto data=juce::JSON::parse(file)["nodes"];
    for(size_t i=0;i<stitch.nodes.size();++i) {
        const auto& n=stitch.nodes[i]; if(n.floor!=1) continue;
        auto members=data[int(i)]["members"];
        juce::String name=members.isArray() && members.size()>0 ? members[0].toString() : "Morpheus " + juce::String(int(i)+1);
        lib.addNamed(n.words,name,1,false); sources.push_back("native/python/workstation/stitch.json node " + juce::String(int(i)));
    }
    return frames(lib.frames,sources);
}
mxArray* klattTable() {
    const auto v=trench::core::p2k::klatt_vowels(); const char* keys[]={"symbol","F","B"};
    auto* a=mxCreateStructMatrix(1,v.size(),3,keys);
    for(size_t i=0;i<v.size();++i) {
        field(a,i,"symbol",mxCreateString(std::string(v[i].symbol).c_str()));
        auto* f=matrix(1,3); auto* b=matrix(1,3);
        for(int j=0;j<3;++j) { mxGetPr(f)[j]=v[i].f[j].hz; mxGetPr(b)[j]=v[i].f[j].bw_hz; }
        field(a,i,"F",f); field(a,i,"B",b);
    } return a;
}
bool chordCommand(const std::string& cmd,int nr,Args p,std::vector<mxArray*>& out) {
    if(cmd=="loadFrames") { count(nr,2); out.push_back(loadFrames(juce::File(str(p[1])))); }
    else if(cmd=="klattVowels") { count(nr,1); out.push_back(klattTable()); }
    else if(cmd=="groupNames") {
        count(nr,1); auto* a=mxCreateCellMatrix(1,7);
        for(int i=0;i<7;++i) mxSetCell(a,i,mxCreateString(ws::kGroupNames[i])); out.push_back(a);
    } else if(cmd=="decompile" || cmd=="decompileAt") {
        count(nr,cmd=="decompile"?2:3); out.push_back(put(ws::decompile(words(p[1]),nr==3?scalar(p[2]):ws::kDatumHz)));
    } else if(cmd=="compile" || cmd=="compileAt") {
        count(nr,cmd=="compile"?2:3); out.push_back(put(ws::compile(chord(p[1]),nr==3?scalar(p[2]):ws::kDatumHz)));
    } else if(cmd=="fitVoices" || cmd=="sharpen") {
        count(nr,cmd=="fitVoices"?2:3); auto c=chord(p[1]);
        if(cmd=="sharpen") ws::sharpen(c,scalar(p[2]));
        else for(auto& s:c) { if(s.pole.on) ws::fitVoice(s.pole,true,ws::kDatumHz); if(s.zero.on) ws::fitVoice(s.zero,false,ws::kDatumHz); }
        out.push_back(put(c));
    } else if(cmd=="chordFrom") {
        count(nr,5); auto v=doubles(p[2]); require(v.size()==6,"Expected six intervals"); std::array<double,6> iv{};
        std::copy(v.begin(),v.end(),iv.begin()); out.push_back(put(ws::chordFrom(scalar(p[1]),iv,scalar(p[3]),scalar(p[4]))));
    } else if(cmd=="leadTo" || cmd=="leadCost") {
        count(nr,3); auto a=chord(p[1]),b=chord(p[2]);
        if(cmd=="leadCost") out.push_back(mxCreateDoubleScalar(lead(a,b).cost));
        else { auto l=lead(a,b); out={put(l.a),put(l.b)}; std::vector<double> m;
            for(int i:l.map) m.push_back(i+1); out.push_back(put(m)); out.push_back(mxCreateDoubleScalar(l.cost)); }
    } else if(cmd=="shapeOf") {
        count(nr,2); auto s=ws::Stitch::shapeOf(chord(p[1])); out.push_back(put(std::vector<double>{s.root,s.voicing,s.width,double(s.any)}));
    } else if(cmd=="intervalsOf") { count(nr,2); out.push_back(string(ws::intervalsOf(chord(p[1])))); }
    else if(cmd=="noteName") { count(nr,2); out.push_back(string(ws::noteName(scalar(p[1])))); }
    else if(cmd=="noteOf" || cmd=="hzOf" || cmd=="widthOf" || cmd=="radiusOf") {
        count(nr,(cmd=="noteOf" || cmd=="hzOf")?2:3); auto v=doubles(p[1]); double y=nr==3?scalar(p[2]):0;
        for(auto& x:v) x=cmd=="noteOf"?ws::noteOf(x):cmd=="hzOf"?ws::hzOf(x):cmd=="widthOf"?ws::widthOf(x,y,ws::kDatumHz):ws::radiusOf(x,y,ws::kDatumHz);
        out.push_back(put(v));
    } else return false;
    return true;
}
}
