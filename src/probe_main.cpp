#include "core/AlertEngine.h"
#include "core/SystemSampler.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace {

constexpr double bytesPerGiB = 1073741824.0;

std::string reading(double value, const char* unit, bool available) {
    if (!available) {
        return "N/A";
    }
    std::ostringstream text;
    text << std::fixed << std::setprecision(1) << value << unit;
    return text.str();
}

double gib(std::uint64_t bytes) {
    return static_cast<double>(bytes) / bytesPerGiB;
}

std::string rpm(const std::optional<int>& value) {
    return value ? std::to_string(*value) + " RPM" : "N/A";
}

} // namespace

int main() {
    vg::SystemSampler sampler;
    vg::AlertEngine alerts(sampler.profile());
    (void)sampler.sample(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(1050));
    const auto snapshot = sampler.sample(true);
    const auto& profile = sampler.profile();
    const auto& cpu = snapshot.cpu;
    const auto& ram = snapshot.memory;
    const auto& integrated = snapshot.integratedGpu;
    const auto& gpu = snapshot.discreteGpu;
    const auto& fans = snapshot.fans;
    const char* mode = fans.cpuMode == vg::FanMode::Maximum ? "maximum"
        : fans.cpuMode == vg::FanMode::Automatic ? "automatic" : "unknown";

    std::cout << std::fixed << std::setprecision(1)
              << "Monitor Sistem · probe perangkat keras\n"
              << "Device: " << profile.displayName << " · " << profile.expectedCpu << '\n'
              << "Host: " << snapshot.hostname << " · " << snapshot.systemName << ' ' << snapshot.kernel << '\n'
              << "CPU: " << reading(cpu.usagePercent, "%", cpu.usageAvailable) << " · "
              << reading(cpu.averageClockGhz, " GHz", cpu.averageClockGhz > 0) << " · "
              << reading(cpu.temperatureC, " °C", cpu.temperatureC > 0) << '\n'
              << "CPU core temperatures:";
    if (cpu.coreTemperaturesC.empty()) {
        std::cout << " N/A";
    }
    for (const auto value : cpu.coreTemperaturesC) {
        std::cout << ' ' << value << " °C";
    }
    std::cout << "\nFan CPU: " << mode << " · " << rpm(fans.cpuRpm)
              << "\nFan GPU: " << rpm(fans.gpuRpm)
              << "\nRAM: " << gib(ram.usedBytes) << " / " << gib(ram.totalBytes)
              << " GiB · " << gib(ram.availableBytes) << " GiB available\n"
              << "Swap: ";
    if (ram.swapAvailable) {
        std::cout << gib(ram.swapUsedBytes) << " / " << gib(ram.swapTotalBytes) << " GiB";
    } else {
        std::cout << "N/A";
    }
    std::cout << "\nIntegrated GPU: "
              << (integrated.available ? integrated.name : "N/A") << " · "
              << reading(integrated.usagePercent, "%", integrated.utilizationAvailable)
              << (integrated.utilizationFromClients ? " (application load)" : "")
              << "\nDiscrete GPU: " << (gpu.available ? gpu.name : "N/A") << " · "
              << (gpu.sleeping ? "RTD3 sleep" : reading(gpu.temperatureC, " °C", gpu.temperatureC > 0))
              << "\nGraphics adapters:";
    if (snapshot.graphicsAdapters.empty()) {
        std::cout << " N/A";
    }
    for (const auto& name : snapshot.graphicsAdapters) {
        std::cout << "\n  " << name;
    }
    std::cout << "\nAC: " << (snapshot.battery.acOnline
                  ? (*snapshot.battery.acOnline ? "online" : "offline") : "N/A")
              << " · Battery: " << reading(snapshot.battery.percent, "%",
                  snapshot.battery.present && snapshot.battery.percent >= 0)
              << "\nDisk: " << gib(snapshot.disk.usedBytes) << " / " << gib(snapshot.disk.totalBytes) << " GiB"
              << "\nProcesses: " << snapshot.processes.size()
              << "\nAlerts: " << alerts.evaluate(snapshot).size() << '\n';
    return cpu.logicalCores > 0 && ram.totalBytes > 0 && snapshot.disk.totalBytes > 0 ? 0 : 1;
}
