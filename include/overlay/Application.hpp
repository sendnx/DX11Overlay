#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "overlay/Logger.hpp"
#include "overlay/PatternScanner.hpp"

namespace overlay {
    using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn =
        HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    struct ModuleRecord {
        HMODULE handle = nullptr;
        std::string name;
        std::string path;
        uintptr_t base = 0;
        size_t size = 0;
    };

    struct ScanRecord {
        std::string module;
        std::string pattern;
        std::vector<uintptr_t> addresses;
    };

    class Application {
    public:
        Application() = default;
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        ~Application();

        [[nodiscard]] bool Initialize(HMODULE module);
        void Run();
        static void RequestStop() noexcept;

    private:
        struct Config {
            bool showDemo = false;
            bool drawCursorCircle = true;
            float circleRadius = 48.0f;
        };

        static HRESULT STDMETHODCALLTYPE PresentHook(
            IDXGISwapChain*, UINT, UINT);
        static HRESULT STDMETHODCALLTYPE ResizeBuffersHook(
            IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
        static LRESULT CALLBACK WndProcHook(HWND, UINT, WPARAM, LPARAM);

        [[nodiscard]] bool InstallHooks(HMODULE module);
        [[nodiscard]] bool InitializeRenderer(IDXGISwapChain* swapChain);
        [[nodiscard]] bool CreateRenderTarget(IDXGISwapChain* swapChain);
        void Shutdown() noexcept;

        void Render();
        void RenderOverviewTab();
        void RenderModulesTab();
        void RenderScannerTab();
        void RenderHexViewerTab();
        void RenderLogTab();

        void RefreshModules();
        void SelectModule(size_t index);
        void BeginPatternScan();
        void RefreshHexView();
        void InspectorWorker(std::stop_token stopToken);

        Config m_config;
        Logger m_logger;

        std::atomic<bool> m_running{true};
        std::atomic<bool> m_shuttingDown{false};
        std::atomic<bool> m_menuOpen{true};
        std::atomic<bool> m_scanning{false};
        std::atomic<bool> m_headerHealthy{false};

        Microsoft::WRL::ComPtr<ID3D11Device> m_device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTarget;
        IDXGISwapChain* m_activeSwapChain = nullptr;
        HWND m_window = nullptr;
        WNDPROC m_originalWndProc = nullptr;
        bool m_imguiInitialized = false;
        bool m_win32Initialized = false;
        bool m_dx11Initialized = false;
        bool m_ownsMinHook = false;

        void* m_presentTarget = nullptr;
        void* m_resizeTarget = nullptr;
        std::mutex m_renderMutex;

        std::vector<ModuleRecord> m_modules;
        size_t m_selectedModule = 0;
        std::vector<ModuleSection> m_sections;

        std::array<char, 256> m_pattern{
            "48 89 5C 24 ? 57 48 81 EC ? ? ? ?"
        };
        std::mutex m_scanMutex;
        std::vector<ScanRecord> m_scanHistory;
        std::string m_scanStatus{"Ready"};

        std::array<char, 32> m_hexAddress{"0"};
        int m_hexLength = 128;
        std::vector<std::byte> m_hexBytes;
        std::string m_hexStatus{"Enter an address in hexadecimal."};

        std::jthread m_inspectorThread;
        std::jthread m_scanThread;

        static inline std::atomic<Application*> s_instance{nullptr};
        static inline PresentFn s_originalPresent = nullptr;
        static inline ResizeBuffersFn s_originalResize = nullptr;
    };
}
