#pragma once

#include <windows.h>

#if defined(DX11_OVERLAY_EXPORTS)
#define DX11_OVERLAY_API extern "C" __declspec(dllexport)
#else
#define DX11_OVERLAY_API extern "C" __declspec(dllimport)
#endif

// StartOverlay must be called after LoadLibrary has returned. It is idempotent
// while the overlay thread is already running.
DX11_OVERLAY_API BOOL WINAPI StartOverlay();

// Requests cooperative shutdown. This function does not wait.
DX11_OVERLAY_API void WINAPI RequestOverlayShutdown();

// Waits for the overlay thread itself to exit, not merely for renderer cleanup
// to begin. WAIT_OBJECT_0 means that unloading the DLL is now permitted.
DX11_OVERLAY_API DWORD WINAPI WaitForOverlayShutdown(DWORD timeoutMilliseconds);

// Reports the state of the retained OS thread handle. FALSE means the overlay
// thread has actually exited (or was never started).
DX11_OVERLAY_API BOOL WINAPI IsOverlayRunning();

#undef DX11_OVERLAY_API
