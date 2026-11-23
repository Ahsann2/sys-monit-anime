#include "core/Cleaner.h"
#include "core/ProcessController.h"

namespace vg {

Cleaner::Cleaner() = default;

std::vector<CleanupTarget> Cleaner::scan() const {
    return {};
}

CleanupResult Cleaner::clean(const std::vector<std::string>&) const {
    CleanupResult result;
    result.errors.emplace_back("Pembersihan belum tersedia di platform ini.");
    return result;
}

bool ProcessController::terminate(const ProcessInfo&, bool, std::string& error) {
    error = "Penghentian proses belum tersedia di platform ini.";
    return false;
}

std::vector<ProcessInfo> ProcessController::applicationCandidates(
    const std::vector<ProcessInfo>&) {
    return {};
}

std::vector<ProcessInfo> ProcessController::memoryRescueCandidates(
    const std::vector<ProcessInfo>&, std::uint64_t) {
    return {};
}

ProcessActionResult ProcessController::closeApplications(
    const std::vector<ProcessInfo>& processes, bool) {
    ProcessActionResult result;
    result.skipped = processes.size();
    return result;
}

} // namespace vg
