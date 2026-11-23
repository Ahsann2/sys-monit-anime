#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vg {

enum class Severity {
    Normal = 0,
    Info = 1,
    Warning = 2,
    Critical = 3,
};

struct PressureStats {
    bool available{false};
    double someAvg10{0.0};
    double someAvg60{0.0};
    double someAvg300{0.0};
    double fullAvg10{0.0};
    double fullAvg60{0.0};
    double fullAvg300{0.0};
};

struct CpuStats {
    bool usageAvailable{false};
    double usagePercent{0.0};
    std::vector<double> perCorePercent;
    double load1{0.0};
    double load5{0.0};
    double load15{0.0};
    double averageClockGhz{0.0};
    double temperatureC{0.0};
    std::vector<double> coreTemperaturesC;
    std::optional<std::uint64_t> packageThrottleCount;
    int logicalCores{0};
    PressureStats pressure;
};

enum class FanMode { Unknown, Automatic, Maximum };

struct FanStats {
    std::optional<int> cpuRpm;
    std::optional<int> gpuRpm;
    FanMode cpuMode{FanMode::Unknown};
    std::string controlPath;
};

struct MemoryStats {
    bool swapAvailable{false};
    std::uint64_t totalBytes{0};
    std::uint64_t availableBytes{0};
    std::uint64_t usedBytes{0};
    std::uint64_t cachedBytes{0};
    std::uint64_t buffersBytes{0};
    std::uint64_t swapTotalBytes{0};
    std::uint64_t swapUsedBytes{0};
    PressureStats pressure;

    [[nodiscard]] double usedPercent() const {
        return totalBytes == 0
            ? 0.0
            : static_cast<double>(usedBytes) * 100.0 /
                  static_cast<double>(totalBytes);
    }
};

struct GpuStats {
    bool available{false};
    bool sleeping{false};
    bool discrete{false};
    std::string name{"Not detected"};
    double usagePercent{0.0};
    double memoryUsagePercent{0.0};
    std::uint64_t memoryUsedBytes{0};
    std::uint64_t memoryTotalBytes{0};
    double temperatureC{0.0};
    bool utilizationAvailable{false};
    double powerWatts{-1.0};
    int fanPercent{-1};
    int graphicsClockMhz{-1};
};

struct DiskStats {
    bool ratesAvailable{false};
    std::uint64_t totalBytes{0};
    std::uint64_t freeBytes{0};
    std::uint64_t usedBytes{0};
    double readBytesPerSecond{0.0};
    double writeBytesPerSecond{0.0};
    double temperatureC{0.0};
    PressureStats pressure;

    [[nodiscard]] double usedPercent() const {
        return totalBytes == 0
            ? 0.0
            : static_cast<double>(usedBytes) * 100.0 /
                  static_cast<double>(totalBytes);
    }
};

struct NetworkStats {
    bool available{false};
    std::uint64_t receivedBytes{0};
    std::uint64_t transmittedBytes{0};
    double receiveBytesPerSecond{0.0};
    double transmitBytesPerSecond{0.0};
};

struct BatteryStats {
    std::optional<bool> acOnline;
    bool present{false};
    bool charging{false};
    double percent{-1.0};
    double healthPercent{-1.0};
    double powerWatts{-1.0};
    double voltageVolts{-1.0};
    double temperatureC{0.0};
    int cycleCount{-1};
    std::int64_t estimatedSeconds{-1};
    std::string status{"Unknown"};
};

struct ProcessInfo {
    int pid{0};
    int parentPid{0};
    unsigned int uid{0};
    std::uint64_t startTicks{0};
    std::string name;
    std::string command;
    std::string cgroup;
    std::string project;
    double cpuPercent{-1.0};
    bool residentAvailable{false};
    bool ioAvailable{false};
    std::uint64_t residentBytes{0};
    std::uint64_t readBytes{0};
    std::uint64_t writeBytes{0};
    int threadCount{0};
    bool protectedProcess{false};
};

struct SystemSnapshot {
    std::chrono::system_clock::time_point timestamp{
        std::chrono::system_clock::now()};
    std::chrono::seconds uptime{0};
    CpuStats cpu;
    FanStats fans;
    MemoryStats memory;
    GpuStats integratedGpu;
    GpuStats discreteGpu;
    std::vector<std::string> graphicsAdapters;
    DiskStats disk;
    NetworkStats network;
    BatteryStats battery;
    std::vector<ProcessInfo> processes;
    std::string hostname;
    std::string kernel;
    std::string systemName;
};

struct Alert {
    std::string id;
    Severity severity{Severity::Normal};
    std::string title;
    std::string detail;
    std::string recommendation;
};

struct HistoryPoint {
    std::chrono::system_clock::time_point timestamp;
    double cpu{0.0};
    double memory{0.0};
    double gpu{0.0};
    double cpuTemperature{0.0};
    double gpuTemperature{0.0};
};

inline double clampPercent(double value) {
    return std::clamp(value, 0.0, 100.0);
}

} // namespace vg
