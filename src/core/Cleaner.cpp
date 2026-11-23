#include "core/Cleaner.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

namespace vg {
namespace {

struct CloseDirectory {
    void operator()(DIR* directory) const {
        closedir(directory);
    }
};

using Directory = std::unique_ptr<DIR, CloseDirectory>;

Directory openDirectory(int parentFd, const char* name) {
    constexpr int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;
    const int fd = openat(parentFd, name, flags);
    if (fd < 0) {
        return {};
    }
    Directory result(fdopendir(fd));
    if (!result) {
        close(fd);
    }
    return result;
}

Directory openPath(const fs::path& path) {
    if (!path.is_absolute()) {
        return {};
    }

    auto directory = openDirectory(AT_FDCWD, "/");
    for (const auto& component : path.relative_path()) {
        if (!directory || component == "..") {
            return {};
        }
        if (!component.empty() && component != ".") {
            directory = openDirectory(dirfd(directory.get()), component.c_str());
        }
    }
    return directory;
}

Directory openOwnedSubdirectory(int parentFd, const char* name, dev_t device) {
    auto directory = openDirectory(parentFd, name);
    struct stat details {};
    if (!directory || fstat(dirfd(directory.get()), &details) != 0 ||
        details.st_uid != getuid() || details.st_dev != device) {
        return {};
    }
    return directory;
}

bool isDotEntry(std::string_view name) {
    return name == "." || name == "..";
}

class CacheVisit {
public:
    CacheVisit(const CleanupTarget& target, CleanupResult* result)
        : result_(result),
          cutoff_(target.olderThanDays > 0
              ? std::time(nullptr) - static_cast<std::time_t>(target.olderThanDays) * 86400
              : 0) {}

    std::uint64_t bytes() const {
        return bytes_;
    }

    void visit(DIR* directory, const fs::path& path, dev_t device, bool recurse) {
        const int fd = dirfd(directory);
        while (const auto* entry = readdir(directory)) {
            const char* name = entry->d_name;
            if (isDotEntry(name)) {
                continue;
            }
            struct stat details {};
            if (fstatat(fd, name, &details, AT_SYMLINK_NOFOLLOW) != 0 ||
                details.st_uid != getuid() || details.st_dev != device) {
                continue;
            }
            if (recurse && S_ISDIR(details.st_mode)) {
                auto child = openOwnedSubdirectory(fd, name, device);
                if (child) {
                    visit(child.get(), path / name, device, true);
                }
                continue;
            }
            if (!S_ISREG(details.st_mode) ||
                (cutoff_ != 0 && details.st_mtim.tv_sec > cutoff_)) {
                continue;
            }

            const auto size = static_cast<std::uint64_t>(details.st_size);
            if (result_ == nullptr) {
                bytes_ += size;
            } else if (unlinkat(fd, name, 0) == 0) {
                bytes_ += size;
                ++result_->filesRemoved;
            } else if (errno != ENOENT) {
                const auto error = std::error_code(errno, std::generic_category());
                result_->errors.push_back((path / name).string() + ": " + error.message());
            }
        }
    }

    void visitBrowser(DIR* directory, const CleanupTarget& target, dev_t device) {
        static constexpr std::array chromiumCaches{"Cache", "Code Cache", "GPUCache"};
        static constexpr std::array firefoxCaches{"cache2"};
        const int fd = dirfd(directory);
        while (const auto* entry = readdir(directory)) {
            const char* name = entry->d_name;
            if (isDotEntry(name)) {
                continue;
            }
            auto profile = openOwnedSubdirectory(fd, name, device);
            if (!profile) {
                continue;
            }
            const auto profilePath = target.path / name;
            if (target.id == "chrome-cache") {
                visitProfile(dirfd(profile.get()), profilePath, device, chromiumCaches);
            } else {
                visitProfile(dirfd(profile.get()), profilePath, device, firefoxCaches);
            }
        }
    }

private:
    void visitProfile(int fd, const fs::path& path, dev_t device, const auto& cacheNames) {
        for (const auto* cacheName : cacheNames) {
            auto directory = openOwnedSubdirectory(fd, cacheName, device);
            if (directory) {
                visit(directory.get(), path / cacheName, device, true);
            }
        }
    }

    CleanupResult* result_;
    std::time_t cutoff_;
    std::uint64_t bytes_{0};
};

std::uint64_t visitTarget(const CleanupTarget& target, CleanupResult* result) {
    auto directory = openPath(target.path);
    struct stat details {};
    const bool topLevelOnly = target.id == "old-temp";
    if (!directory || fstat(dirfd(directory.get()), &details) != 0 ||
        (!topLevelOnly && details.st_uid != getuid())) {
        return 0;
    }

    CacheVisit visitor(target, result);
    if (target.browserRelated) {
        visitor.visitBrowser(directory.get(), target, details.st_dev);
    } else {
        visitor.visit(directory.get(), target.path, details.st_dev, !topLevelOnly);
    }
    return visitor.bytes();
}

} // namespace

Cleaner::Cleaner() {
    const char* home = std::getenv("HOME");
    home_ = home == nullptr ? fs::path{} : fs::path{home};
}

std::vector<CleanupTarget> Cleaner::targets() const {
    std::vector<CleanupTarget> result;
    if (home_.empty()) {
        return result;
    }

    result.push_back({
        "thumbnails",
        "Cache Thumbnail",
        "Dibuat ulang secara otomatis oleh sistem desktop.",
        home_ / ".cache/thumbnails",
        0,
        true,
        false,
        false,
        0
    });
    result.push_back({
        "shader-cache",
        "Cache Mesa Shader",
        "Shader grafik yang dikompilasi, dapat dibuat ulang saat dibutuhkan.",
        home_ / ".cache/mesa_shader_cache",
        0,
        true,
        false,
        false,
        0
    });
    result.push_back({
        "font-cache",
        "Cache Indeks Font",
        "Indeks font lokal; dibuat ulang secara otomatis setelah dibersihkan.",
        home_ / ".cache/fontconfig",
        0,
        true,
        false,
        false,
        0
    });
    result.push_back({
        "chrome-cache",
        "Cache Browser Chromium",
        "Hanya cache halaman, kode, dan GPU yang dapat dibuat ulang.",
        home_ / ".cache/google-chrome",
        0,
        false,
        true,
        false,
        0
    });
    result.push_back({
        "firefox-cache",
        "Cache Browser Firefox",
        "Hanya cache konten Firefox dalam direktori cache2.",
        home_ / ".cache/mozilla/firefox",
        0,
        false,
        true,
        false,
        0
    });
    result.push_back({
        "old-temp",
        "Berkas Sementara Lama",
        "Hanya berkas reguler milik Anda di tingkat teratas /tmp dengan umur minimal 7 hari.",
        "/tmp",
        0,
        false,
        false,
        false,
        7
    });
    return result;
}

std::vector<CleanupTarget> Cleaner::scan() const {
    auto result = targets();
    for (auto& target : result) {
        target.sizeBytes = visitTarget(target, nullptr);
    }
    return result;
}

CleanupResult Cleaner::clean(const std::vector<std::string>& targetIds) const {
    CleanupResult result;
    const std::unordered_set<std::string> selected{targetIds.begin(), targetIds.end()};
    for (const auto& target : targets()) {
        if (selected.contains(target.id)) {
            result.bytesFreed += visitTarget(target, &result);
        }
    }
    return result;
}

} // namespace vg
