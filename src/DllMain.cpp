#include "overlay/Application.hpp"
#include "overlay/OverlayApi.hpp"

#include <atomic>
#include <cstdint>
#include <memory>

namespace {
    std::atomic<HMODULE> g_module{nullptr};
    SRWLOCK g_threadLock = SRWLOCK_INIT;
    HANDLE g_overlayThread = nullptr;
    std::uint64_t g_threadGeneration = 0;
    std::atomic<bool> g_stopRequested{false};

    DWORD WINAPI OverlayMain(LPVOID parameter) {
        auto application = std::make_unique<overlay::Application>();
        if (!application->Initialize(static_cast<HMODULE>(parameter))) {
            return 1;
        }

        if (g_stopRequested.load(std::memory_order_acquire)) {
            overlay::Application::RequestStop();
        }
        application->Run();
        application.reset();
        return 0;
    }

    bool ThreadMayBeActive(HANDLE thread) noexcept {
        if (!thread) {
            return false;
        }
        DWORD exitCode = 0;
        // Treat an unexpected query failure conservatively: never report that
        // unloading is safe unless the OS confirms thread termination.
        return !GetExitCodeThread(thread, &exitCode) ||
            exitCode == STILL_ACTIVE;
    }
}

extern "C" __declspec(dllexport) BOOL WINAPI StartOverlay() {
    AcquireSRWLockExclusive(&g_threadLock);

    if (ThreadMayBeActive(g_overlayThread)) {
        ReleaseSRWLockExclusive(&g_threadLock);
        return TRUE;
    }

    if (g_overlayThread) {
        CloseHandle(g_overlayThread);
        g_overlayThread = nullptr;
    }

    const HMODULE module = g_module.load(std::memory_order_acquire);
    if (!module) {
        ReleaseSRWLockExclusive(&g_threadLock);
        return FALSE;
    }

    g_stopRequested.store(false, std::memory_order_release);
    g_overlayThread = CreateThread(
        nullptr, 0, &OverlayMain, module, 0, nullptr);
    if (g_overlayThread) {
        ++g_threadGeneration;
    }
    const BOOL started = g_overlayThread ? TRUE : FALSE;
    ReleaseSRWLockExclusive(&g_threadLock);
    return started;
}

extern "C" __declspec(dllexport) void WINAPI RequestOverlayShutdown() {
    g_stopRequested.store(true, std::memory_order_release);
    overlay::Application::RequestStop();
}

extern "C" __declspec(dllexport) DWORD WINAPI WaitForOverlayShutdown(
    DWORD timeoutMilliseconds) {
    HANDLE waitHandle = nullptr;
    std::uint64_t observedGeneration = 0;
    bool threadWasPresent = false;
    DWORD duplicateError = ERROR_SUCCESS;

    AcquireSRWLockShared(&g_threadLock);
    if (g_overlayThread) {
        threadWasPresent = true;
        observedGeneration = g_threadGeneration;
        if (!DuplicateHandle(
            GetCurrentProcess(),
            g_overlayThread,
            GetCurrentProcess(),
            &waitHandle,
            SYNCHRONIZE,
            FALSE,
            0)) {
            duplicateError = GetLastError();
        }
    }
    ReleaseSRWLockShared(&g_threadLock);

    if (threadWasPresent && !waitHandle) {
        SetLastError(duplicateError);
        return WAIT_FAILED;
    }
    if (!waitHandle) {
        return WAIT_OBJECT_0;
    }

    DWORD result = WaitForSingleObject(waitHandle, timeoutMilliseconds);
    CloseHandle(waitHandle);

    if (result == WAIT_OBJECT_0) {
        AcquireSRWLockExclusive(&g_threadLock);
        if (observedGeneration == g_threadGeneration &&
            g_overlayThread && !ThreadMayBeActive(g_overlayThread)) {
            CloseHandle(g_overlayThread);
            g_overlayThread = nullptr;
        } else if (observedGeneration != g_threadGeneration &&
                   ThreadMayBeActive(g_overlayThread)) {
            SetLastError(ERROR_BUSY);
            result = WAIT_FAILED;
        }
        ReleaseSRWLockExclusive(&g_threadLock);
    }
    return result;
}

extern "C" __declspec(dllexport) BOOL WINAPI IsOverlayRunning() {
    AcquireSRWLockExclusive(&g_threadLock);
    const bool active = ThreadMayBeActive(g_overlayThread);
    if (!active && g_overlayThread) {
        CloseHandle(g_overlayThread);
        g_overlayThread = nullptr;
    }
    const BOOL running = active ? TRUE : FALSE;
    ReleaseSRWLockExclusive(&g_threadLock);
    return running;
}

BOOL WINAPI DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module.store(module, std::memory_order_release);
        DisableThreadLibraryCalls(module);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_module.store(nullptr, std::memory_order_release);
    }
    return TRUE;
}
