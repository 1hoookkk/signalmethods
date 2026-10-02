#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <atomic>
#include <string>

namespace headspace {

class MidiInput {
public:
    MidiInput() {
        for (UINT id = 0; id < midiInGetNumDevs(); ++id) {
            if (midiInOpen(&device_, id, reinterpret_cast<DWORD_PTR>(&callback),
                           reinterpret_cast<DWORD_PTR>(this), CALLBACK_FUNCTION) != MMSYSERR_NOERROR) continue;
            if (midiInStart(device_) == MMSYSERR_NOERROR) {
                MIDIINCAPSA caps{};
                midiInGetDevCapsA(id, &caps, sizeof(caps)); name = caps.szPname; return;
            }
            midiInClose(device_); device_ = nullptr;
        }
    }
    ~MidiInput() { if (device_) { midiInStop(device_); midiInReset(device_); midiInClose(device_); } }
    MidiInput(const MidiInput&) = delete;
    MidiInput& operator=(const MidiInput&) = delete;
    bool pop(std::uint32_t& message) {
        if (overflow_.exchange(false)) {
            read_.store(write_.load(std::memory_order_acquire), std::memory_order_release);
            message = 176 | (120 << 8); return true;
        }
        const auto read = read_.load(std::memory_order_relaxed);
        if (read == write_.load(std::memory_order_acquire)) return false;
        message = queue_[read]; read_.store((read + 1) % queue_.size(), std::memory_order_release); return true;
    }
    std::string name;
private:
    static void CALLBACK callback(HMIDIIN, UINT event, DWORD_PTR instance, DWORD_PTR data, DWORD_PTR) {
        if (event != MIM_DATA) return;
        auto& self = *reinterpret_cast<MidiInput*>(instance);
        const auto write = self.write_.load(std::memory_order_relaxed);
        const auto next = (write + 1) % self.queue_.size();
        if (next == self.read_.load(std::memory_order_acquire)) { self.overflow_.store(true); return; }
        self.queue_[write] = static_cast<std::uint32_t>(data);
        self.write_.store(next, std::memory_order_release);
    }
    HMIDIIN device_{};
    std::array<std::uint32_t, 256> queue_{};
    std::atomic<std::size_t> read_{}, write_{};
    std::atomic<bool> overflow_{};
};

}
