#include "overlay/Application.hpp"

#include "overlay/Memory.hpp"

#include <psapi.h>
#include <MinHook.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam);

namespace overlay {
    namespace {
        const char* LevelName(LogLevel level) noexcept {
            switch (level) {
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            }
            return "?";
        }

        ImVec4 LevelColor(LogLevel level) noexcept {
            switch (level) {
            case LogLevel::Info: return {0.65f, 0.85f, 1.0f, 1.0f};
            case LogLevel::Warning: return {1.0f, 0.78f, 0.30f, 1.0f};
            case LogLevel::Error: return {1.0f, 0.35f, 0.35f, 1.0f};
            }
            return {1, 1, 1, 1};
        }

        std::string HexAddress(uintptr_t address) {
            std::ostringstream stream;
            stream << "0x" << std::hex << std::uppercase << address;
            return stream.str();
        }

        std::string SectionFlags(unsigned long flags) {
            std::string result;
            if ((flags & IMAGE_SCN_MEM_READ) != 0) result += 'R';
            if ((flags & IMAGE_SCN_MEM_WRITE) != 0) result += 'W';
            if ((flags & IMAGE_SCN_MEM_EXECUTE) != 0) result += 'X';
            if (result.empty()) result = "-";
            return result;
        }

        template <typename Function>
        void* FunctionAddress(Function function) noexcept {
            static_assert(sizeof(Function) == sizeof(void*));
            void* address = nullptr;
            std::memcpy(&address, &function, sizeof(address));
            return address;
        }

        template <typename Function>
        Function FunctionFromAddress(void* address) noexcept {
            static_assert(sizeof(Function) == sizeof(void*));
            Function function = nullptr;
            std::memcpy(&function, &address, sizeof(function));
            return function;
        }
    }

    Application::~Application() {
        Shutdown();
    }

    void Application::RequestStop() noexcept {
        if (Application* app = s_instance.load(std::memory_order_acquire)) {
            app->m_running.store(false, std::memory_order_release);
        }
    }

    HRESULT STDMETHODCALLTYPE Application::PresentHook(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags) {
        Application* app = s_instance.load(std::memory_order_acquire);
        const PresentFn original = s_originalPresent;
        if (!original) {
            return E_FAIL;
        }
        if (!app || app->m_shuttingDown.load(std::memory_order_acquire)) {
            return original(swapChain, syncInterval, flags);
        }

        {
            std::lock_guard renderLock(app->m_renderMutex);
            if (!app->m_imguiInitialized) {
                (void)app->InitializeRenderer(swapChain);
            }

            if (app->m_imguiInitialized &&
                app->m_activeSwapChain == swapChain &&
                !app->m_shuttingDown.load(std::memory_order_relaxed)) {
                if (GetAsyncKeyState(VK_INSERT) & 1) {
                    app->m_menuOpen.store(
                        !app->m_menuOpen.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                }
                if (GetAsyncKeyState(VK_END) & 1) {
                    app->m_running.store(false, std::memory_order_release);
                }

                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                app->Render();
                ImGui::Render();

                if (app->m_context && app->m_renderTarget) {
                    ID3D11RenderTargetView* target = app->m_renderTarget.Get();
                    app->m_context->OMSetRenderTargets(1, &target, nullptr);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                }
            }
        }

        return original(swapChain, syncInterval, flags);
    }

    HRESULT STDMETHODCALLTYPE Application::ResizeBuffersHook(
        IDXGISwapChain* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        UINT swapChainFlags) {
        Application* app = s_instance.load(std::memory_order_acquire);
        const ResizeBuffersFn original = s_originalResize;
        if (!original) {
            return E_FAIL;
        }
        if (!app || app->m_shuttingDown.load(std::memory_order_acquire) ||
            app->m_activeSwapChain != swapChain) {
            return original(
                swapChain, bufferCount, width, height, format, swapChainFlags);
        }

        std::lock_guard renderLock(app->m_renderMutex);
        app->m_renderTarget.Reset();
        const HRESULT result = original(
            swapChain, bufferCount, width, height, format, swapChainFlags);
        if (SUCCEEDED(result) && app->m_imguiInitialized &&
            !app->CreateRenderTarget(swapChain)) {
            app->m_logger.Write(
                LogLevel::Error,
                "ResizeBuffers succeeded but render-target recreation failed.");
        }
        return result;
    }

    LRESULT CALLBACK Application::WndProcHook(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) {
        Application* app = s_instance.load(std::memory_order_acquire);
        if (!app || !app->m_originalWndProc) {
            return DefWindowProc(window, message, wParam, lParam);
        }

        if (app->m_menuOpen.load(std::memory_order_relaxed) &&
            ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam)) {
            return TRUE;
        }
        return CallWindowProc(
            app->m_originalWndProc, window, message, wParam, lParam);
    }

    bool Application::CreateRenderTarget(IDXGISwapChain* swapChain) {
        if (!swapChain || !m_device) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
        if (FAILED(swapChain->GetBuffer(
                0,
                __uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(backBuffer.ReleaseAndGetAddressOf())))) {
            return false;
        }

        m_renderTarget.Reset();
        return SUCCEEDED(m_device->CreateRenderTargetView(
            backBuffer.Get(), nullptr, m_renderTarget.ReleaseAndGetAddressOf()));
    }

    bool Application::InitializeRenderer(IDXGISwapChain* swapChain) {
        if (!swapChain) {
            return false;
        }

        DXGI_SWAP_CHAIN_DESC description{};
        if (FAILED(swapChain->GetDesc(&description)) ||
            !description.OutputWindow ||
            !IsWindow(description.OutputWindow)) {
            return false;
        }

        RECT client{};
        if (!GetClientRect(description.OutputWindow, &client) ||
            client.right <= client.left || client.bottom <= client.top) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Device> device;
        if (FAILED(swapChain->GetDevice(
                __uuidof(ID3D11Device),
                reinterpret_cast<void**>(device.ReleaseAndGetAddressOf())))) {
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        device->GetImmediateContext(context.ReleaseAndGetAddressOf());
        if (!context) {
            return false;
        }

        m_device = std::move(device);
        m_context = std::move(context);
        m_window = description.OutputWindow;
        m_activeSwapChain = swapChain;

        if (!CreateRenderTarget(swapChain)) {
            m_device.Reset();
            m_context.Reset();
            m_window = nullptr;
            m_activeSwapChain = nullptr;
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::StyleColorsDark();

        if (!ImGui_ImplWin32_Init(m_window)) {
            ImGui::DestroyContext();
            m_renderTarget.Reset();
            m_device.Reset();
            m_context.Reset();
            m_window = nullptr;
            m_activeSwapChain = nullptr;
            return false;
        }
        m_win32Initialized = true;

        if (!ImGui_ImplDX11_Init(m_device.Get(), m_context.Get())) {
            ImGui_ImplWin32_Shutdown();
            m_win32Initialized = false;
            ImGui::DestroyContext();
            m_renderTarget.Reset();
            m_device.Reset();
            m_context.Reset();
            m_window = nullptr;
            m_activeSwapChain = nullptr;
            return false;
        }
        m_dx11Initialized = true;

        SetLastError(0);
        const LONG_PTR previous = SetWindowLongPtr(
            m_window,
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(FunctionAddress(&WndProcHook)));
        if (previous == 0 && GetLastError() != 0) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            m_dx11Initialized = false;
            m_win32Initialized = false;
            ImGui::DestroyContext();
            m_renderTarget.Reset();
            m_device.Reset();
            m_context.Reset();
            m_window = nullptr;
            m_activeSwapChain = nullptr;
            return false;
        }

        m_originalWndProc = FunctionFromAddress<WNDPROC>(
            reinterpret_cast<void*>(previous));
        m_imguiInitialized = true;
        m_logger.Write(LogLevel::Info, "ImGui and the DX11 renderer initialized.");
        return true;
    }

    bool Application::InstallHooks(HMODULE module) {
        const std::string className =
            "DX11OverlayDummy_" + std::to_string(GetCurrentProcessId());

        WNDCLASSEXA windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_CLASSDC;
        windowClass.lpfnWndProc = DefWindowProcA;
        windowClass.hInstance = module;
        windowClass.lpszClassName = className.c_str();
        if (!RegisterClassExA(&windowClass)) {
            m_logger.Write(LogLevel::Error, "Dummy window class creation failed.");
            return false;
        }

        HWND dummyWindow = CreateWindowExA(
            0, className.c_str(), "DX11 Overlay Dummy", WS_OVERLAPPEDWINDOW,
            0, 0, 300, 300, nullptr, nullptr, module, nullptr);
        if (!dummyWindow) {
            UnregisterClassA(className.c_str(), module);
            m_logger.Write(LogLevel::Error, "Dummy window creation failed.");
            return false;
        }

        DXGI_SWAP_CHAIN_DESC description{};
        description.BufferCount = 1;
        description.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.OutputWindow = dummyWindow;
        description.SampleDesc.Count = 1;
        description.Windowed = TRUE;
        description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;

        auto createDevice = [&](D3D_DRIVER_TYPE driver) {
            return D3D11CreateDeviceAndSwapChain(
                nullptr, driver, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                &description, swapChain.ReleaseAndGetAddressOf(),
                device.ReleaseAndGetAddressOf(), nullptr,
                context.ReleaseAndGetAddressOf());
        };

        HRESULT result = createDevice(D3D_DRIVER_TYPE_HARDWARE);
        if (FAILED(result)) {
            result = createDevice(D3D_DRIVER_TYPE_WARP);
        }
        if (FAILED(result) || !swapChain) {
            DestroyWindow(dummyWindow);
            UnregisterClassA(className.c_str(), module);
            m_logger.Write(LogLevel::Error, "Dummy DX11 swap chain creation failed.");
            return false;
        }

        void** vtable = *reinterpret_cast<void***>(swapChain.Get());
        m_presentTarget = vtable[8];
        m_resizeTarget = vtable[13];

        context.Reset();
        device.Reset();
        swapChain.Reset();
        DestroyWindow(dummyWindow);
        UnregisterClassA(className.c_str(), module);

        const MH_STATUS initStatus = MH_Initialize();
        if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
            m_logger.Write(LogLevel::Error, "MinHook initialization failed.");
            return false;
        }
        m_ownsMinHook = initStatus == MH_OK;

        void* originalPresent = nullptr;
        if (MH_CreateHook(
                m_presentTarget,
                FunctionAddress(&PresentHook),
                &originalPresent) != MH_OK) {
            if (m_ownsMinHook) MH_Uninitialize();
            m_ownsMinHook = false;
            m_presentTarget = nullptr;
            m_resizeTarget = nullptr;
            m_logger.Write(LogLevel::Error, "Present hook creation failed.");
            return false;
        }
        s_originalPresent =
            FunctionFromAddress<PresentFn>(originalPresent);

        void* originalResize = nullptr;
        if (MH_CreateHook(
                m_resizeTarget,
                FunctionAddress(&ResizeBuffersHook),
                &originalResize) != MH_OK) {
            MH_RemoveHook(m_presentTarget);
            if (m_ownsMinHook) MH_Uninitialize();
            m_ownsMinHook = false;
            m_presentTarget = nullptr;
            m_resizeTarget = nullptr;
            s_originalPresent = nullptr;
            m_logger.Write(LogLevel::Error, "ResizeBuffers hook creation failed.");
            return false;
        }
        s_originalResize =
            FunctionFromAddress<ResizeBuffersFn>(originalResize);

        const MH_STATUS presentEnabled = MH_EnableHook(m_presentTarget);
        const MH_STATUS resizeEnabled = MH_EnableHook(m_resizeTarget);
        if (presentEnabled != MH_OK || resizeEnabled != MH_OK) {
            MH_DisableHook(m_presentTarget);
            MH_DisableHook(m_resizeTarget);
            MH_RemoveHook(m_presentTarget);
            MH_RemoveHook(m_resizeTarget);
            if (m_ownsMinHook) MH_Uninitialize();
            m_ownsMinHook = false;
            m_presentTarget = nullptr;
            m_resizeTarget = nullptr;
            s_originalPresent = nullptr;
            s_originalResize = nullptr;
            m_logger.Write(LogLevel::Error, "Hook activation failed.");
            return false;
        }

        m_logger.Write(LogLevel::Info, "Present and ResizeBuffers hooks installed.");
        return true;
    }

    bool Application::Initialize(HMODULE module) {
        Application* expected = nullptr;
        if (!s_instance.compare_exchange_strong(
                expected, this, std::memory_order_acq_rel)) {
            return false;
        }
        RefreshModules();

        if (!InstallHooks(module)) {
            s_instance.store(nullptr, std::memory_order_release);
            return false;
        }

        m_inspectorThread = std::jthread([this](std::stop_token token) {
            InspectorWorker(token);
        });
        return true;
    }

    void Application::Run() {
        while (m_running.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void Application::RefreshModules() {
        std::array<HMODULE, 1024> handles{};
        DWORD bytesNeeded = 0;
        if (!EnumProcessModules(
                GetCurrentProcess(),
                handles.data(),
                static_cast<DWORD>(sizeof(handles)),
                &bytesNeeded)) {
            m_logger.Write(LogLevel::Error, "EnumProcessModules failed.");
            return;
        }

        const size_t count = std::min<size_t>(
            handles.size(), bytesNeeded / sizeof(HMODULE));
        std::vector<ModuleRecord> modules;
        modules.reserve(count);

        for (size_t index = 0; index < count; ++index) {
            MODULEINFO information{};
            if (!GetModuleInformation(
                    GetCurrentProcess(),
                    handles[index],
                    &information,
                    sizeof(information))) {
                continue;
            }

            std::array<char, MAX_PATH> name{};
            std::array<char, MAX_PATH> path{};
            GetModuleBaseNameA(
                GetCurrentProcess(), handles[index], name.data(),
                static_cast<DWORD>(name.size()));
            GetModuleFileNameExA(
                GetCurrentProcess(), handles[index], path.data(),
                static_cast<DWORD>(path.size()));

            modules.push_back({
                handles[index],
                name.data(),
                path.data(),
                reinterpret_cast<uintptr_t>(information.lpBaseOfDll),
                information.SizeOfImage
            });
        }

        std::sort(modules.begin(), modules.end(), [](const auto& left, const auto& right) {
            return left.base < right.base;
        });
        m_modules = std::move(modules);
        SelectModule(m_modules.empty() ? 0 : std::min(m_selectedModule, m_modules.size() - 1));
        m_logger.Write(LogLevel::Info, "Process module list refreshed.");
    }

    void Application::SelectModule(size_t index) {
        if (m_modules.empty() || index >= m_modules.size()) {
            m_selectedModule = 0;
            m_sections.clear();
            return;
        }
        m_selectedModule = index;
        PatternScanner scanner(m_modules[index].handle);
        m_sections = scanner.Sections();
        std::snprintf(
            m_hexAddress.data(),
            m_hexAddress.size(),
            "%llX",
            static_cast<unsigned long long>(m_modules[index].base));
    }

    void Application::BeginPatternScan() {
        if (m_modules.empty() ||
            m_scanning.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        if (m_scanThread.joinable()) {
            m_scanThread.join();
        }

        const ModuleRecord module = m_modules[m_selectedModule];
        const std::string pattern = m_pattern.data();
        {
            std::lock_guard lock(m_scanMutex);
            m_scanStatus = "Scanning " + module.name + "...";
        }

        m_scanThread = std::jthread(
            [this, module, pattern](std::stop_token token) {
                PatternScanner scanner(module.handle);
                auto addresses = scanner.FindAll(pattern, 64, token);

                if (!token.stop_requested()) {
                    std::lock_guard lock(m_scanMutex);
                    m_scanHistory.insert(
                        m_scanHistory.begin(),
                        ScanRecord{module.name, pattern, std::move(addresses)});
                    if (m_scanHistory.size() > 20) {
                        m_scanHistory.resize(20);
                    }
                    m_scanStatus =
                        std::to_string(m_scanHistory.front().addresses.size()) +
                        " result(s) found.";
                }
                m_scanning.store(false, std::memory_order_release);
            });
    }

    void Application::RefreshHexView() {
        std::string_view text{m_hexAddress.data()};
        if (text.starts_with("0x") || text.starts_with("0X")) {
            text.remove_prefix(2);
        }

        uintptr_t address = 0;
        const auto parsed = std::from_chars(
            text.data(), text.data() + text.size(), address, 16);
        if (text.empty() || parsed.ec != std::errc{} ||
            parsed.ptr != text.data() + text.size()) {
            m_hexBytes.clear();
            m_hexStatus = "Invalid hexadecimal address.";
            return;
        }

        m_hexLength = std::clamp(m_hexLength, 16, 512);
        const auto bytes = memory::ReadBytes(
            address, static_cast<size_t>(m_hexLength));
        if (!bytes) {
            m_hexBytes.clear();
            m_hexStatus = "The requested range is not fully readable.";
            return;
        }

        m_hexBytes = *bytes;
        m_hexStatus =
            std::to_string(m_hexBytes.size()) + " byte(s) read from " +
            HexAddress(address) + ".";
    }

    void Application::RenderOverviewTab() {
        ImGui::Text("Renderer: %s", m_imguiInitialized ? "DX11 ready" : "waiting");
        ImGui::Text(
            "PE header monitor: %s",
            m_headerHealthy.load(std::memory_order_relaxed) ? "healthy" : "unavailable");
        ImGui::Text("Loaded modules: %zu", m_modules.size());
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Separator();
        ImGui::Checkbox("Show ImGui demo", &m_config.showDemo);
        ImGui::Checkbox("Draw cursor circle", &m_config.drawCursorCircle);
        ImGui::SliderFloat("Circle radius", &m_config.circleRadius, 8.0f, 200.0f);
        ImGui::Spacing();
        ImGui::TextUnformatted("INSERT toggles the menu. END performs a clean shutdown.");
    }

    void Application::RenderModulesTab() {
        if (ImGui::Button("Refresh modules")) {
            RefreshModules();
        }
        ImGui::SameLine();
        ImGui::Text("%zu module(s)", m_modules.size());

        if (ImGui::BeginTable(
                "modules",
                3,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY,
                ImVec2(0, 180))) {
            ImGui::TableSetupColumn("Module");
            ImGui::TableSetupColumn("Base");
            ImGui::TableSetupColumn("Size");
            ImGui::TableHeadersRow();

            for (size_t index = 0; index < m_modules.size(); ++index) {
                const auto& module = m_modules[index];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable(
                        module.name.c_str(),
                        index == m_selectedModule,
                        ImGuiSelectableFlags_SpanAllColumns)) {
                    SelectModule(index);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", module.path.c_str());
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%p", reinterpret_cast<void*>(module.base));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%zu KB", module.size / 1024);
            }
            ImGui::EndTable();
        }

        ImGui::SeparatorText("PE sections");
        if (ImGui::BeginTable(
                "sections",
                4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_ScrollY,
                ImVec2(0, 170))) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Size");
            ImGui::TableSetupColumn("Access");
            ImGui::TableHeadersRow();
            for (const auto& section : m_sections) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(section.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%p", reinterpret_cast<void*>(section.address));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%zu", section.size);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(SectionFlags(section.characteristics).c_str());
            }
            ImGui::EndTable();
        }
    }

    void Application::RenderScannerTab() {
        if (!m_modules.empty()) {
            ImGui::Text("Target: %s", m_modules[m_selectedModule].name.c_str());
        }
        ImGui::InputText("Signature", m_pattern.data(), m_pattern.size());

        const bool scanning = m_scanning.load(std::memory_order_acquire);
        if (scanning) ImGui::BeginDisabled();
        if (ImGui::Button("Scan readable PE sections") && !scanning) {
            BeginPatternScan();
        }
        if (scanning) ImGui::EndDisabled();

        {
            std::lock_guard lock(m_scanMutex);
            ImGui::SameLine();
            ImGui::TextUnformatted(m_scanStatus.c_str());
            ImGui::Separator();

            for (size_t recordIndex = 0;
                 recordIndex < m_scanHistory.size();
                 ++recordIndex) {
                const auto& record = m_scanHistory[recordIndex];
                const std::string label =
                    record.module + " | " + record.pattern + "##" +
                    std::to_string(recordIndex);
                if (ImGui::TreeNode(
                        label.c_str(), "%s (%zu)", record.module.c_str(),
                        record.addresses.size())) {
                    ImGui::TextWrapped("%s", record.pattern.c_str());
                    for (const uintptr_t address : record.addresses) {
                        ImGui::BulletText("%p", reinterpret_cast<void*>(address));
                    }
                    ImGui::TreePop();
                }
            }
        }
    }

    void Application::RenderHexViewerTab() {
        ImGui::InputText("Address", m_hexAddress.data(), m_hexAddress.size());
        ImGui::InputInt("Length", &m_hexLength, 16, 64);
        if (ImGui::Button("Read safe range")) {
            RefreshHexView();
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(m_hexStatus.c_str());

        uintptr_t base = 0;
        std::string_view text{m_hexAddress.data()};
        if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
        std::from_chars(text.data(), text.data() + text.size(), base, 16);

        ImGui::Separator();
        ImGui::BeginChild("hex-data", ImVec2(0, 330), true);
        for (size_t offset = 0; offset < m_hexBytes.size(); offset += 16) {
            std::ostringstream hex;
            std::string ascii;
            for (size_t column = 0; column < 16; ++column) {
                if (offset + column < m_hexBytes.size()) {
                    const unsigned int value =
                        std::to_integer<unsigned int>(m_hexBytes[offset + column]);
                    hex << std::hex << std::uppercase << std::setfill('0')
                        << std::setw(2) << value << ' ';
                    ascii.push_back(
                        value >= 32 && value <= 126 ? static_cast<char>(value) : '.');
                } else {
                    hex << "   ";
                    ascii.push_back(' ');
                }
            }
            ImGui::Text(
                "%p  %s |%s|",
                reinterpret_cast<void*>(base + offset),
                hex.str().c_str(),
                ascii.c_str());
        }
        ImGui::EndChild();
    }

    void Application::RenderLogTab() {
        if (ImGui::Button("Clear")) {
            m_logger.Clear();
        }
        ImGui::Separator();
        ImGui::BeginChild("logs", ImVec2(0, 370), true);
        const auto entries = m_logger.Snapshot();
        for (const auto& entry : entries) {
            const std::time_t rawTime =
                std::chrono::system_clock::to_time_t(entry.timestamp);
            std::tm local{};
            localtime_s(&local, &rawTime);
            char timeText[16]{};
            std::strftime(timeText, sizeof(timeText), "%H:%M:%S", &local);
            ImGui::TextColored(
                LevelColor(entry.level),
                "[%s] [%s] %s",
                timeText,
                LevelName(entry.level),
                entry.message.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    void Application::Render() {
        bool open = m_menuOpen.load(std::memory_order_relaxed);
        if (open) {
            ImGui::SetNextWindowSize(ImVec2(760, 520), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("DX11 Overlay Inspector", &open)) {
                if (ImGui::BeginTabBar("main-tabs")) {
                    if (ImGui::BeginTabItem("Overview")) {
                        RenderOverviewTab();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Modules")) {
                        RenderModulesTab();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Scanner")) {
                        RenderScannerTab();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Hex Viewer")) {
                        RenderHexViewerTab();
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Log")) {
                        RenderLogTab();
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::End();
            m_menuOpen.store(open, std::memory_order_relaxed);
        }

        if (m_config.showDemo) {
            ImGui::ShowDemoWindow(&m_config.showDemo);
        }

        if (m_config.drawCursorCircle && m_window) {
            POINT cursor{};
            if (GetCursorPos(&cursor) && ScreenToClient(m_window, &cursor)) {
                ImGui::GetBackgroundDrawList()->AddCircle(
                    ImVec2(
                        static_cast<float>(cursor.x),
                        static_cast<float>(cursor.y)),
                    m_config.circleRadius,
                    IM_COL32(80, 230, 120, 180),
                    64,
                    2.0f);
            }
        }
    }

    void Application::InspectorWorker(std::stop_token token) {
        while (!token.stop_requested()) {
            const uintptr_t base =
                reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
            const auto dos = memory::Read<IMAGE_DOS_HEADER>(base);
            m_headerHealthy.store(
                dos && dos->e_magic == IMAGE_DOS_SIGNATURE,
                std::memory_order_relaxed);

            for (int tick = 0; tick < 10 && !token.stop_requested(); ++tick) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    void Application::Shutdown() noexcept {
        if (m_shuttingDown.exchange(true, std::memory_order_acq_rel)) {
            return;
        }
        m_running.store(false, std::memory_order_release);

        if (m_inspectorThread.joinable()) {
            m_inspectorThread.request_stop();
            m_inspectorThread.join();
        }
        if (m_scanThread.joinable()) {
            m_scanThread.request_stop();
            m_scanThread.join();
        }

        if (m_presentTarget) MH_DisableHook(m_presentTarget);
        if (m_resizeTarget) MH_DisableHook(m_resizeTarget);

        {
            std::lock_guard renderLock(m_renderMutex);
            if (m_originalWndProc && m_window && IsWindow(m_window)) {
                SetWindowLongPtr(
                    m_window,
                    GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(
                        FunctionAddress(m_originalWndProc)));
            }
            m_originalWndProc = nullptr;

            if (m_dx11Initialized) {
                ImGui_ImplDX11_Shutdown();
                m_dx11Initialized = false;
            }
            if (m_win32Initialized) {
                ImGui_ImplWin32_Shutdown();
                m_win32Initialized = false;
            }
            if (m_imguiInitialized) {
                ImGui::DestroyContext();
                m_imguiInitialized = false;
            }

            m_renderTarget.Reset();
            m_context.Reset();
            m_device.Reset();
            m_activeSwapChain = nullptr;
            m_window = nullptr;
        }

        if (m_presentTarget) MH_RemoveHook(m_presentTarget);
        if (m_resizeTarget) MH_RemoveHook(m_resizeTarget);
        if (m_ownsMinHook) MH_Uninitialize();

        s_originalPresent = nullptr;
        s_originalResize = nullptr;
        s_instance.store(nullptr, std::memory_order_release);
    }
}
