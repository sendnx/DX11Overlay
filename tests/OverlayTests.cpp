#include "overlay/Memory.hpp"
#include "overlay/PatternScanner.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <stop_token>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma section(".ovtest", read)
__declspec(allocate(".ovtest"))
#endif
volatile const unsigned char g_patternNeedle[] = {
    0xD3, 0x7A, 0x19, 0xC4, 0x5E, 0xB2, 0x68, 0x0F,
    0x91, 0xAD, 0x43, 0xE7, 0x26, 0xBC, 0x54, 0x8D
};

namespace {
    int failures = 0;

    void Check(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }

    struct Leaf {
        int value = 42;
    };

    struct Root {
        Leaf* leaf = nullptr;
    };
}

int main() {
    const auto parsed = overlay::PatternScanner::ParsePattern(
        "48 8B ?? ? FF");
    Check(parsed.has_value(), "valid signature parses");
    Check(parsed && parsed->size() == 5, "signature has five tokens");
    Check(parsed && (*parsed)[0] == 0x48, "first byte is parsed");
    Check(parsed && !(*parsed)[2].has_value(), "double wildcard is parsed");
    Check(parsed && !(*parsed)[3].has_value(), "single wildcard is parsed");
    Check(
        !overlay::PatternScanner::ParsePattern("48 XYZ 90").has_value(),
        "invalid signature is rejected");
    Check(
        !overlay::PatternScanner::ParsePattern("").has_value(),
        "empty signature is rejected");

    const int value = 1337;
    const auto readValue = overlay::memory::Read<int>(
        reinterpret_cast<uintptr_t>(&value));
    Check(readValue && *readValue == value, "local integer can be read");
    Check(
        !overlay::memory::Read<int>(0).has_value(),
        "null address is rejected");

    void* inaccessible = VirtualAlloc(
        nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    Check(inaccessible != nullptr, "no-access test page is allocated");
    if (inaccessible) {
        Check(
            !overlay::memory::Read<int>(
                reinterpret_cast<uintptr_t>(inaccessible)).has_value(),
            "PAGE_NOACCESS memory is rejected");
        VirtualFree(inaccessible, 0, MEM_RELEASE);
    }

    const std::string text = "overlay-test";
    const auto readText = overlay::memory::ReadString(
        reinterpret_cast<uintptr_t>(text.c_str()), 64);
    Check(readText && *readText == text, "local string can be read");

    Leaf leaf{};
    Root root{&leaf};
    const auto resolved = overlay::memory::ResolvePointerChain(
        reinterpret_cast<uintptr_t>(&root),
        {
            offsetof(Root, leaf),
            offsetof(Leaf, value)
        });
    Check(
        resolved &&
            *resolved == reinterpret_cast<uintptr_t>(&leaf.value),
        "pointer chain resolves its final field");

    overlay::PatternScanner scanner(GetModuleHandleA(nullptr));
    Check(scanner.IsValid(), "main executable scanner is valid");
    Check(!scanner.Sections().empty(), "main executable exposes PE sections");

    const auto matches = scanner.FindAll(
        "D3 7A 19 C4 5E B2 68 0F 91 AD 43 E7 26 BC 54 8D");
    const uintptr_t needleAddress =
        reinterpret_cast<uintptr_t>(g_patternNeedle);
    Check(
        std::find(matches.begin(), matches.end(), needleAddress) != matches.end(),
        "scanner finds a known signature in a dedicated PE section");

    const auto wildcardMatches = scanner.FindAll(
        "D3 7A ?? C4 5E B2 68 0F 91 AD 43 E7 26 BC 54 8D");
    Check(
        std::find(
            wildcardMatches.begin(),
            wildcardMatches.end(),
            needleAddress) != wildcardMatches.end(),
        "scanner wildcard matches the known signature");

    std::stop_source stopSource;
    stopSource.request_stop();
    Check(
        scanner.FindAll(
            "D3 7A 19 C4",
            64,
            stopSource.get_token()).empty(),
        "scanner honors a pre-requested stop token");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }

    std::cout << "All overlay tests passed.\n";
    return 0;
}
