#pragma once
#define NOMINMAX
#include <miniaudio.h>
#include <trench/core/audition.hpp>
#include <trench/core/native_body.hpp>
#include <atomic>
#include <vector>
#include <memory>
#include <string>

namespace ta {
struct Clip { std::vector<float> samples; double rate=44100; };
struct WordSlot {
    std::atomic<unsigned long long> sequence{0};
    std::array<std::atomic<std::uint16_t>,30> words{};
};
class Engine {
public:
    ma_device device{};
    bool open=false,duplex=false;
    std::string error;
    std::atomic<bool> playing{false},wet{true};
    std::atomic<int> source{0};
    std::atomic<double> in{0},out{0},head{0};
    std::atomic<float> peak{0};
    std::atomic<Clip*> clip{nullptr};
    std::vector<std::unique_ptr<Clip>> clips;
    std::array<WordSlot,2> slots;
    std::atomic<unsigned long long> published{0};
    unsigned long long consumed=0;
    std::array<std::atomic<float>,16384> outputRing{};
    std::unique_ptr<std::atomic<float>[]> input;
    size_t inputSize=0;
    std::atomic<unsigned long long> outputWritten{0},inputWritten{0};
    trench::core::CascadeRunner runner;
    double phase=0,position=0;
    std::uint32_t random=123456789;
    double pink0=0,pink1=0,pink2=0;
    double rate=44100;
    bool start(bool live=false);
    void stop();
    void publish(const std::array<std::uint16_t,30>& words);
    void consume();
    float next(float inputSample);
    void process(float* output,const float* inputSamples,size_t count);
};
}
