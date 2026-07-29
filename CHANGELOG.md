# Changelog

All notable changes will be documented in this file.

The project follows semantic versioning after the first stable release.

## [Unreleased]

- Added explicit `StartOverlay`, `RequestOverlayShutdown`,
  `WaitForOverlayShutdown`, and `IsOverlayRunning` exports.
- Removed worker creation from `DllMain`.
- Added retained OS-thread-handle shutdown synchronization.
- Added Debug and Release Windows CI with warnings as errors.
- Added real PE-section pattern matching, wildcard, cancellation, and
  inaccessible-page tests.
- Added a real DX11 smoke host covering Present, renderer initialization,
  ResizeBuffers, WndProc restoration, and clean shutdown.
- Linked the Dear ImGui DX11 backend's D3D compiler dependency explicitly.
- Added install staging and external dependency target support.
- Added bilingual case-study documentation and contributor/security guidance.
- Adopted the MIT License.

## [0.1.0]

- Initial educational DX11 overlay inspector case study.
