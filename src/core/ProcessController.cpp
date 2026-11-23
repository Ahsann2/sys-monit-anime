#include "core/ProcessController.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace vg {
namespace {

bool looksLikeDesktopApplication(const ProcessInfo& process) {
    if (!process.project.empty() ||
        process.cgroup.find("vte-spawn-") != std::string::npos ||
        process.cgroup.find("org.gnome.Terminal") != std::string::npos) {
        return false;
    }
    if (process.cgroup.find("app-gnome-") != std::string::npos ||
        process.cgroup.find("/app.slice/app-") != std::string::npos) {
        return true;
    }
    static const std::unordered_set<std::string> knownApplications{
        "chrome", "google-chrome", "chromium", "firefox", "code",
        "code-insiders", "slack", "discord", "spotify", "steam",
        "obs", "obsidian", "telegram-desktop", "nautilus",
        "android-studio",
        "emulator", "qemu-system-x86"
    };
    return knownApplications.contains(process.name);
}

} // namespace

bool ProcessController::ownedByCurrentUser(int pid) {
    std::ifstream status(
        "/proc/" + std::to_string(pid) + "/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("Uid:", 0) != 0) {
            continue;
        }
        std::istringstream parser(line.substr(4));
        unsigned int uid = 0;
        parser >> uid;
        return uid == getuid();
    }
    return false;
}

bool ProcessController::sendSignal(
    const ProcessInfo& process,
    int signal,
    std::string& error) {
#if defined(SYS_pidfd_open) && defined(SYS_pidfd_send_signal)
    const int descriptor = static_cast<int>(
        syscall(SYS_pidfd_open, process.pid, 0));
    if (descriptor < 0) {
        error = std::strerror(errno);
        return false;
    }
    const std::filesystem::path proc =
        "/proc/" + std::to_string(process.pid);
    std::ifstream stat(proc / "stat");
    std::string line;
    std::getline(stat, line);
    const auto commEnd = line.rfind(") ");
    std::uint64_t startTicks = 0;
    if (commEnd != std::string::npos) {
        std::istringstream fields(line.substr(commEnd + 2));
        std::string value;
        for (int index = 0; index < 19 && fields >> value; ++index) {}
        fields >> startTicks;
    }
    std::ifstream commFile(proc / "comm");
    std::string name;
    std::getline(commFile, name);
    std::ifstream cgroupFile(proc / "cgroup");
    std::string currentCgroup;
    while (std::getline(cgroupFile, line)) {
        const auto delimiter = line.rfind(':');
        if (delimiter != std::string::npos) {
            if (!currentCgroup.empty()) {
                currentCgroup += ';';
            }
            currentCgroup += line.substr(delimiter + 1);
        }
    }
    bool sameProject = true;
    if (!process.project.empty()) {
        std::error_code cwdError;
        const auto cwd = std::filesystem::read_symlink(proc / "cwd", cwdError);
        const auto relative = cwd.lexically_relative(process.project);
        sameProject = !cwdError && !relative.empty() &&
            *relative.begin() != "..";
    }
    if (startTicks == 0 || startTicks != process.startTicks ||
        name != process.name || !sameProject || currentCgroup.empty() ||
        currentCgroup != process.cgroup ||
        !ownedByCurrentUser(process.pid)) {
        close(descriptor);
        error = "identitas proses sudah berubah";
        return false;
    }

    const long sent = syscall(
        SYS_pidfd_send_signal, descriptor, signal, nullptr, 0);
    const int savedError = errno;
    close(descriptor);
    if (sent == 0) {
        return true;
    }
    error = std::strerror(savedError);
#else
    (void)process;
    (void)signal;
    error = "kernel tidak menyediakan pidfd";
#endif
    return false;
}

bool ProcessController::terminate(
    const ProcessInfo& process,
    bool force,
    std::string& error) {
    if (process.protectedProcess || process.pid <= 1 ||
        process.pid == static_cast<int>(getpid())) {
        error = "protected process";
        return false;
    }
    return sendSignal(process, force ? SIGKILL : SIGTERM, error);
}

std::vector<ProcessInfo> ProcessController::applicationCandidates(
    const std::vector<ProcessInfo>& processes) {
    std::vector<ProcessInfo> result;
    const int ownPid = static_cast<int>(getpid());
    for (const auto& process : processes) {
        if (process.pid == ownPid || process.protectedProcess ||
            !looksLikeDesktopApplication(process)) {
            continue;
        }
        result.push_back(process);
    }
    std::sort(result.begin(), result.end(),
        [](const ProcessInfo& left, const ProcessInfo& right) {
            return left.residentBytes > right.residentBytes;
        });
    return result;
}

std::vector<ProcessInfo> ProcessController::memoryRescueCandidates(
    const std::vector<ProcessInfo>& processes,
    std::uint64_t minimumResidentBytes) {
    auto candidates = applicationCandidates(processes);
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
            [minimumResidentBytes](const ProcessInfo& process) {
                return process.residentBytes < minimumResidentBytes;
            }),
        candidates.end());
    return candidates;
}

ProcessActionResult ProcessController::closeApplications(
    const std::vector<ProcessInfo>& processes,
    bool force) {
    ProcessActionResult result;
    for (const auto& process : applicationCandidates(processes)) {
        std::string error;
        if (terminate(process, force, error)) {
            ++result.succeeded;
        } else {
            ++result.skipped;
            result.errors.push_back(
                process.name + " (" + std::to_string(process.pid) +
                "): " + error);
        }
    }
    return result;
}

} // namespace vg
