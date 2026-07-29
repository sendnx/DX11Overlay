# Contributing

Contributions should preserve the project's educational and self-process-only
scope.

Before opening a pull request:

1. Keep memory access read-only and limited to the current process.
2. Keep ImGui and DX11 calls on the render path.
3. Document ownership and synchronization for new shared state.
4. Add or update tests for parser, memory, and lifecycle contracts.
5. Run the Debug and Release commands in [`BUILDING.md`](BUILDING.md).
6. Update both language sections when changing a concept described in the
   bilingual README.

Pull requests that add process injection, anti-cheat bypasses, protection
evasion, or third-party memory modification are out of scope.
