#include "overlay/Memory.hpp"

#include <windows.h>

#include <cstring>
#include <limits>

namespace overlay::memory {
    namespace {
        bool IsReadableProtection(DWORD protection) noexcept {
            if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
                return false;
            }
            switch (protection & 0xff) {
            case PAGE_READONLY:
            case PAGE_READWRITE:
            case PAGE_WRITECOPY:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        bool CopyWithSeh(void* destination, const void* source, size_t size) noexcept {
#if defined(_MSC_VER)
            __try {
                std::memcpy(destination, source, size);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
#else
            std::memcpy(destination, source, size);
            return true;
#endif
        }
    }

    std::optional<Region> QueryRegion(uintptr_t address) noexcept {
        if (address == 0) {
            return std::nullopt;
        }

        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(
                reinterpret_cast<const void*>(address),
                &information,
                sizeof(information)) == 0) {
            return std::nullopt;
        }

        return Region{
            reinterpret_cast<uintptr_t>(information.BaseAddress),
            information.RegionSize,
            information.Protect,
            information.State == MEM_COMMIT,
            information.State == MEM_COMMIT &&
                IsReadableProtection(information.Protect)
        };
    }

    bool IsRangeReadable(uintptr_t address, size_t size) noexcept {
        if (address == 0 || size == 0 ||
            address > std::numeric_limits<uintptr_t>::max() - size) {
            return false;
        }

        const uintptr_t end = address + size;
        uintptr_t cursor = address;
        while (cursor < end) {
            const auto region = QueryRegion(cursor);
            if (!region || !region->readable ||
                region->base > std::numeric_limits<uintptr_t>::max() - region->size) {
                return false;
            }

            const uintptr_t regionEnd = region->base + region->size;
            if (regionEnd <= cursor) {
                return false;
            }
            cursor = regionEnd;
        }
        return true;
    }

    bool CopyReadable(void* destination, uintptr_t source, size_t size) noexcept {
        return destination && IsRangeReadable(source, size) &&
            CopyWithSeh(destination, reinterpret_cast<const void*>(source), size);
    }

    std::optional<std::vector<std::byte>> ReadBytes(
        uintptr_t address,
        size_t size) {
        if (!IsRangeReadable(address, size)) {
            return std::nullopt;
        }
        std::vector<std::byte> bytes(size);
        if (!CopyWithSeh(bytes.data(), reinterpret_cast<const void*>(address), size)) {
            return std::nullopt;
        }
        return bytes;
    }

    std::optional<std::string> ReadString(uintptr_t address, size_t maxLength) {
        std::string result;
        result.reserve(maxLength);
        for (size_t index = 0; index < maxLength; ++index) {
            if (address > std::numeric_limits<uintptr_t>::max() - index) {
                return std::nullopt;
            }
            const auto character = Read<char>(address + index);
            if (!character) {
                return std::nullopt;
            }
            if (*character == '\0') {
                break;
            }
            result.push_back(*character);
        }
        return result;
    }

    std::optional<uintptr_t> ResolvePointerChain(
        uintptr_t base,
        const std::vector<uintptr_t>& offsets) noexcept {
        if (base == 0 || offsets.empty()) {
            return std::nullopt;
        }

        uintptr_t current = base;
        for (size_t index = 0; index < offsets.size(); ++index) {
            if (current > std::numeric_limits<uintptr_t>::max() - offsets[index]) {
                return std::nullopt;
            }
            current += offsets[index];
            if (index + 1 == offsets.size()) {
                return IsRangeReadable(current, 1)
                    ? std::optional<uintptr_t>{current}
                    : std::nullopt;
            }
            const auto next = Read<uintptr_t>(current);
            if (!next || *next == 0) {
                return std::nullopt;
            }
            current = *next;
        }
        return std::nullopt;
    }
}
