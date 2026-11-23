#include "core/SystemSampler.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <cstddef>
#include <limits>
#include <memory>
#include <string_view>

namespace vg {
namespace {

std::string utf8(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const auto length = static_cast<int>(value.size());
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.data(), length, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), length, result.data(), bytes, nullptr, nullptr);
    return result;
}

std::wstring registryText(const wchar_t* key, const wchar_t* entry) {
    wchar_t value[256]{};
    DWORD bytes = sizeof(value);
    return RegGetValueW(HKEY_LOCAL_MACHINE, key, entry, RRF_RT_REG_SZ, nullptr, value, &bytes)
        == ERROR_SUCCESS ? value : L"";
}

std::uint64_t ticks(FILETIME value) {
    return (std::uint64_t{value.dwHighDateTime} << 32) | value.dwLowDateTime;
}

struct CloseKernelHandle {
    void operator()(void* value) const { CloseHandle(value); }
};
using KernelHandle = std::unique_ptr<void, CloseKernelHandle>;

struct ReleaseCom {
    template<typename T>
    void operator()(T* value) const { value->Release(); }
};

std::optional<double> pdhValue(PDH_HCOUNTER counter) {
    PDH_FMT_COUNTERVALUE value{};
    if (counter && PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value)
        == ERROR_SUCCESS && (value.CStatus == PDH_CSTATUS_VALID_DATA ||
                             value.CStatus == PDH_CSTATUS_NEW_DATA)) {
        return value.doubleValue;
    }
    return std::nullopt;
}

std::vector<double> perProcessor(PDH_HCOUNTER counter) {
    DWORD bytes = 0;
    DWORD count = 0;
    if (!counter || PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &count, nullptr)
            != static_cast<PDH_STATUS>(PDH_MORE_DATA) || bytes == 0 || bytes > 1024 * 1024) {
        return {};
    }
    auto buffer = std::make_unique<std::byte[]>(bytes);
    auto* array = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.get());
    if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &count, array) != ERROR_SUCCESS) {
        return {};
    }
    std::vector<double> result;
    for (DWORD index = 0; index < count; ++index) {
        const auto& item = array[index];
        if (item.szName && !std::wstring_view(item.szName).ends_with(L"_Total") &&
            (item.FmtValue.CStatus == PDH_CSTATUS_VALID_DATA ||
             item.FmtValue.CStatus == PDH_CSTATUS_NEW_DATA)) {
            result.push_back(clampPercent(item.FmtValue.doubleValue));
        }
    }
    return result;
}

struct PagingFilePages {
    std::uint64_t capacity{0};
    std::uint64_t used{0};
};

BOOL CALLBACK sumPages(void* state, PENUM_PAGE_FILE_INFORMATION entry, LPCWSTR) {
    auto& pages = *static_cast<PagingFilePages*>(state);
    pages.capacity += entry->TotalSize;
    pages.used += entry->TotalInUse;
    return TRUE;
}

} // namespace

HardwareProfile HardwareProfile::detect() {
    const auto bios = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
    auto profile = forDevice("", utf8(registryText(bios, L"SystemProductName")));
    if (profile.displayName == "Linux") {
        profile.displayName = "PC Windows";
    }
    const auto cpu = registryText(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                                  L"ProcessorNameString");
    if (!cpu.empty()) {
        profile.expectedCpu = utf8(cpu);
    }
    return profile;
}

SystemSampler::~SystemSampler() {
    if (pdhQuery_) {
        PdhCloseQuery(pdhQuery_);
    }
}

void SystemSampler::samplePlatformIdentity(SystemSnapshot& snapshot) const {
    snapshot.systemName = "Windows";
    wchar_t name[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(name, &size)) {
        snapshot.hostname = utf8({name, size});
    }

    const auto versionKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    snapshot.kernel = utf8(registryText(versionKey, L"DisplayVersion"));
    const auto build = utf8(registryText(versionKey, L"CurrentBuildNumber"));
    if (!build.empty()) {
        snapshot.kernel += (snapshot.kernel.empty() ? "" : " · ") + std::string("build ") + build;
    }
    snapshot.uptime = std::chrono::seconds(GetTickCount64() / 1000);
}

void SystemSampler::sampleCpu(SystemSnapshot& snapshot) {
    auto& cpu = snapshot.cpu;
    cpu.logicalCores = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    FILETIME idle{}, kernel{}, user{};
    if (GetSystemTimes(&idle, &kernel, &user)) {
        const auto total = ticks(kernel) + ticks(user);
        const auto busy = total - std::min(total, ticks(idle));
        if (!previousCpuTimes_.empty()) {
            const auto [lastBusy, lastTotal] = previousCpuTimes_.front();
            if (total > lastTotal && busy >= lastBusy) {
                cpu.usagePercent = clampPercent(100.0 * static_cast<double>(busy - lastBusy) /
                                                      static_cast<double>(total - lastTotal));
                cpu.usageAvailable = true;
            }
        }
        previousCpuTimes_.assign(1, {busy, total});
    }

    if (!pdhQuery_ && PdhOpenQueryW(nullptr, 0, &pdhQuery_) == ERROR_SUCCESS) {
        PdhAddEnglishCounterW(pdhQuery_, L"\\Processor Information(*)\\% Processor Time", 0, &pdhCpu_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &pdhDiskRead_);
        PdhAddEnglishCounterW(pdhQuery_, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &pdhDiskWrite_);
    }
    if (pdhQuery_ && PdhCollectQueryData(pdhQuery_) == ERROR_SUCCESS) {
        cpu.perCorePercent = perProcessor(pdhCpu_);
    }
}

void SystemSampler::sampleMemory(SystemSnapshot& snapshot) const {
    auto& memory = snapshot.memory;
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        memory.totalBytes = status.ullTotalPhys;
        memory.availableBytes = std::min(memory.totalBytes, status.ullAvailPhys);
        memory.usedBytes = memory.totalBytes - memory.availableBytes;
    }
    PagingFilePages pages;
    if (EnumPageFilesW(sumPages, &pages)) {
        SYSTEM_INFO info{};
        GetNativeSystemInfo(&info);
        memory.swapTotalBytes = pages.capacity * info.dwPageSize;
        memory.swapUsedBytes = pages.used * info.dwPageSize;
        memory.swapAvailable = true;
    }
}

void SystemSampler::sampleGpu(SystemSnapshot& snapshot) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= nextGraphicsDiscovery_) {
        nextGraphicsDiscovery_ = now + std::chrono::seconds{15};
        IDXGIFactory1* rawFactory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&rawFactory)))) {
            graphicsDevices_.clear();
            const std::unique_ptr<IDXGIFactory1, ReleaseCom> factory(rawFactory);
            for (UINT index = 0; index < 32; ++index) {
                IDXGIAdapter1* rawAdapter = nullptr;
                if (FAILED(factory->EnumAdapters1(index, &rawAdapter))) {
                    break;
                }
                const std::unique_ptr<IDXGIAdapter1, ReleaseCom> adapter(rawAdapter);
                DXGI_ADAPTER_DESC1 desc{};
                if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                    continue;
                }
                const auto name = utf8(desc.Description);
                ID3D12Device* rawDevice = nullptr;
                bool integrated = false;
                if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0,
                                               __uuidof(ID3D12Device), reinterpret_cast<void**>(&rawDevice)))) {
                    const std::unique_ptr<ID3D12Device, ReleaseCom> device(rawDevice);
                    D3D12_FEATURE_DATA_ARCHITECTURE architecture{};
                    integrated = SUCCEEDED(device->CheckFeatureSupport(
                        D3D12_FEATURE_ARCHITECTURE, &architecture, sizeof(architecture))) && architecture.UMA;
                }
                GpuStats gpu;
                gpu.available = true;
                gpu.discrete = !integrated;
                gpu.name = name;
                gpu.memoryTotalBytes = desc.DedicatedVideoMemory;
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
    auto nvidia = nvml_.sample(false);
    if (nvidia.available) {
        snapshot.discreteGpu = std::move(nvidia);
        if (snapshot.graphicsAdapters.empty()) {
            snapshot.graphicsAdapters.push_back(snapshot.discreteGpu.name);
        }
    }
}

void SystemSampler::sampleDisk(SystemSnapshot& snapshot) {
    wchar_t windowsDirectory[MAX_PATH]{};
    const auto length = GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        const auto root = std::filesystem::path(windowsDirectory).root_path();
        ULARGE_INTEGER available{}, total{}, free{};
        if (!root.empty() && GetDiskFreeSpaceExW(root.c_str(), &available, &total, &free)) {
            snapshot.disk.totalBytes = total.QuadPart;
            snapshot.disk.freeBytes = std::min(total.QuadPart, free.QuadPart);
            snapshot.disk.usedBytes = snapshot.disk.totalBytes - snapshot.disk.freeBytes;
        }
    }
    const auto read = pdhValue(pdhDiskRead_);
    const auto write = pdhValue(pdhDiskWrite_);
    if (read && write) {
        snapshot.disk.ratesAvailable = true;
        snapshot.disk.readBytesPerSecond = *read;
        snapshot.disk.writeBytesPerSecond = *write;
    }
}

void SystemSampler::sampleNetwork(SystemSnapshot& snapshot) {
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR) {
        return;
    }
    const std::unique_ptr<MIB_IF_TABLE2, decltype(&FreeMibTable)> owner(table, FreeMibTable);
    DeviceCounters current;
    auto& stat = snapshot.network;
    stat.available = true;
    for (ULONG index = 0; index < table->NumEntries; ++index) {
        const auto& row = table->Table[index];
        if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        const auto id = std::to_string(row.InterfaceLuid.Value);
        const Counters values{row.InOctets, row.OutOctets};
        const auto [received, sent] = counterDelta(previousNetworkCounters_, id, values);
        current.emplace(id, values);
        stat.receivedBytes += values.first;
        stat.transmittedBytes += values.second;
        stat.receiveBytesPerSecond += static_cast<double>(received) / currentIntervalSeconds_;
        stat.transmitBytesPerSecond += static_cast<double>(sent) / currentIntervalSeconds_;
    }
    previousNetworkCounters_ = std::move(current);
}

void SystemSampler::sampleBattery(SystemSnapshot& snapshot) const {
    SYSTEM_POWER_STATUS power{};
    if (!GetSystemPowerStatus(&power)) {
        return;
    }
    auto& battery = snapshot.battery;
    if (power.ACLineStatus <= 1) {
        battery.acOnline = power.ACLineStatus == 1;
    }
    battery.present = power.BatteryFlag != 255 && (power.BatteryFlag & 128) == 0;
    if (battery.present) {
        battery.charging = (power.BatteryFlag & 8) != 0;
        if (power.BatteryLifePercent <= 100) {
            battery.percent = power.BatteryLifePercent;
        }
        if (battery.charging) {
            battery.status = "Charging";
        } else if (battery.acOnline.has_value()) {
            battery.status = !*battery.acOnline ? "Discharging"
                : (battery.percent == 100 ? "Full" : "Not charging");
        }
        if (power.BatteryLifeTime != MAXDWORD) {
            battery.estimatedSeconds = power.BatteryLifeTime;
        }
    }
}

void SystemSampler::sampleProcesses(SystemSnapshot& snapshot) {
    void* handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    const KernelHandle list(handle);
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(list.get(), &entry)) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const double elapsed = previousProcessSampleTime_ == std::chrono::steady_clock::time_point{}
        ? 0.0 : std::chrono::duration<double>(now - previousProcessSampleTime_).count();
    decltype(previousProcessTicks_) current;

    do {
        if (entry.th32ProcessID == 0 ||
            entry.th32ProcessID > static_cast<DWORD>(std::numeric_limits<int>::max())) {
            continue;
        }
        ProcessInfo process;
        process.pid = static_cast<int>(entry.th32ProcessID);
        process.threadCount = static_cast<int>(entry.cntThreads);
        process.name = utf8(entry.szExeFile);
        process.command = process.name;
        process.protectedProcess = true;
        KernelHandle task(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
        if (task) {
            FILETIME created{}, exited{}, kernel{}, user{};
            if (GetProcessTimes(task.get(), &created, &exited, &kernel, &user)) {
                process.startTicks = ticks(created);
                const auto cpuNs = (ticks(kernel) + ticks(user)) * 100;
                measureProcessCpu(process, cpuNs, elapsed, snapshot.cpu.logicalCores);
                current.emplace(process.pid, Counters{process.startTicks, cpuNs});
            }
            PROCESS_MEMORY_COUNTERS mem{};
            if (GetProcessMemoryInfo(task.get(), &mem, sizeof(mem))) {
                process.residentAvailable = true;
                process.residentBytes = mem.WorkingSetSize;
            }
            IO_COUNTERS io{};
            if (GetProcessIoCounters(task.get(), &io)) {
                process.ioAvailable = true;
                process.readBytes = io.ReadTransferCount;
                process.writeBytes = io.WriteTransferCount;
            }
        }
        snapshot.processes.push_back(std::move(process));
    } while (Process32NextW(list.get(), &entry));
    previousProcessTicks_ = std::move(current);
    previousProcessSampleTime_ = now;
}

} // namespace vg
