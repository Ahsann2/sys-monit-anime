#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

bool readMessage(std::string& payload) {
    std::uint32_t length = 0;
    if (!std::cin.read(
            reinterpret_cast<char*>(&length),
            static_cast<std::streamsize>(sizeof(length)))) {
        return false;
    }
    if (length > 1024U * 1024U) {
        return false;
    }
    payload.resize(length);
    return static_cast<bool>(std::cin.read(
        payload.data(),
        static_cast<std::streamsize>(length)));
}

void writeMessage(const std::string& payload) {
    const auto length = static_cast<std::uint32_t>(payload.size());
    std::cout.write(
        reinterpret_cast<const char*>(&length),
        static_cast<std::streamsize>(sizeof(length)));
    std::cout.write(
        payload.data(),
        static_cast<std::streamsize>(payload.size()));
    std::cout.flush();
}

fs::path commandPath(const std::string& browser) {
    const std::string fileName =
        "browser-command-" + browser;
    const char* runtimeDirectory = std::getenv("XDG_RUNTIME_DIR");
    if (runtimeDirectory != nullptr && *runtimeDirectory != '\0') {
        return fs::path{runtimeDirectory} /
            "vivoguard" / fileName;
    }
    return fs::path{"/tmp"} /
        ("vivoguard-" + std::to_string(getuid())) /
        "vivoguard" / fileName;
}

std::string consumeCommand(const std::string& request) {
    const std::string browser =
        request.find("\"browser\":\"firefox\"") != std::string::npos
        ? "firefox"
        : "chromium";
    const fs::path path = commandPath(browser);
    std::error_code error;
    if (!fs::is_regular_file(fs::symlink_status(path, error)) || error) {
        return {};
    }
    const auto modified = fs::last_write_time(path, error);
    if (error) {
        return {};
    }
    const auto age = fs::file_time_type::clock::now() - modified;
    if (age < std::chrono::seconds::zero() || age > std::chrono::seconds(10)) {
        fs::remove(path, error);
        return {};
    }
    std::ifstream stream(path);
    std::string command;
    std::getline(stream, command);
    stream.close();
    if (!command.empty()) {
        fs::remove(path, error);
    }
    return command;
}

} // namespace

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string request;
    while (readMessage(request)) {
        const std::string command = consumeCommand(request);
        if (command == "close_tabs") {
            writeMessage(
                R"({"command":"close_tabs","preserve_pinned":true})");
        } else {
            writeMessage(R"({"command":"noop"})");
        }
    }
    return 0;
}
