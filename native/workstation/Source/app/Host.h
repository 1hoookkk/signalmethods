#pragma once

#include <windows.h>
#include <string>

namespace headspace {

class Host {
public:
    bool init(HINSTANCE instance, int show, bool compact);
    bool running();
    void beginFrame();
    void endFrame();
    void shutdown();
    void capture(const char* filename) const;
    HWND window() const { return window_; }

private:
    HWND window_{};
};

}
