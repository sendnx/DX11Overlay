#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace overlay::memory {
    struct Region {
        uintptr_t base = 0;
        size_t size = 0;
        unsigned long protection = 0;
        bool committed = false;
        bool readable = false;
    };

    [[nodiscard]] std::optional<Region> QueryRegion(uintptr_t address) noexcept;
    [[nodiscard]] bool IsRangeReadable(uintptr_t address, size_t size) noexcept;
    [[nodiscard]] bool CopyReadable(
        void* destination,
        uintptr_t source,
        size_t size) noexcept;

    template <typename T>
    [[nodiscard]] std::optional<T> Read(uintptr_t address) noexcept {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Read<T> requires a trivially copyable type");
        T value{};
        if (!CopyReadable(&value, address, sizeof(value))) {
            return std::nullopt;
        }
        return value;
    }

    [[nodiscard]] std::optional<std::vector<std::byte>> ReadBytes(
        uintptr_t address,
        size_t size);
    [[nodiscard]] std::optional<std::string> ReadString(
        uintptr_t address,
        size_t maxLength = 256);

    // Adds each offset to the current address, dereferences intermediate
    // addresses, and returns the final address without dereferencing it.
    [[nodiscard]] std::optional<uintptr_t> ResolvePointerChain(
        uintptr_t base,
        const std::vector<uintptr_t>& offsets) noexcept;
}
