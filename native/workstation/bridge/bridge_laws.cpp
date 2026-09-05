#include "bridge_values.hpp"

namespace mb {
ws::Lead lead(const ws::Chord& a,const ws::Chord& b) {
    auto ca=a,cb=b;
    ws::Stage fixed; fixed.pole={true,1000000,.25}; fixed.zero={true,1000000,.25};
    ca[5]=fixed; cb[5]=fixed;
    auto result=ws::leadTo(ca,cb);
    require(result.map[5]==5,"Ceiling voice leading failed");
    result.a[5]=a[5]; result.b[5]=b[5];
    return result;
}
std::array<ws::Words,4> bodyCorners(const std::array<ws::Words,4>& raw,const std::array<bool,6>& rows,bool unity) {
    std::array<ws::Chord,4> chords;
    for(int i=0;i<4;++i) chords[i]=ws::decompile(raw[i],ws::kDatumHz);
    auto pinned=chords[0];
    for(int i=1;i<4;++i) if(raw[i]!=raw[0]) { pinned=lead(chords[0],chords[i]).a; break; }
    std::array<ws::Words,4> out;
    for(int i=0;i<4;++i) {
        auto c=raw[i]==raw[0]?pinned:lead(pinned,chords[i]).b;
        out[i]=ws::compile(c,ws::kDatumHz);
        for(int row=0;row<6;++row) if(!rows[row]) out[i][row]=trench::core::kIdentitySection;
        if(unity) ws::unityDc(out[i]);
    }
    return out;
}
std::array<std::uint8_t,240> bodyBytes(const std::array<ws::Words,4>& corners) {
    trench::core::PackedBody body;
    for(int i=0;i<8;++i) {
        body.words[i].fill(trench::core::kIdentitySection);
        std::copy(corners[i%4].begin(),corners[i%4].end(),body.words[i].begin());
    }
    return body.legacy_bytes();
}
}
