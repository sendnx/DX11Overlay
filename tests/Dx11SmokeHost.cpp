#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstring>
#include <iostream>

namespace {
    using Microsoft::WRL::ComPtr;

    using StartOverlayFn = BOOL(WINAPI*)();
    using RequestStopFn = void(WINAPI*)();
    using WaitForStopFn = DWORD(WINAPI*)(DWORD);

    LRESULT CALLBACK SmokeWndProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) {
        if (message == WM_CLOSE) {
            DestroyWindow(window);
            return 0;
        }
        if (message == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    template <typename Function>
    Function Resolve(HMODULE module, const char* name) {
        const FARPROC address = GetProcAddress(module, name);
        static_assert(sizeof(Function) == sizeof(address));
        Function function = nullptr;
        std::memcpy(&function, &address, sizeof(function));
        return function;
    }

    bool CreateRenderTarget(
        IDXGISwapChain* swapChain,
        ID3D11Device* device,
        ComPtr<ID3D11RenderTargetView>& target) {
        ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(swapChain->GetBuffer(
                0,
                __uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(
                    backBuffer.ReleaseAndGetAddressOf())))) {
            return false;
        }
        return SUCCEEDED(device->CreateRenderTargetView(
            backBuffer.Get(), nullptr, target.ReleaseAndGetAddressOf()));
    }
}

int main(int argumentCount, char** arguments) {
    if (argumentCount != 2) {
        std::cerr << "usage: overlay_dx11_smoke_host <DX11Overlay.dll>\n";
        return 2;
    }

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"DX11OverlaySmokeHost";

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = SmokeWndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;
    if (!RegisterClassExW(&windowClass)) {
        std::cerr << "FAIL: window class registration failed\n";
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        className,
        L"DX11 Overlay Smoke Test",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!window) {
        UnregisterClassW(className, instance);
        std::cerr << "FAIL: test window creation failed\n";
        return 1;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);

    DXGI_SWAP_CHAIN_DESC description{};
    description.BufferCount = 2;
    description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.OutputWindow = window;
    description.SampleDesc.Count = 1;
    description.Windowed = TRUE;
    description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    auto createDevice = [&](D3D_DRIVER_TYPE driver) {
        return D3D11CreateDeviceAndSwapChain(
            nullptr,
            driver,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &description,
            swapChain.ReleaseAndGetAddressOf(),
            device.ReleaseAndGetAddressOf(),
            nullptr,
            context.ReleaseAndGetAddressOf());
    };

    HRESULT result = createDevice(D3D_DRIVER_TYPE_HARDWARE);
    if (FAILED(result)) {
        result = createDevice(D3D_DRIVER_TYPE_WARP);
    }
    if (FAILED(result) || !swapChain || !device || !context) {
        DestroyWindow(window);
        UnregisterClassW(className, instance);
        std::cerr << "FAIL: DX11 device and swap chain creation failed\n";
        return 1;
    }

    ComPtr<ID3D11RenderTargetView> renderTarget;
    if (!CreateRenderTarget(swapChain.Get(), device.Get(), renderTarget)) {
        DestroyWindow(window);
        UnregisterClassW(className, instance);
        std::cerr << "FAIL: initial render target creation failed\n";
        return 1;
    }

    const LONG_PTR originalWndProc = GetWindowLongPtrW(window, GWLP_WNDPROC);
    HMODULE overlayModule = LoadLibraryA(arguments[1]);
    if (!overlayModule) {
        DestroyWindow(window);
        UnregisterClassW(className, instance);
        std::cerr << "FAIL: overlay DLL loading failed\n";
        return 1;
    }

    const auto start = Resolve<StartOverlayFn>(overlayModule, "StartOverlay");
    const auto requestStop =
        Resolve<RequestStopFn>(overlayModule, "RequestOverlayShutdown");
    const auto waitForStop =
        Resolve<WaitForStopFn>(overlayModule, "WaitForOverlayShutdown");
    if (!start || !requestStop || !waitForStop || !start()) {
        FreeLibrary(overlayModule);
        DestroyWindow(window);
        UnregisterClassW(className, instance);
        std::cerr << "FAIL: overlay startup failed\n";
        return 1;
    }

    bool rendererObserved = false;
    bool resizeSucceeded = false;
    bool windowClosed = false;
    for (int frame = 0; frame < 180 && !windowClosed; ++frame) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                windowClosed = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (windowClosed) {
            break;
        }

        const float color[4] = {
            0.08f,
            0.12f + static_cast<float>(frame % 60) / 600.0f,
            0.18f,
            1.0f
        };
        ID3D11RenderTargetView* target = renderTarget.Get();
        context->OMSetRenderTargets(1, &target, nullptr);
        context->ClearRenderTargetView(renderTarget.Get(), color);
        if (FAILED(swapChain->Present(1, 0))) {
            break;
        }

        if (GetWindowLongPtrW(window, GWLP_WNDPROC) != originalWndProc) {
            rendererObserved = true;
        }

        if (frame == 90) {
            context->OMSetRenderTargets(0, nullptr, nullptr);
            context->Flush();
            renderTarget.Reset();
            const HRESULT resizeResult = swapChain->ResizeBuffers(
                0, 900, 650, DXGI_FORMAT_UNKNOWN, 0);
            resizeSucceeded =
                SUCCEEDED(resizeResult) &&
                CreateRenderTarget(
                    swapChain.Get(), device.Get(), renderTarget);
            if (!resizeSucceeded) {
                break;
            }
        }
        Sleep(8);
    }

    requestStop();
    const DWORD stopped = waitForStop(10'000);
    const bool wndProcRestored =
        GetWindowLongPtrW(window, GWLP_WNDPROC) == originalWndProc;

    renderTarget.Reset();
    context.Reset();
    device.Reset();
    swapChain.Reset();

    const bool safeToUnload = stopped == WAIT_OBJECT_0;
    if (safeToUnload) {
        FreeLibrary(overlayModule);
    }
    if (IsWindow(window)) {
        DestroyWindow(window);
    }
    UnregisterClassW(className, instance);

    if (!rendererObserved) {
        std::cerr << "FAIL: overlay renderer was never observed\n";
        return 1;
    }
    if (!resizeSucceeded) {
        std::cerr << "FAIL: hooked DX11 resize did not complete\n";
        return 1;
    }
    if (!safeToUnload) {
        std::cerr << "FAIL: overlay thread did not stop safely\n";
        return 1;
    }
    if (!wndProcRestored) {
        std::cerr << "FAIL: original window procedure was not restored\n";
        return 1;
    }

    std::cout
        << "DX11 smoke test passed: renderer, Present, resize and shutdown.\n";
    return 0;
}
