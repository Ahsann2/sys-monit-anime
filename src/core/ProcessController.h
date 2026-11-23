#pragma once

#include "core/Types.h"

#include <string>
#include <vector>

namespace vg {

struct ProcessActionResult {
    std::size_t succeeded{0};
    std::size_t skipped{0};
    std::vector<std::string> errors;
};

class ProcessController {
public:
    [[nodiscard]] static bool terminate(
        const ProcessInfo& process,
        bool force,
        std::string& error);
    [[nodiscard]] static std::vector<ProcessInfo> applicationCandidates(
        const std::vector<ProcessInfo>& processes);
    [[nodiscard]] static std::vector<ProcessInfo> memoryRescueCandidates(
        const std::vector<ProcessInfo>& processes,
        std::uint64_t minimumResidentBytes =
            300ULL * 1024ULL * 1024ULL);
    [[nodiscard]] static ProcessActionResult closeApplications(
        const std::vector<ProcessInfo>& processes,
        bool force = false);

private:
    [[nodiscard]] static bool ownedByCurrentUser(int pid);
    [[nodiscard]] static bool sendSignal(
        const ProcessInfo& process,
        int signal,
        std::string& error);
};

} // namespace vg
