#include "core/SystemSampler.h"

namespace vg {

SystemSampler::SystemSampler(HardwareProfile profile)
    : profile_(std::move(profile)) {}

const HardwareProfile& SystemSampler::profile() const noexcept {
    return profile_;
}

SystemSnapshot SystemSampler::sample(bool includeProcesses) {
    const auto now = std::chrono::steady_clock::now();
    currentIntervalSeconds_ = std::max(0.05,
        std::chrono::duration<double>(now - previousSampleTime_).count());
    previousSampleTime_ = now;

    SystemSnapshot snapshot;
    samplePlatformIdentity(snapshot);
    sampleCpu(snapshot);
    sampleMemory(snapshot);
    sampleGpu(snapshot);
    sampleDisk(snapshot);
    sampleNetwork(snapshot);
    sampleBattery(snapshot);
    if (includeProcesses) {
        sampleProcesses(snapshot);
    }
    return snapshot;
}

void SystemSampler::measureProcessCpu(
    ProcessInfo& process, std::uint64_t cpuTimeNs, double elapsed, int logicalCores) const {
    const auto prior = previousProcessTicks_.find(process.pid);
    if (elapsed > 0 && process.startTicks != 0 && prior != previousProcessTicks_.end() &&
        prior->second.first == process.startTicks && prior->second.second <= cpuTimeNs) {
        process.cpuPercent = std::min(100.0 * std::max(1, logicalCores),
            static_cast<double>(cpuTimeNs - prior->second.second) / (elapsed * 1e7));
    }
}

} // namespace vg
