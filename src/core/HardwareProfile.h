#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vg {

struct HardwareProfile {
    std::string displayName;
    std::string expectedCpu;
    std::string expectedDiscreteGpu;
    std::string expectedIntegratedGpu;
    std::uint64_t expectedRamBytes{0};

    double cpuWarningC{88.0};
    double cpuCriticalC{96.0};
    double gpuWarningC{80.0};
    double gpuCriticalC{86.0};
    double memoryWarningPercent{85.0};
    double memoryCriticalPercent{93.0};
    double diskWarningPercent{90.0};
    double diskCriticalPercent{96.0};

    bool batteryAwareDiscreteGpu{true};
    bool gl752vwFanControl{false};
    std::vector<std::string> preferredCpuSensors;
    std::vector<std::string> protectedProcessNames;

    [[nodiscard]] static HardwareProfile vivoBookPro15Oled();
    [[nodiscard]] static HardwareProfile forDevice(
        const std::string& vendor, const std::string& product);
    [[nodiscard]] static HardwareProfile detect();
};

} // namespace vg
