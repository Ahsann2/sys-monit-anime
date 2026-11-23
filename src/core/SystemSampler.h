#pragma once

#include "core/HardwareProfile.h"
#include "core/NvmlMonitor.h"
#include "core/Types.h"
#if defined(__linux__)
#include "core/IntelGpuUsage.h"
#endif

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vg {

class SystemSampler {
public:
    explicit SystemSampler(
        HardwareProfile profile = HardwareProfile::detect());
    ~SystemSampler();

    [[nodiscard]] SystemSnapshot sample(bool includeProcesses = true);
    [[nodiscard]] const HardwareProfile& profile() const noexcept;

private:
    using CpuTimes = std::pair<std::uint64_t, std::uint64_t>;
    using Counters = std::pair<std::uint64_t, std::uint64_t>;
    using DeviceCounters = std::unordered_map<std::string, Counters>;

    void sampleCpu(SystemSnapshot& snapshot);
    void sampleMemory(SystemSnapshot& snapshot) const;
    void sampleThermals(SystemSnapshot& snapshot) const;
    void sampleGpu(SystemSnapshot& snapshot);
    void sampleDisk(SystemSnapshot& snapshot);
    void sampleNetwork(SystemSnapshot& snapshot);
    void sampleBattery(SystemSnapshot& snapshot) const;
    void sampleProcesses(SystemSnapshot& snapshot);
    void samplePlatformIdentity(SystemSnapshot& snapshot) const;
    void measureProcessCpu(ProcessInfo& process, std::uint64_t cpuTimeNs,
                           double elapsedSeconds, int logicalCores) const;

    [[nodiscard]] static PressureStats readPressure(
        const std::filesystem::path& path);
    [[nodiscard]] static std::uint64_t readUnsigned(
        const std::filesystem::path& path,
        std::uint64_t fallback = 0);
    [[nodiscard]] static std::string readText(
        const std::filesystem::path& path);
    [[nodiscard]] static Counters counterDelta(
        const DeviceCounters& previous, const std::string& device, Counters current) {
        const auto entry = previous.find(device);
        if (entry == previous.end()) {
            return {};
        }
        const auto [first, second] = entry->second;
        return {current.first >= first ? current.first - first : 0,
                current.second >= second ? current.second - second : 0};
    }
    [[nodiscard]] static bool isProtectedProcess(
        const ProcessInfo& process,
        const HardwareProfile& profile);
    [[nodiscard]] GpuStats sampleIntegratedGpu();

    HardwareProfile profile_;
#if !defined(__APPLE__)
    NvmlMonitor nvml_;
#endif
    std::vector<CpuTimes> previousCpuTimes_;
    std::uint64_t lastSystemDeltaJiffies_{0};
    DeviceCounters previousDiskCounters_;
    DeviceCounters previousNetworkCounters_;
    std::uint64_t processDeltaJiffies_{0};
    std::unordered_map<int, std::pair<std::uint64_t, std::uint64_t>>
        previousProcessTicks_;
    std::chrono::steady_clock::time_point previousSampleTime_{
        std::chrono::steady_clock::now()};
    double currentIntervalSeconds_{1.0};
    std::vector<GpuStats> graphicsDevices_;
    std::chrono::steady_clock::time_point nextGraphicsDiscovery_{};
    std::chrono::steady_clock::time_point previousProcessSampleTime_{};
#if defined(__linux__)
    IntelGpuUsage intelGpuUsage_;
#endif
#if defined(_WIN32)
    void* pdhQuery_{nullptr};
    void* pdhCpu_{nullptr};
    void* pdhDiskRead_{nullptr};
    void* pdhDiskWrite_{nullptr};
#endif
};

} // namespace vg
