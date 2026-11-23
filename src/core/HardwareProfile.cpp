#include "core/HardwareProfile.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace vg {
namespace {

#if defined(__linux__)
std::string readFirstLine(const fs::path& path) {
    std::ifstream stream(path);
    std::string value;
    std::getline(stream, value);
    const auto terminator = value.find('\0');
    if (terminator != std::string::npos) {
        value.resize(terminator);
    }
    return value;
}
#endif

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

} // namespace

HardwareProfile HardwareProfile::vivoBookPro15Oled() {
    HardwareProfile profile;
    profile.displayName = "ASUS VivoBook Pro 15 OLED";
    profile.expectedCpu = "AMD Ryzen 5 5600H";
    profile.expectedDiscreteGpu = "NVIDIA GeForce RTX 3050 Laptop GPU";
    profile.expectedIntegratedGpu = "AMD Radeon Graphics";
    profile.expectedRamBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    profile.cpuWarningC = 88.0;
    profile.cpuCriticalC = 96.0;
    profile.gpuWarningC = 80.0;
    profile.gpuCriticalC = 86.0;
    profile.memoryWarningPercent = 85.0;
    profile.memoryCriticalPercent = 93.0;
    profile.diskWarningPercent = 90.0;
    profile.diskCriticalPercent = 96.0;
    profile.batteryAwareDiscreteGpu = true;
    profile.preferredCpuSensors = {
        "k10temp", "zenpower", "asus_wmi_sensors", "acpitz"
    };
    profile.protectedProcessNames = {
        "systemd", "systemd-userwor", "gnome-shell", "gnome-session-b",
        "gnome-session-c", "dbus-daemon", "dbus-broker", "pipewire",
        "pipewire-pulse", "wireplumber", "Xorg", "Xwayland",
        "NetworkManager", "gdm-wayland-ses", "gdm-x-session",
        "xdg-desktop-por", "vivoguard", "codex", "aimcp",
        "gnome-terminal-", "pulseaudio", "wireplumber"
    };
    return profile;
}

HardwareProfile HardwareProfile::forDevice(
    const std::string& vendor, const std::string& product) {
    HardwareProfile profile = vivoBookPro15Oled();
    const std::string model = lowercase(product);
    const bool asus = lowercase(vendor).find("asus") != std::string::npos;
    if (asus && model.find("m3500") != std::string::npos) {
        return profile;
    }

    if (asus && model == "gl752vw") {
        profile.displayName = "ASUS ROG GL752VW";
        profile.expectedCpu = "Intel Core i7-6700HQ";
        profile.expectedDiscreteGpu = "NVIDIA GeForce GTX 960M";
        profile.expectedIntegratedGpu = "Intel HD Graphics 530";
        profile.expectedRamBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL;
        profile.cpuWarningC = 85.0;
        profile.cpuCriticalC = 95.0;
        profile.gl752vwFanControl = true;
        profile.preferredCpuSensors = {"coretemp", "asus", "acpitz"};
        return profile;
    }

    profile.displayName = product.empty() ? "Linux" : product;
    profile.expectedCpu = "CPU";
    profile.expectedDiscreteGpu = "GPU diskrit";
    profile.expectedIntegratedGpu = "GPU terintegrasi";
    profile.expectedRamBytes = 0;
    profile.preferredCpuSensors = {"coretemp", "k10temp", "zenpower",
                                   "asus_wmi_sensors", "acpitz"};
    return profile;
}

#if defined(__linux__)
HardwareProfile HardwareProfile::detect() {
    auto product = readFirstLine("/sys/class/dmi/id/product_name");
    if (product.empty()) {
        product = readFirstLine("/sys/firmware/devicetree/base/model");
    }
    auto profile = forDevice(readFirstLine("/sys/class/dmi/id/sys_vendor"),
                             product);
    if (profile.expectedCpu == "CPU") {
        std::ifstream info("/proc/cpuinfo");
        for (std::string line; std::getline(info, line);) {
            const auto separator = line.find(':');
            if (separator != std::string::npos && line.starts_with("model name")) {
                const auto start = line.find_first_not_of(" \t", separator + 1);
                if (start != std::string::npos) {
                    profile.expectedCpu = line.substr(start);
                    break;
                }
            }
        }
    }
    return profile;
}
#endif

} // namespace vg
