# Third-party notices

DX11 Overlay Inspector uses the following pinned dependencies when
`DX11_OVERLAY_FETCH_DEPS` is enabled.

## Dear ImGui

- Version: `v1.91.9b`
- Copyright: 2014-2025 Omar Cornut
- License: MIT
- Source: <https://github.com/ocornut/imgui/tree/v1.91.9b>
- License text: <https://github.com/ocornut/imgui/blob/v1.91.9b/LICENSE.txt>

## MinHook

- Version: `v1.3.4`
- Copyright: 2009-2017 Tsuda Kageyu
- License: 2-clause BSD
- Source: <https://github.com/TsudaKageyu/minhook/tree/v1.3.4>
- License text: <https://github.com/TsudaKageyu/minhook/blob/v1.3.4/LICENSE.txt>

MinHook contains Hacker Disassembler Engine 32/64 portions copyrighted by
Vyacheslav Patkov. Their notices and license terms are included in MinHook's
license file.

The CMake install step copies the exact upstream license files into
`share/licenses/DX11Overlay/` for binary distributions. When external
dependency targets are supplied, the parent project is responsible for
shipping the corresponding license texts.
