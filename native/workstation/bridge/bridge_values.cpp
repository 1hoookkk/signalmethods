#include "bridge_values.hpp"
#include <map>

namespace mb {
std::map<std::array<double,6>,ws::Stage> rawRows;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void count(int nr, int expected) { require(nr == expected, "Wrong number of arguments"); }
std::string str(const mxArray* a) {
    require(mxIsChar(a), "Expected character text");
    char* p = mxArrayToUTF8String(a);
    require(p != nullptr, "Invalid text");
    std::string s(p); mxFree(p); return s;
}
double scalar(const mxArray* a) {
    require((mxIsNumeric(a) || mxIsLogical(a)) && !mxIsComplex(a)
      && mxGetNumberOfElements(a)==1, "Expected real scalar");
    double x=mxGetScalar(a); require(std::isfinite(x), "Non-finite number"); return x;
}
std::vector<double> doubles(const mxArray* a) {
    require(mxIsDouble(a) && !mxIsComplex(a) && !mxIsSparse(a), "Expected full real double array");
    const auto* p=mxGetPr(a); std::vector<double> v(p,p+mxGetNumberOfElements(a));
    for (double x:v) require(std::isfinite(x), "Non-finite number"); return v;
}
ws::Words words(const mxArray* a,int corner) {
    const auto* dims=mxGetDimensions(a);
    require(mxIsUint16(a) && !mxIsComplex(a) && dims[0]==6 && dims[1]==5
      && mxGetNumberOfElements(a)>=30u*(corner+1), "Expected uint16 6 x 5 words");
    ws::Words w{}; const auto* p=static_cast<const std::uint16_t*>(mxGetData(a));
    for (int s=0;s<6;++s) for(int k=0;k<5;++k) w[s][k]=p[corner*30+k*6+s]; return w;
}
std::array<ws::Words,4> corners(const mxArray* a) {
    require(mxGetNumberOfDimensions(a)==3 && mxGetDimensions(a)[2]==4,"Expected 6 x 5 x 4 corners");
    return {words(a,0),words(a,1),words(a,2),words(a,3)};
}
ws::Chord chord(const mxArray* a) {
    require(mxGetM(a)==6 && mxGetN(a)==7,"Expected double 6 x 7 chord");
    const auto v=doubles(a); ws::Chord c{};
    for(int s=0;s<6;++s) {
        require((v[s]==0 || v[s]==1) && (v[s+18]==0 || v[s+18]==1),"Voice on must be 0 or 1");
        require(v[s+12]>=0 && v[s+12]<=120 && v[s+30]>=0 && v[s+30]<=120,"Width outside 0..120");
        std::array<double,6> key{}; for(int k=0;k<6;++k) key[k]=v[s+k*6];
        auto it=rawRows.find(key); if(it!=rawRows.end()) c[s]=it->second;
        c[s].pole={v[s]!=0,v[s+6],v[s+12]}; c[s].zero={v[s+18]!=0,v[s+24],v[s+30]}; c[s].gainDb=v[s+36];
    } return c;
}
mxArray* matrix(mwSize r,mwSize c) { return mxCreateDoubleMatrix(r,c,mxREAL); }
mxArray* put(const ws::Words& w) {
    auto* a=mxCreateNumericMatrix(6,5,mxUINT16_CLASS,mxREAL); auto* p=static_cast<std::uint16_t*>(mxGetData(a));
    for(int s=0;s<6;++s) for(int k=0;k<5;++k) p[k*6+s]=w[s][k]; return a;
}
mxArray* put(const ws::Chord& c) {
    auto* a=matrix(6,7); auto* p=mxGetPr(a);
    for(int s=0;s<6;++s) {
        std::array<double,6> key{double(c[s].pole.on),c[s].pole.note,c[s].pole.width,
            double(c[s].zero.on),c[s].zero.note,c[s].zero.width};
        rawRows[key]=c[s]; for(int k=0;k<6;++k) p[k*6+s]=key[k]; p[36+s]=c[s].gainDb;
    } return a;
}
mxArray* put(const std::vector<double>& v) {
    auto* a=matrix(v.size(),1); std::copy(v.begin(),v.end(),mxGetPr(a)); return a;
}
mxArray* string(const juce::String& s) { return mxCreateString(s.toRawUTF8()); }
void field(mxArray* a,mwIndex i,const char* key,mxArray* v) { mxSetField(a,i,key,v); }
}
