# Validation report

This file records concrete build and runtime evidence. It is intentionally
separate from the instructions in `BUILDING.md`: instructions describe what
should happen, while this report records what actually happened.

## Validation run: 2026-07-29

### Environment

- Host: Linux Mint 22.3, x86_64
- Windows ABI toolchain: LLVM-MinGW 20260616, LLVM/Clang 22.1.8, UCRT
- Build system: CMake 4.3.3
- Runtime compatibility layer: Wine 9.0
- Graphics path: Wine DX11 with Mesa on Intel Haswell
- Target: Windows x64, C++20
- Dependencies:
  - Dear ImGui `v1.91.9b`
  - MinHook `v1.3.4`

Both downloaded tool archives were checked before use:

```text
CMake:
927b2368a946c37269c3a66225ab00544e756459cdd0b5d0da438694fb9ff802

LLVM-MinGW:
534b92e067b22a6b4441f48ae9240a3341b17825d04d577eab0cf85c44b4deda
```

These values matched the hashes published with the corresponding releases.

### Build results

The complete project, including Dear ImGui, MinHook, the DLL, and all test
hosts, was compiled twice:

| Configuration | Result | DLL output |
|---|---|---|
| Release | Passed | `build/DX11Overlay.dll` |
| Debug | Passed | `out/DX11Overlay.dll` |

The Release DLL was identified as:

```text
PE32+ executable (DLL), x86-64, for MS Windows
```

The four expected lifecycle exports were present:

```text
StartOverlay
RequestOverlayShutdown
WaitForOverlayShutdown
IsOverlayRunning
```

The Release DLL SHA-256 was:

```text
2389239d0f2ca813d850a7b02b018396263fe7737804450c4bda99063a6f1b2b
```

### Automated runtime results

CTest executed the Windows binaries through Wine.

Release:

```text
1/3 overlay_tests          Passed
2/3 overlay_lifecycle      Passed
3/3 overlay_dx11_smoke     Passed

100% tests passed, 0 tests failed out of 3
```

Debug:

```text
1/3 overlay_tests          Passed
2/3 overlay_lifecycle      Passed
3/3 overlay_dx11_smoke     Passed

100% tests passed, 0 tests failed out of 3
```

The tests covered:

- readable, null, and inaccessible memory behavior;
- pointer-chain semantics;
- real PE-section pattern matching and wildcard matching;
- cancellation through a stop token;
- DLL loading and lifecycle export discovery;
- idempotent startup;
- active-thread observation;
- cooperative shutdown and retained-thread-handle waiting;
- restart after clean shutdown;
- an immediate stop request during startup;
- creation of a real DX11 window, device, and swap chain;
- observation of overlay renderer initialization through WndProc subclassing;
- hooked `Present` calls;
- hooked `ResizeBuffers` and render-target recreation;
- restoration of the original WndProc;
- safe DLL unloading after the overlay thread exited.

### Install staging

The Release install step completed and produced:

- `bin/DX11Overlay.dll`;
- the import library;
- `include/overlay/OverlayApi.hpp`;
- the project MIT License;
- third-party notices;
- exact Dear ImGui and MinHook license texts.

The staged DLL hash matched the built Release DLL hash.

## Issues found by this validation

The first real link attempt found that the Dear ImGui DX11 backend's
`D3DCompile` dependency was not linked explicitly. `d3dcompiler` is now part of
the CMake target.

The compiler also reported an intentionally ignored `[[nodiscard]]` renderer
initialization result. The call now marks the discard explicitly.

The DX11 smoke host initially kept its own render target bound during resize.
It now unbinds and flushes the context before releasing that target.

## Important remaining native-Windows gate

This run is strong cross-build and Wine/DX11 evidence, but it is not a
substitute for the repository's native Visual Studio job.

Before publishing an official binary release:

1. Push the repository so `.github/workflows/windows-ci.yml` can run.
2. Require both native MSVC Debug and Release jobs to pass.
3. Download the MSVC-built artifact produced by that workflow.
4. Perform one final smoke run on Windows 10 or 11.

The local LLVM-MinGW build dynamically uses `libc++.dll` and `libunwind.dll`.
Those runtime DLLs were placed beside the test binaries for this validation.
Do not publish the local cross-compiled `stage/` directory as the official
Windows package. The intended release artifact is the native MSVC artifact
from Windows CI.

## Clean revalidation: 2026-07-29

Both configurations were rebuilt with `--clean-first`, forcing all project,
Dear ImGui, MinHook, and test-host objects to be compiled and linked again.

The repeated runtime results were:

```text
Release: 3/3 passed, 0 failed
Debug:   3/3 passed, 0 failed
```

The repeated runs again covered the pure tests, DLL lifecycle test, and real
DX11 Present/resize/shutdown smoke test.

Wine briefly failed to start its window driver when several fresh Wine
processes were launched back-to-back. Keeping `wineserver` alive removed that
environmental startup race. Afterward, both complete suites and the two
graphics tests run individually passed.

The rebuilt Release DLL was again identified as a Windows x64 PE32+ DLL and
still exported all four lifecycle functions. Its new build hash was:

```text
510c0abc9ef7ef179585a2b1e54a75cdcd3e0e424a8e175561c405e102cd056d
```

The reinstalled `stage/bin/DX11Overlay.dll` matched that Release hash exactly.
