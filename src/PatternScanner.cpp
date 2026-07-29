#include "overlay/PatternScanner.hpp"

#include "overlay/Memory.hpp"

#include <psapi.h>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <limits>
#include <sstream>
#include <system_error>
#include <utility>

namespace overlay {
    PatternScanner::PatternScanner(HMODULE module) noexcept {
        if (!module) {
            module = GetModuleHandleA(nullptr);
        }

        MODULEINFO information{};
        if (!module || !GetModuleInformation(
                GetCurrentProcess(), module, &information, sizeof(information))) {
            return;
        }

        m_base = reinterpret_cast<uintptr_t>(information.lpBaseOfDll);
        m_size = information.SizeOfImage;
        ReadSections(module);
    }

    void PatternScanner::ReadSections(HMODULE module) noexcept {
        const auto dos = memory::Read<IMAGE_DOS_HEADER>(
            reinterpret_cast<uintptr_t>(module));
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
            return;
        }

        const uintptr_t ntAddress =
            reinterpret_cast<uintptr_t>(module) +
            static_cast<uintptr_t>(dos->e_lfanew);
        const auto nt = memory::Read<IMAGE_NT_HEADERS>(ntAddress);
        if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) {
            return;
        }

        const uintptr_t firstSection =
            ntAddress + offsetof(IMAGE_NT_HEADERS, OptionalHeader) +
            nt->FileHeader.SizeOfOptionalHeader;

        for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
            const auto section = memory::Read<IMAGE_SECTION_HEADER>(
                firstSection + index * sizeof(IMAGE_SECTION_HEADER));
            if (!section) {
                break;
            }

            char name[IMAGE_SIZEOF_SHORT_NAME + 1]{};
            std::memcpy(name, section->Name, IMAGE_SIZEOF_SHORT_NAME);
            const size_t size = std::max<size_t>(
                section->Misc.VirtualSize,
                section->SizeOfRawData);
            const uintptr_t sectionAddress =
                m_base + section->VirtualAddress;
            const uintptr_t moduleEnd = m_base + m_size;
            if (sectionAddress >= moduleEnd) {
                continue;
            }
            m_sections.push_back({
                name,
                sectionAddress,
                std::min<size_t>(size, moduleEnd - sectionAddress),
                section->Characteristics
            });
        }
    }

    std::optional<PatternScanner::Pattern> PatternScanner::ParsePattern(
        std::string_view text) {
        Pattern result;
        std::istringstream stream{std::string(text)};
        std::string token;

        while (stream >> token) {
            if (token == "?" || token == "??") {
                result.emplace_back(std::nullopt);
                continue;
            }
            if (token.empty() || token.size() > 2) {
                return std::nullopt;
            }

            unsigned int value = 0;
            const auto parsed = std::from_chars(
                token.data(), token.data() + token.size(), value, 16);
            if (parsed.ec != std::errc{} ||
                parsed.ptr != token.data() + token.size() ||
                value > 0xff) {
                return std::nullopt;
            }
            result.emplace_back(static_cast<std::uint8_t>(value));
        }

        return result.empty()
            ? std::nullopt
            : std::optional<Pattern>{std::move(result)};
    }

    std::vector<uintptr_t> PatternScanner::FindAll(
        std::string_view patternText,
        size_t maxResults,
        std::stop_token stopToken) const {
        std::vector<uintptr_t> results;
        const auto pattern = ParsePattern(patternText);
        if (!pattern || !IsValid() || maxResults == 0) {
            return results;
        }

        std::vector<ModuleSection> ranges = m_sections;
        if (ranges.empty()) {
            ranges.push_back({"image", m_base, m_size, IMAGE_SCN_MEM_READ});
        }

        for (const auto& range : ranges) {
            if (stopToken.stop_requested() || results.size() >= maxResults ||
                range.size < pattern->size() ||
                (range.characteristics & IMAGE_SCN_MEM_DISCARDABLE) != 0) {
                continue;
            }

            uintptr_t cursor = range.address;
            const uintptr_t rangeEnd =
                range.address <= std::numeric_limits<uintptr_t>::max() - range.size
                    ? range.address + range.size
                    : std::numeric_limits<uintptr_t>::max();

            while (cursor < rangeEnd && results.size() < maxResults) {
                const auto region = memory::QueryRegion(cursor);
                if (!region) {
                    break;
                }

                const uintptr_t regionEnd =
                    region->base <= std::numeric_limits<uintptr_t>::max() - region->size
                        ? std::min(region->base + region->size, rangeEnd)
                        : rangeEnd;
                const uintptr_t scanStart = std::max(cursor, region->base);

                if (region->readable && regionEnd >= scanStart &&
                    regionEnd - scanStart >= pattern->size()) {
                    const auto bytes = memory::ReadBytes(
                        scanStart,
                        static_cast<size_t>(regionEnd - scanStart));
                    if (!bytes) {
                        cursor = regionEnd;
                        continue;
                    }

                    const size_t last = bytes->size() - pattern->size();
                    for (size_t offset = 0;
                         offset <= last && results.size() < maxResults;
                         ++offset) {
                        if ((offset & 0x3fff) == 0 && stopToken.stop_requested()) {
                            return results;
                        }

                        bool matches = true;
                        for (size_t index = 0; index < pattern->size(); ++index) {
                            if ((*pattern)[index] &&
                                std::to_integer<std::uint8_t>(
                                    (*bytes)[offset + index]) != *(*pattern)[index]) {
                                matches = false;
                                break;
                            }
                        }
                        if (matches) {
                            results.push_back(scanStart + offset);
                        }
                    }
                }

                if (regionEnd <= cursor) {
                    break;
                }
                cursor = regionEnd;
            }
        }
        return results;
    }
}
