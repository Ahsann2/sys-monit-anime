#include "core/SystemSampler.h"

#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace vg {
namespace {

constexpr std::uint64_t kKibibyte = 1024ULL;

bool startsWith(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

bool isNumeric(const std::string& value) {
    return !value.empty() &&
        std::all_of(value.begin(), value.end(),
            [](unsigned char character) {
                return std::isdigit(character) != 0;
            });
}

std::string trim(std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::optional<std::uint64_t> parseUnsigned(const std::string& value) {
    std::uint64_t result = 0;
    const char* begin = value.data();
    const char* end = begin + value.size();
    const auto parsed = std::from_chars(begin, end, result);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        return std::nullopt;
    }
    return result;
}

double safeTemperature(std::uint64_t raw) {
    return static_cast<double>(raw) / 1000.0;
}

std::vector<std::string> splitWhitespace(const std::string& value) {
    std::istringstream stream(value);
    return {std::istream_iterator<std::string>(stream),
            std::istream_iterator<std::string>()};
}

} // namespace

SystemSampler::SystemSampler(HardwareProfile profile)
    : profile_(std::move(profile)) {}

SystemSampler::~SystemSampler() = default;

const HardwareProfile& SystemSampler::profile() const noexcept {
    return profile_;
}

std::string SystemSampler::readText(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return {};
    }
    // A disappearing procfs or sysfs entry may open successfully and fail
    // during the read. Use the stream API so EIO becomes badbit.
    std::array<char, 4096> data;
    stream.read(data.data(), static_cast<std::streamsize>(data.size()));
    if (stream.bad()) {
        return {};
    }
    return trim(std::string(data.data(), static_cast<std::size_t>(stream.gcount())));
}

std::uint64_t SystemSampler::readUnsigned(
    const fs::path& path,
    std::uint64_t fallback) {
    const std::string value = readText(path);
    const auto parsed = parseUnsigned(value);
    return parsed.value_or(fallback);
}

PressureStats SystemSampler::readPressure(const fs::path& path) {
    PressureStats stats;
    std::ifstream stream(path);
    std::string line;
    while (std::getline(stream, line)) {
        const bool full = startsWith(line, "full ");
        const bool some = startsWith(line, "some ");
        if (!full && !some) {
            continue;
        }
        stats.available = true;
        double avg10 = 0.0;
        double avg60 = 0.0;
        double avg300 = 0.0;
        const auto number = [](std::string_view value) {
            double result = 0.0;
            const auto end = value.data() + value.size();
            const auto parsed = std::from_chars(value.data(), end, result);
            return parsed.ec == std::errc{} && parsed.ptr == end &&
                std::isfinite(result) ? result : 0.0;
        };
        for (const std::string& token : splitWhitespace(line)) {
            if (startsWith(token, "avg10=")) {
                avg10 = number(std::string_view(token).substr(6));
            } else if (startsWith(token, "avg60=")) {
                avg60 = number(std::string_view(token).substr(6));
            } else if (startsWith(token, "avg300=")) {
                avg300 = number(std::string_view(token).substr(7));
            }
        }
        if (full) {
            stats.fullAvg10 = avg10;
            stats.fullAvg60 = avg60;
            stats.fullAvg300 = avg300;
        } else {
            stats.someAvg10 = avg10;
            stats.someAvg60 = avg60;
            stats.someAvg300 = avg300;
        }
    }
    return stats;
}

SystemSnapshot SystemSampler::sample(bool includeProcesses) {
    const auto now = std::chrono::steady_clock::now();
    currentIntervalSeconds_ =
        std::chrono::duration<double>(now - previousSampleTime_).count();
    if (currentIntervalSeconds_ < 0.05 ||
        !std::isfinite(currentIntervalSeconds_)) {
        currentIntervalSeconds_ = 1.0;
    }

    SystemSnapshot snapshot;
    snapshot.timestamp = std::chrono::system_clock::now();
    snapshot.systemName = "Linux";

    std::array<char, 256> hostname{};
    if (gethostname(hostname.data(), hostname.size()) == 0) {
        snapshot.hostname = hostname.data();
    }

    struct utsname kernelInfo {};
    if (uname(&kernelInfo) == 0) {
        snapshot.kernel = kernelInfo.release;
    }

    std::ifstream uptimeStream("/proc/uptime");
    double uptimeSeconds = 0.0;
    if (uptimeStream >> uptimeSeconds) {
        snapshot.uptime =
            std::chrono::seconds(static_cast<std::int64_t>(uptimeSeconds));
    }

    sampleCpu(snapshot);
    processDeltaJiffies_ += lastSystemDeltaJiffies_;
    sampleMemory(snapshot);
    sampleThermals(snapshot);
    sampleGpu(snapshot);
    sampleDisk(snapshot);
    sampleNetwork(snapshot);
    sampleBattery(snapshot);
    if (includeProcesses) {
        sampleProcesses(snapshot);
    }

    previousSampleTime_ = now;
    return snapshot;
}

void SystemSampler::sampleCpu(SystemSnapshot& snapshot) {
    std::ifstream stream("/proc/stat");
    std::string line;
    std::vector<CpuTimes> currentTimes;

    while (std::getline(stream, line) && startsWith(line, "cpu")) {
        std::istringstream parser(line);
        std::string label;
        parser >> label;
        if (label != "cpu" && !isNumeric(label.substr(3))) {
            break;
        }

        std::vector<std::uint64_t> fields;
        std::uint64_t field = 0;
        while (parser >> field) {
            fields.push_back(field);
        }
        if (fields.size() < 4) {
            continue;
        }
        // Guest CPU time is already included in user and nice.
        const auto end = fields.begin() + std::min<std::size_t>(8, fields.size());
        const std::uint64_t total = std::accumulate(fields.begin(), end, 0ULL);
        const std::uint64_t idle =
            fields[3] + (fields.size() > 4 ? fields[4] : 0ULL);
        currentTimes.emplace_back(total - idle, total);
    }

    snapshot.cpu.logicalCores =
        currentTimes.empty() ? 0 : static_cast<int>(currentTimes.size() - 1);
    snapshot.cpu.perCorePercent.clear();

    if (previousCpuTimes_.size() == currentTimes.size() &&
        !currentTimes.empty()) {
        for (std::size_t index = 0; index < currentTimes.size(); ++index) {
            const auto [active, total] = currentTimes[index];
            const auto [previousActive, previousTotal] =
                previousCpuTimes_[index];
            const std::uint64_t activeDelta =
                active >= previousActive ? active - previousActive : 0ULL;
            const std::uint64_t totalDelta =
                total >= previousTotal ? total - previousTotal : 0ULL;
            const double usage = totalDelta == 0
                ? 0.0
                : static_cast<double>(activeDelta) * 100.0 /
                      static_cast<double>(totalDelta);
            if (index == 0) {
                snapshot.cpu.usagePercent = clampPercent(usage);
                snapshot.cpu.usageAvailable = totalDelta > 0;
                lastSystemDeltaJiffies_ = totalDelta;
            } else {
                snapshot.cpu.perCorePercent.push_back(clampPercent(usage));
            }
        }
    } else {
        lastSystemDeltaJiffies_ = 0;
        snapshot.cpu.perCorePercent.resize(
            static_cast<std::size_t>(std::max(0, snapshot.cpu.logicalCores)),
            0.0);
    }
    previousCpuTimes_ = std::move(currentTimes);

    std::ifstream loadStream("/proc/loadavg");
    loadStream >> snapshot.cpu.load1 >> snapshot.cpu.load5 >>
        snapshot.cpu.load15;

    double clockTotalKhz = 0.0;
    int clockCount = 0;
    std::error_code error;
    const fs::path cpuRoot{"/sys/devices/system/cpu"};
    for (const auto& entry : fs::directory_iterator(
             cpuRoot, fs::directory_options::skip_permission_denied, error)) {
        const std::string name = entry.path().filename().string();
        if (!startsWith(name, "cpu") || !isNumeric(name.substr(3))) {
            continue;
        }
        const auto current = readUnsigned(
            entry.path() / "cpufreq/scaling_cur_freq");
        if (current > 0) {
            clockTotalKhz += static_cast<double>(current);
            ++clockCount;
        }
    }
    if (clockCount > 0) {
        snapshot.cpu.averageClockGhz =
            clockTotalKhz / static_cast<double>(clockCount) / 1'000'000.0;
    }
    snapshot.cpu.pressure = readPressure("/proc/pressure/cpu");
}

void SystemSampler::sampleMemory(SystemSnapshot& snapshot) const {
    std::ifstream stream("/proc/meminfo");
    std::unordered_map<std::string, std::uint64_t> fields;
    std::string line;
    while (std::getline(stream, line)) {
        std::istringstream parser(line);
        std::string key;
        std::uint64_t value = 0;
        parser >> key >> value;
        if (!parser) {
            continue;
        }
        if (!key.empty() && key.back() == ':') {
            key.pop_back();
        }
        fields[key] = value * kKibibyte;
    }

    auto get = [&fields](const std::string& name) {
        const auto found = fields.find(name);
        return found == fields.end() ? 0ULL : found->second;
    };

    snapshot.memory.totalBytes = get("MemTotal");
    snapshot.memory.availableBytes = get("MemAvailable");
    snapshot.memory.usedBytes =
        snapshot.memory.totalBytes > snapshot.memory.availableBytes
            ? snapshot.memory.totalBytes - snapshot.memory.availableBytes
            : 0ULL;
    snapshot.memory.cachedBytes =
        get("Cached") + get("SReclaimable");
    snapshot.memory.buffersBytes = get("Buffers");
    snapshot.memory.swapTotalBytes = get("SwapTotal");
    snapshot.memory.swapAvailable = fields.contains("SwapTotal") && fields.contains("SwapFree");
    const std::uint64_t swapFree = get("SwapFree");
    snapshot.memory.swapUsedBytes =
        snapshot.memory.swapTotalBytes > swapFree
            ? snapshot.memory.swapTotalBytes - swapFree
            : 0ULL;
    snapshot.memory.pressure = readPressure("/proc/pressure/memory");
}

void SystemSampler::sampleThermals(SystemSnapshot& snapshot) const {
    std::error_code error;
    const fs::path hwmonRoot{"/sys/class/hwmon"};
    double cpuTemperature = 0.0;
    double diskTemperature = 0.0;
    auto& fans = snapshot.fans;
    auto cpuPriority = profile_.preferredCpuSensors.end();
    std::vector<std::pair<unsigned int, double>> coreTemperatures;

    if (fs::exists(hwmonRoot, error)) {
        for (const auto& device : fs::directory_iterator(
                 hwmonRoot,
                 fs::directory_options::skip_permission_denied,
                 error)) {
            const std::string driver = readText(device.path() / "name");
            const auto sensorPriority = std::find(
                profile_.preferredCpuSensors.begin(),
                profile_.preferredCpuSensors.end(), driver);
            const bool diskSensor =
                driver.find("nvme") != std::string::npos ||
                driver.find("drivetemp") != std::string::npos;

            if (driver == "asus" || driver == "asus_wmi_sensors") {
                const auto fanRpm = [&device](const char* file)
                    -> std::optional<int> {
                    const auto rpm = parseUnsigned(
                        readText(device.path() / file));
                    return rpm && *rpm <= 10000
                        ? std::optional<int>{static_cast<int>(*rpm)}
                        : std::nullopt;
                };
                if (const auto rpm = fanRpm("fan1_input")) {
                    fans.cpuRpm = rpm;
                }
                if (const auto rpm = fanRpm("fan2_input")) {
                    fans.gpuRpm = rpm;
                }
            }

            double deviceTemperature = 0.0;
            double packageTemperature = 0.0;
            for (const auto& sensor : fs::directory_iterator(
                     device.path(),
                     fs::directory_options::skip_permission_denied,
                     error)) {
                const std::string fileName =
                    sensor.path().filename().string();
                if (!startsWith(fileName, "temp") ||
                    !fileName.ends_with("_input")) {
                    continue;
                }
                const double temperature =
                    safeTemperature(readUnsigned(sensor.path()));
                if (temperature < 1.0 || temperature > 130.0) {
                    continue;
                }
                if (sensorPriority != profile_.preferredCpuSensors.end()) {
                    deviceTemperature = std::max(deviceTemperature, temperature);
                    if (driver == "coretemp") {
                        const auto label = readText(device.path() /
                            (fileName.substr(0, fileName.size() - 6) + "_label"));
                        if (startsWith(label, "Package id ")) {
                            packageTemperature = std::max(
                                packageTemperature, temperature);
                        } else if (startsWith(label, "Core ")) {
                            const auto core = parseUnsigned(label.substr(5));
                            if (core && *core <= 1024) {
                                coreTemperatures.emplace_back(
                                    static_cast<unsigned int>(*core), temperature);
                            }
                        }
                    }
                }
                if (diskSensor) {
                    diskTemperature = std::max(diskTemperature, temperature);
                }
            }
            if (deviceTemperature > 0.0 && sensorPriority <= cpuPriority) {
                if (sensorPriority < cpuPriority) {
                    cpuTemperature = 0.0;
                    cpuPriority = sensorPriority;
                }
                cpuTemperature = std::max(cpuTemperature,
                    packageTemperature > 0.0 ? packageTemperature
                                             : deviceTemperature);
            }
        }
    }

    if (profile_.gl752vwFanControl) {
        const fs::path asusHwmon{
            "/sys/devices/platform/asus-nb-wmi/hwmon"};
        std::error_code fanError;
        for (const auto& device : fs::directory_iterator(asusHwmon, fanError)) {
            const auto path = device.path() / "pwm1_enable";
            const auto name = device.path().filename().string();
            if (!startsWith(name, "hwmon") || !isNumeric(name.substr(5)) ||
                readText(device.path() / "name") != "asus" ||
                readText(device.path() / "fan1_label") != "cpu_fan") {
                continue;
            }
            const auto mode = readText(path);
            if (mode == "0" || mode == "2") {
                fans.cpuMode = mode == "0" ? FanMode::Maximum : FanMode::Automatic;
                fans.controlPath = path.string();
            }
            break;
        }
    }

    if (cpuTemperature == 0.0) {
        const fs::path thermalRoot{"/sys/class/thermal"};
        if (fs::exists(thermalRoot, error)) {
            for (const auto& zone : fs::directory_iterator(
                     thermalRoot,
                     fs::directory_options::skip_permission_denied,
                     error)) {
                if (!startsWith(zone.path().filename().string(),
                        "thermal_zone")) {
                    continue;
                }
                const double temperature =
                    safeTemperature(readUnsigned(zone.path() / "temp"));
                if (temperature > 1.0 && temperature < 130.0) {
                    cpuTemperature =
                        std::max(cpuTemperature, temperature);
                }
            }
        }
    }

    snapshot.cpu.temperatureC = cpuTemperature;
    snapshot.cpu.packageThrottleCount = parseUnsigned(readText(
        "/sys/devices/system/cpu/cpu0/thermal_throttle/package_throttle_count"));
    std::sort(coreTemperatures.begin(), coreTemperatures.end());
    for (const auto& core : coreTemperatures) {
        snapshot.cpu.coreTemperaturesC.push_back(core.second);
    }
    snapshot.disk.temperatureC = diskTemperature;
}

GpuStats SystemSampler::sampleIntegratedGpu() {
    GpuStats result;
    result.discrete = false;
    result.name = profile_.expectedIntegratedGpu;

    std::error_code error;
    const fs::path drmRoot{"/sys/class/drm"};
    if (!fs::exists(drmRoot, error)) {
        return result;
    }

    for (const auto& card : fs::directory_iterator(
             drmRoot, fs::directory_options::skip_permission_denied, error)) {
        const std::string cardName = card.path().filename().string();
        if (!startsWith(cardName, "card") ||
            !isNumeric(cardName.substr(4))) {
            continue;
        }
        const fs::path device = card.path() / "device";
        const auto vendor = readText(device / "vendor");
        if (vendor == "0x8086" && readUnsigned(device / "boot_vga") == 1) {
            result.available = true;
            const auto frequency = parseUnsigned(readText(
                card.path() / "gt_cur_freq_mhz"));
            if (frequency && *frequency <= 10000) {
                result.graphicsClockMhz = static_cast<int>(*frequency);
            }
            std::error_code deviceError;
            const auto driver = fs::read_symlink(device / "driver", deviceError);
            if (driver.filename() == "i915") {
                const auto pciDevice = fs::canonical(device, deviceError);
                if (const auto usage = intelGpuUsage_.sample(pciDevice)) {
                    result.usagePercent = *usage;
                    result.utilizationAvailable = true;
                    result.utilizationFromClients = true;
                }
            }
            break;
        }
        if (vendor != "0x1002" || profile_.gl752vwFanControl) {
            continue;
        }

        result.available = true;
        const auto usage = parseUnsigned(readText(device / "gpu_busy_percent"));
        if (usage) {
            result.usagePercent = clampPercent(static_cast<double>(*usage));
            result.utilizationAvailable = true;
        }
        result.memoryUsedBytes =
            readUnsigned(device / "mem_info_vram_used");
        result.memoryTotalBytes =
            readUnsigned(device / "mem_info_vram_total");
        if (result.memoryTotalBytes > 0) {
            result.memoryUsagePercent =
                static_cast<double>(result.memoryUsedBytes) * 100.0 /
                static_cast<double>(result.memoryTotalBytes);
        }

        const fs::path hwmon = device / "hwmon";
        if (fs::exists(hwmon, error)) {
            for (const auto& monitor : fs::directory_iterator(
                     hwmon,
                     fs::directory_options::skip_permission_denied,
                     error)) {
                result.temperatureC = safeTemperature(
                    readUnsigned(monitor.path() / "temp1_input"));
                const std::uint64_t power =
                    readUnsigned(monitor.path() / "power1_average");
                if (power > 0) {
                    result.powerWatts =
                        static_cast<double>(power) / 1'000'000.0;
                }
                break;
            }
        }
        break;
    }
    return result;
}

void SystemSampler::sampleGpu(SystemSnapshot& snapshot) {
    snapshot.integratedGpu = sampleIntegratedGpu();
    snapshot.discreteGpu =
        nvml_.sample(profile_.batteryAwareDiscreteGpu);
    if (snapshot.discreteGpu.available && snapshot.discreteGpu.sleeping) {
        snapshot.discreteGpu.name = profile_.expectedDiscreteGpu;
    }
    const auto now = std::chrono::steady_clock::now();
    if (now >= nextGraphicsDiscovery_) {
        nextGraphicsDiscovery_ = now + std::chrono::seconds{15};
        graphicsDevices_.clear();
        std::error_code error;
        for (const auto& card : fs::directory_iterator("/sys/class/drm",
                fs::directory_options::skip_permission_denied, error)) {
            const auto name = card.path().filename().string();
            if (!startsWith(name, "card") || !isNumeric(name.substr(4))) {
                continue;
            }
            const auto device = card.path() / "device";
            const auto vendor = readText(device / "vendor");
            const auto model = readText(device / "device");
            if (vendor.empty() || model.empty()) {
                continue;
            }
            const auto manufacturer = vendor == "0x8086" ? "Intel"
                : vendor == "0x1002" ? "AMD" : vendor == "0x10de" ? "NVIDIA" : "PCI";
            GpuStats detected;
            detected.name = std::string(manufacturer) + " GPU (" + vendor + ":" + model + ")";
            graphicsDevices_.push_back(std::move(detected));
        }
    }
    for (const auto& detected : graphicsDevices_) {
        snapshot.graphicsAdapters.push_back(detected.name);
    }
    if (snapshot.graphicsAdapters.empty()) {
        for (const auto* gpu : {&snapshot.integratedGpu, &snapshot.discreteGpu}) {
            if (gpu->available) {
                snapshot.graphicsAdapters.push_back(gpu->name);
            }
        }
    }
}

void SystemSampler::sampleDisk(SystemSnapshot& snapshot) {
    struct statvfs fileSystemStats {};
    if (statvfs("/", &fileSystemStats) == 0) {
        const std::uint64_t blockSize =
            static_cast<std::uint64_t>(fileSystemStats.f_frsize);
        snapshot.disk.totalBytes =
            static_cast<std::uint64_t>(fileSystemStats.f_blocks) * blockSize;
        snapshot.disk.freeBytes =
            static_cast<std::uint64_t>(fileSystemStats.f_bavail) * blockSize;
        snapshot.disk.usedBytes =
            snapshot.disk.totalBytes > snapshot.disk.freeBytes
                ? snapshot.disk.totalBytes - snapshot.disk.freeBytes
                : 0ULL;
    }

    std::ifstream stream("/proc/diskstats");
    DeviceCounters currentCounters;
    double readSectors = 0;
    double writeSectors = 0;
    std::string line;
    while (std::getline(stream, line)) {
        std::istringstream parser(line);
        unsigned int major = 0;
        unsigned int minor = 0;
        std::string device;
        std::uint64_t reads = 0;
        std::uint64_t readsMerged = 0;
        std::uint64_t sectorsRead = 0;
        std::uint64_t readMilliseconds = 0;
        std::uint64_t writes = 0;
        std::uint64_t writesMerged = 0;
        std::uint64_t sectorsWritten = 0;
        parser >> major >> minor >> device >> reads >> readsMerged >>
            sectorsRead >> readMilliseconds >> writes >> writesMerged >>
            sectorsWritten;
        if (!parser || startsWith(device, "loop") ||
            startsWith(device, "ram") || startsWith(device, "zram") ||
            startsWith(device, "dm-") || startsWith(device, "sr")) {
            continue;
        }
        std::error_code deviceError;
        if (!fs::exists(fs::path{"/sys/block"} / device, deviceError)) {
            continue;
        }
        const auto identity = device + ':' + std::to_string(major) + ':' + std::to_string(minor);
        const Counters counters{sectorsRead, sectorsWritten};
        const auto [read, written] = counterDelta(previousDiskCounters_, identity, counters);
        readSectors += static_cast<double>(read);
        writeSectors += static_cast<double>(written);
        currentCounters.emplace(identity, counters);
    }

    snapshot.disk.readBytesPerSecond = readSectors * 512.0 / currentIntervalSeconds_;
    snapshot.disk.writeBytesPerSecond = writeSectors * 512.0 / currentIntervalSeconds_;
    snapshot.disk.ratesAvailable = !currentCounters.empty();
    previousDiskCounters_ = std::move(currentCounters);
    snapshot.disk.pressure = readPressure("/proc/pressure/io");
}

void SystemSampler::sampleNetwork(SystemSnapshot& snapshot) {
    std::ifstream stream("/proc/net/dev");
    std::string line;
    DeviceCounters currentCounters;
    double receivedDelta = 0;
    double transmittedDelta = 0;
    while (std::getline(stream, line)) {
        const auto delimiter = line.find(':');
        if (delimiter == std::string::npos) {
            continue;
        }
        const std::string interfaceName =
            trim(line.substr(0, delimiter));
        if (interfaceName == "lo") {
            continue;
        }
        std::istringstream parser(line.substr(delimiter + 1));
        std::array<std::uint64_t, 16> fields{};
        for (auto& field : fields) {
            parser >> field;
        }
        if (parser) {
            const Counters counters{fields[0], fields[8]};
            const auto [received, transmitted] = counterDelta(
                previousNetworkCounters_, interfaceName, counters);
            snapshot.network.receivedBytes += counters.first;
            snapshot.network.transmittedBytes += counters.second;
            receivedDelta += static_cast<double>(received);
            transmittedDelta += static_cast<double>(transmitted);
            currentCounters.emplace(interfaceName, counters);
        }
    }

    snapshot.network.receiveBytesPerSecond = receivedDelta / currentIntervalSeconds_;
    snapshot.network.transmitBytesPerSecond = transmittedDelta / currentIntervalSeconds_;
    snapshot.network.available = !currentCounters.empty();
    previousNetworkCounters_ = std::move(currentCounters);
}

void SystemSampler::sampleBattery(SystemSnapshot& snapshot) const {
    std::error_code error;
    const fs::path powerRoot{"/sys/class/power_supply"};
    if (!fs::exists(powerRoot, error)) {
        return;
    }

    for (const auto& supply : fs::directory_iterator(
             powerRoot, fs::directory_options::skip_permission_denied, error)) {
        const std::string type = readText(supply.path() / "type");
        if (type == "Mains") {
            const auto online = parseUnsigned(readText(supply.path() / "online"));
            if (online) {
                snapshot.battery.acOnline = snapshot.battery.acOnline.value_or(false)
                    || *online != 0;
            }
        }
        if (type != "Battery" || snapshot.battery.present ||
            readUnsigned(supply.path() / "present", 1) == 0) {
            continue;
        }

        auto& battery = snapshot.battery;
        battery.present = true;
        battery.status = readText(supply.path() / "status");
        battery.charging = battery.status == "Charging";
        const auto measure = [&supply](const char* attribute) {
            return parseUnsigned(readText(supply.path() / attribute));
        };
        if (const auto capacity = measure("capacity"); capacity && *capacity <= 100) {
            battery.percent = static_cast<double>(*capacity);
        }

        const auto energyFull = measure("energy_full");
        const auto chargeFull = measure("charge_full");
        const auto health = [](std::optional<std::uint64_t> full,
                               std::optional<std::uint64_t> design) -> std::optional<double> {
            if (!full || !design || *design == 0) {
                return std::nullopt;
            }
            return std::min(100.0, static_cast<double>(*full) * 100.0 /
                                       static_cast<double>(*design));
        };
        if (const auto energyHealth = health(energyFull, measure("energy_full_design"))) {
            battery.healthPercent = *energyHealth;
        } else {
            battery.healthPercent = health(chargeFull, measure("charge_full_design")).value_or(-1.0);
        }

        const auto power = measure("power_now");
        const auto current = measure("current_now");
        const auto voltage = measure("voltage_now");
        if (voltage) {
            battery.voltageVolts = static_cast<double>(*voltage) / 1'000'000.0;
        }
        if (power) {
            battery.powerWatts = static_cast<double>(*power) / 1'000'000.0;
        } else if (current && voltage) {
            battery.powerWatts = static_cast<double>(*current) * battery.voltageVolts / 1'000'000.0;
        }
        const std::uint64_t temperature =
            readUnsigned(supply.path() / "temp");
        if (temperature > 0) {
            battery.temperatureC =
                static_cast<double>(temperature) / 10.0;
        }
        const std::string cycleText =
            readText(supply.path() / "cycle_count");
        if (!cycleText.empty()) {
            const auto cycles = parseUnsigned(cycleText);
            if (cycles.has_value() &&
                *cycles <= static_cast<std::uint64_t>(
                    std::numeric_limits<int>::max())) {
                battery.cycleCount = static_cast<int>(*cycles);
            }
        }

        if (battery.charging || battery.status == "Discharging") {
            const auto estimate = [charging = battery.charging](
                std::optional<std::uint64_t> now, std::optional<std::uint64_t> full,
                std::optional<std::uint64_t> rate) -> std::optional<std::int64_t> {
                if (!now || !rate || *rate == 0 || (charging && !full)) {
                    return std::nullopt;
                }
                const auto remaining = charging ? std::max(*full, *now) - *now : *now;
                const double seconds = static_cast<double>(remaining) * 3600.0 /
                    static_cast<double>(*rate);
                if (seconds >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
                    return std::nullopt;
                }
                return static_cast<std::int64_t>(seconds);
            };
            if (const auto byEnergy = estimate(measure("energy_now"), energyFull, power)) {
                battery.estimatedSeconds = *byEnergy;
            } else {
                battery.estimatedSeconds = estimate(measure("charge_now"), chargeFull, current)
                    .value_or(-1);
            }
        }
    }
}

bool SystemSampler::isProtectedProcess(
    const ProcessInfo& process,
    const HardwareProfile& profile) {
    if (process.pid <= 1 || process.name.empty()) {
        return true;
    }
    if (process.cgroup.find("init.scope") != std::string::npos ||
        process.cgroup.find("session.slice") != std::string::npos) {
        return true;
    }
    return std::find(profile.protectedProcessNames.begin(),
               profile.protectedProcessNames.end(), process.name) !=
        profile.protectedProcessNames.end();
}

void SystemSampler::sampleProcesses(SystemSnapshot& snapshot) {
    const uid_t currentUser = getuid();
    const passwd* user = getpwuid(currentUser);
    const fs::path home = user && user->pw_dir ? user->pw_dir : "";
    std::unordered_map<int, std::pair<std::uint64_t, std::uint64_t>> currentTicks;
    std::error_code error;
    const fs::path procRoot{"/proc"};

    for (const auto& entry : fs::directory_iterator(
             procRoot, fs::directory_options::skip_permission_denied, error)) {
        const std::string pidText = entry.path().filename().string();
        if (!isNumeric(pidText)) {
            continue;
        }
        const auto parsedPid = parseUnsigned(pidText);
        if (!parsedPid || *parsedPid > static_cast<unsigned>(std::numeric_limits<int>::max())) {
            continue;
        }
        const int pid = static_cast<int>(*parsedPid);
        ProcessInfo process;
        process.pid = pid;

        std::ifstream status(entry.path() / "status");
        std::string line;
        bool correctUser = false;
        while (std::getline(status, line)) {
            if (startsWith(line, "Name:")) {
                process.name = trim(line.substr(5));
            } else if (startsWith(line, "Uid:")) {
                std::istringstream parser(line.substr(4));
                parser >> process.uid;
                correctUser = process.uid == currentUser;
            } else if (startsWith(line, "VmRSS:")) {
                std::istringstream parser(line.substr(6));
                std::uint64_t kibibytes = 0;
                parser >> kibibytes;
                process.residentAvailable = static_cast<bool>(parser);
                process.residentBytes = kibibytes * kKibibyte;
            } else if (startsWith(line, "Threads:")) {
                std::istringstream parser(line.substr(8));
                parser >> process.threadCount;
            }
        }
        if (!correctUser) {
            continue;
        }

        const std::string stat = readText(entry.path() / "stat");
        const auto closeParen = stat.rfind(')');
        if (closeParen == std::string::npos ||
            closeParen + 2 >= stat.size()) {
            continue;
        }
        const std::vector<std::string> statFields =
            splitWhitespace(stat.substr(closeParen + 2));
        if (statFields.size() < 20 || statFields[0] == "Z") {
            continue;
        }
        const auto parentPid = parseUnsigned(statFields[1]);
        const auto userTicks = parseUnsigned(statFields[11]);
        const auto systemTicks = parseUnsigned(statFields[12]);
        const auto startTicks = parseUnsigned(statFields[19]);
        if (!parentPid || *parentPid > static_cast<unsigned>(std::numeric_limits<int>::max()) ||
            !userTicks || !systemTicks || !startTicks) {
            continue;
        }
        process.parentPid = static_cast<int>(*parentPid);
        process.startTicks = *startTicks;
        const std::uint64_t processTicks = *userTicks + *systemTicks;
        currentTicks[pid] = {process.startTicks, processTicks};
        const auto previous = previousProcessTicks_.find(pid);
        if (previous != previousProcessTicks_.end() &&
            process.startTicks == previous->second.first &&
            processTicks >= previous->second.second &&
            processDeltaJiffies_ > 0) {
            const std::uint64_t delta = processTicks - previous->second.second;
            const double capacity = 100.0 * std::max(1, snapshot.cpu.logicalCores);
            process.cpuPercent = std::min(
                capacity, static_cast<double>(delta) * capacity /
                    static_cast<double>(processDeltaJiffies_));
        }

        std::string command = readText(entry.path() / "cmdline");
        std::replace(command.begin(), command.end(), '\0', ' ');
        command = trim(command);
        if (command.size() > 220) {
            command.resize(217);
            command += "...";
        }
        process.command = command.empty() ? process.name : command;

        std::ifstream cgroupStream(entry.path() / "cgroup");
        while (std::getline(cgroupStream, line)) {
            const auto delimiter = line.rfind(':');
            if (delimiter != std::string::npos) {
                if (!process.cgroup.empty()) {
                    process.cgroup += ';';
                }
                process.cgroup += line.substr(delimiter + 1);
            }
        }

        std::ifstream ioStream(entry.path() / "io");
        while (std::getline(ioStream, line)) {
            if (startsWith(line, "read_bytes:")) {
                process.ioAvailable = true;
                process.readBytes = parseUnsigned(
                    trim(line.substr(11))).value_or(0);
            } else if (startsWith(line, "write_bytes:")) {
                process.writeBytes = parseUnsigned(
                    trim(line.substr(12))).value_or(0);
            }
        }
        process.protectedProcess =
            isProtectedProcess(process, profile_);
        if (!process.protectedProcess && !home.empty() &&
            (process.name == "node" || process.name == "workerd" ||
             process.name == "esbuild" || startsWith(process.name, "npm run dev") ||
             (process.name == "sh" && command.find("vite") != std::string::npos))) {
            std::error_code cwdError;
            const auto cwd = fs::read_symlink(entry.path() / "cwd", cwdError);
            if (!cwdError) {
                const auto relative = cwd.lexically_relative(home);
                if (!relative.empty() && *relative.begin() != "..") {
                    for (auto folder = cwd; folder != home && folder != folder.root_path();
                         folder = folder.parent_path()) {
                        std::error_code repositoryError;
                        if (fs::exists(folder / ".git", repositoryError)) {
                            process.project = folder.string();
                            break;
                        }
                    }
                }
            }
        }
        snapshot.processes.push_back(std::move(process));
    }

    previousProcessTicks_ = std::move(currentTicks);
    processDeltaJiffies_ = 0;
    std::sort(snapshot.processes.begin(), snapshot.processes.end(),
        [](const ProcessInfo& left, const ProcessInfo& right) {
            if (left.cpuPercent != right.cpuPercent) {
                return left.cpuPercent > right.cpuPercent;
            }
            return left.residentBytes > right.residentBytes;
        });
}

} // namespace vg
