#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace overlay {
    struct ModuleSection {
        std::string name;
        uintptr_t address = 0;
        size_t size = 0;
        unsigned long characteristics = 0;
    };

    class PatternScanner {
    public:
        using Pattern = std::vector<std::optional<std::uint8_t>>;

        explicit PatternScanner(HMODULE module = nullptr) noexcept;

        [[nodiscard]] bool IsValid() const noexcept { return m_base != 0; }
        [[nodiscard]] uintptr_t Base() const noexcept { return m_base; }
        [[nodiscard]] size_t Size() const noexcept { return m_size; }
        [[nodiscard]] const std::vector<ModuleSection>& Sections() const noexcept {
            return m_sections;
        }

        [[nodiscard]] static std::optional<Pattern> ParsePattern(
            std::string_view text);

        [[nodiscard]] std::vector<uintptr_t> FindAll(
            std::string_view pattern,
            size_t maxResults = 64,
            std::stop_token stopToken = {}) const;

    private:
        void ReadSections(HMODULE module) noexcept;

        uintptr_t m_base = 0;
        size_t m_size = 0;
        std::vector<ModuleSection> m_sections;
    };
}
