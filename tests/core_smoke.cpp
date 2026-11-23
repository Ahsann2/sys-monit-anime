#include "core/AlertEngine.h"
#include "core/Cleaner.h"
#include "core/IntelGpuUsage.h"
#include "core/ProcessController.h"
#include "core/SystemSampler.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <thread>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool checksIntelGpuUsage() {
    namespace fs = std::filesystem;
    std::string directory = (fs::temp_directory_path() / "monitor-intel-XXXXXX").string();
    if (!mkdtemp(directory.data())) {
        return false;
    }
    const struct Scratch {
        fs::path path;
        ~Scratch() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } scratch{directory};

    const auto device = scratch.path / "0000:00:02.0";
    fs::create_directories(device / "drm/renderD128");
    const auto proc = scratch.path / "proc";
    const auto writeClient = [&](int fd, int id, int render, int video,
                                 std::string_view address = "0000:00:02.0") {
        const auto process = proc / "123";
        const auto name = std::to_string(fd);
        fs::create_directories(process / "fd");
        fs::create_directories(process / "fdinfo");
        const auto link = process / "fd" / name;
        if (!fs::is_symlink(link)) {
            fs::create_symlink("/dev/dri/renderD128", link);
        }
        std::ofstream(process / "fdinfo" / name)
            << "drm-driver:\ti915\ndrm-pdev:\t" << address
            << "\ndrm-client-id:\t" << id
            << "\ndrm-engine-render:\t" << render << " ns"
            << "\ndrm-engine-video:\t" << video << " ns"
            << "\ndrm-engine-capacity-video:\t2\n";
    };
    writeClient(4, 1, 100, 100);
    writeClient(5, 1, 100, 100);
    writeClient(6, 2, 100, 100);
    writeClient(7, 3, 100, 100, "0000:01:00.0");

    vg::IntelGpuUsage usage(proc);
    const auto start = std::chrono::steady_clock::now();
    using namespace std::chrono_literals;
    if (usage.sample(device, start)) {
        return false;
    }
    writeClient(4, 1, 300, 700);
    writeClient(5, 1, 300, 700);
    writeClient(6, 2, 200, 500);
    writeClient(7, 3, 5000, 5000, "0000:01:00.0");
    if (usage.sample(device, start + 1000ns) != 50.0) {
        return false;
    }
    writeClient(4, 1, 200, 700);
    writeClient(5, 1, 200, 700);
    if (usage.sample(device, start + 2000ns) != 0.0) {
        return false;
    }
    writeClient(4, 1, 400, 700);
    writeClient(5, 1, 400, 700);
    if (usage.sample(device, start + 3000ns) != 10.0) {
        return false;
    }
    fs::remove_all(proc / "123");
    return !usage.sample(device, start + 4000ns);
}

bool checksCleanupScope() {
    namespace fs = std::filesystem;
    std::string directory = (fs::temp_directory_path() / "vivoguard-cleaner-XXXXXX").string();
    if (!mkdtemp(directory.data())) {
        return false;
    }
    const struct Scratch {
        fs::path path;
        ~Scratch() {
            std::error_code error;
            fs::remove_all(path, error);
        }
    } scratch{directory};

    const auto makeFile = [](const fs::path& file, std::string_view content) {
        fs::create_directories(file.parent_path());
        std::ofstream(file) << content;
    };
    const auto cleanerFor = [](const fs::path& home) {
        const char* originalHome = std::getenv("HOME");
        const bool hadHome = originalHome != nullptr;
        const std::string savedHome = hadHome ? originalHome : "";
        setenv("HOME", home.c_str(), 1);
        vg::Cleaner cleaner;
        if (hadHome) {
            setenv("HOME", savedHome.c_str(), 1);
        } else {
            unsetenv("HOME");
        }
        return cleaner;
    };
    const auto sizeFor = [](const auto& targets, std::string_view id) {
        const auto match = std::find_if(targets.begin(), targets.end(),
            [id](const auto& item) { return item.id == id; });
        return match == targets.end() ? 0 : match->sizeBytes;
    };

    const auto home = scratch.path / "home";
    const auto outside = scratch.path / "private";
    const auto sentinel = outside / "thumbnails" / "draft";
    const auto thumbnails = home / ".cache/thumbnails";
    const auto chromium = home / ".cache/google-chrome/Default";
    const auto firefox = home / ".cache/mozilla/firefox/profile";
    const auto trash = home / ".local/share/Trash/files/draft";
    makeFile(sentinel, "private draft");
    makeFile(thumbnails / "nested/icon", "abc");
    makeFile(chromium / "Cache/entry", "defg");
    makeFile(chromium / "Cookies", "cookie");
    makeFile(firefox / "cache2/entries/item", "hi");
    makeFile(firefox / "logins.json", "login");
    makeFile(trash, "trashed draft");
    fs::create_symlink(sentinel, thumbnails / "linked-file");
    fs::create_directory_symlink(outside, thumbnails / "linked-dir");
    if (mkfifo((thumbnails / "active-pipe").c_str(), 0600) != 0) {
        return false;
    }
    const auto redirectedHome = scratch.path / "redirected-home";
    fs::create_directory(redirectedHome);
    fs::create_directory_symlink(outside, redirectedHome / ".cache");

    const auto cleaner = cleanerFor(home);
    const auto targets = cleaner.scan();
    const auto oldTemp = std::find_if(targets.begin(), targets.end(),
        [](const auto& item) { return item.id == "old-temp"; });
    if (sizeFor(targets, "thumbnails") != 3 || sizeFor(targets, "chrome-cache") != 4 ||
        sizeFor(targets, "firefox-cache") != 2 || oldTemp == targets.end() ||
        oldTemp->selectedByDefault ||
        std::any_of(targets.begin(), targets.end(),
            [](const auto& item) { return item.id == "trash"; })) {
        return false;
    }

    const auto redirected = cleanerFor(redirectedHome);
    if (sizeFor(redirected.scan(), "thumbnails") != 0 ||
        redirected.clean({"thumbnails"}).filesRemoved != 0) {
        return false;
    }
    const auto result = cleaner.clean({"thumbnails", "chrome-cache", "firefox-cache", "trash"});
    return result.bytesFreed == 9 && result.filesRemoved == 3 && result.errors.empty() &&
        !fs::exists(thumbnails / "nested/icon") &&
        !fs::exists(chromium / "Cache/entry") &&
        !fs::exists(firefox / "cache2/entries/item") &&
        fs::is_symlink(thumbnails / "linked-file") &&
        fs::is_symlink(thumbnails / "linked-dir") &&
        fs::is_fifo(thumbnails / "active-pipe") &&
        fs::exists(sentinel) && fs::exists(chromium / "Cookies") &&
        fs::exists(firefox / "logins.json") && fs::exists(trash);
}

bool checksProcessIdentity(vg::SystemSampler& sampler) {
    const pid_t child = fork();
    if (child == 0) {
        for (;;) {
            pause();
        }
    }
    if (child < 0) {
        return false;
    }

    const auto snapshot = sampler.sample();
    const auto process = std::find_if(snapshot.processes.begin(),
        snapshot.processes.end(), [child](const auto& p) { return p.pid == child; });
    bool passed = false;
    if (process != snapshot.processes.end()) {
        auto stale = *process;
        ++stale.startTicks;
        auto moved = *process;
        moved.cgroup += "/old-scope";
        std::string error;
        passed = !vg::ProcessController::terminate(stale, false, error)
            && error == "identitas proses sudah berubah"
            && !vg::ProcessController::terminate(moved, false, error)
            && error == "identitas proses sudah berubah"
            && vg::ProcessController::terminate(*process, false, error);
    }
    if (passed) {
        for (int attempt = 0; attempt < 100; ++attempt) {
            int status = 0;
            if (waitpid(child, &status, WNOHANG) == child) {
                return WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    return false;
}

} // namespace

int main() {
    vg::SystemSampler sampler;
    (void)sampler.sample(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    const vg::SystemSnapshot snapshot = sampler.sample(true);

    if (snapshot.cpu.logicalCores <= 0) {
        std::cerr << "No logical CPU cores detected\n";
        return 1;
    }
    if (snapshot.memory.totalBytes == 0) {
        std::cerr << "No system memory detected\n";
        return 2;
    }
    if (snapshot.disk.totalBytes == 0) {
        std::cerr << "No root filesystem detected\n";
        return 3;
    }
    if (snapshot.hostname.empty()) {
        std::cerr << "No hostname detected\n";
        return 4;
    }

    vg::AlertEngine engine;
    (void)engine.evaluate(snapshot);

    vg::Cleaner cleaner;
    const auto targets = cleaner.scan();
    if (targets.empty()) {
        std::cerr << "Cleaner target discovery failed\n";
        return 5;
    }

    const auto rog = vg::HardwareProfile::forDevice(
        "ASUSTeK COMPUTER INC.", "GL752VW");
    const auto generic = vg::HardwareProfile::forDevice("Other", "GL752VW");
    if (!rog.gl752vwFanControl || rog.preferredCpuSensors.front() != "coretemp"
        || generic.gl752vwFanControl ||
        !vg::HardwareProfile::forDevice("ASUS", "M3500QC").expectedCpu.starts_with("AMD")) {
        std::cerr << "Hardware profiles are not correctly separated\n";
        return 6;
    }
    if (!checksProcessIdentity(sampler)) {
        std::cerr << "Signal identity check failed\n";
        return 7;
    }
    if (!checksCleanupScope()) {
        std::cerr << "Cleaner scope check failed\n";
        return 8;
    }
    if (!checksIntelGpuUsage()) {
        std::cerr << "Intel GPU usage counters check failed\n";
        return 9;
    }

    std::cout << "core-smoke: PASS · "
              << snapshot.cpu.logicalCores << " logical cores · "
              << snapshot.processes.size() << " user processes · "
              << targets.size() << " safe cleanup targets\n";
    return 0;
}
