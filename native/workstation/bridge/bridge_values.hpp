#pragma once
#include "mex.h"
#include "Frame.h"
#include "Morph.h"
#include "Body.h"
#include "Library.h"
#include "Stitch.h"
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace mb {
using Args = const mxArray**;
void require(bool ok, const char* message);
std::string str(const mxArray* a);
double scalar(const mxArray* a);
std::vector<double> doubles(const mxArray* a);
ws::Words words(const mxArray* a, int corner = 0);
std::array<ws::Words,4> corners(const mxArray* a);
ws::Chord chord(const mxArray* a);
mxArray* put(const ws::Words& w);
mxArray* put(const ws::Chord& c);
mxArray* put(const std::vector<double>& v);
mxArray* string(const juce::String& s);
mxArray* matrix(mwSize rows, mwSize cols);
void field(mxArray* a, mwIndex i, const char* key, mxArray* v);
mxArray* frames(const std::vector<ws::Frame>& f, const std::vector<juce::String>& sources);
mxArray* loadFrames(const juce::File& root);
bool chordCommand(const std::string& cmd, int nr, Args p, std::vector<mxArray*>& out);
bool dataCommand(const std::string& cmd, int nr, Args p, std::vector<mxArray*>& out);
void count(int nr, int expected);
ws::Lead lead(const ws::Chord& a,const ws::Chord& b);
std::array<ws::Words,4> bodyCorners(const std::array<ws::Words,4>& raw,const std::array<bool,6>& rows,bool unity);
std::array<std::uint8_t,240> bodyBytes(const std::array<ws::Words,4>& corners);
}
