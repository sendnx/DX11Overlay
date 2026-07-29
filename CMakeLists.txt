cmake_minimum_required(VERSION 3.24)

project(DX11Overlay VERSION 0.1.0 LANGUAGES CXX C)

if(NOT WIN32)
    message(FATAL_ERROR "DX11Overlay requires Windows and the DirectX 11 SDK.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "DX11Overlay currently supports only Windows x64.")
endif()

option(DX11_OVERLAY_BUILD_TESTS "Build the small self-process tests" ON)
option(DX11_OVERLAY_FETCH_DEPS "Download Dear ImGui and MinHook" ON)
option(DX11_OVERLAY_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
include(GNUInstallDirs)

if(DX11_OVERLAY_FETCH_DEPS)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b
        GIT_SHALLOW TRUE
    )
    FetchContent_Declare(
        minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG v1.3.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui minhook)

    add_library(imgui_dx11 STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    )
    target_include_directories(imgui_dx11 PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(
        imgui_dx11
        PUBLIC d3d11 dxgi d3dcompiler dwmapi imm32
    )

    install(
        FILES ${imgui_SOURCE_DIR}/LICENSE.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/DX11Overlay
        RENAME DearImGui-LICENSE.txt
    )
    install(
        FILES ${minhook_SOURCE_DIR}/LICENSE.txt
        DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/DX11Overlay
        RENAME MinHook-LICENSE.txt
    )
else()
    if(NOT TARGET imgui_dx11 OR NOT TARGET minhook)
        message(FATAL_ERROR
            "DX11_OVERLAY_FETCH_DEPS=OFF requires existing imgui_dx11 and "
            "minhook CMake targets.")
    endif()
endif()

add_library(dx11_overlay SHARED
    src/Application.cpp
    src/DllMain.cpp
    src/Logger.cpp
    src/Memory.cpp
    src/PatternScanner.cpp
)

target_include_directories(dx11_overlay PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_definitions(dx11_overlay PRIVATE
    DX11_OVERLAY_EXPORTS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
target_link_libraries(dx11_overlay PRIVATE
    imgui_dx11
    minhook
    d3d11
    dxgi
    psapi
)
set_target_properties(dx11_overlay PROPERTIES
    OUTPUT_NAME "DX11Overlay"
    PREFIX ""
)

install(TARGETS dx11_overlay
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(
    FILES include/overlay/OverlayApi.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/overlay
)
install(
    FILES LICENSE THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/DX11Overlay
)

if(MSVC)
    target_compile_options(dx11_overlay PRIVATE /W4 /permissive- /EHsc /utf-8)
    if(DX11_OVERLAY_WARNINGS_AS_ERRORS)
        target_compile_options(dx11_overlay PRIVATE /WX)
    endif()
endif()

if(DX11_OVERLAY_BUILD_TESTS)
    enable_testing()
    add_executable(overlay_tests
        tests/OverlayTests.cpp
        src/Memory.cpp
        src/PatternScanner.cpp
    )
    target_include_directories(overlay_tests PRIVATE include)
    target_compile_definitions(overlay_tests PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(overlay_tests PRIVATE psapi)
    if(MSVC)
        target_compile_options(overlay_tests PRIVATE /W4 /permissive- /EHsc /utf-8)
        if(DX11_OVERLAY_WARNINGS_AS_ERRORS)
            target_compile_options(overlay_tests PRIVATE /WX)
        endif()
    endif()
    add_test(NAME overlay_tests COMMAND overlay_tests)

    add_executable(overlay_lifecycle_host tests/LifecycleHost.cpp)
    target_compile_definitions(overlay_lifecycle_host PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    add_dependencies(overlay_lifecycle_host dx11_overlay)
    if(MSVC)
        target_compile_options(
            overlay_lifecycle_host PRIVATE /W4 /permissive- /EHsc /utf-8)
        if(DX11_OVERLAY_WARNINGS_AS_ERRORS)
            target_compile_options(overlay_lifecycle_host PRIVATE /WX)
        endif()
    endif()
    add_test(
        NAME overlay_lifecycle
        COMMAND overlay_lifecycle_host $<TARGET_FILE:dx11_overlay>
    )

    add_executable(overlay_dx11_smoke_host tests/Dx11SmokeHost.cpp)
    target_compile_definitions(overlay_dx11_smoke_host PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(overlay_dx11_smoke_host PRIVATE d3d11 dxgi)
    add_dependencies(overlay_dx11_smoke_host dx11_overlay)
    if(MSVC)
        target_compile_options(
            overlay_dx11_smoke_host PRIVATE /W4 /permissive- /EHsc /utf-8)
        if(DX11_OVERLAY_WARNINGS_AS_ERRORS)
            target_compile_options(overlay_dx11_smoke_host PRIVATE /WX)
        endif()
    endif()
    add_test(
        NAME overlay_dx11_smoke
        COMMAND overlay_dx11_smoke_host $<TARGET_FILE:dx11_overlay>
    )
endif()
