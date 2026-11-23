#include "core/SystemSampler.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    vg::SystemSampler sampler;
    (void)sampler.sample(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    const auto result = sampler.sample(true);
#if defined(_WIN32)
    constexpr auto platform = "Windows";
#else
    constexpr auto platform = "macOS";
#endif
    if (result.systemName != platform || sampler.profile().gl752vwFanControl ||
        result.cpu.logicalCores <= 0 || !result.cpu.usageAvailable ||
        result.cpu.usagePercent < 0 || result.cpu.usagePercent > 100 ||
        result.memory.totalBytes == 0 || result.memory.usedBytes > result.memory.totalBytes ||
        result.disk.totalBytes == 0 || result.disk.usedBytes > result.disk.totalBytes ||
        result.processes.empty()) {
        std::cerr << "Platform sample invalid: " << result.systemName
                  << ", CPU " << result.cpu.logicalCores << " / " << result.cpu.usageAvailable
                  << ", RAM " << result.memory.totalBytes << ", disk " << result.disk.totalBytes
                  << ", processes " << result.processes.size() << '\n';
        return 1;
    }
    for (const auto& process : result.processes) {
        if (process.pid <= 0 || !process.protectedProcess) {
            std::cerr << "Invalid or actionable process: " << process.pid << '\n';
            return 2;
        }
    }
    return 0;
}
