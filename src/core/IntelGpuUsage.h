#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace vg {

class IntelGpuUsage {
    using Clock = std::chrono::steady_clock;

public:
    explicit IntelGpuUsage(std::filesystem::path procRoot = "/proc");

    [[nodiscard]] std::optional<double> sample(const std::filesystem::path& device,
                                                Clock::time_point now = Clock::now());

private:
    struct EngineCounter {
        std::uint64_t busyNs{0};
        std::uint64_t capacity{1};
    };
    using Engines = std::unordered_map<std::string, EngineCounter>;
    using Clients = std::unordered_map<std::uint64_t, Engines>;

    void discoverDescriptors();
    [[nodiscard]] Clients readClients() const;

    std::filesystem::path procRoot_;
    std::filesystem::path device_;
    std::vector<std::filesystem::path> descriptors_;
    Clock::time_point nextDiscovery_{};
    Clock::time_point previousSample_{};
    Clients previousClients_;
};

} // namespace vg
