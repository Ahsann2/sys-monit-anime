#pragma once

#include "core/Types.h"

#include <chrono>
#include <string>

namespace vg {

class NvmlMonitor {
public:
    NvmlMonitor() = default;
    ~NvmlMonitor();

    NvmlMonitor(const NvmlMonitor&) = delete;
    NvmlMonitor& operator=(const NvmlMonitor&) = delete;

    [[nodiscard]] GpuStats sample(bool batteryAware);
    [[nodiscard]] bool libraryAvailable() const;

private:
    bool initialize();
    void shutdown();
    [[nodiscard]] static std::optional<bool> nvidiaDeviceSuspended();

    void* library_{nullptr};
    void* device_{nullptr};
    bool initialized_{false};
    std::chrono::steady_clock::time_point nextInitialization_{};

    using Return = int;
    using InitFn = Return (*)();
    using ShutdownFn = Return (*)();
    using GetCountFn = Return (*)(unsigned int*);
    using GetHandleFn = Return (*)(unsigned int, void**);
    using GetNameFn = Return (*)(void*, char*, unsigned int);
    using GetUtilizationFn = Return (*)(void*, void*);
    using GetMemoryFn = Return (*)(void*, void*);
    using GetTemperatureFn = Return (*)(void*, unsigned int, unsigned int*);
    using GetFanSpeedFn = Return (*)(void*, unsigned int*);
    using GetPowerUsageFn = Return (*)(void*, unsigned int*);
    using GetClockInfoFn = Return (*)(void*, unsigned int, unsigned int*);

    InitFn initFn_{nullptr};
    ShutdownFn shutdownFn_{nullptr};
    GetCountFn getCountFn_{nullptr};
    GetHandleFn getHandleFn_{nullptr};
    GetNameFn getNameFn_{nullptr};
    GetUtilizationFn getUtilizationFn_{nullptr};
    GetMemoryFn getMemoryFn_{nullptr};
    GetTemperatureFn getTemperatureFn_{nullptr};
    GetFanSpeedFn getFanSpeedFn_{nullptr};
    GetPowerUsageFn getPowerUsageFn_{nullptr};
    GetClockInfoFn getClockInfoFn_{nullptr};
};

} // namespace vg
