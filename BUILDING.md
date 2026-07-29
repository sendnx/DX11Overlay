# Building and validation

The main README is a case study. Build and release mechanics live here so the
lesson remains focused.

Concrete results from completed build and runtime checks are recorded in
[`VALIDATION.md`](VALIDATION.md).

## Prerequisites

- Windows 10 or 11, x64
- Visual Studio 2022 with Desktop development with C++
- CMake 3.24 or newer
- Git access for the default FetchContent dependency mode

## Configure, build, and test

Run from an x64 Native Tools Command Prompt or Developer PowerShell:

```powershell
cmake -S . -B build -A x64 `
  -DDX11_OVERLAY_BUILD_TESTS=ON `
  -DDX11_OVERLAY_WARNINGS_AS_ERRORS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix stage
```

The staged public API, import library, and DLL are written below `stage/`.

## Dependency modes

The default mode downloads the pinned Dear ImGui and MinHook revisions.

For an offline or parent-project build, define compatible `imgui_dx11` and
`minhook` CMake targets before adding this project, then configure with:

```cmake
set(DX11_OVERLAY_FETCH_DEPS OFF)
add_subdirectory(path/to/DX11Overlay)
```

The expected `imgui_dx11` target must expose the core Dear ImGui sources plus
the Win32 and DX11 backends. The `minhook` target must expose `MinHook.h`.

## Host lifecycle contract

Loading the DLL does not start worker threads. Resolve and call the public API
from [`OverlayApi.hpp`](include/overlay/OverlayApi.hpp):

```cpp
HMODULE module = LoadLibraryW(L"DX11Overlay.dll");
if (!module) {
    return;
}

auto start = reinterpret_cast<BOOL(WINAPI*)()>(
    GetProcAddress(module, "StartOverlay"));
auto requestStop = reinterpret_cast<void(WINAPI*)()>(
    GetProcAddress(module, "RequestOverlayShutdown"));
auto waitForStop = reinterpret_cast<DWORD(WINAPI*)(DWORD)>(
    GetProcAddress(module, "WaitForOverlayShutdown"));

if (!start || !requestStop || !waitForStop || !start()) {
    FreeLibrary(module);
    return;
}

// Run the authorized DX11 test host...

requestStop();
if (waitForStop(INFINITE) == WAIT_OBJECT_0) {
    FreeLibrary(module);
}
```

Never call `FreeLibrary` while `WaitForOverlayShutdown` reports a timeout or
failure.

## Release checklist

- Debug and Release jobs pass in Windows CI.
- Tests run with warnings treated as errors.
- The DX11 smoke test observes renderer initialization, completes a hooked
  resize, restores WndProc, and shuts the overlay thread down cleanly.
- The Release install artifact contains the DLL, import library, and API header.
- The host follows the explicit Start/Stop/Wait lifecycle.
- The changelog and project version agree with the intended tag.
- The staged artifact contains the project MIT License and exact upstream
  dependency license texts.
