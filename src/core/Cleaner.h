#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace vg {

struct CleanupTarget {
    std::string id;
    std::string title;
    std::string description;
    std::filesystem::path path;
    std::uint64_t sizeBytes{0};
    bool selectedByDefault{true};
    bool browserRelated{false};
    bool trash{false};
    int olderThanDays{0};
};

struct CleanupResult {
    std::uint64_t bytesFreed{0};
    std::size_t filesRemoved{0};
    std::vector<std::string> errors;
};

class Cleaner {
public:
    Cleaner();

    [[nodiscard]] std::vector<CleanupTarget> scan() const;
    [[nodiscard]] CleanupResult clean(
        const std::vector<std::string>& targetIds) const;

private:
    [[nodiscard]] std::vector<CleanupTarget> targets() const;

    std::filesystem::path home_;
};

} // namespace vg
