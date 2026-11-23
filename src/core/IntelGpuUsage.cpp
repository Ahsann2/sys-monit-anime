#include "core/IntelGpuUsage.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace fs = std::filesystem;

namespace vg {
namespace {

constexpr std::string_view enginePrefix = "drm-engine-";
constexpr std::string_view capacityPrefix = "drm-engine-capacity-";

std::optional<std::uint64_t> parseCounter(std::string_view text) {
    std::uint64_t value = 0;
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return std::nullopt;
    }
    return value;
}

} // namespace

IntelGpuUsage::IntelGpuUsage(fs::path procRoot)
    : procRoot_(std::move(procRoot)) {}

void IntelGpuUsage::discoverDescriptors() {
    descriptors_.clear();
    const auto options = fs::directory_options::skip_permission_denied;
    const fs::directory_iterator end;
    std::error_code error;
    std::unordered_set<fs::path> nodes;
    for (fs::directory_iterator node(device_ / "drm", options, error);
         node != end; node.increment(error)) {
        nodes.insert(fs::path("/dev/dri") / node->path().filename());
    }
    if (nodes.empty()) {
        return;
    }
    for (fs::directory_iterator process(procRoot_, options, error);
         process != end; process.increment(error)) {
        const auto name = process->path().filename().string();
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) {
            continue;
        }
        std::error_code descriptorError;
        for (fs::directory_iterator fd(process->path() / "fd", options, descriptorError);
             fd != end; fd.increment(descriptorError)) {
            std::error_code linkError;
            if (nodes.contains(fs::read_symlink(fd->path(), linkError))) {
                descriptors_.push_back(process->path() / "fdinfo" / fd->path().filename());
            }
        }
    }
}

IntelGpuUsage::Clients IntelGpuUsage::readClients() const {
    Clients clients;
    const auto pciAddress = device_.filename().string();
    for (const auto& descriptor : descriptors_) {
        std::ifstream stream(descriptor);
        std::string line;
        bool intel = false;
        bool matchingDevice = false;
        std::optional<std::uint64_t> clientId;
        Engines engines;
        std::unordered_map<std::string, std::uint64_t> capacities;
        while (std::getline(stream, line)) {
            const auto separator = line.find(':');
            if (separator == std::string::npos) {
                continue;
            }
            const auto key = std::string_view(line).substr(0, separator);
            auto value = std::string_view(line).substr(separator + 1);
            const auto start = value.find_first_not_of(" \t");
            if (start == std::string_view::npos) {
                continue;
            }
            value.remove_prefix(start);
            if (key == "drm-driver") {
                intel = value == "i915";
            } else if (key == "drm-pdev") {
                matchingDevice = value == pciAddress;
            } else if (key == "drm-client-id") {
                clientId = parseCounter(value);
            } else if (key.starts_with(capacityPrefix)) {
                if (const auto capacity = parseCounter(value); capacity && *capacity > 0) {
                    capacities.emplace(key.substr(capacityPrefix.size()), *capacity);
                }
            } else if (key.starts_with(enginePrefix) && value.ends_with(" ns")) {
                if (const auto time = parseCounter(value.substr(0, value.size() - 3))) {
                    engines.emplace(key.substr(enginePrefix.size()), EngineCounter{*time});
                }
            }
        }
        if (!intel || !matchingDevice || !clientId || engines.empty()) {
            continue;
        }
        // Duplicated file descriptors can report the same client at different times.
        auto& client = clients[*clientId];
        for (const auto& [engine, counter] : engines) {
            auto& saved = client[engine];
            saved.busyNs = std::max(saved.busyNs, counter.busyNs);
            if (const auto capacity = capacities.find(engine); capacity != capacities.end()) {
                saved.capacity = std::max(saved.capacity, capacity->second);
            }
        }
    }
    return clients;
}

std::optional<double> IntelGpuUsage::sample(const fs::path& device, Clock::time_point now) {
    if (device.empty()) {
        return std::nullopt;
    }
    if (device != device_) {
        device_ = device;
        previousClients_.clear();
        nextDiscovery_ = {};
    }
    if (now >= nextDiscovery_) {
        discoverDescriptors();
        nextDiscovery_ = now + std::chrono::seconds{5};
    }

    auto clients = readClients();
    const double elapsedNs = std::chrono::duration<double, std::nano>(now - previousSample_).count();
    std::unordered_map<std::string, double> engineUsage;
    if (elapsedNs > 0.0) {
        for (auto& [id, engines] : clients) {
            const auto previous = previousClients_.find(id);
            if (previous == previousClients_.end()) {
                continue;
            }
            for (auto& [engine, counter] : engines) {
                const auto baseline = previous->second.find(engine);
                if (baseline == previous->second.end()) {
                    continue;
                }
                // The DRM ABI allows temporary drops while accounting catches up.
                counter.busyNs = std::max(counter.busyNs, baseline->second.busyNs);
                engineUsage[engine] +=
                    static_cast<double>(counter.busyNs - baseline->second.busyNs)
                    / (elapsedNs * static_cast<double>(counter.capacity));
            }
        }
    }
    previousClients_ = std::move(clients);
    previousSample_ = now;
    if (engineUsage.empty()) {
        return std::nullopt;
    }

    double busiest = 0.0;
    for (const auto& [engine, load] : engineUsage) {
        busiest = std::max(busiest, load);
    }
    return std::min(busiest * 100.0, 100.0);
}

} // namespace vg
