#pragma once

#include "core/HardwareProfile.h"
#include "core/Types.h"

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

namespace vg {

class AlertEngine {
public:
    explicit AlertEngine(
        HardwareProfile profile = HardwareProfile::detect());

    [[nodiscard]] std::vector<Alert> evaluate(
        const SystemSnapshot& snapshot);
    void resetContinuity() { states_.clear(); }
    [[nodiscard]] static Severity overallSeverity(
        const std::vector<Alert>& alerts);

private:
    struct RuleState {
        bool active{false};
        std::chrono::steady_clock::time_point activeSince{};
    };

    [[nodiscard]] bool sustained(
        const std::string& id,
        bool condition,
        std::chrono::seconds duration);
    [[nodiscard]] static std::string topMemoryConsumer(
        const SystemSnapshot& snapshot);

    HardwareProfile profile_;
    std::unordered_map<std::string, RuleState> states_;
};

} // namespace vg
