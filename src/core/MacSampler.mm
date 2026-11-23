#include "core/SystemSampler.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <Metal/Metal.h>
#include <libproc.h>
#include <mach/mach.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <limits>

namespace vg {
namespace {

template<typename T>
std::optional<T> sysValue(const char* name) {
    T value{};
    std::size_t size = sizeof(value);
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0 || size != sizeof(value)) {
        return std::nullopt;
    }
    return value;
}

std::string sysText(const char* name) {
    std::size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0 || size > 1024) {
        return {};
    }
    std::string value(size, '\0');
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0 || size == 0) {
        return {};
    }
    value.resize(size);
    if (value.back() == '\0') {
        value.pop_back();
    }
    return value;
}

std::optional<std::int64_t> number(CFDictionaryRef values, CFStringRef key) {
    const auto entry = CFDictionaryGetValue(values, key);
    std::int64_t value = 0;
    if (entry && CFGetTypeID(entry) == CFNumberGetTypeID() &&
        CFNumberGetValue(static_cast<CFNumberRef>(entry), kCFNumberSInt64Type, &value)) {
        return value;
    }
    return std::nullopt;
}

} // namespace

HardwareProfile HardwareProfile::detect() {
    auto profile = forDevice("", sysText("hw.model"));
    if (profile.displayName == "Linux") {
        profile.displayName = "Mac";
    }
    profile.expectedCpu = sysText("machdep.cpu.brand_string");
    if (profile.expectedCpu.empty()) {
        profile.expectedCpu = "CPU";
    }
    profile.expectedRamBytes = sysValue<std::uint64_t>("hw.memsize").value_or(0);
    return profile;
}

SystemSampler::~SystemSampler() = default;

void SystemSampler::samplePlatformIdentity(SystemSnapshot& snapshot) const {
    snapshot.systemName = "macOS";
    snapshot.kernel = sysText("kern.osproductversion");
    const auto build = sysText("kern.osversion");
    if (!build.empty()) {
        snapshot.kernel += (snapshot.kernel.empty() ? "" : " · ") + std::string("build ") + build;
    }
    struct utsname name{};
    if (uname(&name) == 0) {
        snapshot.hostname = name.nodename;
    }
    if (auto boot = sysValue<timeval>("kern.boottime")) {
        const auto uptime = std::time(nullptr) - boot->tv_sec;
        if (uptime >= 0) {
            snapshot.uptime = std::chrono::seconds(uptime);
        }
    }
}

void SystemSampler::sampleCpu(SystemSnapshot& snapshot) {
    auto& cpu = snapshot.cpu;
    cpu.logicalCores = sysValue<int>("hw.logicalcpu").value_or(0);
    double averages[3]{};
    if (getloadavg(averages, 3) == 3) {
        cpu.load1 = averages[0];
        cpu.load5 = averages[1];
        cpu.load15 = averages[2];
    }

    const auto host = mach_host_self();
    natural_t count = 0;
    mach_msg_type_number_t length = 0;
    processor_info_array_t ticks = nullptr;
    const auto code = host_processor_info(host, PROCESSOR_CPU_LOAD_INFO, &count, &ticks, &length);
    mach_port_deallocate(mach_task_self(), host);
    if (code != KERN_SUCCESS || !ticks) {
        return;
    }

    std::vector<CpuTimes> current;
    current.reserve(count);
    std::uint64_t busyDelta = 0;
    std::uint64_t totalDelta = 0;
    for (natural_t index = 0; index < count && index < length / CPU_STATE_MAX; ++index) {
        const auto* values = ticks + index * CPU_STATE_MAX;
        const auto busy = std::uint64_t{static_cast<unsigned>(values[CPU_STATE_USER])} +
            static_cast<unsigned>(values[CPU_STATE_SYSTEM]) +
            static_cast<unsigned>(values[CPU_STATE_NICE]);
        const auto total = busy + static_cast<unsigned>(values[CPU_STATE_IDLE]);
        current.emplace_back(busy, total);
        if (count == previousCpuTimes_.size()) {
            const auto [lastBusy, lastTotal] = previousCpuTimes_[index];
            if (total > lastTotal && busy >= lastBusy) {
                busyDelta += busy - lastBusy;
                totalDelta += total - lastTotal;
                cpu.perCorePercent.push_back(clampPercent(
                    100.0 * static_cast<double>(busy - lastBusy) / static_cast<double>(total - lastTotal)));
            } else {
                cpu.perCorePercent.push_back(-1.0);
            }
        }
    }
    vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(ticks),
                  static_cast<vm_size_t>(length) * sizeof(*ticks));
    cpu.logicalCores = static_cast<int>(current.size());
    if (totalDelta > 0) {
        cpu.usageAvailable = true;
        cpu.usagePercent = clampPercent(100.0 * static_cast<double>(busyDelta) /
                                               static_cast<double>(totalDelta));
    }
    previousCpuTimes_ = std::move(current);
}

void SystemSampler::sampleMemory(SystemSnapshot& snapshot) const {
    auto& memory = snapshot.memory;
    const auto host = mach_host_self();
    vm_statistics64_data_t stats{};
    mach_msg_type_number_t length = HOST_VM_INFO64_COUNT;
    const auto code = host_statistics64(host, HOST_VM_INFO64,
        reinterpret_cast<host_info64_t>(&stats), &length);
    mach_port_deallocate(mach_task_self(), host);
    if (code == KERN_SUCCESS) {
        memory.totalBytes = profile_.expectedRamBytes;
        const auto pages = std::uint64_t{stats.free_count} + stats.inactive_count;
        memory.availableBytes = std::min(memory.totalBytes,
            pages * static_cast<std::uint64_t>(getpagesize()));
        memory.usedBytes = memory.totalBytes - memory.availableBytes;
    }
    if (const auto swap = sysValue<xsw_usage>("vm.swapusage")) {
        memory.swapAvailable = true;
        memory.swapTotalBytes = swap->xsu_total;
        memory.swapUsedBytes = swap->xsu_used;
    }
}

void SystemSampler::sampleGpu(SystemSnapshot& snapshot) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= nextGraphicsDiscovery_) {
        nextGraphicsDiscovery_ = now + std::chrono::seconds{15};
        graphicsDevices_.clear();
        @autoreleasepool {
            for (id<MTLDevice> device in MTLCopyAllDevices()) {
                const char* label = [device.name UTF8String];
                GpuStats gpu;
                gpu.available = true;
                gpu.discrete = !device.hasUnifiedMemory;
                gpu.name = label ? label : "GPU Metal";
                graphicsDevices_.push_back(std::move(gpu));
            }
        }
    }
    for (const auto& detected : graphicsDevices_) {
        snapshot.graphicsAdapters.push_back(detected.name);
        auto& gpu = detected.discrete ? snapshot.discreteGpu : snapshot.integratedGpu;
        if (!gpu.available) {
            gpu = detected;
        }
    }
}

void SystemSampler::sampleDisk(SystemSnapshot& snapshot) {
    auto& disk = snapshot.disk;
    struct statvfs space {};
    if (statvfs("/", &space) == 0) {
        disk.totalBytes = std::uint64_t{space.f_blocks} * space.f_frsize;
        disk.freeBytes = std::min(disk.totalBytes, std::uint64_t{space.f_bfree} * space.f_frsize);
        disk.usedBytes = disk.totalBytes - disk.freeBytes;
    }

    io_iterator_t devices = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(MACH_PORT_NULL,
            IOServiceMatching(kIOBlockStorageDriverClass), &devices) != KERN_SUCCESS) {
        return;
    }
    DeviceCounters current;
    while (const io_service_t drive = IOIteratorNext(devices)) {
        auto values = IORegistryEntryCreateCFProperty(drive,
            CFSTR(kIOBlockStorageDriverStatisticsKey), kCFAllocatorDefault, 0);
        std::uint64_t id = 0;
        if (values && CFGetTypeID(values) == CFDictionaryGetTypeID() &&
            IORegistryEntryGetRegistryEntryID(drive, &id) == KERN_SUCCESS) {
            const auto fields = static_cast<CFDictionaryRef>(values);
            const auto read = number(fields, CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey));
            const auto write = number(fields, CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey));
            if (read && write && *read >= 0 && *write >= 0) {
                const auto key = std::to_string(id);
                const Counters valuesNow{static_cast<std::uint64_t>(*read), static_cast<std::uint64_t>(*write)};
                const auto [readDelta, writeDelta] = counterDelta(previousDiskCounters_, key, valuesNow);
                current.emplace(key, valuesNow);
                disk.ratesAvailable = true;
                disk.readBytesPerSecond += static_cast<double>(readDelta) / currentIntervalSeconds_;
                disk.writeBytesPerSecond += static_cast<double>(writeDelta) / currentIntervalSeconds_;
            }
        }
        if (values) {
            CFRelease(values);
        }
        IOObjectRelease(drive);
    }
    IOObjectRelease(devices);
    previousDiskCounters_ = std::move(current);
}

void SystemSampler::sampleNetwork(SystemSnapshot& snapshot) {
    int query[]{CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
    std::size_t length = 0;
    if (sysctl(query, 6, nullptr, &length, nullptr, 0) != 0 ||
        length == 0 || length > 16 * 1024 * 1024) {
        return;
    }
    std::vector<char> bytes(length);
    if (sysctl(query, 6, bytes.data(), &length, nullptr, 0) != 0) {
        return;
    }

    DeviceCounters current;
    auto& network = snapshot.network;
    network.available = true;
    for (std::size_t offset = 0; length - offset >= sizeof(if_msghdr);) {
        if_msghdr header{};
        std::memcpy(&header, bytes.data() + offset, sizeof(header));
        if (header.ifm_msglen == 0 || header.ifm_msglen > length - offset) {
            break;
        }
        if (header.ifm_type == RTM_IFINFO2 && header.ifm_msglen >= sizeof(if_msghdr2)) {
            if_msghdr2 row{};
            std::memcpy(&row, bytes.data() + offset, sizeof(row));
            if ((row.ifm_flags & IFF_LOOPBACK) == 0) {
                const auto key = std::to_string(row.ifm_index);
                const Counters values{row.ifm_data.ifi_ibytes, row.ifm_data.ifi_obytes};
                const auto [received, sent] = counterDelta(previousNetworkCounters_, key, values);
                current.emplace(key, values);
                network.receivedBytes += values.first;
                network.transmittedBytes += values.second;
                network.receiveBytesPerSecond += static_cast<double>(received) / currentIntervalSeconds_;
                network.transmitBytesPerSecond += static_cast<double>(sent) / currentIntervalSeconds_;
            }
        }
        offset += header.ifm_msglen;
    }
    previousNetworkCounters_ = std::move(current);
}

void SystemSampler::sampleBattery(SystemSnapshot& snapshot) const {
    const auto power = IOPSCopyPowerSourcesInfo();
    if (!power) {
        return;
    }
    auto& battery = snapshot.battery;
    const auto provider = IOPSGetProvidingPowerSourceType(power);
    if (provider) {
        if (CFEqual(provider, CFSTR(kIOPMACPowerKey))) {
            battery.acOnline = true;
        } else if (CFEqual(provider, CFSTR(kIOPMBatteryPowerKey))) {
            battery.acOnline = false;
        }
    }

    const auto sources = IOPSCopyPowerSourcesList(power);
    if (sources) {
        for (CFIndex index = 0; index < CFArrayGetCount(sources); ++index) {
            const auto source = CFArrayGetValueAtIndex(sources, index);
            const auto fields = IOPSGetPowerSourceDescription(power, source);
            if (!fields) {
                continue;
            }
            const auto type = CFDictionaryGetValue(fields, CFSTR(kIOPSTypeKey));
            if (!type || !CFEqual(type, CFSTR(kIOPSInternalBatteryType))) {
                continue;
            }
            battery.present = true;
            const auto current = number(fields, CFSTR(kIOPSCurrentCapacityKey));
            const auto maximum = number(fields, CFSTR(kIOPSMaxCapacityKey));
            if (current && maximum && *current >= 0 && *maximum > 0) {
                battery.percent = clampPercent(static_cast<double>(*current) * 100 / static_cast<double>(*maximum));
            }
            const auto charging = CFDictionaryGetValue(fields, CFSTR(kIOPSIsChargingKey));
            battery.charging = charging && CFEqual(charging, kCFBooleanTrue);
            const auto mins = number(fields, battery.charging
                ? CFSTR(kIOPSTimeToFullChargeKey) : CFSTR(kIOPSTimeToEmptyKey));
            if (mins && *mins >= 0 && *mins < 60 * 24) {
                battery.estimatedSeconds = *mins * 60;
            }
            if (battery.charging) {
                battery.status = "Charging";
            } else if (battery.acOnline.has_value()) {
                battery.status = !*battery.acOnline ? "Discharging"
                    : (battery.percent == 100 ? "Full" : "Not charging");
            }
            break;
        }
        CFRelease(sources);
    }
    CFRelease(power);
}

void SystemSampler::sampleProcesses(SystemSnapshot& snapshot) {
    const int estimate = proc_listallpids(nullptr, 0);
    if (estimate <= 0 || estimate > 131072) {
        return;
    }
    std::vector<pid_t> pids(static_cast<std::size_t>(estimate) + 128);
    const int count = proc_listallpids(pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = previousProcessSampleTime_ == std::chrono::steady_clock::time_point{}
        ? 0.0 : std::chrono::duration<double>(now - previousProcessSampleTime_).count();
    decltype(previousProcessTicks_) current;
    for (int index = 0; index < count && static_cast<std::size_t>(index) < pids.size(); ++index) {
        const pid_t pid = pids[static_cast<std::size_t>(index)];
        proc_bsdinfo identity{};
        if (pid <= 0 || proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &identity, sizeof(identity)) != sizeof(identity)) {
            continue;
        }
        ProcessInfo process;
        process.pid = pid;
        process.uid = identity.pbi_uid;
        process.parentPid = static_cast<int>(identity.pbi_ppid);
        process.name = identity.pbi_name[0] ? identity.pbi_name : identity.pbi_comm;
        process.command = process.name;
        process.startTicks = identity.pbi_start_tvsec * 1'000'000ULL + identity.pbi_start_tvusec;
        process.protectedProcess = true;
        proc_taskinfo task{};
        if (proc_pidinfo(pid, PROC_PIDTASKINFO, 0, &task, sizeof(task)) == sizeof(task)) {
            process.threadCount = task.pti_threadnum;
            process.residentAvailable = true;
            process.residentBytes = task.pti_resident_size;
            const auto used = task.pti_total_user + task.pti_total_system;
            measureProcessCpu(process, used, elapsed, snapshot.cpu.logicalCores);
            current.emplace(pid, Counters{process.startTicks, used});
        }
        rusage_info_v2 usage{};
        if (proc_pid_rusage(pid, RUSAGE_INFO_V2, reinterpret_cast<rusage_info_t*>(&usage)) == 0) {
            process.ioAvailable = true;
            process.readBytes = usage.ri_diskio_bytesread;
            process.writeBytes = usage.ri_diskio_byteswritten;
        }
        snapshot.processes.push_back(std::move(process));
    }
    previousProcessTicks_ = std::move(current);
    previousProcessSampleTime_ = now;
}

} // namespace vg
