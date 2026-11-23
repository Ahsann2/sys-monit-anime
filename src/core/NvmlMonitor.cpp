#include "core/NvmlMonitor.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace vg {
namespace {

constexpr int kNvmlSuccess = 0;
constexpr unsigned int kTemperatureGpu = 0;
constexpr unsigned int kClockGraphics = 0;

struct NvmlUtilization {
    unsigned int gpu;
    unsigned int memory;
};

struct NvmlMemory {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};

template <typename Function>
Function symbol(void* library, const char* name) {
#if defined(_WIN32)
    const auto address = GetProcAddress(static_cast<HMODULE>(library), name);
    Function function{};
    if (address != nullptr) {
        static_assert(sizeof(function) == sizeof(address));
        std::memcpy(&function, &address, sizeof(function));
    }
    return function;
#else
    return reinterpret_cast<Function>(dlsym(library, name));
#endif
}

#if !defined(_WIN32)
std::string readLine(const fs::path& path) {
    std::ifstream stream(path);
    std::string value;
    std::getline(stream, value);
    return value;
}
#endif

} // namespace

NvmlMonitor::~NvmlMonitor() {
    shutdown();
}

bool NvmlMonitor::libraryAvailable() const {
    return library_ != nullptr;
}

std::optional<bool> NvmlMonitor::nvidiaDeviceSuspended() {
#if defined(_WIN32)
    return false;
#else
    const fs::path pciRoot{"/sys/bus/pci/devices"};
    std::error_code error;
    if (!fs::exists(pciRoot, error)) {
        return std::nullopt;
    }

    bool foundNvidia = false;
    bool allSuspended = true;
    for (const auto& entry : fs::directory_iterator(
             pciRoot, fs::directory_options::skip_permission_denied, error)) {
        if (readLine(entry.path() / "vendor") != "0x10de") {
            continue;
        }
        const std::string classCode =
            readLine(entry.path() / "class");
        if (classCode.rfind("0x03", 0) != 0) {
            continue;
        }
        foundNvidia = true;
        const std::string state = readLine(entry.path() / "power/runtime_status");
        if (state != "suspended") {
            allSuspended = false;
            break;
        }
    }
    return foundNvidia ? std::optional<bool>{allSuspended} : std::nullopt;
#endif
}

bool NvmlMonitor::initialize() {
    if (initialized_) {
        return true;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now < nextInitialization_) {
        return false;
    }
    // A driver starting or resuming can fail once without staying unavailable.
    nextInitialization_ = now + std::chrono::seconds{5};

#if defined(_WIN32)
    library_ = LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
    library_ = dlopen("libnvidia-ml.so.1", RTLD_LAZY | RTLD_LOCAL);
#endif
    if (library_ == nullptr) {
        return false;
    }

    initFn_ = symbol<InitFn>(library_, "nvmlInit_v2");
    shutdownFn_ = symbol<ShutdownFn>(library_, "nvmlShutdown");
    getCountFn_ = symbol<GetCountFn>(library_, "nvmlDeviceGetCount_v2");
    getHandleFn_ = symbol<GetHandleFn>(
        library_, "nvmlDeviceGetHandleByIndex_v2");
    getNameFn_ = symbol<GetNameFn>(library_, "nvmlDeviceGetName");
    getUtilizationFn_ = symbol<GetUtilizationFn>(
        library_, "nvmlDeviceGetUtilizationRates");
    getMemoryFn_ = symbol<GetMemoryFn>(library_, "nvmlDeviceGetMemoryInfo");
    getTemperatureFn_ = symbol<GetTemperatureFn>(
        library_, "nvmlDeviceGetTemperature");
    getFanSpeedFn_ = symbol<GetFanSpeedFn>(
        library_, "nvmlDeviceGetFanSpeed");
    getPowerUsageFn_ = symbol<GetPowerUsageFn>(
        library_, "nvmlDeviceGetPowerUsage");
    getClockInfoFn_ = symbol<GetClockInfoFn>(
        library_, "nvmlDeviceGetClockInfo");

    if (initFn_ == nullptr || shutdownFn_ == nullptr ||
        getCountFn_ == nullptr || getHandleFn_ == nullptr) {
        shutdown();
        return false;
    }

    if (initFn_() != kNvmlSuccess) {
        shutdown();
        return false;
    }
    initialized_ = true;

    unsigned int count = 0;
    if (getCountFn_(&count) != kNvmlSuccess || count == 0 ||
        getHandleFn_(0, &device_) != kNvmlSuccess) {
        shutdown();
        return false;
    }

    return true;
}

void NvmlMonitor::shutdown() {
    if (initialized_ && shutdownFn_ != nullptr) {
        shutdownFn_();
    }
    initialized_ = false;
    device_ = nullptr;
    if (library_ != nullptr) {
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(library_));
#else
        dlclose(library_);
#endif
        library_ = nullptr;
    }
}

GpuStats NvmlMonitor::sample(bool batteryAware) {
    GpuStats stats;
    stats.discrete = true;
    stats.name = "NVIDIA GPU";

    const auto suspended = nvidiaDeviceSuspended();
    if (!suspended) {
        return stats;
    }
    if (batteryAware && *suspended) {
        if (initialized_) {
            shutdown();
            nextInitialization_ = {};
        }
        stats.available = true;
        stats.sleeping = true;
        return stats;
    }

#if !defined(_WIN32)
    if (access("/dev/nvidiactl", R_OK | W_OK) != 0) {
        return stats;
    }
#endif
    if (!initialize()) {
        stats.available = false;
        stats.name = "NVIDIA GPU unavailable";
        return stats;
    }

    stats.available = true;
    std::array<char, 128> name{};
    if (getNameFn_ != nullptr &&
        getNameFn_(device_, name.data(),
            static_cast<unsigned int>(name.size())) == kNvmlSuccess) {
        stats.name = name.data();
    }

    NvmlUtilization utilization{};
    if (getUtilizationFn_ != nullptr &&
        getUtilizationFn_(device_, &utilization) == kNvmlSuccess) {
        stats.usagePercent = static_cast<double>(utilization.gpu);
        stats.utilizationAvailable = true;
    }

    NvmlMemory memory{};
    if (getMemoryFn_ != nullptr &&
        getMemoryFn_(device_, &memory) == kNvmlSuccess) {
        stats.memoryTotalBytes = memory.total;
        stats.memoryUsedBytes = memory.used;
        if (memory.total > 0) {
            stats.memoryUsagePercent =
                static_cast<double>(memory.used) * 100.0 /
                static_cast<double>(memory.total);
        }
    }

    unsigned int value = 0;
    if (getTemperatureFn_ != nullptr &&
        getTemperatureFn_(device_, kTemperatureGpu, &value) == kNvmlSuccess) {
        stats.temperatureC = static_cast<double>(value);
    }
    if (getFanSpeedFn_ != nullptr &&
        getFanSpeedFn_(device_, &value) == kNvmlSuccess) {
        stats.fanPercent = static_cast<int>(value);
    }
    if (getPowerUsageFn_ != nullptr &&
        getPowerUsageFn_(device_, &value) == kNvmlSuccess) {
        stats.powerWatts = static_cast<double>(value) / 1000.0;
    }
    if (getClockInfoFn_ != nullptr &&
        getClockInfoFn_(device_, kClockGraphics, &value) == kNvmlSuccess) {
        stats.graphicsClockMhz = static_cast<int>(value);
    }
    return stats;
}

} // namespace vg
