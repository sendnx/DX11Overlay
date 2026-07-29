#include <windows.h>

#include <cstring>
#include <iostream>

namespace {
    using StartOverlayFn = BOOL(WINAPI*)();
    using RequestStopFn = void(WINAPI*)();
    using WaitForStopFn = DWORD(WINAPI*)(DWORD);
    using IsRunningFn = BOOL(WINAPI*)();

    template <typename Function>
    Function Resolve(HMODULE module, const char* name) {
        const FARPROC address = GetProcAddress(module, name);
        static_assert(sizeof(Function) == sizeof(address));
        Function function = nullptr;
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }

    int Fail(HMODULE module, const char* message, bool safeToUnload = true) {
        std::cerr << "FAIL: " << message << '\n';
        if (module && safeToUnload) {
            FreeLibrary(module);
        }
        return 1;
    }
}

int main(int argumentCount, char** arguments) {
    if (argumentCount != 2) {
        std::cerr << "usage: overlay_lifecycle_host <DX11Overlay.dll>\n";
        return 2;
    }

    HMODULE module = LoadLibraryA(arguments[1]);
    if (!module) {
        return Fail(nullptr, "LoadLibrary failed");
    }

    const auto start = Resolve<StartOverlayFn>(module, "StartOverlay");
    const auto requestStop =
        Resolve<RequestStopFn>(module, "RequestOverlayShutdown");
    const auto waitForStop =
        Resolve<WaitForStopFn>(module, "WaitForOverlayShutdown");
    const auto isRunning =
        Resolve<IsRunningFn>(module, "IsOverlayRunning");

    if (!start || !requestStop || !waitForStop || !isRunning) {
        return Fail(module, "one or more lifecycle exports are missing");
    }
    if (!start()) {
        return Fail(module, "StartOverlay failed");
    }
    Sleep(250);
    if (!isRunning()) {
        waitForStop(0);
        return Fail(module, "overlay initialization thread exited unexpectedly");
    }
    if (!start()) {
        requestStop();
        const DWORD stopped = waitForStop(10'000);
        return Fail(
            module,
            "StartOverlay is not idempotent",
            stopped == WAIT_OBJECT_0);
    }
    if (!isRunning()) {
        waitForStop(0);
        return Fail(module, "overlay thread exited during initialization");
    }
    if (waitForStop(0) != WAIT_TIMEOUT) {
        requestStop();
        const DWORD stopped = waitForStop(10'000);
        return Fail(
            module,
            "zero-time wait did not observe the active thread",
            stopped == WAIT_OBJECT_0);
    }

    requestStop();
    if (waitForStop(10'000) != WAIT_OBJECT_0) {
        return Fail(
            module,
            "overlay thread did not stop within ten seconds",
            false);
    }
    if (isRunning()) {
        return Fail(module, "thread state remained active after successful wait");
    }

    if (!start()) {
        return Fail(module, "overlay could not restart after a clean shutdown");
    }
    requestStop();
    if (waitForStop(10'000) != WAIT_OBJECT_0) {
        return Fail(
            module,
            "immediate shutdown request was lost during startup",
            false);
    }

    FreeLibrary(module);
    std::cout << "Overlay lifecycle test passed.\n";
    return 0;
}
